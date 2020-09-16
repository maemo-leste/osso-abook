#include <glib.h>
#include <glib-object.h>
#include <libedata-book/libedata-book.h>
#include <gtk/gtkprivate.h>

#include "config.h"

#include "osso-abook-account-manager.h"
#include "osso-abook-waitable.h"
#include "osso-abook-roster-manager.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-debug.h"
#include "osso-abook-string-list.h"
#include "osso-abook-marshal.h"
#include "osso-abook-enums.h"
#include "osso-abook-util.h"
#include "osso-abook-log.h"
#include "eds.h"

enum {
  ROSTER_CREATED,
  ROSTER_REMOVED,
  ACCOUNT_CREATED,
  ACCOUNT_CHANGED,
  ACCOUNT_REMOVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};

enum
{
  PROP_PRESENCE = 1,
  PROP_ACTIVE_ACCOUNTS_ONLY,
  PROP_ALLOWED_ACCOUNTS,
  PROP_ALLOWED_CAPABILITIES,
  PROP_REQUIRED_CAPABILITIES,
  PROP_ACCOUNT_PROTOCOL,
  PROP_RUNNING
};

enum ACCOUNT_MANAGER_STATE
{
  ACCOUNT_MANAGER_STATE_1 = 0x1,
  ACCOUNT_MANAGER_STATE_RUNNING = 0x2,
  ACCOUNT_MANAGER_STATE_ROSTERS_COMPLETED = 0x4,
  ACCOUNT_MANAGER_ACTIVE_ACCOUNTS_ONLY = 0x8,
};

#define DEFAULT_ALLOWED_CAPABILITIES \
  (OSSO_ABOOK_CAPS_ALL | OSSO_ABOOK_CAPS_CHAT_ADDITIONAL | \
  OSSO_ABOOK_CAPS_VOICE_ADDITIONAL | OSSO_ABOOK_CAPS_ADDRESSBOOK | \
  OSSO_ABOOK_CAPS_IMMUTABLE_STREAMS)

struct account_info
{
  OssoABookAccountManager *manager;
  TpAccount *account;
  TpConnectionManager *cm;
  OssoABookRoster *roster;
  unsigned char flags;
  volatile gint refcount;
};

typedef struct _OssoABookAccountManagerPrivate OssoABookAccountManagerPrivate;

static void osso_abook_account_manager_waitable_iface_init(
    OssoABookWaitableIface *iface);

static void osso_abook_account_manager_roster_manager_iface_init(
    OssoABookRosterManagerIface *iface);

struct _OssoABookAccountManagerPrivate
{
  TpDBusDaemon *tp_dbus;
  TpAccountManager *tp_am;
  EBookQuery *query;
  GHashTable *rosters;
  GHashTable *field_10;
  GHashTable *protocol_rosters;
  GHashTable *override_rosters;
  gulong account_ready_id;
  gulong account_removed_id;
  /* protocol_name -> vcard field */
  GHashTable *vcard_fields;
  GList *uri_schemes;
  int presence;
  OssoABookCapsFlags allowed_capabilities;
  GList *allowed_accounts;
  OssoABookCapsFlags required_capabilities;
  gchar *account_protocol;
  GList *closures;
  GError *error;
  int pending_accounts;
  unsigned char flags;
};

typedef struct _OssoABookAccountManagerPrivate OssoABookAccountManagerPrivate;

#define OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager) \
  ((OssoABookAccountManagerPrivate *)osso_abook_account_manager_get_instance_private(manager))

G_DEFINE_TYPE_WITH_CODE(
  OssoABookAccountManager,
  osso_abook_account_manager,
  G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(
      OSSO_ABOOK_TYPE_WAITABLE,
      osso_abook_account_manager_waitable_iface_init);
  G_IMPLEMENT_INTERFACE(
    OSSO_ABOOK_TYPE_ROSTER_MANAGER,
    osso_abook_account_manager_roster_manager_iface_init);
  G_ADD_PRIVATE(OssoABookAccountManager);
)

static OssoABookWaitableClosure *
osso_abook_account_manager_waitable_pop(OssoABookWaitable *waitable,
                                        OssoABookWaitableClosure *closure)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE((OssoABookAccountManager *)waitable);

  return osso_abook_list_pop(&priv->closures, closure);
}

static gboolean
osso_abook_account_manager_waitable_is_ready(OssoABookWaitable *waitable,
                                             const GError **error)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(OSSO_ABOOK_ACCOUNT_MANAGER(waitable));
  gboolean is_ready;

  if (priv->flags & ACCOUNT_MANAGER_STATE_1 && !priv->pending_accounts)
    is_ready = (priv->flags & (ACCOUNT_MANAGER_STATE_ROSTERS_COMPLETED|ACCOUNT_MANAGER_STATE_RUNNING)) == (ACCOUNT_MANAGER_STATE_ROSTERS_COMPLETED|ACCOUNT_MANAGER_STATE_RUNNING);
  else
    is_ready = FALSE;

  OSSO_ABOOK_NOTE(
        TP, "account-manager: %s (%d accounts pending), roster-manager: %s (%s) => %s",
        priv->flags & ACCOUNT_MANAGER_STATE_1 ? "ready" : "pending",
        priv->pending_accounts,
        priv->flags & ACCOUNT_MANAGER_STATE_ROSTERS_COMPLETED ? "ready" : "pending",
        priv->flags & ACCOUNT_MANAGER_STATE_RUNNING ? "running" : "idle",
        is_ready ? "ready" : "pending");

  if (error)
    *error = priv->error;

  return is_ready;
}

