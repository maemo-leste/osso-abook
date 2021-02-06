#include <glib.h>
#include <glib-object.h>
#include <libedata-book/libedata-book.h>
#include <gtk/gtkprivate.h>

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
#include "osso-abook-log.h"
#include "osso-abook-eventlogger.h"
#include "osso-abook-contact-private.h"
#include "osso-abook-string-list.h"

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
  EBookBackendSExp *sexp;
  GPtrArray *filters;
  OssoABookRosterManager *roster_manager;
  OssoABookWaitableClosure *closure;
  OssoABookVoicemailContact *voicemail_contact;
  GHashTable *master_contacts;
  GHashTable *roster_contacts;
  GHashTable *temp_master_contacts;
  /* master uid -> GHashTable (postponed contacts) */
  GHashTable *postponed_contacts;
  GPtrArray *contacts_added;
  GPtrArray *contacts_removed;
  GPtrArray *contacts_changed;
  GList *pending_rosters;
  GList *complete_rosters;
  gboolean sequence_complete : 1;  /* priv->flags & 1 */
  gboolean is_ready : 1;           /* priv->flags & 2 */
  gboolean roster_manager_set : 1; /* priv->flags & 4 */
  gboolean is_system_book : 1;     /* priv->flags & 8 */
};

typedef struct _OssoABookAggregatorPrivate OssoABookAggregatorPrivate;

static void osso_abook_aggregator_osso_abook_waitable_iface_init(
    OssoABookWaitableIface *iface);
static void osso_abook_aggregator_real_set_roster_manager(
    OssoABookAggregator *aggregator, OssoABookRosterManager *roster_manager);

#define OSSO_ABOOK_AGGREGATOR_PRIVATE(agg) \
                ((OssoABookAggregatorPrivate *)osso_abook_aggregator_get_instance_private(agg))

G_DEFINE_TYPE_WITH_CODE(
  OssoABookAggregator,
  osso_abook_aggregator,
  OSSO_ABOOK_TYPE_ROSTER,
  G_IMPLEMENT_INTERFACE(
      OSSO_ABOOK_TYPE_WAITABLE,
      osso_abook_aggregator_osso_abook_waitable_iface_init);
  G_ADD_PRIVATE(OssoABookAggregator);
);

static void contact_filter_changed_cb(OssoABookContactFilter *filter,
                                      gpointer user_data);
static void create_temporary_master(OssoABookAggregator *aggregator,
                                    OssoABookContact *roster_contact,
                                    const char *tag);

static OssoABookRoster *default_aggregator = NULL;

static void
g_object_unref0(gpointer object)
{
  if (object)
    g_object_unref(object);
}

static gboolean
contact_passes_filters(OssoABookAggregatorPrivate *priv, const char *uid,
                       OssoABookContact *contact)
{
  int i;

  if (!priv->filters)
    return TRUE;

  for (i = 0; i < priv->filters->len; i++)
  {
    OssoABookContactFilter *filter = priv->filters->pdata[i];

    if (!osso_abook_contact_filter_accept(filter, uid, contact))
    {
      OSSO_ABOOK_NOTE(AGGREGATOR, "contact %s rejected by %s", uid,
                      G_OBJECT_TYPE_NAME(filter));
      return FALSE;
    }
  }

  return TRUE;
}

static gboolean
accept_contact(OssoABookAggregatorPrivate *priv, OssoABookContact *contact)
{
  const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);
  GList *roster_contacts;
  GList *l;
  GHashTable *table;

  if (contact && priv->sexp && osso_abook_contact_is_roster_contact(contact) &&
      !e_book_backend_sexp_match_contact(priv->sexp, E_CONTACT(contact)))
  {
    OSSO_ABOOK_NOTE(AGGREGATOR, "roster contact %s rejected by query", uid);
    return FALSE;
  }

  if (contact_passes_filters(priv, uid, contact))
    return TRUE;

  roster_contacts = osso_abook_contact_get_roster_contacts(contact);

  for (l = roster_contacts; l; l = l->next)
  {
    if (contact_passes_filters(priv, e_contact_get_const(E_CONTACT(l->data),
                                                         E_CONTACT_UID),
                               l->data))
    {
      g_list_free(roster_contacts);
      return TRUE;
    }
  }

  g_list_free(roster_contacts);

  table = g_hash_table_lookup(priv->postponed_contacts, uid);

  if (table)
  {
    GHashTableIter iter;
    gpointer key;
    gpointer value;

    g_hash_table_iter_init(&iter, table);

    while (g_hash_table_iter_next(&iter, &key, &value))
    {
      if (contact_passes_filters(priv, key, value))
        return TRUE;
    }
  }

  return FALSE;
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

  exists = !!g_hash_table_lookup(priv->master_contacts, uid);

  if (priv->is_system_book)
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

  priv->is_system_book = system_book_used;

  if (priv->is_system_book)
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

  priv->master_contacts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                         g_object_unref0);
  priv->roster_contacts = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                         g_object_unref0);
  priv->temp_master_contacts = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                     g_free, g_object_unref0);
  priv->postponed_contacts = g_hash_table_new_full(
        g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_hash_table_unref);
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

  if (priv->master_contacts)
    g_hash_table_remove_all(priv->master_contacts);

  if (priv->roster_contacts)
    g_hash_table_remove_all(priv->roster_contacts);

  if (priv->temp_master_contacts)
    g_hash_table_remove_all(priv->temp_master_contacts);

  if (priv->postponed_contacts)
    g_hash_table_remove_all(priv->postponed_contacts);

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

  g_hash_table_destroy(priv->master_contacts);
  g_hash_table_destroy(priv->roster_contacts);
  g_hash_table_destroy(priv->temp_master_contacts);
  g_hash_table_destroy(priv->postponed_contacts);
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

