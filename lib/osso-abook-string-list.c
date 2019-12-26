#include <glib-object.h>

#include "osso-abook-string-list.h"

static gpointer
osso_abook_string_list_copy(gpointer boxed)
{
  GList *l;
  GList *copy = NULL;

  for(l = boxed; l; l = l->next)
      copy = g_list_prepend(copy, g_strdup(l->data));

  return g_list_reverse(copy);
}

void
osso_abook_string_list_free(OssoABookStringList list)
{
  GList *l;

  for (l = list; l; l = l->next)
  {
    g_free(l->data);
    l = g_list_delete_link(l, l);
  }
}

static void
_osso_abook_string_list_free(gpointer boxed)
{
  osso_abook_string_list_free(boxed);
}

G_DEFINE_BOXED_TYPE(OssoABookStringList,
                    osso_abook_string_list,
                    osso_abook_string_list_copy,
                    _osso_abook_string_list_free);