static void
osso_abook_account_manager_waitable_push(OssoABookWaitable *waitable,
                                         OssoABookWaitableClosure *closure)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE((OssoABookAccountManager *)waitable);

  osso_abook_list_push(&priv->closures, closure);
}

static void
osso_abook_account_manager_waitable_iface_init(OssoABookWaitableIface *iface)
{
  iface->pop = osso_abook_account_manager_waitable_pop;
  iface->is_ready = osso_abook_account_manager_waitable_is_ready;
  iface->push = osso_abook_account_manager_waitable_push;
}

static void
get_all_uri_schemes(OssoABookAccountManagerPrivate *priv)
{
  GHashTable *hash;
  const gchar *const *fields;
  GHashTableIter iter;
  struct account_info *info;

  osso_abook_string_list_free(priv->uri_schemes);
  g_hash_table_iter_init(&iter, priv->rosters);
  hash = g_hash_table_new((GHashFunc)&g_str_hash, (GEqualFunc)&g_str_equal);

  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&info))
  {
    for (fields = tp_account_get_uri_schemes(info->account); fields; fields++)
    {
      if (!*fields)
        break;

      if (!g_hash_table_lookup(hash, *fields))
      {
        OSSO_ABOOK_NOTE(TP, "adding secondary vcard field [%s]", *fields);

        g_hash_table_insert(hash, g_strdup(*fields), GINT_TO_POINTER(TRUE));
      }
    }
  }

  priv->uri_schemes = g_hash_table_get_keys(hash);
  g_hash_table_destroy(hash);
}

static void
check_pending_accounts(OssoABookAccountManager *manager)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  GError *error = NULL;

  if (priv->pending_accounts)
    OSSO_ABOOK_NOTE(TP, "accounts pending: %d", priv->pending_accounts);
  else if (priv->flags & ACCOUNT_MANAGER_STATE_1)
  {
    OSSO_ABOOK_NOTE(TP, "no more accounts pending...");
    get_all_uri_schemes(priv);
  }

  if (priv->flags & ACCOUNT_MANAGER_STATE_RUNNING)
  {
    GHashTableIter iter;
    struct account_info *info;
    gboolean pending = FALSE;

    OSSO_ABOOK_NOTE(TP, "searching pending rosters...");

    g_hash_table_iter_init(&iter, priv->rosters);

    while (g_hash_table_iter_next(&iter, 0, (gpointer *)&info))
    {
      if (info->flags & 1)
      {
        OSSO_ABOOK_NOTE(TP, "roster for %s is pending still",
                        tp_account_get_path_suffix(info->account));
        pending = TRUE;
        break;
      }
    }

    if (!pending)
      priv->flags |= ACCOUNT_MANAGER_STATE_ROSTERS_COMPLETED;
    else
      priv->flags &= !ACCOUNT_MANAGER_STATE_ROSTERS_COMPLETED;
  }

  if (osso_abook_waitable_is_ready(OSSO_ABOOK_WAITABLE(manager), &error) )
  {
    osso_abook_waitable_notify(OSSO_ABOOK_WAITABLE(manager), error);
    g_clear_error(&error);
  }
}