static gboolean
is_dangling_roster_contact(OssoABookAggregator *aggregator,
                           OssoABookContact *master_contact,
                           OssoABookContact *roster_contact)
{
  const char *attr_name = osso_abook_contact_get_vcard_field(roster_contact);
  GList *attr;

  g_return_val_if_fail(NULL != attr_name, TRUE);

  for (attr = e_vcard_get_attributes(E_VCARD(master_contact)); attr;
       attr = attr->next)
  {
    if (!strcmp(attr_name, e_vcard_attribute_get_name(attr->data)))
    {
      GList *v = e_vcard_attribute_get_values(attr->data);

      if (v && osso_abook_contact_matches_username(roster_contact, v->data,
                                                   attr_name, 0))
      {
          return FALSE;
      }
    }
  }

  OSSO_ABOOK_NOTE(
        AGGREGATOR,
        "%s@%p: ignoring dangling master contact reference: %s on %s\n",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator,
        e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID),
        e_contact_get_const(E_CONTACT(master_contact), E_CONTACT_UID));

  return TRUE;
}

static void
process_postponed_contacts(OssoABookAggregator *aggregator,
                           OssoABookContact *contact)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  const char *uid;
  GHashTable *postponed_contacts;
  gboolean attached = FALSE;
  GHashTableIter iter;
  OssoABookContact *roster_contact;

  if (!contact)
    return;

  uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

  if (!uid)
    return;

  postponed_contacts = g_hash_table_lookup(priv->postponed_contacts, uid);

  if (!postponed_contacts)
    return;

  OSSO_ABOOK_NOTE(AGGREGATOR, "%s@%p: processing postponed contacts for %s",
                  osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
                  aggregator, uid);

  g_hash_table_iter_init(&iter, postponed_contacts);

  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&roster_contact) )
  {
    if (!is_dangling_roster_contact(aggregator, contact, roster_contact))
    {
      if (osso_abook_contact_attach(contact, roster_contact))
      {
        if (priv->sequence_complete)
        {
          _osso_abook_eventlogger_update_roster(
                osso_abook_contact_get_account(roster_contact),
                osso_abook_contact_get_bound_name(roster_contact), uid);
        }

        attached = TRUE;
      }
    }
  }

  if (attached)
    g_hash_table_remove(priv->postponed_contacts, uid);
}

static void
remove_temporary_master(OssoABookAggregator *aggregator, const char *temp_uid,
                        OssoABookContact *master_contact, const char *reason)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  OssoABookContact *contact =
      g_hash_table_lookup(priv->temp_master_contacts, temp_uid);

  if (!contact)
    return;

  OSSO_ABOOK_NOTE(
        AGGREGATOR, "%s@%p: dropping temporary master \"%s\" (%s) for %s",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator, osso_abook_contact_get_display_name(contact),
        e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID), reason);

  g_ptr_array_add(priv->contacts_removed, g_object_ref(contact));
  g_hash_table_remove(priv->temp_master_contacts, temp_uid);

  if (master_contact)
  {
    g_signal_emit(aggregator, signals[TEMPORARY_CONTACT_SAVED], 0,
                  get_temporary_uid(contact), master_contact);
  }
}

static void
discard_temporary_contacts(OssoABookAggregator *aggregator,
                           OssoABookContact *contact)
{
  GList *l;

  for (l = osso_abook_contact_get_roster_contacts(contact); l;
       l = g_list_delete_link(l, l))
  {
    remove_temporary_master(
          aggregator,
          e_contact_get_const(E_CONTACT(l->data), E_CONTACT_UID),
          contact, __FUNCTION__);
  }
}

static void
create_master_contact(OssoABookAggregator *aggregator,
                      OssoABookContact *contact)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

  if (accept_contact(priv, contact))
  {

    if (!osso_abook_is_temporary_uid(uid))
    {
      if (priv->sequence_complete)
        _osso_abook_eventlogger_update(contact, NULL);
      else
        _osso_abook_eventlogger_update_phone_table(contact);
    }

    OSSO_ABOOK_NOTE(
          AGGREGATOR, "%s@%p: storing master contact: %s (%s)",
          osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
          aggregator, osso_abook_contact_get_display_name(contact), uid);

    g_hash_table_insert(priv->master_contacts, g_strdup(uid),
                        g_object_ref(contact));

    process_postponed_contacts(aggregator, contact);

    if (osso_abook_is_temporary_uid(uid))
      return;

    discard_temporary_contacts(aggregator, contact);
  }
  else
    g_hash_table_insert(priv->master_contacts, g_strdup(uid), NULL);
}
static void
osso_abook_aggregator_contacts_added(OssoABookRoster *roster,
                                     OssoABookContact **contacts)
{
  OssoABookAggregator *aggregator = OSSO_ABOOK_AGGREGATOR(roster);
  GSignalInvocationHint *hint = g_signal_get_invocation_hint(roster);

  g_return_if_fail(signals[CONTACTS_ADDED] == hint->signal_id);

  if (hint->run_type & G_SIGNAL_RUN_FIRST)
  {
    OssoABookContact **contact = contacts;

    while (*contact)
      create_master_contact(aggregator, *contact++);
  }

  OSSO_ABOOK_ROSTER_CLASS(osso_abook_aggregator_parent_class)->
      contacts_added(roster, contacts);
  g_object_notify(G_OBJECT(roster), "master-contact-count");
}

static void
remove_master_contact(const char *uid, OssoABookAggregatorPrivate *priv)
{
  OssoABookContact *contact = g_hash_table_lookup(priv->master_contacts, uid);

  if (!contact)
    return;

  OSSO_ABOOK_NOTE(AGGREGATOR, "removing master contact %s (%s)",
                  osso_abook_contact_get_display_name(contact), uid);

  if (!osso_abook_is_temporary_uid(uid))
    _osso_abook_eventlogger_remove(contact);

  g_hash_table_remove(priv->postponed_contacts, uid);
  g_hash_table_remove(priv->master_contacts, uid);
}

static void
osso_abook_aggregator_contacts_removed(OssoABookRoster *roster,
                                       const char **uids)
{
  OssoABookAggregator *aggregator = OSSO_ABOOK_AGGREGATOR(roster);
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  GSignalInvocationHint *hint = g_signal_get_invocation_hint(roster);


  g_return_if_fail(signals[CONTACTS_REMOVED] == hint->signal_id);

  if (hint->run_type & G_SIGNAL_RUN_FIRST)
  {
    const char **uid = uids;

    while (*uid)
      remove_master_contact(*uid++, priv);
  }

  OSSO_ABOOK_ROSTER_CLASS(osso_abook_aggregator_parent_class)->
      contacts_removed(roster, uids);
  g_object_notify(G_OBJECT(roster), "master-contact-count");
}

