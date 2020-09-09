#include <glib.h>
#include <glib-object.h>
#include <libedata-book/libedata-book.h>
#include <gtk/gtkprivate.h>
#include <dbus/dbus-glib-bindings.h>

#include <sys/types.h>
#include <unistd.h>

#include "config.h"

#include "osso-abook-aggregator.h"
#include "osso-abook-debug.h"
#include "osso-abook-roster.h"
#include "osso-abook-waitable.h"
#include "eds.h"
#include "osso-abook-errors.h"
#include "osso-abook-util.h"
#include "osso-abook-marshal.h"
#include "osso-abook-enums.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-voicemail-contact.h"

enum OSSO_ABOOK_AGGREGATOR_FLAGS
{
  OSSO_ABOOK_AGGREGATOR_FLAGS_1 = 1,
  OSSO_ABOOK_AGGREGATOR_FLAGS_2 = 2,
  OSSO_ABOOK_AGGREGATOR_FLAGS_4 = 4,
  OSSO_ABOOK_AGGREGATOR_FLAGS_SYSTEM_BOOK = 8,
};

enum {
  PROP_ROSTER_MANAGER = 1,
  PROP_STATE,
  PROP_MASTER_CONTACT_COUNT
};

enum {
  CONTACTS_ADDED,
  CONTACTS_CHANGED,
  CONTACTS_REMOVED,
  ROSTER_SEQUENCE_COMPLETE,
  TEMPORARY_CONTACT_SAVED,
  EBOOK_STATUS,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};
static GQuark master_quark;
static GQuark roster_quark;

struct _OssoABookAggregatorPrivate
{
  gchar *sexp;
  GPtrArray *filters;
  OssoABookRosterManager *roster_manager;
  OssoABookWaitableClosure *closure;
  OssoABookVoicemailContact *voicemail_contact;
  GHashTable *contacts;
  GHashTable *field_18;
  GHashTable *field_1C;
  GHashTable *field_20;
  GPtrArray *contacts_added;
  GPtrArray *contacts_removed;
  GPtrArray *contacts_changed;
  int field_30;
  int field_34;
  unsigned char flags;
};

typedef struct _OssoABookAggregatorPrivate OssoABookAggregatorPrivate;

static void osso_abook_aggregator_osso_abook_waitable_iface_init(
    OssoABookWaitableIface *iface);

#define OSSO_ABOOK_AGGREGATOR_PRIVATE(contact) \
                ((OssoABookAggregatorPrivate *)osso_abook_aggregator_get_instance_private(contact))

G_DEFINE_TYPE_WITH_CODE(
  OssoABookAggregator,
  osso_abook_aggregator,
  OSSO_ABOOK_TYPE_ROSTER,
  G_IMPLEMENT_INTERFACE(
      OSSO_ABOOK_TYPE_WAITABLE,
      osso_abook_aggregator_osso_abook_waitable_iface_init);
  G_ADD_PRIVATE(OssoABookAggregator);
);

static OssoABookRoster *default_aggregator = NULL;

static void
g_object_unref0(gpointer object)
{
  if ( object )
    g_object_unref(object);
}

static gboolean
_osso_abook_is_addressbook()
{
  static pid_t pid;

  G_STATIC_ASSERT(sizeof(pid_t) == sizeof(dbus_uint32_t));

  if (!pid)
  {
    DBusGConnection *bus = dbus_g_bus_get(DBUS_BUS_SESSION, NULL);
    DBusGProxy *proxy;

    if (bus && (proxy = dbus_g_proxy_new_for_name(bus, DBUS_SERVICE_DBUS, "/",
                                                  DBUS_INTERFACE_DBUS)))
    {
      dbus_uint32_t _pid = 0;

      org_freedesktop_DBus_get_connection_unix_process_id(
            proxy, "com.nokia.osso_addressbook", &_pid, NULL);
      g_object_unref(proxy);
      pid = _pid;
    }
  }

  return getpid() == pid;
}