/*
  don't really like it, but I was not able to find a way to create
  in-memory ESource
*/
static ESource *
create_roster_source(const gchar *uid)
{
  gchar *_uid = g_strdup(uid);
  GError *error = NULL;
  ESourceRegistry *registry;
  ESource *source = NULL;

  registry = e_source_registry_new_sync(NULL, &error);

  if (error)
  {
    OSSO_ABOOK_WARN("Creating ESourceRegistry for uid %s failed - %s", uid,
                    error->message);
    g_clear_error(&error);
    goto err_out;
  }

  e_filename_make_safe(_uid);
  source = e_source_registry_ref_source(registry, _uid);

  if (source)
  {
    g_warn_if_fail(e_source_has_extension(source,
                                          E_SOURCE_EXTENSION_ADDRESS_BOOK));
    g_warn_if_fail(e_source_has_extension(source, E_SOURCE_EXTENSION_RESOURCE));
  }
  else
  {
    OSSO_ABOOK_NOTE(TP, "creating new EDS source %s for %s", _uid, uid);
    source = e_source_new_with_uid(_uid, NULL, &error);

    if (source)
    {
      ESourceBackend *backend =
          e_source_get_extension (source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
      ESourceResource *resource =
          e_source_get_extension (source, E_SOURCE_EXTENSION_RESOURCE);
      GList *sources = NULL;

      e_source_resource_set_identity(resource, uid);
      e_source_backend_set_backend_name (backend, "tp");
      e_source_set_display_name(source, uid);

      sources = g_list_append(sources, source);
      e_source_registry_create_sources_sync(registry, sources, NULL, &error);

      g_list_free(sources);
    }

    if (error)
    {
      OSSO_ABOOK_WARN("Creating ESource for uid %s failed - %s", uid,
                      error->message);
      g_clear_error(&error);

      if (source)
      {
        g_object_unref(source);
        source = NULL;
      }
    }
  }

  g_object_unref(registry);

err_out:

  g_free(_uid);

  return source;
}

static EBookQuery *
get_telepathy_not_blocked_query()
{
  static EBookQuery *query = NULL;

  if (!query)
  {
    query = e_book_query_vcard_field_test(OSSO_ABOOK_VCA_TELEPATHY_BLOCKED,
                                          E_BOOK_QUERY_IS,
                                          "yes");
    query = e_book_query_not(query, TRUE);
  }

  return e_book_query_ref(query);
}

static void
account_info_disconnect_account_signals(struct account_info *info)
{
  g_signal_handlers_disconnect_matched(info->account, G_SIGNAL_MATCH_DATA, 0, 0,
                                       NULL, NULL, info);
}

static void
account_info_unref(struct account_info *info)
{
  if (g_atomic_int_add(&info->refcount, -1) == 1)
  {
    account_info_disconnect_account_signals(info);
    g_object_unref(info->manager);
    g_object_unref(info->account);

    if (info->cm)
      g_object_unref(info->cm);

    g_slice_free(struct account_info, info);
  }
}

static void
roster_get_book_view_cb(EBook *book, EBookStatus status, EBookView *book_view,
                        gpointer user_data)
{
  struct account_info *info = user_data;
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(info->manager);
  TpAccount *account = info->account;
  const gchar *path_suffix = tp_account_get_path_suffix(account);
  const gchar *vcard_field = g_hash_table_lookup(
        priv->vcard_fields, tp_account_get_protocol_name(account));

  OSSO_ABOOK_NOTE(TP, "got book view for %s", path_suffix);

  if (status)
    osso_abook_handle_estatus(NULL, status, book);
  else if (info->flags & 1)
  {
    info->roster = osso_abook_roster_new(path_suffix, book_view, vcard_field);
    g_signal_emit(info->manager, signals[ROSTER_CREATED], 0, info->roster);

    if (priv->flags & ACCOUNT_MANAGER_STATE_RUNNING)
      osso_abook_roster_start(info->roster);
  }

  if (info->flags & 1)
  {
    info->flags = info->flags & ~1;
    check_pending_accounts(info->manager);
  }

  account_info_unref(info);

  if (book_view)
    g_object_unref(book_view);
}

static void
osso_abook_account_manager_create_roster(struct account_info *info)
{
  OssoABookAccountManager *manager = info->manager;
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  TpAccount *account = info->account;
  GError *error = NULL;
  const gchar *vcard_field = g_hash_table_lookup(
        priv->vcard_fields, tp_account_get_protocol_name(account));
  const gchar *path_suffix = tp_account_get_path_suffix(account);
  OssoABookCapsFlags caps = OSSO_ABOOK_CAPS_NONE;
  gchar *uid = NULL;

  if (priv->override_rosters)
  {
    uid = g_strdup(g_hash_table_lookup(priv->override_rosters, path_suffix));

    if (uid)
    {
      g_message("overriding roster UID for %s: %s\n", path_suffix, uid);
      caps |= OSSO_ABOOK_CAPS_ADDRESSBOOK;
    }
  }

  if (!vcard_field)
    return;

  if (info->flags & 2)
  {
    caps |= osso_abook_caps_from_account(account);

    if (caps & OSSO_ABOOK_CAPS_ADDRESSBOOK)
    {
      info->flags |= 1;

      if (osso_abook_roster_manager_is_running(
            OSSO_ABOOK_ROSTER_MANAGER(manager)))
      {
        ESource *source;

        if (!uid)
          uid = g_strdup(path_suffix);

        source = create_roster_source(uid);

        if (source)
        {
          EBook *book = e_book_new(source, &error);
          g_object_unref(source);

          if (book)
          {
            OSSO_ABOOK_NOTE(TP, "creating roster %s for %s contacts",
                            path_suffix, vcard_field);

            if (!priv->query)
              priv->query = get_telepathy_not_blocked_query();

            g_atomic_int_add(&info->refcount, 1);
            _osso_abook_async_get_book_view(book, priv->query, 0, 0,
                                            roster_get_book_view_cb, info, 0);
            g_object_unref(book);
          }
          else
          {
            OSSO_ABOOK_WARN("%s", error->message);
            g_clear_error(&error);
          }
        }

        g_free(uid);
      }
    }
  }
}

static void
osso_abook_account_manager_roster_manager_start(OssoABookRosterManager *manager)
{
  OssoABookAccountManager *am = OSSO_ABOOK_ACCOUNT_MANAGER(manager);
  OssoABookAccountManagerPrivate *priv = OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(am);
  GHashTableIter iter;
  struct account_info *info;

  OSSO_ABOOK_NOTE(TP, "running=%d",
                  priv->flags & ACCOUNT_MANAGER_STATE_RUNNING);

  if (!(priv->flags & ACCOUNT_MANAGER_STATE_RUNNING))
  {
    priv->flags |= ACCOUNT_MANAGER_STATE_RUNNING;
    g_object_notify(G_OBJECT(manager), "running");
    g_hash_table_iter_init(&iter, priv->rosters);

    while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&info))
      osso_abook_account_manager_create_roster(info);

    check_pending_accounts(am);
  }
}

static void
osso_abook_account_manager_destroy_account_info(struct account_info *info)
{
  info->flags &= ~1;

  if (info->roster)
  {
    OSSO_ABOOK_NOTE(TP, "removing roster %s",
                    tp_account_get_path_suffix(info->account));

    g_signal_emit(info->manager, signals[ROSTER_REMOVED], 0, info->roster);
    g_object_unref(info->roster);
    info->roster = NULL;
  }

  check_pending_accounts(info->manager);
}

static void
osso_abook_account_manager_roster_manager_stop(OssoABookRosterManager *manager)
{
  OssoABookAccountManager *am = OSSO_ABOOK_ACCOUNT_MANAGER(manager);
  OssoABookAccountManagerPrivate *priv = OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(am);

  OSSO_ABOOK_NOTE(TP, "running=%d",
                  priv->flags & ACCOUNT_MANAGER_STATE_RUNNING);

  if (priv->flags & ACCOUNT_MANAGER_STATE_RUNNING)
  {
    GHashTableIter iter;
    struct account_info *info;

    priv->flags &= ~ACCOUNT_MANAGER_STATE_RUNNING;
    g_object_notify(G_OBJECT(manager), "running");
    g_hash_table_iter_init(&iter, priv->rosters);

    while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&info))
      osso_abook_account_manager_destroy_account_info(info);

    check_pending_accounts(am);
  }
}

