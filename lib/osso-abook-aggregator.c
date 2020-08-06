#include <glib.h>
#include <glib-object.h>
#include <libedata-book/libedata-book.h>

#include "config.h"

#include "osso-abook-aggregator.h"
#include "osso-abook-debug.h"
#include "osso-abook-roster.h"
#include "osso-abook-waitable.h"

struct _OssoABookAggregatorPrivate
{
  int sexp;
  int field_4;
  int field_8;
  int field_C;
  int field_10;
  GHashTable *contacts;
  int field_18;
  GHashTable *field_1C;
  int field_20;
  int field_24;
  GPtrArray *field_28;
  int field_2C;
  int field_30;
  int field_34;
  int flags;
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
osso_abook_aggregator_init(OssoABookAggregator *aggregator)
{
#pragma message("FIXME!!!")
  g_assert(0);
}

static void osso_abook_aggregator_class_init(OssoABookAggregatorClass *klass)
{
#pragma message("FIXME!!!")
  g_assert(0);
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

  if (priv->flags & 1)
    state |= OSSO_ABOOK_AGGREGATOR_MASTERS_READY;

  if (priv->flags & 2 && !priv->field_30)
    state |= OSSO_ABOOK_AGGREGATOR_ROSTERS_READY;

  return state;
}