static void
osso_abook_aggregator_contacts_changed(OssoABookRoster *roster,
                                       OssoABookContact **contacts)
{
  OssoABookAggregator *aggregator = OSSO_ABOOK_AGGREGATOR(roster);
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  GSignalInvocationHint *hint = g_signal_get_invocation_hint(roster);
  OssoABookContact **_contacts = contacts;

  g_return_if_fail(signals[CONTACTS_CHANGED] == hint->signal_id);

  if (hint->detail == master_quark && hint->run_type & G_SIGNAL_RUN_FIRST)
  {
    while (*_contacts)
    {
      OssoABookContact *contact = *_contacts;
      const gchar *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);
      OssoABookContact *master_contact =
          g_hash_table_lookup(priv->master_contacts, uid);

      _osso_abook_eventlogger_update(contact, master_contact);

      if (master_contact && contact != master_contact)
      {
        OSSO_ABOOK_NOTE(AGGREGATOR, "reset master contact %s (%s)",
                        osso_abook_contact_get_display_name(master_contact),
                        uid);
        osso_abook_contact_reset(master_contact, contact);
        contact = master_contact;
      }

      if (contact != *_contacts)
      {
        g_object_unref(*_contacts);
        contact = g_object_ref(contact);
        *_contacts = contact;
      }

      process_postponed_contacts(aggregator, contact);
      _contacts++;
    }
  }

  OSSO_ABOOK_ROSTER_CLASS(osso_abook_aggregator_parent_class)->
      contacts_changed(roster, contacts);
}


static void
reject_dangling_roster_contacts(OssoABookAggregator *aggregator,
                                OssoABookContact *roster_contact)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  GList *l;

  if (!_osso_abook_is_addressbook())
    return;

  for (l = osso_abook_contact_get_master_uids(roster_contact); l; l = l->next)
  {
    OssoABookContact *contact =
        g_hash_table_lookup(priv->master_contacts, l->data);

    if (!contact ||
        is_dangling_roster_contact(aggregator, contact, roster_contact))
    {
      _osso_abook_contact_reject_for_uid_full(roster_contact, l->data, 1, 0);
    }
  }
}

static void
process_unclaimed_contacts(OssoABookAggregator *aggregator)
{
  OssoABookAggregatorState state = osso_abook_aggregator_get_state(aggregator);
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  GHashTable *unclaimed_roster_contacts;
  GHashTableIter postponed_iter;
  GHashTableIter contacts_iter;
  const char *uid;
  GHashTable *postponed_table;
  OssoABookContact *roster_contact;

  if (!priv->sequence_complete)
  {
    priv->sequence_complete = TRUE;
    osso_abook_aggregator_change_state(aggregator, state);
  }

  OSSO_ABOOK_NOTE(AGGREGATOR,
                  "%s@%p: processing postponed contacts not claimed yet",
                  osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
                  aggregator);

  unclaimed_roster_contacts = g_hash_table_new_full(g_str_hash, g_str_equal,
                                                    g_free, g_object_unref);
  g_hash_table_iter_init(&postponed_iter, priv->postponed_contacts);

  while (g_hash_table_iter_next(&postponed_iter, (gpointer *)&uid,
                                (gpointer *)&postponed_table))
  {
    g_hash_table_iter_init(&contacts_iter, postponed_table);

    while (g_hash_table_iter_next(&contacts_iter, NULL,
                                  (gpointer *)&roster_contact))
    {
      OSSO_ABOOK_NOTE(
            AGGREGATOR, "%s@%p: unclaimed roster contact %s(%p) for %s",
            osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
            aggregator,
            e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID),
            roster_contact, uid);

      g_hash_table_insert(
            unclaimed_roster_contacts,
            g_strdup(e_contact_get_const(E_CONTACT(roster_contact),
                                         E_CONTACT_UID)),
            g_object_ref(roster_contact));
      g_hash_table_iter_remove(&contacts_iter);
    }
  }

  g_hash_table_iter_init(&contacts_iter, unclaimed_roster_contacts);

  while (g_hash_table_iter_next(&contacts_iter, NULL,
                                (gpointer *)&roster_contact))
  {
    reject_dangling_roster_contacts(aggregator, roster_contact);

    if (g_hash_table_lookup(
          priv->temp_master_contacts,
          e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID)))
    {

      OSSO_ABOOK_NOTE(
            AGGREGATOR, "%s@%p: skipping attached roster contact %s(%p)",
            osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
            aggregator,
            e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID),
            roster_contact);
    }
    else
    {
      GList *ruid = osso_abook_contact_get_master_uids(roster_contact);

      while (ruid && !g_hash_table_lookup(priv->master_contacts, ruid->data))
        ruid = ruid->next;

      if (!ruid)
        create_temporary_master(aggregator, roster_contact, __FUNCTION__);
    }
  }

  g_hash_table_unref(unclaimed_roster_contacts);
}

static void
osso_abook_aggregator_sequence_complete(OssoABookRoster *roster,
                                        EBookViewStatus status)
{
  OssoABookAggregator *aggregator = OSSO_ABOOK_AGGREGATOR(roster);

  if (g_signal_get_invocation_hint(roster)->run_type & G_SIGNAL_RUN_FIRST)
  {
    process_unclaimed_contacts(aggregator);
    osso_abook_aggregator_emit_all(aggregator, "process_unclaimed_contacts");
    g_signal_emit(roster, signals[ROSTER_SEQUENCE_COMPLETE], 0, roster, status);
    _osso_abook_eventlogger_apply();
  }

  OSSO_ABOOK_ROSTER_CLASS(osso_abook_aggregator_parent_class)->
      sequence_complete(roster, status);
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

  /* upstream EDS doesn't have e_book_view_get_query() */
  /* roster_class->set_book_view = osso_abook_aggregator_set_book_view; */

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
    default_aggregator = NULL;
  }
}

OssoABookRoster *
osso_abook_aggregator_get_default(GError **error)
{
  if (!default_aggregator)
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
  g_hash_table_iter_init(&iter, priv->master_contacts);

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

  if (priv->sequence_complete)
    state |= OSSO_ABOOK_AGGREGATOR_MASTERS_READY;

  if (priv->is_ready && !priv->pending_rosters)
    state |= OSSO_ABOOK_AGGREGATOR_ROSTERS_READY;

  return state;
}