static const char *
get_temporary_uid(OssoABookContact *contact)
{
  return g_object_get_data(G_OBJECT(contact), "temporary-uid");
}

static void
osso_abook_aggregator_emit(OssoABookAggregator *aggregator, guint signal_id,
                           GQuark detail, GPtrArray *arr, const char *tag)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  OssoABookContact **contacts;
  int i;

  if (!arr->len)
    return;

  /* remove duplicates */
  for (int i = 1; i < arr->len; i++)
  {
    if (arr->pdata[i - 1] == arr->pdata[i])
    {
      g_ptr_array_remove_index_fast(arr, i);
      i--;
    }
  }

  if (signals[CONTACTS_REMOVED] == signal_id)
  {
    for (i = 0; i < arr->len; i++)
    {
      gchar *uid = g_strdup(get_temporary_uid(arr->pdata[i]));

      if (!uid)
        uid = e_contact_get(arr->pdata[i], E_CONTACT_UID);

      g_object_unref(arr->pdata[i]);
      arr->pdata[i] = uid;
    }
  }
  else if (signals[CONTACTS_CHANGED] == signal_id)
  {
    for (i = 0; i < arr->len; i++)
    {
      if (!osso_abook_contact_is_roster_contact(arr->pdata[i]) &&
          !accept_contact(priv, arr->pdata[i]) )
      {
        OSSO_ABOOK_NOTE(
              AGGREGATOR, "%s@%p: dropping %s in %s signal",
              osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
              aggregator, e_contact_get_const(arr->pdata[i], E_CONTACT_UID),
              g_signal_name(signal_id));
        g_object_unref(arr->pdata[i]);
        g_ptr_array_remove_index_fast(arr, i);
      }
    }
  }

  OSSO_ABOOK_NOTE(
        AGGREGATOR, "%s@%p: emitting %s with %d contacts for %s",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator, g_signal_name(signal_id), arr->len, tag);

  for (i = 0; i < arr->len; i++)
  {
    if (signals[CONTACTS_REMOVED] == signal_id)
    {
      OSSO_ABOOK_NOTE(
            AGGREGATOR, "%s@%p: #%d: %s",
            osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
            aggregator, i, arr->pdata[i]);
    }
    else
    {
      OSSO_ABOOK_NOTE(
            AGGREGATOR, "%s@%p: #%d: %s (%s)",
            osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
            aggregator, i,
            osso_abook_contact_get_display_name(arr->pdata[i]),
            e_contact_get_const(arr->pdata[i], E_CONTACT_UID));
    }
  }

  contacts = g_new(OssoABookContact *, arr->len + 1);
  memcpy(contacts, arr->pdata, arr->len * sizeof(contacts[0]));
  contacts[arr->len] = NULL;
  g_ptr_array_remove_range(arr, 0, arr->len);
  g_signal_emit(aggregator, signal_id, detail, contacts);
}

static void
osso_abook_aggregator_emit_all(OssoABookAggregator *aggregator, const char *tag)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  osso_abook_aggregator_emit(aggregator, signals[CONTACTS_REMOVED],
                             0, priv->contacts_removed, tag);
  osso_abook_aggregator_emit(aggregator, signals[CONTACTS_ADDED],
                             0, priv->contacts_added, tag);
  osso_abook_aggregator_emit(aggregator, signals[CONTACTS_CHANGED],
                             roster_quark, priv->contacts_changed, tag);
}

