#include <gdk/gdk.h>
#include <dbus/dbus.h>
#include <dbus/dbus-glib-bindings.h>

#include <math.h>

#include "osso-abook-utils-private.h"
#include "osso-abook-log.h"

#include "config.h"

__attribute__ ((visibility ("hidden"))) void
disconnect_signal_if_connected(gpointer instance, gulong handler)
{
  if (g_signal_handler_is_connected(instance, handler))
    g_signal_handler_disconnect(instance, handler);
}

__attribute__ ((visibility ("hidden"))) gchar *
_osso_abook_avatar_get_cache_name(int width, int height, gboolean crop,
                                  int radius, const guint8 border_color[4])
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

GdkPixbuf *
_osso_abook_scale_pixbuf_and_crop(const GdkPixbuf *image, int width, int height,
                                  int crop, int border_size)
{
  int in_w;
  int in_h;
  double w;
  double h;
  double ratio_x;
  double ratio_y;
  double c;
  int cy;
  int cx;
  int pix_x;
  int pix_y;
  GdkPixbuf *scaled;
  GdkPixbuf *src_pixbuf;

  in_w = gdk_pixbuf_get_width(image);
  in_h = gdk_pixbuf_get_height(image);

  if (width <= 0)
    width = in_w;

  if (height <= 0)
    height = in_h;

  w = (double)in_w;
  h = (double)in_h;

  ratio_x = (double)width / w;
  ratio_y = (double)height / h;

  if (crop)
  {
    if (ratio_x <= ratio_y)
      ratio_x = (double)height / (double)in_h;
  }
  else if (ratio_x >= ratio_y)
    ratio_x = (double)height / (double)in_h;


  if (ratio_x < 3.0)
    c = ratio_x;
  else
    c = 3.0;

  cy = c * h;
  cx = c * w;

  pix_x = (width - cx) / 2;
  pix_y = (height - cy) / 2;

  src_pixbuf = gdk_pixbuf_scale_simple(image, cx, cy, GDK_INTERP_BILINEAR);

  scaled = gdk_pixbuf_new(0, 1, 8, width + 2 * border_size,
                          height + 2 * border_size);

  gdk_pixbuf_fill(scaled, 0x7F7F7F00);

  gdk_pixbuf_copy_area(src_pixbuf,
                       width > cx ? 0 : (width - cx) / -2,
                       height > cy ? 0 : (height - cy) / -2,
                       cx >= width ? width : cx,
                       cy >= height ? height : cy , scaled,
                       pix_x < 0 ? border_size : border_size + pix_x,
                       pix_y < 0 ? border_size : border_size + pix_y);

  g_object_unref(src_pixbuf);

  return scaled;
}