static void
aggregator_get_book_view_cb(EBook *book, EBookStatus status,
                            EBookView *book_view, gpointer user_data)
{
  gboolean result;

  g_signal_emit(user_data, signals[EBOOK_STATUS], 0, status, &result);

  if (status)
  {
    if (!result)
      osso_abook_handle_estatus(NULL, status, book);
  }
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

static void
postpone_roster_contact(OssoABookAggregator *aggregator, const char *master_uid,
                        OssoABookContact *contact)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);
  GHashTable *postponed;

  postponed = g_hash_table_lookup(priv->postponed_contacts, master_uid);

  OSSO_ABOOK_NOTE(
        AGGREGATOR, "%s@%p: postponing roster contact %s for %s",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator, uid, master_uid);

  if (!postponed)
  {
    postponed = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                      g_object_unref);
    g_hash_table_insert(priv->postponed_contacts, g_strdup(master_uid),
                        postponed);
  }

  g_hash_table_insert(postponed, g_strdup(uid), g_object_ref(contact));
}

static gboolean
passes_by_uid(OssoABookAggregatorPrivate *priv, const char *uid)
{
  GPtrArray *filters = priv->filters;
  guint i;

  if (!filters || !filters->len)
    return TRUE;

  for (i = 0; i < filters->len; i++)
  {
    OssoABookContactFilter *filter = filters->pdata[i++];

    if (osso_abook_contact_filter_get_flags(filter) &
        OSSO_ABOOK_CONTACT_FILTER_ONLY_READS_UID)
    {
      if (!osso_abook_contact_filter_accept(filter, uid, NULL))
      {
        OSSO_ABOOK_NOTE(AGGREGATOR, "contact %s rejected by %s", uid,
                        G_OBJECT_TYPE_NAME(filter));
        return FALSE;
      }
    }
  }

  return TRUE;
}

static void
restore_master_contact_cb(EBook *book, EBookStatus status, EContact *econtact,
                          gpointer closure)
{
  OssoABookAggregator *aggregator = closure;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  if (status)
  {
    if (status == E_BOOK_ERROR_CONTACT_NOT_FOUND)
      OSSO_ABOOK_NOTE(TODO, "unload contact with unknown UID");
    else
      osso_abook_handle_estatus(NULL, status, book);
  }
  else
  {
    const char *uid = e_contact_get_const(econtact, E_CONTACT_UID);

    if (!g_hash_table_lookup(priv->master_contacts, uid))
    {
      OssoABookContact *contact =
          osso_abook_contact_new_from_template(econtact);

      if (contact)
      {
        if (accept_contact(priv, contact))
        {
          g_ptr_array_add(priv->contacts_added, g_object_ref(contact));
          g_hash_table_insert(priv->master_contacts, g_strdup(uid),
                              g_object_ref(contact));
        }

        g_object_unref(contact);
      }
    }
  }

  osso_abook_aggregator_emit_all(aggregator, __FUNCTION__);

  if (econtact)
    g_object_unref(econtact);
}

static void
restore_master_contact(OssoABookAggregator *aggregator, const char *uid)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  const char *voicemail_uid = NULL;

  if (priv->voicemail_contact)
  {
    voicemail_uid = e_contact_get_const(E_CONTACT(priv->voicemail_contact),
                                        E_CONTACT_UID);
  }

  OSSO_ABOOK_NOTE(
        AGGREGATOR, "%s@%p: reloading master contact %s",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator, uid);

  if (voicemail_uid && !strcmp(uid, voicemail_uid))
  {
    if (accept_contact(priv, OSSO_ABOOK_CONTACT(priv->voicemail_contact)))
    {
      g_ptr_array_add(priv->contacts_added,
                      g_object_ref(priv->voicemail_contact));
    }
  }
  else
  {
    e_book_async_get_contact(
          osso_abook_roster_get_book(OSSO_ABOOK_ROSTER(aggregator)), uid,
          restore_master_contact_cb, aggregator);
  }
}

static void
create_temporary_master(OssoABookAggregator *aggregator,
                        OssoABookContact *roster_contact, const char *tag)
{
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  GList *l;
  const char *uid =
      e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID);

  if (accept_contact(priv, roster_contact))
  {
    OssoABookContact *temp_master =
        g_hash_table_lookup(priv->temp_master_contacts, uid);

    if (temp_master)
    {
      osso_abook_contact_attach(temp_master, roster_contact);
      g_ptr_array_add(priv->contacts_changed, g_object_ref(temp_master));
    }
    else
    {
      gchar *temp_uid;
      OssoABookContact *master_contact = osso_abook_contact_new();
      const char *vcard_field =
          osso_abook_contact_get_vcard_field(roster_contact);

      for (l = e_vcard_get_attributes(E_VCARD(roster_contact)); l; l = l->next)
      {
        const char *attr_name = e_vcard_attribute_get_name(l->data);

        if (g_str_equal(attr_name, "N") || g_str_equal(attr_name, "NICKNAME") ||
            (vcard_field && g_str_equal(attr_name, vcard_field)))
        {
          e_vcard_add_attribute(E_VCARD(master_contact),
                                e_vcard_attribute_copy(l->data));
        }
      }

      temp_uid = osso_abook_create_temporary_uid();

      OSSO_ABOOK_NOTE(
            AGGREGATOR, "%s@%p: wrapping \"%s\" (%s) in %s for %s",
            osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
            aggregator, osso_abook_contact_get_display_name(roster_contact),
            uid, temp_uid, tag);

      e_contact_set(E_CONTACT(master_contact), E_CONTACT_UID, temp_uid);

      osso_abook_contact_set_roster(master_contact,
                                    OSSO_ABOOK_ROSTER(aggregator));
      g_object_set_data_full(G_OBJECT(master_contact), "temporary-uid",
                             temp_uid, g_free);

      for (l = e_vcard_get_attributes(E_VCARD(master_contact)); l; l = l->next)
      {
        const char *attr_name = e_vcard_attribute_get_name(l->data);

        if (g_str_has_prefix(attr_name, "X-TELEPATHY-") ||
            !strcmp(attr_name, OSSO_ABOOK_VCA_OSSO_MASTER_UID))
        {
          e_vcard_remove_attribute(E_VCARD(master_contact), l->data);
        }
      }

      g_hash_table_insert(priv->temp_master_contacts, g_strdup(uid),
                          g_object_ref(master_contact));
      g_ptr_array_add(priv->contacts_added, master_contact);
      osso_abook_contact_attach(master_contact, roster_contact);
    }
  }
  else
  {
    OSSO_ABOOK_NOTE(
          AGGREGATOR,
          "%s@%p: skipping rejected roster contact \"%s\" (%s) for %s",
          osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
          aggregator, osso_abook_contact_get_display_name(roster_contact), uid,
          tag);
  }
}