static void
voicemail_contact_reset_cb(OssoABookVoicemailContact *contact,
                           gpointer user_data)
{
  OssoABookAggregator *aggregator = (OssoABookAggregator *)user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);
  gboolean valid = FALSE;
  gboolean exists;
  GPtrArray *arr;

  g_return_if_fail(NULL != uid);

  exists = !!g_hash_table_lookup(priv->contacts, uid);

  if (priv->flags & OSSO_ABOOK_AGGREGATOR_FLAGS_SYSTEM_BOOK)
  {
    OssoABookGconfContact *gconf_contact = OSSO_ABOOK_GCONF_CONTACT(contact);

    if (!osso_abook_gconf_contact_is_vcard_empty(gconf_contact) &&
        !osso_abook_gconf_contact_is_deleted(gconf_contact))
    {
      valid = TRUE;
    }
  }

  if (valid != exists)
  {
    if (valid)
      arr = priv->contacts_added;
    else
      arr = priv->contacts_removed;

    g_ptr_array_add(arr, g_object_ref(contact));
    osso_abook_aggregator_emit_all(aggregator, __FUNCTION__);
  }
}

static const gchar *
get_book_uid(EBook *book)
{
  ESource *source = e_book_get_source(book);

  return e_source_get_uid(source);
}

static void
notify_book_cb(GObject *gobject, GParamSpec *pspec, gpointer user_data)
{
  OssoABookAggregator *aggregator = (OssoABookAggregator *)user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  EBook *book = osso_abook_roster_get_book(user_data);
  const char *book_uid;
  gboolean system_book_used = FALSE;

  if (book && (book_uid = get_book_uid(book)))
  {
    EBook *system_book = osso_abook_system_book_dup_singleton(FALSE, NULL);

    if (system_book)
    {
      const char *system_book_uid = get_book_uid(system_book);

      system_book_used = !g_strcmp0(system_book_uid, book_uid);
      g_object_unref(system_book);
    }
  }

  if (system_book_used)
    priv->flags |= OSSO_ABOOK_AGGREGATOR_FLAGS_SYSTEM_BOOK;
  else
    priv->flags &= !OSSO_ABOOK_AGGREGATOR_FLAGS_SYSTEM_BOOK;

  if (priv->flags & OSSO_ABOOK_AGGREGATOR_FLAGS_SYSTEM_BOOK )
  {
    if (!priv->voicemail_contact)
    {
      priv->voicemail_contact = osso_abook_voicemail_contact_get_default();
      g_signal_connect(priv->voicemail_contact, "reset",
                       G_CALLBACK(voicemail_contact_reset_cb), aggregator);
    }

    voicemail_contact_reset_cb(priv->voicemail_contact, aggregator);
  }
}

static void
osso_abook_aggregator_init(OssoABookAggregator *aggregator)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  priv->contacts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                         g_object_unref0);
  priv->field_18 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                         g_object_unref0);
  priv->field_1C = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                         g_object_unref0);
  priv->field_20 = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                         (GDestroyNotify)g_hash_table_unref);
  priv->contacts_added = g_ptr_array_new();
  priv->contacts_removed = g_ptr_array_new();
  priv->contacts_changed = g_ptr_array_new();
  g_signal_connect(aggregator, "notify::book",
                   G_CALLBACK(notify_book_cb), aggregator);

  if (_osso_abook_is_addressbook())
    rtcom_el_get_shared();
}

static void
destroy_contacts_array(GPtrArray *arr)
{
  int i;

  for (i = arr->len - 1; i >= 0; i--)
  {
    gpointer object = arr->pdata[i];

    if (object)
      g_object_unref(object);

    g_ptr_array_remove_index_fast(arr, i);
  }
}