static GList *
osso_abook_account_manager_roster_manager_list_rosters(
    OssoABookRosterManager *manager)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(OSSO_ABOOK_ACCOUNT_MANAGER(manager));
  GList *rosters = NULL;
  GHashTableIter iter;
  struct account_info *info;

  g_hash_table_iter_init(&iter, priv->rosters);

  while (g_hash_table_iter_next(&iter, 0, (gpointer *)&info))
  {
    if (info->roster)
      rosters = g_list_prepend(rosters, info->roster);
  }

  return rosters;
}

static OssoABookRoster *
osso_abook_account_manager_roster_manager_get_roster(
    OssoABookRosterManager *manager, const char *account_name)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(OSSO_ABOOK_ACCOUNT_MANAGER(manager));
  struct account_info *info =
      g_hash_table_lookup(priv->rosters, account_name);

  if (info)
    return info->roster;

  return NULL;
}

static void
osso_abook_account_manager_roster_manager_iface_init(
    OssoABookRosterManagerIface *iface)
{
  iface->start = osso_abook_account_manager_roster_manager_start;
  iface->stop = osso_abook_account_manager_roster_manager_stop;
  iface->list_rosters = osso_abook_account_manager_roster_manager_list_rosters;
  iface->get_roster = osso_abook_account_manager_roster_manager_get_roster;

  signals[ROSTER_CREATED] = g_signal_lookup("roster-created",
                                            OSSO_ABOOK_TYPE_ROSTER_MANAGER);
  signals[ROSTER_REMOVED] = g_signal_lookup("roster-removed",
                                            OSSO_ABOOK_TYPE_ROSTER_MANAGER);
}

static void
osso_abook_account_manager_dispose(GObject *object)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(OSSO_ABOOK_ACCOUNT_MANAGER(object));

  if ( priv->error )
    g_clear_error(&priv->error);

  if (priv->account_ready_id)
  {
    g_signal_handler_disconnect(priv->tp_am, priv->account_ready_id);
    priv->account_ready_id = 0;
  }

  if (priv->account_removed_id)
  {
    g_signal_handler_disconnect(priv->tp_am, priv->account_removed_id);
    priv->account_removed_id = 0;
  }

  if (priv->tp_am)
  {
    g_object_unref(priv->tp_am);
    priv->tp_am = NULL;
  }

  if (priv->tp_dbus)
  {
    g_object_unref(priv->tp_dbus);
    priv->tp_dbus = NULL;
  }

  g_hash_table_remove_all(priv->rosters);
  g_hash_table_remove_all(priv->field_10);
  g_hash_table_remove_all(priv->protocol_rosters);
  g_hash_table_remove_all(priv->vcard_fields);
  osso_abook_waitable_reset(OSSO_ABOOK_WAITABLE(object));

  G_OBJECT_CLASS(osso_abook_account_manager_parent_class)->dispose(object);
}

static void
osso_abook_account_manager_finalize(GObject *object)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(OSSO_ABOOK_ACCOUNT_MANAGER(object));

  if (priv->query)
    e_book_query_unref(priv->query);

  if (priv->override_rosters)
    g_hash_table_destroy(priv->override_rosters);

  g_hash_table_destroy(priv->rosters);
  g_hash_table_destroy(priv->field_10);
  g_hash_table_destroy(priv->protocol_rosters);
  g_hash_table_destroy(priv->vcard_fields);
  osso_abook_string_list_free(priv->uri_schemes);
  g_free(priv->account_protocol);
  g_list_free(priv->allowed_accounts);

  G_OBJECT_CLASS(osso_abook_account_manager_parent_class)->finalize(object);
}

static void
osso_abook_account_manager_account_created(OssoABookAccountManager *manager,
                                           TpAccount *account)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  struct account_info *info;

  info = g_hash_table_lookup(priv->rosters,
                             tp_account_get_path_suffix(account));

  g_return_if_fail(NULL != info);

  osso_abook_account_manager_create_roster(info);

  if (!(priv->flags & ACCOUNT_MANAGER_STATE_1))
  {
    priv->pending_accounts--;
    check_pending_accounts(manager);
  }
}

static void
async_book_remove_cb(EBook *book, EBookStatus status, gpointer closure)
{
  if (status)
    g_warning("%s: cannot remove book: %d", __FUNCTION__, status);

  g_object_unref(book);
}

static void
osso_abook_account_manager_account_removed(OssoABookAccountManager *manager,
                                           TpAccount *account)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  struct account_info *info;

  info = g_hash_table_lookup(priv->rosters,
                             tp_account_get_path_suffix(account));
  if (!info)
    return;

  if (info->roster)
  {
    EBook *book = osso_abook_roster_get_book(info->roster);

    if (book)
    {
      if (info->flags & 4)
      {
        ESource *source = e_book_get_source (book);

        if (source)
          OSSO_ABOOK_NOTE(TP, "removing %s", e_source_get_uid(source));
        else
          OSSO_ABOOK_NOTE(TP, "removing unknown book");

        e_book_async_remove(g_object_ref(book), async_book_remove_cb, NULL);
      }
    }
  }

  osso_abook_account_manager_destroy_account_info(info);
}