static void
contact_filter_changed_cb(OssoABookContactFilter *filter, gpointer user_data)
{
  OssoABookAggregator *aggregator = user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  GHashTableIter iter;
  const gchar *uid;
  OssoABookContact *contact;

  g_return_if_fail(NULL != priv->filters);

  g_hash_table_iter_init(&iter, priv->master_contacts);

  while (g_hash_table_iter_next(&iter, (gpointer *)&uid, (gpointer *)&contact))
  {
    if (contact)
    {
      if (!accept_contact(priv, contact))
      {
        GList *l;

        OSSO_ABOOK_NOTE(
              AGGREGATOR,
              "%s@%p: discarding master contact %s",
              osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
              aggregator, uid);

        for (l = osso_abook_contact_get_roster_contacts(contact); l;
             l = g_list_delete_link(l, l))
        {
          postpone_roster_contact(aggregator, uid, l->data);
        }

        g_ptr_array_add(priv->contacts_removed, g_object_ref(contact));
        g_hash_table_insert(priv->master_contacts, g_strdup(uid), NULL);
      }
    }
    else
    {
      gboolean passed = passes_by_uid(priv, uid);

      if (!passed)
      {
        GHashTable *postponed = g_hash_table_lookup(priv->postponed_contacts,
                                                    uid);

        if (postponed)
        {
          gpointer tmp_uid;
          gpointer tmp_contact;
          GHashTableIter tmp_iter;

          g_hash_table_iter_init(&tmp_iter, postponed);

          while (g_hash_table_iter_next(&tmp_iter, &tmp_uid, &tmp_contact))
          {
            if (passes_by_uid(priv, tmp_uid))
            {
              passed = TRUE;
              break;
            }
          }
        }

      }

      if (passed)
        restore_master_contact(aggregator, uid);
    }
  }

  g_hash_table_iter_init(&iter, priv->roster_contacts);

  while (g_hash_table_iter_next(&iter, (gpointer *)&uid, (gpointer *)&contact))
  {
    if (!osso_abook_contact_get_master_uids(contact) &&
        !g_hash_table_lookup(priv->master_contacts, uid))
    {
      if (accept_contact(priv, contact))
        create_temporary_master(aggregator, contact, __FUNCTION__);
    }
  }

  osso_abook_aggregator_emit_all(aggregator, __FUNCTION__);
}

void
osso_abook_aggregator_add_filter(OssoABookAggregator *aggregator,
                                 OssoABookContactFilter *filter)
{
  OssoABookAggregatorPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_AGGREGATOR(aggregator));
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_FILTER(aggregator));

  priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  g_signal_connect(filter, "contact-filter-changed",
                   G_CALLBACK(contact_filter_changed_cb), aggregator);

  if (!priv->filters)
    priv->filters = g_ptr_array_new();

  g_ptr_array_add(priv->filters, g_object_ref((gpointer)filter));
  contact_filter_changed_cb(filter, aggregator);
}

static void
roster_sequence_complete_cb(OssoABookRoster *roster, EBookViewStatus status,
                            gpointer user_data)
{
  OssoABookAggregator *aggregator = user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  OssoABookAggregatorState state = osso_abook_aggregator_get_state(aggregator);

  g_signal_emit(aggregator, signals[ROSTER_SEQUENCE_COMPLETE], 0, roster,
                status);
  priv->pending_rosters = g_list_remove_all(priv->pending_rosters, roster);
  priv->complete_rosters = g_list_prepend(priv->complete_rosters, roster);

  OSSO_ABOOK_NOTE(
        AGGREGATOR,"%s@%p: roster %s sent sequence-complete, "
        "pending-rosters: %d, master-contacts: %d, roster-manager ready: %d",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator, osso_abook_roster_get_book_uri(roster),
        g_list_length(priv->pending_rosters),
        g_hash_table_size(priv->master_contacts), !!priv->is_ready);

  if (priv->is_ready && !priv->pending_rosters)
    osso_abook_aggregator_change_state(aggregator, state);

  _osso_abook_eventlogger_apply();
}

typedef enum {
  NOT_ATTACHED,
  POSTPONED,
  ATTACHED_TO_TEMPORARY,
  ATTACHED_TO_MASTER
} RosterAttachStatus;

static guint
attach_roster_contact(OssoABookAggregator *aggregator, gchar *master_uid,
                      OssoABookContact *roster_contact)
{
  OssoABookAggregatorPrivate *priv;
  OssoABookContact *master_contact = NULL;

  g_return_val_if_fail(!IS_EMPTY(master_uid), NOT_ATTACHED);

  priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  if (g_hash_table_lookup_extended(priv->master_contacts, master_uid, NULL,
                                   (gpointer *)&master_contact))
  {
    if (!master_contact)
    {
      postpone_roster_contact(aggregator, master_uid, roster_contact);

      if (accept_contact(priv, roster_contact))
        restore_master_contact(aggregator, master_uid);

      return POSTPONED;
    }
    else if (!is_dangling_roster_contact(aggregator, master_contact,
                                         roster_contact))
    {
      OSSO_ABOOK_NOTE(
            AGGREGATOR, "%s@%p: attaching %s to master contact %s@%p",
            osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
            aggregator,
            e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID),
            master_uid, master_contact);

      if (!osso_abook_is_temporary_uid(master_uid))
      {
        remove_temporary_master(
              aggregator,
              e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID),
              master_contact, __FUNCTION__);
        osso_abook_contact_attach(master_contact, roster_contact);
      }

      g_ptr_array_add(priv->contacts_changed, g_object_ref(master_contact));
      return ATTACHED_TO_MASTER;
    }
  }

  if (!priv->sequence_complete)
  {
    postpone_roster_contact(aggregator, master_uid, roster_contact);
    return POSTPONED;
  }

  master_contact = g_hash_table_lookup(
        priv->temp_master_contacts,
        e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID));

  if (!master_contact)
    return NOT_ATTACHED;

  OSSO_ABOOK_NOTE(
        AGGREGATOR, "%s@%p: attaching %s to temporary master contact %s@%p",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator,
        e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID),
        master_uid, master_contact);

  osso_abook_contact_attach(master_contact, roster_contact);
  g_ptr_array_add(priv->contacts_changed, g_object_ref(master_contact));

  return ATTACHED_TO_TEMPORARY;
}

