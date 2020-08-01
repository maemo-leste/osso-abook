#include <glib.h>
#include <glib-object.h>

#include "config.h"

#include "osso-abook-aggregator.h"
#include "osso-abook-debug.h"

static OssoABookRoster *default_aggregator = NULL;

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