static void
osso_abook_account_manager_get_property(GObject *object, guint property_id,
                                        GValue *value, GParamSpec *pspec)
{
  OssoABookAccountManager *manager = OSSO_ABOOK_ACCOUNT_MANAGER(object);

  switch ( property_id )
  {
    case PROP_PRESENCE:
    {
      g_value_set_uint(value, osso_abook_account_manager_get_presence(manager));
      break;
    }
    case PROP_ACTIVE_ACCOUNTS_ONLY:
    {
      g_value_set_boolean(
            value,
            osso_abook_account_manager_is_active_accounts_only(manager));
      break;
    }
    case PROP_ALLOWED_ACCOUNTS:
    {
      g_value_set_pointer(
            value, osso_abook_account_manager_get_allowed_accounts(manager));
      break;
    }
    case PROP_ALLOWED_CAPABILITIES:
    {
      g_value_set_flags(
            value,
            osso_abook_account_manager_get_allowed_capabilities(manager));
      break;
    }
    case PROP_REQUIRED_CAPABILITIES:
    {
      g_value_set_flags(
            value,
            osso_abook_account_manager_get_required_capabilities(manager));
      break;
    }
    case PROP_ACCOUNT_PROTOCOL:
    {
      g_value_set_string(
            value, osso_abook_account_manager_get_account_protocol(manager));
      break;
    }
    case PROP_RUNNING:
    {
      OssoABookAccountManagerPrivate *priv =
          OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);

      g_value_set_boolean(value, (priv->flags & ACCOUNT_MANAGER_STATE_RUNNING) == ACCOUNT_MANAGER_STATE_RUNNING);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static gboolean
accept_account(struct account_info *info)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(info->manager);
  OssoABookCapsFlags caps = OSSO_ABOOK_CAPS_NONE;
  TpConnection *connection;
  const gchar *protocol_name;
  GList *l;

  if (priv->flags & ACCOUNT_MANAGER_ACTIVE_ACCOUNTS_ONLY)
  {
    if (!tp_account_is_enabled(info->account))
    {
      OSSO_ABOOK_NOTE(TP, "rejecting inactive account: %s",
                      tp_account_get_path_suffix(info->account));
      return FALSE;
    }

    if (!tp_account_get_has_been_online(info->account))
    {
      OSSO_ABOOK_NOTE(TP, "rejecting account that hasn't been online: %s",
                      tp_account_get_path_suffix(info->account));
      return FALSE;
    }
  }

  connection = tp_account_get_connection(info->account);

  if (connection)
    caps = osso_abook_caps_from_tp_connection(connection);

  if (priv->allowed_capabilities != (caps | priv->allowed_capabilities))
  {
    OSSO_ABOOK_NOTE(
          TP, "rejecting account %s for unallowed caps: allowed=%d, found=%x",
          tp_account_get_path_suffix(info->account),
          priv->allowed_capabilities, caps);
    return FALSE;
  }

  if (priv->required_capabilities != (caps & priv->required_capabilities))
  {
    OSSO_ABOOK_NOTE(
          TP, "rejecting account %s for missing caps: required=%x, found=%x",
          tp_account_get_path_suffix(info->account),
          priv->required_capabilities, caps);
    return FALSE;
  }

  protocol_name = tp_account_get_protocol_name(info->account);

  if (protocol_name && priv->account_protocol &&
      !g_str_equal(priv->account_protocol, protocol_name))
  {
    OSSO_ABOOK_NOTE(
          TP,
          "rejecting account %s for being the wrong protocol: required='%s', found='%s'",
          tp_account_get_path_suffix(info->account),
          priv->account_protocol,
          protocol_name);
    return FALSE;
  }

  if (!priv->allowed_accounts)
    return TRUE;

  for (l = priv->allowed_accounts; l; l = l->next)
  {
    if (!l->data)
      return FALSE;

    if (l->data == info->account)
      return TRUE;
  }

  return FALSE;
}

static gboolean
update_visibility(struct account_info *info)
{
  gboolean was_visible;
  int visible;

  was_visible = info->flags & 2;
  visible = accept_account(info) & 1;

  info->flags = (info->flags & 0xFD) | (2 * visible);

  if ((!!(info->flags & 2)) == was_visible)
    return FALSE;

  OSSO_ABOOK_NOTE(TP, "visibility changed for %s (%d)",
      tp_account_get_path_suffix(info->account), visible);

  if (info->flags & 2)
    g_signal_emit(info->manager, signals[ACCOUNT_CREATED], 0, info->account);
  else
    g_signal_emit(info->manager, signals[ACCOUNT_REMOVED], 0, info->account);

  return TRUE;
}

static void
update_all_visibilities(OssoABookAccountManagerPrivate *priv)
{
  GHashTableIter iter;
  struct account_info *info;

  g_hash_table_iter_init(&iter, priv->rosters);

  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&info))
    update_visibility(info);
}