void
_osso_abook_pixbuf_cut_corners(GdkPixbuf *pixbuf, const int radius,
                               const guint8 border_color[4])
{
  int r;
  guchar *pq3;
  guchar *pq2;
  guchar *pq1;
  guchar *pq4;
  guchar *quad2;
  guchar *quad1;
  guchar *quad3;
  guchar *quad4;
  int i;
  int width;
  int height;
  int rowstride;
  int row_pixels;
  int total_pixels;
  guchar *pixels;

  g_return_if_fail(gdk_pixbuf_get_has_alpha(pixbuf));
  g_return_if_fail(gdk_pixbuf_get_n_channels(pixbuf) == 4);
  g_return_if_fail(gdk_pixbuf_get_bits_per_sample(pixbuf) == 8);

  width = gdk_pixbuf_get_width(pixbuf);
  height = gdk_pixbuf_get_height(pixbuf);
  rowstride = gdk_pixbuf_get_rowstride(pixbuf);
  pixels = gdk_pixbuf_get_pixels(pixbuf);

  if (radius < 0)
    r = (MIN(width, height) + 9) / 10;
  else
    r = MIN((MIN(width, height) + 1) / 2, radius);

  row_pixels = 4 * width;
  total_pixels = height * rowstride;

  quad3 = &pixels[(height - 1) * rowstride];
  quad1 = &pixels[row_pixels - 4];
  quad4 = &quad3[row_pixels - 4];
  quad2 = pixels;

  pq1 = quad1;
  pq2 = quad2;
  pq3 = quad3;
  pq4 = quad4;

  for (i = r; i; i--)
  {
    int i2 = i * i;
    int j;

    for (j = r; j; j--)
    {
      double r1 = sqrt(i2 + j * j) - r;

      if (r1 < -1.0)
        continue;

      if (r1 >= 1.0)
      {
        pq4[3] = 0;
        pq3[3] = 0;
        pq1[3] = 0;
        pq2[3] = 0;
      }
      else if (r1 > 0.0)
      {
        double c = 1.0 - r1;

        if (!border_color)
        {
          pq1[3] = pq1[3] * c;
          pq2[3] = pq2[3] * c;
          pq3[3] = pq3[3] * c;
          pq4[3] = pq4[3] * c;
        }
        else
        {
          guchar a = border_color[3] * c;

          memcpy(pq1, border_color, 3);
          memcpy(pq2, border_color, 3);
          memcpy(pq3, border_color, 3);
          memcpy(pq4, border_color, 3);

          pq1[3] = a;
          pq2[3] = a;
          pq3[3] = a;
          pq4[3] = a;
        }
      }
      else if (border_color)
      {
        guchar b[4];
        double c1 = r1 + 1.0;

        r1 = - r1;

        b[0] = c1 * border_color[0];
        b[1] = c1 * border_color[1];
        b[2] = c1 * border_color[2];
        b[3] = c1 * border_color[3];

        pq1[0] = b[0] + pq1[0] * r1;
        pq1[1] = b[1] + pq1[1] * r1;
        pq1[2] = b[2] + pq1[2] * r1;
        pq1[3] = MAX(pq1[3], b[3]);

        pq2[0] = b[0] + pq2[0] * r1;
        pq2[1] = b[1] + pq2[1] * r1;
        pq2[2] = b[2] + pq2[2] * r1;
        pq2[3] = MAX(pq2[3], b[3]);

        pq3[0] = b[0] + pq3[0] * r1;
        pq3[1] = b[1] + pq3[1] * r1;
        pq3[2] = b[2] + pq3[2] * r1;
        pq3[3] = MAX(pq3[3], b[3]);

        pq4[0] = b[0] + pq4[0] * r1;
        pq4[1] = b[1] + pq4[1] * r1;
        pq4[2] = b[2] + pq4[2] * r1;
        pq4[3] = MAX(pq4[3], b[3]);
      }

      pq1 -= 4;
      pq2 += 4;
      pq3 += 4;
      pq4 -= 4;
    }

    quad4 -= rowstride;
    quad2 += rowstride;
    quad1 += rowstride;
    quad3 -= rowstride;

    pq4 = quad4;
    pq2 = quad2;
    pq3 = quad3;
    pq1 = quad1;
  }

  if (border_color)
  {
    guchar *p = &pixels[4 * r + total_pixels - rowstride];
    guchar *q = &pixels[4 * r];

    for (i = width - r; i > r; i--)
    {
      memcpy(q, border_color, 4);
      memcpy(p, border_color, 4);
      q += 4;
      p += 4;
    }

    p = &pixels[rowstride * r - 4 + row_pixels];
    q = &pixels[rowstride * r];

    for (i = height - r; i > r; i--)
    {
      memcpy(p, border_color, 4);
      memcpy(q, border_color, 4);
      q += rowstride;
      p += rowstride;
    }
  }
}

gchar *
_osso_abook_flags_to_string(GType flags_type, guint value)
{
  GFlagsClass *flags_class;
  GString *s;
  gboolean not_first = FALSE;

  g_return_val_if_fail(G_TYPE_IS_FLAGS(flags_type), NULL);

  flags_class = g_type_class_ref(flags_type);

  g_return_val_if_fail(G_IS_FLAGS_CLASS(flags_class), NULL);


  s = g_string_new(0);
  g_string_append_c(s, '(');

  for (int i = 0; i < flags_class->n_values; i++)
  {
    GFlagsValue *v = &flags_class->values[i];

    if (v->value && (value & v->value) == v->value)
    {
      if (not_first)
        g_string_append_c(s, '|');
      else
        not_first = TRUE;

      g_string_append(s, v->value_nick);
    }
  }

  g_string_append_c(s, ')');

  return g_string_free(s, FALSE);
}

gboolean
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