static void
osso_abook_aggregator_dispose(GObject *object)
{
  OssoABookAggregator *aggregator = OSSO_ABOOK_AGGREGATOR(object);
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  if (priv->contacts)
    g_hash_table_remove_all(priv->contacts);

  if (priv->field_18)
    g_hash_table_remove_all(priv->field_18);

  if (priv->field_1C)
    g_hash_table_remove_all(priv->field_1C);

  if (priv->field_20)
    g_hash_table_remove_all(priv->field_20);

  destroy_contacts_array(priv->contacts_added);
  destroy_contacts_array(priv->contacts_removed);

  if (priv->filters)
  {
    int i;

    for (i = 0; i < priv->filters->len; i++)
    {
      gpointer obj = priv->filters->pdata[i];

      g_signal_handlers_disconnect_matched(
            obj, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
            contact_filter_changed_cb, aggregator);
      g_object_unref(obj);
    }

    g_ptr_array_free(priv->filters, TRUE);
    priv->filters = NULL;
  }

  osso_abook_aggregator_real_set_roster_manager(aggregator, NULL);

  if (priv->voicemail_contact)
  {
    g_signal_handlers_disconnect_matched(
      priv->voicemail_contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      voicemail_contact_reset_cb, aggregator);
    g_object_unref(priv->voicemail_contact);
    priv->voicemail_contact = NULL;
  }

  osso_abook_waitable_reset(OSSO_ABOOK_WAITABLE(object));

  G_OBJECT_CLASS(osso_abook_aggregator_parent_class)->dispose(object);
}

static void
osso_abook_aggregator_finalize(GObject *object)
{
  OssoABookAggregatorPrivate *priv =
      OSSO_ABOOK_AGGREGATOR_PRIVATE(OSSO_ABOOK_AGGREGATOR(object));

  g_hash_table_destroy(priv->contacts);
  g_hash_table_destroy(priv->field_18);
  g_hash_table_destroy(priv->field_1C);
  g_hash_table_destroy(priv->field_20);
  g_ptr_array_free(priv->contacts_added, TRUE);
  g_ptr_array_free(priv->contacts_removed, TRUE);
  g_ptr_array_free(priv->contacts_changed, TRUE);

  G_OBJECT_CLASS(osso_abook_aggregator_parent_class)->finalize(object);
}

static void
osso_abook_aggregator_notify(GObject *object, GParamSpec *pspec)
{
  GObjectClass *object_class =
      G_OBJECT_CLASS(osso_abook_aggregator_parent_class);

  if (pspec->param_id == PROP_STATE &&
      pspec->owner_type == OSSO_ABOOK_TYPE_AGGREGATOR)
  {
    gchar *flags = _osso_abook_flags_to_string(
          OSSO_ABOOK_TYPE_AGGREGATOR_STATE,
          osso_abook_aggregator_get_state(OSSO_ABOOK_AGGREGATOR(object)));

    OSSO_ABOOK_NOTE(AGGREGATOR, "%s@%p: new aggregator state: %s",
                    osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(object)),
                    object, flags);

    g_free(flags);

    if (osso_abook_waitable_is_ready(OSSO_ABOOK_WAITABLE(object), NULL))
      osso_abook_waitable_notify(OSSO_ABOOK_WAITABLE(object), NULL);
  }

  if (object_class->notify)
    object_class->notify(object, pspec);
}