static void
osso_abook_account_manager_set_property(GObject *object, guint property_id,
                                        const GValue *value, GParamSpec *pspec)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(OSSO_ABOOK_ACCOUNT_MANAGER(object));

  switch (property_id)
  {
    case PROP_ACTIVE_ACCOUNTS_ONLY:
    {
      priv->flags = (priv->flags & ~ACCOUNT_MANAGER_ACTIVE_ACCOUNTS_ONLY) | (ACCOUNT_MANAGER_ACTIVE_ACCOUNTS_ONLY * g_value_get_boolean(value));
      update_all_visibilities(priv);
      break;
    }
    case PROP_ALLOWED_ACCOUNTS:
    {
      priv->allowed_accounts = g_list_copy(g_value_get_pointer(value));
      update_all_visibilities(priv);
      break;
    }
    case PROP_ALLOWED_CAPABILITIES:
    {
      priv->allowed_capabilities = g_value_get_flags(value);
      update_all_visibilities(priv);
      break;
    }
    case PROP_REQUIRED_CAPABILITIES:
    {
      priv->required_capabilities = g_value_get_flags(value);
      update_all_visibilities(priv);
      break;
    }
    case PROP_ACCOUNT_PROTOCOL:
    {
      priv->account_protocol = g_value_dup_string(value);
      update_all_visibilities(priv);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_account_manager_class_init(OssoABookAccountManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->get_property = osso_abook_account_manager_get_property;
  object_class->set_property = osso_abook_account_manager_set_property;
  object_class->dispose = osso_abook_account_manager_dispose;
  object_class->finalize = osso_abook_account_manager_finalize;

  klass->account_removed = osso_abook_account_manager_account_removed;
  klass->account_created = osso_abook_account_manager_account_created;

  g_object_class_install_property(
        object_class, PROP_PRESENCE,
        g_param_spec_uint(
          "presence",
          "Presence",
          "Current presence status of this session",
          0, G_MAXUINT, 0,
          GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_ACTIVE_ACCOUNTS_ONLY,
        g_param_spec_boolean(
          "active-accounts-only",
          "Active Accounts Only",
          "Don't report accounts which are disabled",
          TRUE,
          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
        object_class, PROP_ALLOWED_ACCOUNTS,
        g_param_spec_pointer(
          "allowed-accounts",
          "Allowed Accounts",
          "Accounts which may appear in the model (if all additional filters allow them)",
          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
        object_class, PROP_ALLOWED_CAPABILITIES,
        g_param_spec_flags(
          "allowed-capabilities",
          "Account Capabilities",
          "Set of of accepted account capabilities",
          OSSO_ABOOK_TYPE_CAPS_FLAGS, DEFAULT_ALLOWED_CAPABILITIES,
          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
        object_class, PROP_REQUIRED_CAPABILITIES,
        g_param_spec_flags(
          "required-capabilities",
          "Required Account Capabilities",
          "Set of of required account capabilities",
          OSSO_ABOOK_TYPE_CAPS_FLAGS, 0,
          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
        object_class, PROP_ACCOUNT_PROTOCOL,
        g_param_spec_string(
          "account-protocol",
          "Account Protocol",
          "Accepted account protocol (NULL for all protocols)",
          NULL,
          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_override_property(object_class, PROP_RUNNING, "running");

  signals[ACCOUNT_CREATED] =
      g_signal_new("account-created", OSSO_ABOOK_TYPE_ACCOUNT_MANAGER,
                   G_SIGNAL_RUN_FIRST,
                   G_STRUCT_OFFSET(OssoABookAccountManagerClass,
                                   account_created),
                   0, NULL, g_cclosure_marshal_VOID__OBJECT,
                   G_TYPE_NONE,
                   1, TP_TYPE_ACCOUNT);
  signals[ACCOUNT_CHANGED]=
      g_signal_new("account-changed", OSSO_ABOOK_TYPE_ACCOUNT_MANAGER,
                   G_SIGNAL_DETAILED | G_SIGNAL_RUN_FIRST,
                   G_STRUCT_OFFSET(OssoABookAccountManagerClass,
                                   account_changed),
                   0, NULL, osso_abook_marshal_VOID__OBJECT_UINT_BOXED,
                   G_TYPE_NONE,
                   3, TP_TYPE_ACCOUNT, G_TYPE_UINT, G_TYPE_VALUE);
  signals[ACCOUNT_REMOVED]=
      g_signal_new("account-removed", OSSO_ABOOK_TYPE_ACCOUNT_MANAGER,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(OssoABookAccountManagerClass,
                                   account_removed),
                   0, NULL, g_cclosure_marshal_VOID__OBJECT,
                   G_TYPE_NONE,
                   1, TP_TYPE_ACCOUNT);
}

static void
get_roster_overrides(OssoABookAccountManagerPrivate *priv)
{
  GKeyFile *keyfile = g_key_file_new();
  gchar *fname =
      g_build_filename(osso_abook_get_work_dir(), "rosters.conf", NULL);
  gchar **groups;
  gchar **group;

  #define OVERRIDE "override:"

  g_key_file_load_from_file(keyfile, fname, G_KEY_FILE_NONE, NULL);
  g_free(fname);
  groups = g_key_file_get_groups(keyfile, NULL);

  for(group = groups; *group; group++)
  {
    gchar *uri;
    const gchar *g = *group;

    if (g_str_has_prefix(g, OVERRIDE) &&
        (uri = g_key_file_get_string(keyfile, g, "uri", NULL)))
    {
      if (!priv->override_rosters)
      {
        priv->override_rosters =
            g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
      }

      g_hash_table_insert(
            priv->override_rosters,
            g_strchomp(g_strchug(g_strdup(&g[strlen(OVERRIDE)]))), uri);
    }
  }

  g_key_file_free(keyfile);
  g_strfreev(groups);
}

static void
account_validity_changed_cb(TpAccountManager *am, TpAccount *account,
                            gboolean valid, gpointer user_data)
{
  OssoABookAccountManager *manager = OSSO_ABOOK_ACCOUNT_MANAGER(user_data);
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  const gchar *path_suffix = tp_account_get_path_suffix(account);
  const gchar *protocol_name;
  struct account_info *info;

  OSSO_ABOOK_NOTE(TP, "account validity changed: %s", path_suffix);

  if (!valid)
    return;

  info = g_slice_new0(struct account_info);
  info->manager = g_object_ref(manager);
  info->refcount = 1;
  info->account = g_object_ref(account);

  g_hash_table_insert(priv->rosters, g_strdup(path_suffix), info);

#ifdef TEST
  g_print("!!!! FIXME\n");
#else
  g_assert(0);
#endif


  protocol_name = tp_account_get_protocol_name(info->account);

  if (protocol_name && *protocol_name)
  {
    GList *protocol_rosters = g_list_copy(
          g_hash_table_lookup(priv->protocol_rosters, protocol_name));

    g_hash_table_insert(priv->protocol_rosters, g_strdup(protocol_name),
                        g_list_prepend(protocol_rosters, info));

  if (priv->flags & ACCOUNT_MANAGER_STATE_1)
    get_all_uri_schemes(priv);

/*  g_signal_connect(info->account, "flag-changed", flag_changed_cb, info, 0, 0);
  g_signal_connect(info->account, "presence-changed", (GCallback)update_presence_status, info, 0, 0);
  g_signal_connect(info->account, "string-changed", (GCallback)string_changed_cb, info, 0, 0);
  g_signal_connect(info->account, "parameters-changed", (GCallback)parameters_changed_cb, info, 0, 0);
*/
  info->flags = (info->flags & 0xFD) | 2 * (accept_account(info) & 1);

  if (info->flags & 2)
  {
    g_signal_emit(manager, signals[ACCOUNT_CREATED], 0, info->account);
  }
  else if (!(priv->flags & ACCOUNT_MANAGER_STATE_1))
  {
    priv->pending_accounts--;
    check_pending_accounts(manager);
  }
}

static void
account_removed_cb(TpAccountManager *am, TpAccount *account,
                   gpointer user_data)
{
  OssoABookAccountManager *manager = OSSO_ABOOK_ACCOUNT_MANAGER(user_data);
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  const gchar *path_suffix = tp_account_get_path_suffix(account);
  struct account_info *info;

  OSSO_ABOOK_NOTE(TP, "account removed: %s", path_suffix);

  info = g_hash_table_lookup(priv->rosters, path_suffix);

  g_return_if_fail(NULL != info);

  account_info_disconnect_account_signals(info);

  info->flags |= 4;

#ifdef TEST
  g_print("!!!! FIXME\n");
#else
  g_assert(0);
#endif


  if (priv->flags & ACCOUNT_MANAGER_STATE_1)
    get_all_uri_schemes(priv);

  if ((info->flags & 2))
    g_signal_emit(user_data, signals[ACCOUNT_REMOVED], 0, info->account);

  if ((info->flags & 1))
  {
    info->flags &= ~1;
    check_pending_accounts(info->manager);
  }

  g_hash_table_remove(priv->rosters, path_suffix);
}

static void
connecion_prepared_cb(GObject *object, GAsyncResult *res, gpointer user_data)
{
  OssoABookAccountManager *manager = user_data;
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  TpConnection *connection = (TpConnection *)object;
  GError *error = NULL;

  if (!tp_proxy_prepare_finish (object, res, &error))
    OSSO_ABOOK_WARN("Error preparing connection: %s\n", error->message);

  account_validity_changed_cb(
        priv->tp_am, tp_connection_get_account(connection), TRUE, manager);
}

static void
am_prepared_cb(GObject *object, GAsyncResult *res, gpointer user_data)
{
  TpAccountManager *am = (TpAccountManager *)object;
  OssoABookAccountManager *manager = user_data;
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  GList *accounts, *l;
  GError *error = NULL;

  OSSO_ABOOK_NOTE(TP, "account manager %p ready", manager);

  if (priv->error)
    g_clear_error(&priv->error);

  if (!tp_proxy_prepare_finish (object, res, &error))
  {
    OSSO_ABOOK_WARN("Error preparing AM: %s\n", error->message);

    priv->error = g_error_copy(error);
  }
  else
  {
    accounts = tp_account_manager_dup_valid_accounts(am);

    for (l = accounts; l != NULL; l = l->next)
    {
      TpAccount *account = l->data;

      priv->pending_accounts++;
      TpConnection *connection = tp_account_get_connection(account);

      if (!connection)
        account_validity_changed_cb(am, account, TRUE, manager);
      else if (connection)
      {
        GQuark features[] = {
          TP_CONNECTION_FEATURE_CAPABILITIES,
          TP_CONNECTION_FEATURE_CONTACT_LIST,
          0
        };

        tp_proxy_prepare_async(connection, features, connecion_prepared_cb,
                               manager);
      }
    }

    g_list_free_full (accounts, g_object_unref);
  }

  priv->flags |= ACCOUNT_MANAGER_STATE_1;
  check_pending_accounts(manager);
}

static void
get_accounts(OssoABookAccountManager *manager)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  TpSimpleClientFactory *factory = tp_proxy_get_factory(priv->tp_am);

  tp_simple_client_factory_add_account_features_varargs(
        factory,
        TP_ACCOUNT_FEATURE_CONNECTION,
        0);

  tp_proxy_prepare_async (priv->tp_am, NULL, am_prepared_cb, manager);

  priv->account_ready_id =
      g_signal_connect(priv->tp_am, "account-validity-changed",
                       G_CALLBACK(account_validity_changed_cb), manager);

  priv->account_removed_id =
      g_signal_connect(priv->tp_am, "account-removed",
                       G_CALLBACK(account_removed_cb), manager);
}

static void
cms_ready_cb(GObject *object, GAsyncResult *res, gpointer user_data)
{
  OssoABookAccountManager *manager = OSSO_ABOOK_ACCOUNT_MANAGER(user_data);
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);
  GError *error = NULL;
  GList *cms = tp_list_connection_managers_finish (res, &error);

  if (error != NULL)
  {
    OSSO_ABOOK_WARN("Error getting list of CMs: %s", error->message);
    g_error_free (error);
  }
  else if (cms == NULL)
    OSSO_ABOOK_WARN("No Telepathy connection managers found");
  else
  {
    GList *cm;

    for (cm = cms; cm; cm = cm->next)
    {
      GList *protocols = tp_connection_manager_dup_protocols(cm->data);
      GList *p;

      for (p = protocols; p; p = p->next)
      {
        const gchar *vcard_field = tp_protocol_get_vcard_field(p->data);

        if (vcard_field)
        {
          const gchar *protocol_name = tp_protocol_get_name(p->data);

          OSSO_ABOOK_NOTE(TP, "adding %s primary vcard field: %s",
                          protocol_name, vcard_field);
          g_hash_table_insert(priv->vcard_fields, g_strdup(protocol_name),
                              g_strdup(vcard_field));
        }
      }

      g_list_free_full(protocols, g_object_unref);
    }
  }

  g_list_free_full(cms, g_object_unref);

  get_accounts(manager);
}

static void
osso_abook_account_manager_init(OssoABookAccountManager *manager)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);

  priv->rosters = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, (GDestroyNotify)account_info_unref);
  priv->protocol_rosters = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_list_free);
  priv->field_10 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  priv->tp_dbus = tp_dbus_daemon_dup(NULL);
  priv->tp_am = tp_account_manager_new(priv->tp_dbus);
  priv->vcard_fields =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
  priv->flags |= ACCOUNT_MANAGER_ACTIVE_ACCOUNTS_ONLY;

  get_roster_overrides(priv);
  tp_list_connection_managers_async(priv->tp_dbus, cms_ready_cb, manager);
}

TpConnectionPresenceType
osso_abook_account_manager_get_presence(OssoABookAccountManager *manager)
{
  if (!manager)
    manager = osso_abook_account_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ACCOUNT_MANAGER(manager),
                       TP_CONNECTION_PRESENCE_TYPE_UNSET);

  return OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager)->presence;
}

gboolean
osso_abook_account_manager_is_active_accounts_only(
    OssoABookAccountManager *manager)
{
  if (!manager)
    manager = osso_abook_account_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ACCOUNT_MANAGER(manager), TRUE);

  return OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager)->flags & ACCOUNT_MANAGER_ACTIVE_ACCOUNTS_ONLY;
}