TpConnectionPresenceType
default_presence_convert(TpConnectionPresenceType presence_type)
{
  switch (presence_type)
  {
    case TP_CONNECTION_PRESENCE_TYPE_UNSET:
    case TP_CONNECTION_PRESENCE_TYPE_AVAILABLE:
    case TP_CONNECTION_PRESENCE_TYPE_ERROR:
      return presence_type;
    case TP_CONNECTION_PRESENCE_TYPE_OFFLINE:
    case TP_CONNECTION_PRESENCE_TYPE_HIDDEN:
      return TP_CONNECTION_PRESENCE_TYPE_OFFLINE;
    case TP_CONNECTION_PRESENCE_TYPE_AWAY:
    case TP_CONNECTION_PRESENCE_TYPE_EXTENDED_AWAY:
      return TP_CONNECTION_PRESENCE_TYPE_AWAY;
    case TP_CONNECTION_PRESENCE_TYPE_BUSY:
      return TP_CONNECTION_PRESENCE_TYPE_BUSY;
    default:
      return TP_CONNECTION_PRESENCE_TYPE_UNKNOWN;
  }
}

/**
 * e_vcard_attribute_param_merge_value:
 * @param: an #EVCardAttributeParam
 * @value: a string value to add
 * @cmp_func: a #GCompareFunc or %NULL
 *
 * Adds @value to @param's list of values if value is not already there.
 * (Eg. doesn't allow duplicate values).
 *
 * To decide whether @value is already present or not @cmp_func is called
 * with the two strings to compare as arguments. If @cmp_func is %NULL then
 * g_ascii_strcasecmp() is used.
 *
 * Note that if param is ENCODING then its value gets overwritten.
 *
 * Function copied from Fremantle EDS
 **/
void
osso_abook_e_vcard_attribute_param_merge_value(EVCardAttributeParam *param,
                                               const char *value,
                                               GCompareFunc cmp_func)
{
  g_return_if_fail(param != NULL);

  if (!cmp_func)
    cmp_func = (GCompareFunc)g_ascii_strcasecmp;

  if (!strcmp(e_vcard_attribute_param_get_name(param), EVC_ENCODING))
  {
    /* replace the value */
    e_vcard_attribute_param_remove_values(param);
    e_vcard_attribute_param_add_value(param, value);
  }
  else
  {
    GList *tmp = g_list_find_custom(e_vcard_attribute_param_get_values(param),
                                    value, cmp_func);

    if (!tmp)
      e_vcard_attribute_param_add_value(param, value);
  }
}

void
_osso_abook_button_set_date_style(HildonButton *button)
{
  g_return_if_fail(HILDON_IS_BUTTON(button));

  hildon_button_set_title_alignment(button, 0.0, 0.5);
  hildon_button_set_value_alignment(button, 0.0, 0.5);
  gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5);
  hildon_button_set_style(button, HILDON_BUTTON_STYLE_PICKER);
}

GdkPixbuf *
_osso_abook_get_cached_icon(gpointer widget, const gchar *icon_name, gint size)
{
  GHashTable *icon_cache =
      g_object_get_data(G_OBJECT(widget), "osso-abook-icon-cache");
  GdkPixbuf *icon;

  if (!icon_cache)
  {
    icon_cache = g_hash_table_new_full(
                   g_str_hash,
                   g_str_equal,
                   (GDestroyNotify)g_free,
                   (GDestroyNotify)g_object_unref);

    g_object_set_data_full(G_OBJECT(widget), "osso-abook-icon-cache",
                           icon_cache, (GDestroyNotify)g_hash_table_unref);
  }

  icon = g_hash_table_lookup(icon_cache, icon_name);

  if (!icon)
  {
    GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(widget));
    GtkIconTheme *theme = gtk_icon_theme_get_for_screen(screen);

    icon = gtk_icon_theme_load_icon(theme, icon_name, size, 0, NULL);

    if (!icon)
      return icon;

    g_hash_table_insert(icon_cache, g_strdup(icon_name), icon);
  }

  g_object_ref(icon);

  return icon;
}

gchar *
_osso_abook_get_safe_folder(const char *folder)
{
  gchar *mydocs_path = g_strdup(g_getenv("MYDOCSDIR"));
  gchar *path;

  /* whatever NB#93536 bug is */
  OSSO_ABOOK_WARN("drop this function when NB#93536 is fixed");

  if (mydocs_path)
    path = g_build_filename(mydocs_path, folder, NULL);
  else
  {
    const gchar *dir = g_getenv("HOME");

    if (!dir)
      dir = g_get_home_dir();

    mydocs_path = g_build_filename(dir, "MyDocs", NULL);
    path = g_build_filename(mydocs_path, folder, NULL);
  }

  g_free(mydocs_path);

  return path;
}