static void
osso_abook_aggregator_set_property(GObject *object, guint property_id,
                                   const GValue *value, GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_ROSTER_MANAGER:
    {
      osso_abook_aggregator_real_set_roster_manager(
            OSSO_ABOOK_AGGREGATOR(object), g_value_get_object(value));
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_aggregator_get_property(GObject *object, guint property_id,
                                   GValue *value, GParamSpec *pspec)
{
  OssoABookAggregator *aggregator = OSSO_ABOOK_AGGREGATOR(object);

  switch ( property_id )
  {
    case PROP_STATE:
    {
      g_value_set_flags(value, osso_abook_aggregator_get_state(aggregator));
      break;
    }
    case PROP_MASTER_CONTACT_COUNT:
    {
      g_value_set_uint(
            value, osso_abook_aggregator_get_master_contact_count(aggregator));
      break;
    }
    case PROP_ROSTER_MANAGER:
    {
      g_value_set_object(value,
                         osso_abook_aggregator_get_roster_manager(aggregator));
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_aggregator_change_state(OssoABookAggregator *aggregator,
                                   OssoABookAggregatorState state)
{
  if (osso_abook_aggregator_get_state(aggregator) != state)
    g_object_notify(G_OBJECT(aggregator), "state");
}

static void
osso_abook_aggregator_start(OssoABookRoster *roster)
{
  OssoABookAggregator *aggregator = OSSO_ABOOK_AGGREGATOR(roster);
  OssoABookAggregatorState state = osso_abook_aggregator_get_state(aggregator);
  OssoABookRosterManager *manager;

  OSSO_ABOOK_ROSTER_CLASS(osso_abook_aggregator_parent_class)->start(roster);

  manager = osso_abook_aggregator_get_roster_manager(aggregator);
  if (manager)
    osso_abook_roster_manager_start(manager);

  osso_abook_aggregator_change_state(aggregator, state);
}

static void
osso_abook_aggregator_stop(OssoABookRoster *roster)
{
  OssoABookAggregator *aggregator = OSSO_ABOOK_AGGREGATOR(roster);
  OssoABookAggregatorState state = osso_abook_aggregator_get_state(aggregator);
  OssoABookRosterManager *manager =
      OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator)->roster_manager;

  if (manager)
    osso_abook_roster_manager_stop(manager);

  OSSO_ABOOK_ROSTER_CLASS(osso_abook_aggregator_parent_class)->stop(roster);
  osso_abook_aggregator_change_state(aggregator, state);
}

static void
osso_abook_aggregator_class_init(OssoABookAggregatorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  OssoABookRosterClass *roster_class = OSSO_ABOOK_ROSTER_CLASS(klass);

  object_class->set_property = osso_abook_aggregator_set_property;
  object_class->get_property = osso_abook_aggregator_get_property;
  object_class->finalize = osso_abook_aggregator_finalize;
  object_class->dispose = osso_abook_aggregator_dispose;
  object_class->notify = osso_abook_aggregator_notify;

  roster_class->contacts_added = osso_abook_aggregator_contacts_added;
  roster_class->contacts_removed = osso_abook_aggregator_contacts_removed;
  roster_class->contacts_changed = osso_abook_aggregator_contacts_changed;
  roster_class->sequence_complete = osso_abook_aggregator_sequence_complete;
  roster_class->set_book_view = osso_abook_aggregator_set_book_view;
  roster_class->start = osso_abook_aggregator_start;
  roster_class->stop = osso_abook_aggregator_stop;

  g_object_class_install_property(
        object_class, PROP_ROSTER_MANAGER,
        g_param_spec_object(
          "roster-manager",
          "Roster Manager",
          "The roster manager",
          OSSO_ABOOK_TYPE_ROSTER_MANAGER,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_STATE,
        g_param_spec_flags(
          "state",
          "State",
          "Current state of the aggregator",
          OSSO_ABOOK_TYPE_AGGREGATOR_STATE, OSSO_ABOOK_AGGREGATOR_PENDING,
          GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_MASTER_CONTACT_COUNT,
        g_param_spec_uint(
          "master-contact-count",
          "Master Contact Count",
          "Current number of master contacts in the aggregator",
          0, G_MAXUINT, 0,
          GTK_PARAM_READABLE));

  signals[CONTACTS_ADDED] = g_signal_lookup("contacts-added",
                                            OSSO_ABOOK_TYPE_ROSTER);
  signals[CONTACTS_CHANGED] = g_signal_lookup("contacts-changed",
                                              OSSO_ABOOK_TYPE_ROSTER);
  signals[CONTACTS_REMOVED] = g_signal_lookup("contacts-removed",
                                              OSSO_ABOOK_TYPE_ROSTER);
  signals[ROSTER_SEQUENCE_COMPLETE] =
      g_signal_new("roster-sequence-complete",
                   OSSO_ABOOK_TYPE_AGGREGATOR, G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(OssoABookAggregatorClass,
                                   roster_sequence_complete),
                   NULL, NULL,
                   osso_abook_marshal_VOID__OBJECT_UINT,
                   G_TYPE_NONE,
                   2, OSSO_ABOOK_TYPE_ROSTER, G_TYPE_UINT);
  signals[TEMPORARY_CONTACT_SAVED] =
      g_signal_new("temporary-contact-saved",
                   OSSO_ABOOK_TYPE_AGGREGATOR, G_SIGNAL_RUN_FIRST,
                   0,
                   NULL, NULL,
                   osso_abook_marshal_VOID__STRING_OBJECT,
                   G_TYPE_NONE,
                   2, G_TYPE_STRING, G_TYPE_OBJECT);
  signals[EBOOK_STATUS] =
      g_signal_new("ebook-status",
                   OSSO_ABOOK_TYPE_AGGREGATOR, G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(OssoABookAggregatorClass,
                                   ebook_status),
                   g_signal_accumulator_true_handled, NULL,
                   osso_abook_marshal_BOOLEAN__UINT,
                   G_TYPE_BOOLEAN,
                   1, G_TYPE_UINT);

  master_quark = g_quark_from_static_string("master");
  roster_quark = g_quark_from_static_string("roster");
}

static gboolean
osso_abook_aggregator_is_ready(OssoABookWaitable *waitable,
                               const GError **error)
{
  OssoABookAggregatorState state =
      osso_abook_aggregator_get_state(OSSO_ABOOK_AGGREGATOR(waitable));

  if (error)
    *error = NULL;

  return (state & OSSO_ABOOK_AGGREGATOR_READY) == OSSO_ABOOK_AGGREGATOR_READY;
}

static void
osso_abook_aggregator_osso_abook_waitable_iface_init(
    OssoABookWaitableIface *iface)
{
  iface->is_ready = osso_abook_aggregator_is_ready;
}

unsigned int
osso_abook_aggregator_get_master_contact_count(OssoABookAggregator *aggregator)
{
  GList *l = osso_abook_aggregator_list_master_contacts(aggregator);;
  unsigned int count = g_list_length(l);

  g_list_free(l);

  return count;
}

GList *
osso_abook_aggregator_list_master_contacts(OssoABookAggregator *aggregator)
{
  return osso_abook_aggregator_find_contacts(aggregator, NULL);
}

__attribute__((destructor)) static void
release_default_aggregator()
{
  if (default_aggregator)
  {
    OSSO_ABOOK_NOTE(AGGREGATOR, "%s@%p: releasing default aggregator",
                    osso_abook_roster_get_book_uri(default_aggregator),
                    default_aggregator);


    g_object_unref(default_aggregator);
    default_aggregator = 0;
  }
}

OssoABookRoster *
osso_abook_aggregator_get_default(GError **error)
{
  if ( !default_aggregator )
  {
    default_aggregator = osso_abook_aggregator_new(NULL, error);

    OSSO_ABOOK_NOTE(AGGREGATOR, "%s@%p: creating default aggregator",
                    osso_abook_roster_get_book_uri(default_aggregator),
                    default_aggregator);

    if (default_aggregator)
    {
      osso_abook_roster_set_name_order(default_aggregator,
                                       osso_abook_settings_get_name_order());
      osso_abook_roster_start(default_aggregator);
    }
  }

  return default_aggregator;
}

OssoABookRoster *
osso_abook_aggregator_new_with_view(EBookView *view)
{
  g_return_val_if_fail(E_IS_BOOK_VIEW(view), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_AGGREGATOR, "book-view", view, NULL);
}

OssoABookRoster *
osso_abook_aggregator_new(EBook *book, GError **error)
{
  return osso_abook_aggregator_new_with_query(book, NULL, NULL, 0, error);
}

static gboolean
find_contacts_predicate(OssoABookContact *contact, gpointer user_data)
{
  return e_book_backend_sexp_match_contact(user_data, E_CONTACT(contact));
}

GList *
osso_abook_aggregator_find_contacts(OssoABookAggregator *aggregator,
                                    EBookQuery *query)
{
  char *query_text;
  EBookBackendSExp *sexp;
  GList *contacts;

  g_return_val_if_fail(OSSO_ABOOK_IS_AGGREGATOR(aggregator), NULL);

  if (!query)
    return osso_abook_aggregator_find_contacts_full(aggregator, NULL, NULL);

  query_text = e_book_query_to_string(query);
  sexp = e_book_backend_sexp_new(query_text);
  contacts = osso_abook_aggregator_find_contacts_full(
        aggregator, find_contacts_predicate, sexp);

  if (sexp)
    g_object_unref(sexp);

  g_free(query_text);

  return contacts;
}

GList *
osso_abook_aggregator_find_contacts_full(OssoABookAggregator *aggregator,
                                         OssoABookContactPredicate predicate,
                                         gpointer user_data)
{
  OssoABookAggregatorPrivate *priv;
  GList *contacts = NULL;
  GHashTableIter iter;
  OssoABookContact *c;

  g_return_val_if_fail(OSSO_ABOOK_IS_AGGREGATOR(aggregator), NULL);

  priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  g_hash_table_iter_init(&iter, priv->contacts);

  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&c) && c)
  {
    if (predicate && !predicate(c, user_data))
    {
      /* contact was not accepted by the predicate, now try with roster
         contacts */
      GList *rc = osso_abook_contact_get_roster_contacts(c);

      while (rc && !predicate(rc->data, user_data))
        rc = g_list_delete_link(rc, rc);

      if (!rc)
        /* all roster contacts were rejected */
        continue;
      else
        g_list_free(rc);
    }

    contacts = g_list_prepend(contacts, c);
  }

  return contacts;
}

OssoABookAggregatorState
osso_abook_aggregator_get_state(OssoABookAggregator *aggregator)
{
  OssoABookAggregatorPrivate *priv;
  OssoABookAggregatorState state = OSSO_ABOOK_AGGREGATOR_PENDING;

  g_return_val_if_fail(OSSO_ABOOK_IS_AGGREGATOR(aggregator), state);

  priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  if (osso_abook_roster_is_running(OSSO_ABOOK_ROSTER(aggregator)))
    state = OSSO_ABOOK_AGGREGATOR_RUNNING;

  if (priv->flags & OSSO_ABOOK_AGGREGATOR_FLAGS_1)
    state |= OSSO_ABOOK_AGGREGATOR_MASTERS_READY;

  if (priv->flags & OSSO_ABOOK_AGGREGATOR_FLAGS_2 && !priv->field_30)
    state |= OSSO_ABOOK_AGGREGATOR_ROSTERS_READY;

  return state;
}

static void
aggregator_get_book_view_cb(EBook *book, EBookStatus status,
                            EBookView *book_view, gpointer user_data)
{
  g_signal_emit(user_data, signals[EBOOK_STATUS], 0, status);

  if (status)
    osso_abook_handle_estatus(NULL, status, book);
  else
    g_object_set(user_data, "book-view", book_view, NULL);

  if (book_view)
    g_object_unref(book_view);
}

OssoABookRoster *
osso_abook_aggregator_new_with_query(EBook *book, EBookQuery *query,
                                     GList *requested_fields, int max_results,
                                     GError **error)
{
  EBook *_book = NULL;
  EBookQuery *_query = NULL;
  OssoABookRoster *aggr;

  g_return_val_if_fail(!book || E_IS_BOOK(book), NULL);

  if (!book)
    book = _book = osso_abook_system_book_dup_singleton(FALSE, error);

  if (!book)
    return NULL;

  if (!query)
    query = _query = e_book_query_any_field_contains("");

  aggr = g_object_new(OSSO_ABOOK_TYPE_AGGREGATOR, "book", book, NULL);
  _osso_abook_async_get_book_view(book, query, requested_fields, max_results,
                                  aggregator_get_book_view_cb,
                                  g_object_ref(aggr), g_object_unref);

  if (_query)
    e_book_query_unref(_query);

  if (_book)
    g_object_unref(book);

  return aggr;
}