GList *
osso_abook_account_manager_get_allowed_accounts(
    OssoABookAccountManager *manager)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_ACCOUNT_MANAGER(manager), NULL);

  return OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager)->allowed_accounts;
}

OssoABookCapsFlags
osso_abook_account_manager_get_allowed_capabilities(
    OssoABookAccountManager *manager)
{
  if (!manager)
    manager = osso_abook_account_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ACCOUNT_MANAGER(manager),
                       DEFAULT_ALLOWED_CAPABILITIES);

  return OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager)->allowed_capabilities;

}

OssoABookCapsFlags
osso_abook_account_manager_get_required_capabilities(
    OssoABookAccountManager *manager)
{
  if (!manager)
    manager = osso_abook_account_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ACCOUNT_MANAGER(manager), 0);

  return OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager)->required_capabilities;
}

const char *
osso_abook_account_manager_get_account_protocol(
    OssoABookAccountManager *manager)
{
  if (!manager)
    manager = osso_abook_account_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ACCOUNT_MANAGER (manager), NULL);

  return OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager)->account_protocol;
}

TpAccount *
osso_abook_account_manager_lookup_by_name(OssoABookAccountManager *manager,
                                          const char *name)
{
  OssoABookAccountManagerPrivate *priv;
  struct account_info *info;

  if (!manager)
    manager = osso_abook_account_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ACCOUNT_MANAGER(manager), NULL);
  g_return_val_if_fail(NULL != name, NULL);

  priv = OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager);

  info = g_hash_table_lookup(priv->rosters, name);

  if (info)
    return info->account;

  /* "priv->account_manager_ready"); */
  g_return_val_if_fail(priv->flags & ACCOUNT_MANAGER_STATE_1, NULL);

  return NULL;
}