static void
roster_contacts_added_cb(OssoABookRoster *roster, OssoABookContact **contacts,
                         gpointer user_data)
{
  OssoABookAggregator *aggregator = (OssoABookAggregator *)user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  OSSO_ABOOK_NOTE(
        AGGREGATOR, "%s@%p: new roster contacts from %s",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator, osso_abook_roster_get_book_uri(roster));

  while (*contacts)
  {
    OssoABookContact *contact = *contacts++;
    GList *uid;
    const gchar *abook_uid = NULL;
    gboolean at_least_one_postponed = FALSE;
    gboolean at_least_one_not_attached = FALSE;
    gboolean all_detached = TRUE;

    g_hash_table_insert(priv->roster_contacts,
                        g_strdup(e_contact_get_const(E_CONTACT(contact),
                                                     E_CONTACT_UID)),
                        g_object_ref(contact));

    for (uid = osso_abook_contact_get_master_uids(contact); uid;
         uid = uid->next)
    {
      RosterAttachStatus status =
          attach_roster_contact(aggregator, uid->data, contact);

      if (status == NOT_ATTACHED)
        at_least_one_not_attached = TRUE;
      else if (status == POSTPONED)
      {
        at_least_one_postponed = TRUE;
        all_detached = FALSE;
      }
      else if (status == ATTACHED_TO_TEMPORARY)
        all_detached = FALSE;
      else if (status == ATTACHED_TO_MASTER)
      {
        if (!abook_uid)
          abook_uid = uid->data;

        all_detached = FALSE;
      }
      else
        break;
    }

    if (abook_uid)
    {
      if (g_list_find(priv->complete_rosters, roster))
      {
        _osso_abook_eventlogger_update_roster(
              osso_abook_roster_get_account(roster),
              osso_abook_contact_get_bound_name(contact), abook_uid);
      }
    }

    if (at_least_one_not_attached && !at_least_one_postponed)
      reject_dangling_roster_contacts(aggregator, contact);

    if (all_detached)
      create_temporary_master(aggregator, contact, __FUNCTION__);
  }

  osso_abook_aggregator_emit_all(aggregator, __FUNCTION__);
}

static gboolean
detach_roster_contact(OssoABookAggregatorPrivate *priv, const char *master_uid,
                      OssoABookContact *roster_contact)
{
  OssoABookContact *master_contact;
  GHashTable *ppned;

  g_return_val_if_fail(!IS_EMPTY(master_uid), FALSE);

  master_contact = g_hash_table_lookup(priv->master_contacts, master_uid);

  if (master_contact)
  {
    if (g_hash_table_remove(priv->temp_master_contacts, master_uid))
      g_ptr_array_add(priv->contacts_removed, g_object_ref(master_contact));

    osso_abook_contact_detach(master_contact, roster_contact);
    g_ptr_array_add(priv->contacts_changed, g_object_ref(master_contact));
    return TRUE;
  }

  ppned = g_hash_table_lookup(priv->postponed_contacts, master_uid);

  if (ppned && g_hash_table_remove(
        ppned, e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID)))
  {
    if (!g_hash_table_size(ppned))
      g_hash_table_remove(priv->postponed_contacts, master_uid);

    return TRUE;
  }

  return FALSE;
}

static void
roster_contacts_changed_cb(OssoABookRoster *roster, OssoABookContact **contacts,
                           gpointer user_data)
{
  OssoABookAggregator *aggregator = (OssoABookAggregator *)user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  OSSO_ABOOK_NOTE(
        AGGREGATOR, "%s@%p: roster contacts changed for %s",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator, osso_abook_roster_get_book_uri(roster));

  while (*contacts)
  {
    OssoABookContact *contact = *contacts++;
    RosterAttachStatus final_status = NOT_ATTACHED;
    gchar *first_uid = NULL;
    gboolean update = FALSE;
    const char *uid;
    OssoABookContact *roster_contact;
    GList *roster_uids;
    GList *contact_uids;

    uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);
    roster_contact = g_hash_table_lookup(priv->roster_contacts, uid);

    if (!roster_contact)
    {
      OSSO_ABOOK_WARN("roster contact %s not found", uid);
      continue;
    }

    roster_uids = osso_abook_contact_get_master_uids(roster_contact);
    roster_uids = osso_abook_string_list_copy(roster_uids);
    roster_uids = osso_abook_string_list_sort(roster_uids);

    contact_uids = osso_abook_contact_get_master_uids(contact);
    contact_uids = osso_abook_string_list_copy(contact_uids);
    contact_uids = osso_abook_string_list_sort(contact_uids);

    osso_abook_contact_reset(roster_contact, contact);

    while (roster_uids || contact_uids )
    {
      int uid_cmp_res;

      if (contact_uids)
      {
        if (roster_uids)
        {
          uid_cmp_res = strcmp(roster_uids->data, contact_uids->data);

          if (!uid_cmp_res)
          {
            RosterAttachStatus status;

            OSSO_ABOOK_NOTE(
                  AGGREGATOR, "%s@%p: updating %s for %s",
                  osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
                  aggregator, uid, contact_uids->data);

            status = attach_roster_contact(aggregator, contact_uids->data,
                                           roster_contact);

            if (status == ATTACHED_TO_MASTER && !first_uid)
              first_uid = g_strdup(contact_uids->data);

            if (final_status < status)
              final_status = status;

            roster_uids = osso_abook_string_list_chug(roster_uids);
            contact_uids = osso_abook_string_list_chug(contact_uids);
          }
        }
        else
          uid_cmp_res = 1;
      }
      else
        uid_cmp_res = -1;

      if (uid_cmp_res > 0)
      {
        RosterAttachStatus status;

        OSSO_ABOOK_NOTE(
              AGGREGATOR, "%s@%p: attaching %s to %s",
              osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
              aggregator, uid, contact_uids->data);

        status = attach_roster_contact(aggregator, contact_uids->data,
                                       roster_contact);

        if (status == ATTACHED_TO_MASTER)
        {
          update = TRUE;

          if (!first_uid)
            first_uid = g_strdup(contact_uids->data);
        }

        if (final_status < status)
          final_status = status;

        contact_uids = osso_abook_string_list_chug(contact_uids);
      }
      else if (uid_cmp_res < 0)
      {
        OSSO_ABOOK_NOTE(
              AGGREGATOR, "%s@%p: detaching %s from %s",
              osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
              aggregator, uid, roster_uids->data);

        if (detach_roster_contact(priv, roster_uids->data, roster_contact))
          update = TRUE;

        roster_uids = osso_abook_string_list_chug(roster_uids);
      }
    }

    if (final_status == NOT_ATTACHED)
      create_temporary_master(aggregator, roster_contact, __FUNCTION__);

    if (update)
    {
      g_warn_if_fail(first_uid != NULL);

      _osso_abook_eventlogger_update_roster(
            osso_abook_contact_get_account(roster_contact),
            osso_abook_contact_get_bound_name(roster_contact), first_uid);
    }

    g_free(first_uid);
  }

  osso_abook_aggregator_emit_all(aggregator, __FUNCTION__);
}

