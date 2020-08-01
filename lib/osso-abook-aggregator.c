#include <glib.h>
#include <glib-object.h>

#include "osso-abook-aggregator.h"

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
