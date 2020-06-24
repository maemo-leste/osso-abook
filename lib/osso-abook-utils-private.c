#include "osso-abook-utils-private.h"

#include "config.h"

__attribute__ ((visibility ("hidden"))) void
disconnect_signal_if_connected(gpointer instance, gulong handler)
{
  if (g_signal_handler_is_connected(instance, handler))
    g_signal_handler_disconnect(instance, handler);
}

__attribute__ ((visibility ("hidden"))) gchar *
create_avatar_name(int width, int height, gboolean crop, int radius,
                   const guint8 border_color[4])
{
  GString *name = g_string_new(0);

  g_string_printf(name, "%dx%d", width, height);

  if (crop)
    g_string_append(name, ";crop");

  if (radius > 0)
    g_string_append_printf(name, ";radius=%d", radius);
  else
    g_string_append(name, ";radius=auto");

  if (border_color)
  {
    g_string_append_printf(name, ";border=%02x%02x%02x%02x",
                           border_color[0], border_color[1],
                           border_color[2], border_color[3]);
  }

  return g_string_free(name, FALSE);
}

__attribute__ ((visibility ("hidden"))) void
osso_abook_list_push(GList **list, gpointer data)
{
  g_return_if_fail(NULL != list);
  g_return_if_fail(NULL != data);

  *list = g_list_prepend(*list, data);
}

__attribute__ ((visibility ("hidden"))) gpointer
osso_abook_list_pop(GList **list, gpointer data)
{
  GList *l;

  g_return_val_if_fail(NULL != list, NULL);

  if (data)
  {
    l = g_list_find(*list, data);

    if (!l)
      return data;
  }
  else
  {
    l = *list;

    if (!l)
      return data;

    data = l->data;
  }

  *list = g_list_delete_link(*list, l);

  return data;
}