static void
roster_contacts_removed_cb(OssoABookRoster *roster, const char **uids,
                           gpointer user_data)
{
  OssoABookAggregator *aggregator = (OssoABookAggregator *)user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  while (*uids)
  {
    const char *uid = *uids++;
    OssoABookContact *roster_contact;
    GList *master_uids, *l;

    roster_contact = g_hash_table_lookup(priv->roster_contacts, uid);

    if (roster_contact)
    {
      _osso_abook_eventlogger_update_roster(
            osso_abook_contact_get_account(roster_contact),
            osso_abook_contact_get_bound_name(roster_contact),
            NULL);
      master_uids = osso_abook_string_list_copy(
            osso_abook_contact_get_master_uids(roster_contact));

      for (l = master_uids; l; l = l->next)
        detach_roster_contact(priv, l->data, roster_contact);

      osso_abook_string_list_free(master_uids);
      remove_temporary_master(aggregator, uid, NULL, __FUNCTION__);
      g_hash_table_remove(priv->roster_contacts, uid);
    }
    else
      OSSO_ABOOK_WARN("roster contact %s not found", uid);
  }

  osso_abook_aggregator_emit_all(aggregator, __FUNCTION__);
}

static void
roster_created_cb(OssoABookRosterManager *manager, OssoABookRoster *roster,
                  gpointer user_data)
{
  OssoABookAggregator *aggregator = user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  OSSO_ABOOK_NOTE(
        AGGREGATOR, "%s@%p: roster %s added",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator, osso_abook_roster_get_book_uri(roster));

  g_signal_connect(roster, "contacts-added",
                   G_CALLBACK(roster_contacts_added_cb), aggregator);
  g_signal_connect(roster, "contacts-changed",
                   G_CALLBACK(roster_contacts_changed_cb), aggregator);
  g_signal_connect(roster, "contacts-removed",
                   G_CALLBACK(roster_contacts_removed_cb), aggregator);
  g_signal_connect(roster, "sequence-complete",
                   G_CALLBACK(roster_sequence_complete_cb), aggregator);

  if (!priv->is_ready)
    priv->pending_rosters = g_list_prepend(priv->pending_rosters, roster);
}

static void
roster_removed_cb(OssoABookRosterManager *manager, OssoABookRoster *roster,
                  gpointer user_data)
{
  OssoABookAggregator *aggregator = (OssoABookAggregator *)user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  GPtrArray *removed;
  GHashTableIter iter;
  OssoABookContact *roster_contact;

  OSSO_ABOOK_NOTE(AGGREGATOR, "%s@%p: roster %s removed",
                  osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
                  aggregator, osso_abook_roster_get_book_uri(roster));

  g_signal_handlers_disconnect_matched(
    roster, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
        roster_contacts_added_cb, aggregator);
  g_signal_handlers_disconnect_matched(
    roster, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
    roster_contacts_changed_cb, aggregator);
  g_signal_handlers_disconnect_matched(
    roster, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
    roster_contacts_removed_cb, aggregator);
  g_signal_handlers_disconnect_matched(
    roster, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
    roster_sequence_complete_cb, aggregator);

  removed = g_ptr_array_new();
  g_hash_table_iter_init(&iter, priv->roster_contacts);

  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&roster_contact))
  {
    if (roster == osso_abook_contact_get_roster(roster_contact))
    {
      const char *uid =
          e_contact_get_const(E_CONTACT(roster_contact), E_CONTACT_UID);

      g_ptr_array_add(removed, (char *)uid);
    }
  }

  if (removed->len)
  {
    g_ptr_array_add(removed, NULL);
    roster_contacts_removed_cb(roster, (const char **)removed->pdata,
                               aggregator);
  }

  g_ptr_array_free(removed, TRUE);
  priv->pending_rosters = g_list_remove_all(priv->pending_rosters, roster);
  priv->complete_rosters = g_list_remove_all(priv->complete_rosters, roster);
}

static void
roster_manager_ready_cb(OssoABookWaitable *waitable, const GError *error,
                        gpointer user_data)
{
  OssoABookAggregator *aggregator = user_data;
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  OssoABookAggregatorState state = osso_abook_aggregator_get_state(aggregator);
  OssoABookRosterManager *roster_manager;
  GList *l;

  priv->closure = NULL;

  if (error)
  {
    OSSO_ABOOK_WARN("%s: %s", g_quark_to_string(error->domain), error->message);
    return;
  }

  priv->is_ready = TRUE;

  roster_manager = OSSO_ABOOK_ROSTER_MANAGER(waitable);

  for (l = osso_abook_roster_manager_list_rosters(roster_manager); l;
       l = g_list_delete_link(l, l))
  {
    if (!g_list_find(priv->complete_rosters, l->data))
      priv->pending_rosters = g_list_prepend(priv->pending_rosters, l->data);
  }

  OSSO_ABOOK_NOTE(
        AGGREGATOR, "%s@%p: roster-manager ready, %d pending rosters",
        osso_abook_roster_get_book_uri(OSSO_ABOOK_ROSTER(aggregator)),
        aggregator, g_list_length(priv->pending_rosters));

  osso_abook_aggregator_change_state(aggregator, state);
}

