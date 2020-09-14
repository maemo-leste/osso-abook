#include <glib-object.h>

#include "osso-abook-string-list.h"

GList *
osso_abook_string_list_copy(GList *list)
{
  GList *l;
  GList *copy = NULL;

  for(l = list; l; l = l->next)
      copy = g_list_prepend(copy, g_strdup(l->data));

  return g_list_reverse(copy);
}

static gpointer
_osso_abook_string_list_copy(gpointer boxed)
{
  return osso_abook_string_list_copy(boxed);
}

void
osso_abook_string_list_free(OssoABookStringList list)
{
  GList *l;

  for (l = list; l; l = g_list_delete_link(l, l))
    g_free(l->data);
}

static void
_osso_abook_string_list_free(gpointer boxed)
{
  osso_abook_string_list_free(boxed);
}

G_DEFINE_BOXED_TYPE(OssoABookStringList,
                    osso_abook_string_list,
                    _osso_abook_string_list_copy,
                    _osso_abook_string_list_free);

GList *
osso_abook_string_list_sort(GList *list)
{
  return g_list_sort(list, (GCompareFunc)strcmp);
}

GList *
osso_abook_string_list_chug(GList *list)
{
  if (list)
  {
    g_free(list->data);
    list = g_list_delete_link(list, list);
  }

  return list;
}

GList *
osso_abook_string_list_find(GList *list, const char *s)
{
  return g_list_find_custom(list, s, (GCompareFunc)strcmp);
}
