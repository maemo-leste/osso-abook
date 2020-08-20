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
  ACCOUNT_MANAGER_STATE_8 = 0x8,
};

struct roster
{
  OssoABookAccountManager *manager;
  TpAccount *account;
  //McProfile *profile;
  OssoABookRoster *roster;
  unsigned char flags;
  gint refcount;
};


typedef struct _OssoABookAccountManagerPrivate OssoABookAccountManagerPrivate;

static void osso_abook_account_manager_waitable_iface_init(
    OssoABookWaitableIface *iface);

static void osso_abook_account_manager_roster_manager_iface_init(
    OssoABookRosterManagerIface *iface);

struct _OssoABookAccountManagerPrivate
{
  TpDBusDaemon *tp_dbus_daemon;
  TpAccountManager *account_manager;
  EBookQuery *query;
  GHashTable *pending_rosters;
  GHashTable *field_10;
  GHashTable *field_14;
  GHashTable *override_rosters;
  gulong account_ready_id;
  TpProxySignalConnection *account_removed_sc;
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
        MC, "account-manager: %s (%d accounts pending), roster-manager: %s (%s) => %s",
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
  struct roster *roster;

  osso_abook_string_list_free(priv->uri_schemes);
  g_hash_table_iter_init(&iter, priv->pending_rosters);
  hash = g_hash_table_new((GHashFunc)&g_str_hash, (GEqualFunc)&g_str_equal);

  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&roster))
  {
    for (fields = tp_account_get_uri_schemes(roster->account); fields; fields++)
    {
      if (!*fields)
        break;

      if (!g_hash_table_lookup(hash, *fields))
      {
        OSSO_ABOOK_NOTE(MC, "adding secondary vcard field [%s]", *fields);

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
    OSSO_ABOOK_NOTE(MC, "accounts pending: %d", priv->pending_accounts);
  else if (priv->flags & ACCOUNT_MANAGER_STATE_1)
  {
    OSSO_ABOOK_NOTE(MC, "no more accounts pending...");
    get_all_uri_schemes(priv);
  }

  if (priv->flags & ACCOUNT_MANAGER_STATE_RUNNING)
  {
    GHashTableIter iter;
    struct roster *roster;
    gboolean pending = FALSE;

    OSSO_ABOOK_NOTE(MC, "searching pending rosters...");

    g_hash_table_iter_init(&iter, priv->pending_rosters);

    while (g_hash_table_iter_next(&iter, 0, (gpointer *)&roster))
    {
      if (roster->flags & 1)
      {
        OSSO_ABOOK_NOTE(MC, "roster for %s is pending still",
                        tp_account_get_path_suffix(roster->account));
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

static void
osso_abook_account_manager_roster_manager_start(OssoABookRosterManager *manager)
{
  OssoABookAccountManager *am = OSSO_ABOOK_ACCOUNT_MANAGER(manager);
  OssoABookAccountManagerPrivate *priv = OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(am);
  GHashTableIter iter;
  struct roster *roster;

  OSSO_ABOOK_NOTE(MC, "running=%d",
                  priv->flags & ACCOUNT_MANAGER_STATE_RUNNING);

  if (!(priv->flags & ACCOUNT_MANAGER_STATE_RUNNING))
  {
    priv->flags |= ACCOUNT_MANAGER_STATE_RUNNING;
    g_object_notify(G_OBJECT(manager), "running");
    g_hash_table_iter_init(&iter, priv->pending_rosters);

    while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&roster))
      osso_abook_account_manager_create_roster(roster);

    check_pending_accounts(am);
  }
}

static void
osso_abook_account_manager_destroy_roster(struct roster *roster)
{
  roster->flags &= ~1;

  if (roster->roster)
  {
    OSSO_ABOOK_NOTE(MC, "removing roster %s",
                    tp_account_get_path_suffix(roster->account));

    g_signal_emit(roster->manager, signals[ROSTER_REMOVED], 0, roster->roster);
    g_object_unref(roster->roster);
    roster->roster = NULL;
  }

  check_pending_accounts(roster->manager);
}

static void
osso_abook_account_manager_roster_manager_stop(OssoABookRosterManager *manager)
{
  OssoABookAccountManager *am = OSSO_ABOOK_ACCOUNT_MANAGER(manager);
  OssoABookAccountManagerPrivate *priv = OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(am);

  OSSO_ABOOK_NOTE(MC, "running=%d",
                  priv->flags & ACCOUNT_MANAGER_STATE_RUNNING);

  if (priv->flags & ACCOUNT_MANAGER_STATE_RUNNING)
  {
    GHashTableIter iter;
    struct roster *roster;

    priv->flags &= ~ACCOUNT_MANAGER_STATE_RUNNING;
    g_object_notify(G_OBJECT(manager), "running");
    g_hash_table_iter_init(&iter, priv->pending_rosters);

    while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&roster))
      osso_abook_account_manager_destroy_roster(roster);

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
  struct roster *roster;

  g_hash_table_iter_init(&iter, priv->pending_rosters);

  while (g_hash_table_iter_next(&iter, 0, (gpointer *)&roster))
  {
    if (roster->roster)
      rosters = g_list_prepend(rosters, roster->roster);
  }

  return rosters;
}

static OssoABookRoster *
osso_abook_account_manager_roster_manager_get_roster(
    OssoABookRosterManager *manager, const char *account_name)
{
  OssoABookAccountManagerPrivate *priv =
      OSSO_ABOOK_ACCOUNT_MANAGER_PRIVATE(OSSO_ABOOK_ACCOUNT_MANAGER(manager));
  struct roster *roster =
      g_hash_table_lookup(priv->pending_rosters, account_name);

  if (roster)
    return roster->roster;

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
          OSSO_ABOOK_TYPE_CAPS_FLAGS, OSSO_ABOOK_CAPS_ALL |
            OSSO_ABOOK_CAPS_CHAT_ADDITIONAL | OSSO_ABOOK_CAPS_VOICE_ADDITIONAL |
            OSSO_ABOOK_CAPS_ADDRESSBOOK |
            OSSO_ABOOK_CAPS_IMMUTABLE_STREAMS,
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
                                   account_created),
                   0, NULL, g_cclosure_marshal_VOID__OBJECT,
                   G_TYPE_NONE,
                   1, TP_TYPE_ACCOUNT);
}