static void
osso_abook_aggregator_real_set_roster_manager(
    OssoABookAggregator *aggregator, OssoABookRosterManager *roster_manager)
{
  OssoABookAggregatorState state = osso_abook_aggregator_get_state(aggregator);
  OssoABookAggregatorPrivate *priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  priv->roster_manager_set = TRUE;

  if (roster_manager)
  {
    if (osso_abook_roster_is_running(OSSO_ABOOK_ROSTER(aggregator)))
    {
      OSSO_ABOOK_WARN("Cannot replace book view, %s is running.",
                      G_OBJECT_TYPE_NAME(aggregator));
      return;
    }

    g_object_ref(roster_manager);
  }

  if (priv->roster_manager)
  {
    GList *l;

    g_signal_handlers_disconnect_matched(
          priv->roster_manager, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
          0, 0, NULL, roster_created_cb, aggregator);
    g_signal_handlers_disconnect_matched(
          priv->roster_manager, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
          0, 0, NULL, roster_removed_cb, aggregator);

    for (l = osso_abook_roster_manager_list_rosters(priv->roster_manager); l;
         l = g_list_delete_link(l, l))
    {
      roster_removed_cb(priv->roster_manager, l->data, aggregator);
    }

    if (priv->closure)
    {
      osso_abook_waitable_cancel(OSSO_ABOOK_WAITABLE(priv->roster_manager),
                                 priv->closure);
      priv->closure = NULL;
    }

    g_object_unref(priv->roster_manager);
  }

  priv->is_ready = !roster_manager;

  priv->roster_manager = roster_manager;
  g_list_free(priv->pending_rosters);
  priv->pending_rosters = NULL;
  g_list_free(priv->complete_rosters);
  priv->complete_rosters = NULL;

  if (priv->roster_manager)
  {
    GList *l;

    priv->closure = osso_abook_waitable_call_when_ready(
          OSSO_ABOOK_WAITABLE(priv->roster_manager), roster_manager_ready_cb,
          aggregator, NULL);

    g_signal_connect(priv->roster_manager, "roster-created",
                     G_CALLBACK(roster_created_cb), aggregator);
    g_signal_connect(priv->roster_manager, "roster-removed",
                     G_CALLBACK(roster_removed_cb), aggregator);

    for (l = osso_abook_roster_manager_list_rosters(priv->roster_manager); l;
         l = g_list_delete_link(l, l))
    {
      roster_created_cb(priv->roster_manager, l->data, aggregator);
    }
  }

  osso_abook_aggregator_change_state(aggregator, state);
}

OssoABookRosterManager *
osso_abook_aggregator_get_roster_manager(OssoABookAggregator *aggregator)
{
  OssoABookAggregatorPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_AGGREGATOR(aggregator), NULL);

  priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  if (!priv->roster_manager && !priv->roster_manager_set)
  {
    osso_abook_aggregator_set_roster_manager(
          aggregator, osso_abook_roster_manager_get_default());
  }

  return priv->roster_manager;
}

void
osso_abook_aggregator_set_roster_manager(OssoABookAggregator *aggregator,
                                         OssoABookRosterManager *manager)
{
  g_return_if_fail(OSSO_ABOOK_IS_AGGREGATOR(aggregator));
  g_return_if_fail(OSSO_ABOOK_IS_ROSTER_MANAGER(manager) || !manager);

  g_object_set(aggregator,
               "roster-manager", manager,
               NULL);
}

GList *
osso_abook_aggregator_resolve_master_contacts(OssoABookAggregator *aggregator,
                                              OssoABookContact *contact)
{
  OssoABookAggregatorPrivate *priv;
  gpointer temp_master;
  GHashTable *tmp;
  GList *l;

  g_return_val_if_fail(OSSO_ABOOK_IS_AGGREGATOR(aggregator), NULL);
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);

  priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);
  temp_master = g_hash_table_lookup(
        priv->temp_master_contacts,
        e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID));

  tmp = g_hash_table_new(g_direct_hash, g_direct_equal);

  if (temp_master)
    g_hash_table_insert(tmp, temp_master, NULL);

  for (l = osso_abook_contact_get_master_uids(contact); l; l = l->next)
  {
    gpointer c = g_hash_table_lookup(priv->master_contacts, l->data);

    if (c)
      g_hash_table_insert(tmp, c, NULL);
  }

  l = g_hash_table_get_keys(tmp);
  g_hash_table_destroy(tmp);
  l = g_list_sort(l, (GCompareFunc)osso_abook_contact_uid_compare);

  return l;
}

GList *
osso_abook_aggregator_lookup(OssoABookAggregator *aggregator, const char *uid)
{
  OssoABookAggregatorPrivate *priv;
  OssoABookContact * c;

  g_return_val_if_fail(OSSO_ABOOK_IS_AGGREGATOR(aggregator), NULL);
  g_return_val_if_fail(NULL != uid, NULL);

  priv = OSSO_ABOOK_AGGREGATOR_PRIVATE(aggregator);

  c = g_hash_table_lookup(priv->master_contacts, uid);

  if (c)
    return g_list_prepend(NULL, c);

  c = g_hash_table_lookup(priv->roster_contacts, uid);

  if (c)
      return osso_abook_aggregator_resolve_master_contacts(aggregator, c);

  return NULL;
}

GList *
osso_abook_aggregator_find_contacts_for_phone_number(
    OssoABookAggregator *aggregator, const char *phone_number,
    gboolean fuzzy_match)
{
  EBookQuery *query;
  GList *contacts;

  g_return_val_if_fail(OSSO_ABOOK_IS_AGGREGATOR(aggregator), NULL);
  g_return_val_if_fail(!IS_EMPTY (phone_number), NULL);

  query = osso_abook_query_phone_number(phone_number, fuzzy_match);
  contacts = osso_abook_aggregator_find_contacts(aggregator, query);
  e_book_query_unref(query);

  if (contacts && contacts->next)
    return osso_abook_sort_phone_number_matches(contacts, phone_number);

  return contacts;
}