OssoABookAccountManager *
osso_abook_account_manager_get_default()
{
  static OssoABookAccountManager *am = NULL;

  if (!am)
    am = osso_abook_account_manager_new();

  return am;
}

OssoABookAccountManager *
osso_abook_account_manager_new(void)
{
  return g_object_new(OSSO_ABOOK_TYPE_ACCOUNT_MANAGER, NULL);
}

static gboolean
match_vcard_field(gpointer key, gpointer value, gpointer user_data)
{
  return g_strcmp0(value, user_data) == 0;
}

gboolean
osso_abook_account_manager_has_primary_vcard_field(
    OssoABookAccountManager *manager, const char *vcard_field)
{
  if (!manager)
    manager = osso_abook_account_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ACCOUNT_MANAGER(manager), FALSE);
  g_return_val_if_fail(NULL != vcard_field, FALSE);

  return
      g_hash_table_find(
        OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager)->vcard_fields,
        match_vcard_field, (gpointer)vcard_field) != NULL;
}

gboolean
osso_abook_account_manager_has_secondary_vcard_field(
    OssoABookAccountManager *manager, const char *vcard_field)
{
  if (!manager)
    manager = osso_abook_account_manager_get_default();

  g_return_val_if_fail(OSSO_ABOOK_IS_ACCOUNT_MANAGER(manager), FALSE);
  g_return_val_if_fail(NULL != vcard_field, FALSE);

  return g_list_find_custom(
        OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(manager)->uri_schemes,
        vcard_field, (GCompareFunc)&strcmp) != NULL;
}
