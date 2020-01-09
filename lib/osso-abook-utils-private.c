#include "osso-abook-utils-private.h"

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
