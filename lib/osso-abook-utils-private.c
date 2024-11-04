#include "config.h"

#include <dbus/dbus-glib-bindings.h>
#include <dbus/dbus.h>
#include <gdk/gdk.h>
#include <gofono/gofono_manager.h>
#include <gofono/gofono_modem.h>
#include <gofono/gofono_names.h>
#include <gofono/gofono_netreg.h>
#include <gofono/gofono_simmgr.h>

#include <math.h>

#include "osso-abook-account-manager.h"
#include "osso-abook-contact.h"
#include "osso-abook-debug.h"
#include "osso-abook-log.h"
#include "osso-abook-utils-private.h"

#define OSSO_ABOOK_OFONO_TIMEOUT (15 * 1000)

__attribute__ ((visibility("hidden"))) void
disconnect_signal_if_connected(gpointer instance, gulong handler)
{
  if (g_signal_handler_is_connected(instance, handler))
    g_signal_handler_disconnect(instance, handler);
}

__attribute__ ((visibility("hidden"))) gchar *
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

__attribute__ ((visibility("hidden"))) void
osso_abook_list_push(GList **list, gpointer data)
{
  g_return_if_fail(NULL != list);
  g_return_if_fail(NULL != data);

  *list = g_list_prepend(*list, data);
}

__attribute__ ((visibility("hidden"))) gpointer
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
                       cy >= height ? height : cy, scaled,
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

        r1 = -r1;

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

    if (v->value && ((value & v->value) == v->value))
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
  const gchar *name;

  g_return_if_fail(param != NULL);

  if (!cmp_func)
    cmp_func = (GCompareFunc)g_ascii_strcasecmp;

  name = e_vcard_attribute_param_get_name(param);

  if (name && !g_ascii_strcasecmp(name, EVC_ENCODING))
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

static void
_osso_abook_phone_error(const gchar *func, GError **error, GQuark domain,
                        gint code, const gchar *message)
{
  if (error)
    g_set_error_literal(error, domain, code, message);
  else
    g_warning("%s: %s", func, message);
}

static void
_osso_abook_phone_error_message(gchar *func, GError **error, GQuark quark,
                                gint code, gchar *format, ...)
{
  gchar *message;
  va_list va;

  va_start(va, format);
  message = g_strdup_vprintf(format, va);
  _osso_abook_phone_error(func, error, quark, code, message);
  g_free(message);
  va_end(va);
}

static GQuark
_osso_abook_phone_net_error_quark()
{
  return g_quark_from_static_string("osso-abook-phone-net-error");
}

static GQuark
_osso_abook_phone_sim_error_quark()
{
  return g_quark_from_static_string("osso-abook-phone-sim-error");
}

#define osso_abook_phone_error_message(error, quark, code, format, ...) \
  _osso_abook_phone_error_message(G_STRLOC, \
                                  (error), \
                                  (quark), \
                                  (code), \
                                  (format), \
                                  ## __VA_ARGS__);

#define OSSO_ABOOK_PHONE_NET_ERROR (_osso_abook_phone_net_error_quark())
#define OSSO_ABOOK_PHONE_SIM_ERROR (_osso_abook_phone_sim_error_quark())

static OfonoSimMgr *
_get_sim(const char *modem_path, GError **error)
{
  OfonoManager *manager = ofono_manager_new();
  OfonoSimMgr *sim = NULL;
  GError *err = NULL;

  if (ofono_manager_wait_valid(manager, OSSO_ABOOK_OFONO_TIMEOUT, &err))
  {
    GPtrArray *modems = ofono_manager_get_modems(manager);
    OfonoModem *modem = NULL;
    int i;

    for (i = 0; i < modems->len; i++)
    {
      OfonoModem *candidate = g_ptr_array_index(modems, i);

      if (!ofono_modem_wait_valid(candidate, OSSO_ABOOK_OFONO_TIMEOUT, &err))
      {
        OSSO_ABOOK_WARN("OFONO modem [%s] wait ready timeout: '%s'",
                        ofono_modem_path(candidate), err->message);
        g_clear_error(&err);
      }
      else
      {
        if (modem_path)
        {
          if (!g_strcmp0(modem_path, ofono_modem_path(candidate)))
          {
            modem = candidate;
            break;
          }
        }
        else if (ofono_modem_has_interface(candidate,
                                           OFONO_SIMMGR_INTERFACE_NAME))
        {
          modem = candidate;
          break;
        }
      }
    }

    if (modem)
    {
      sim = ofono_simmgr_new(ofono_modem_path(modem));

      if (!ofono_simmgr_wait_valid(sim, OSSO_ABOOK_OFONO_TIMEOUT, &err))
      {
        gint code = err ? err->code : 0;

        osso_abook_phone_error_message(
          error, OSSO_ABOOK_PHONE_SIM_ERROR, code,
          "OFONO sim manager wait ready timeout, error code %d", code);
        g_clear_error(&err);
        ofono_simmgr_unref(sim);
        sim = NULL;
      }
    }
    else
    {
      if (modem_path)
      {
        osso_abook_phone_error_message(
          error, OSSO_ABOOK_PHONE_SIM_ERROR, 0,
          "OFONO modem %s not found", modem_path);
      }
      else
      {
        osso_abook_phone_error_message(
          error, OSSO_ABOOK_PHONE_SIM_ERROR, 0,
          "No OFONO modem with SIM found");
      }
    }
  }
  else
  {
    gint code = err ? err->code : 0;

    osso_abook_phone_error_message(
      error, OSSO_ABOOK_PHONE_SIM_ERROR, code,
      "OFONO manager wait ready timeout, error code %d", code);
    g_clear_error(&err);
  }

  ofono_manager_unref(manager);

  return sim;
}

static gchar *
_osso_abook_get_imsi(const char *modem_path, GError **error)
{
  gchar *imsi = NULL;

  OfonoSimMgr *sim = _get_sim(modem_path, error);

  if (sim)
  {
    if (sim->imsi)
      imsi = g_strdup(sim->imsi);

    ofono_simmgr_unref(sim);
  }

  return imsi;
}

__attribute__ ((visibility("hidden"))) gchar *
_osso_abook_get_operator_id(const char *modem_path, GError **error)
{
  gchar *imsi;
  gchar *id;

  gdk_threads_leave();
  imsi = _osso_abook_get_imsi(modem_path, error);
  gdk_threads_enter();

  if (!imsi)
    return NULL;

  id = g_strndup(imsi, 5);
  g_free(imsi);

  return id;
}

static gchar *
_mbpi_get_name(int mnc, int mcc)
{
  xmlDocPtr doc;
  gchar *name = NULL;

  doc = xmlParseFile(MBPI_DATABASE);

  if (doc)
  {
    /* Create xpath evaluation context */
    xmlXPathContextPtr ctx = xmlXPathNewContext(doc);

    if (ctx)
    {
      gchar *xpath = g_strdup_printf(
        "//network-id[@mcc='%03d' and @mnc='%02d']/../../name/text()", mcc,
        mnc);
      xmlXPathObjectPtr obj = xmlXPathEvalExpression(BAD_CAST xpath, ctx);

      if (obj && obj->nodesetval)
      {
        xmlNodeSetPtr nodes = obj->nodesetval;

        if (nodes->nodeNr)
        {
          xmlChar *content = xmlNodeGetContent(nodes->nodeTab[0]);

          name = g_strdup((const gchar *)content);

          xmlFree(content);
        }

        xmlXPathFreeObject(obj);
      }
      else
        OSSO_ABOOK_WARN("Unable to evaluate xpath expression '%s'", xpath);

      g_free(xpath);
      xmlXPathFreeContext(ctx);
    }
    else
      OSSO_ABOOK_WARN("Unable to create new XPath context");

    xmlFreeDoc(doc);
  }
  else
    OSSO_ABOOK_WARN("Unable to parse '" MBPI_DATABASE "'");

  return name;
}

__attribute__ ((visibility("hidden"))) gchar *
_osso_abook_get_operator_name(const char *modem_path, const char *imsi,
                              GError **error)
{
  gchar *operator_id = NULL;
  gchar *name = NULL;
  int mnc;
  int mcc;

  if (IS_EMPTY(imsi))
  {
    operator_id = _osso_abook_get_operator_id(modem_path, error);
    imsi = operator_id;
  }

  if (!IS_EMPTY(imsi))
  {
    gdk_threads_leave();

    if (sscanf(imsi, "%03d%02d", &mcc, &mnc) == 2)
    {
      OfonoManager *manager = ofono_manager_new();
      GError *err = NULL;
      gchar *spn = NULL;

      if (ofono_manager_wait_valid(manager, OSSO_ABOOK_OFONO_TIMEOUT, &err))
      {
        GPtrArray *modems = ofono_manager_get_modems(manager);
        int i;

        for (i = 0; i < modems->len; i++)
        {
          OfonoModem *modem = g_ptr_array_index(modems, i);

          if (ofono_modem_wait_valid(modem, OSSO_ABOOK_OFONO_TIMEOUT, &err))
          {
            if (ofono_modem_has_interface(modem, OFONO_NETREG_INTERFACE_NAME))
            {
              OfonoNetReg *net = ofono_netreg_new(ofono_modem_path(modem));

              if (ofono_netreg_valid(net) &&
                  net->mcc && net->mnc && !IS_EMPTY(net->name) &&
                  (mcc == atoi(net->mcc)) && (mnc == atoi(net->mnc)))
              {
                name = g_strdup(net->name);
                ofono_netreg_unref(net);
                break;
              }

              ofono_netreg_unref(net);
            }

            if (!spn &&
                ofono_modem_has_interface(modem, OFONO_SIMMGR_INTERFACE_NAME))
            {
              OfonoSimMgr *sim = ofono_simmgr_new(ofono_modem_path(modem));

              if (ofono_simmgr_wait_valid(sim, OSSO_ABOOK_OFONO_TIMEOUT, &err))
              {
                if (sim->mcc && sim->mnc && !IS_EMPTY(sim->spn) &&
                    (mcc == atoi(sim->mcc)) && (mnc == atoi(sim->mnc)))
                {
                  spn = g_strdup(sim->spn);
                }
              }
              else
              {
                OSSO_ABOOK_WARN(
                  "OFONO modem [%s] SIM manager wait ready timeout: '%s'",
                  ofono_modem_path(modem), err->message);
                g_clear_error(&err);
              }

              ofono_simmgr_unref(sim);
            }
          }
          else
          {
            OSSO_ABOOK_WARN("OFONO modem [%s] wait ready timeout: '%s'",
                            ofono_modem_path(modem), err->message);
            g_clear_error(&err);
          }
        }

        if (name)
          g_free(spn);
        else
          name = spn;
      }
      else
      {
        OSSO_ABOOK_WARN("OFONO manager wait ready timeout, [%s]", err->message);
        g_clear_error(&err);
      }

      ofono_manager_unref(manager);

      if (!name)
        name = _mbpi_get_name(mnc, mcc);

      if (!name)
      {
        osso_abook_phone_error_message(
          error,
          OSSO_ABOOK_PHONE_NET_ERROR,
          1001,
          "Failed to lookup operator name for mobile operator with MCC=%03d and MNC=%02d.",
          mcc,
          mnc);
      }
    }
    else
    {
      osso_abook_phone_error_message(
        error, OSSO_ABOOK_PHONE_NET_ERROR, 1001,
        "Invalid IMSI. First five numbers should be MCC and MNC.");
    }

    gdk_threads_enter();
  }

  g_free(operator_id);

  return name;
}

__attribute__ ((visibility("hidden"))) gboolean
_osso_abook_e_vcard_attribute_has_value(EVCardAttribute *attr)
{
  GList *val;

  for (val = e_vcard_attribute_get_values(attr); val; val = val->next)
  {
    gchar *tmp = g_strdup(val->data);

    if (!IS_EMPTY(g_strchomp(tmp)))
    {
      g_free(tmp);
      return TRUE;
    }

    g_free(tmp);
  }

  return FALSE;
}

static gchar *
get_im_service_name(OssoABookContact *rc)
{
  TpProtocol *protocol;
  gchar *name;

  g_return_val_if_fail(osso_abook_contact_is_roster_contact(rc), NULL);

  protocol = osso_abook_contact_get_protocol(rc);
  name = g_strdup(tp_protocol_get_name(protocol));
  g_object_unref(protocol);

  return name;
}

__attribute__ ((visibility("hidden"))) gchar *
_osso_abook_get_delete_confirmation_string(GList *contacts,
                                           gboolean show_contact_name,
                                           const gchar *no_im_format,
                                           gchar *single_service_format,
                                           gchar *mutiple_services_format)
{
  const char *display_name = NULL;
  gchar *service_name = NULL;
  gboolean has_roster_contact = FALSE;
  const char *msgid;
  gchar *confirmation_string;

  g_return_val_if_fail(no_im_format != NULL, NULL);
  g_return_val_if_fail(single_service_format != NULL, NULL);
  g_return_val_if_fail(mutiple_services_format != NULL, NULL);

  if (!contacts)
    return g_strdup(_(no_im_format));

  if (contacts->next)
  {
    if (show_contact_name)
      g_critical("Cannot show the contact name with multiple contacts");

    show_contact_name = FALSE;
  }
  else if (show_contact_name)
    display_name = osso_abook_contact_get_display_name(contacts->data);

  while (contacts)
  {
    GList *roster_contacts;
    gboolean names_mismatch = FALSE;

    if (osso_abook_contact_is_roster_contact(contacts->data))
      roster_contacts = g_list_prepend(NULL, contacts->data);
    else
      roster_contacts = osso_abook_contact_get_roster_contacts(contacts->data);

    if (roster_contacts)
    {
      while (roster_contacts)
      {
        gchar *name = get_im_service_name(roster_contacts->data);

        if (service_name)
        {
          if (strcmp(service_name, name))
          {
            g_free(name);
            g_free(service_name);
            service_name = NULL;
            g_list_free(roster_contacts);
            names_mismatch = TRUE;
            break;
          }

          g_free(name);
        }
        else
          service_name = name;

        roster_contacts = g_list_delete_link(roster_contacts, roster_contacts);
      }

      has_roster_contact = TRUE;
    }

    if (names_mismatch)
      break;

    contacts = contacts->next;
  }

  if (!has_roster_contact)
    msgid = _(no_im_format);
  else if (service_name)
    msgid = _(single_service_format);
  else
    msgid = _(mutiple_services_format);

  if (has_roster_contact && service_name)
  {
    if (show_contact_name)
      confirmation_string = g_strdup_printf(msgid, display_name, service_name);
    else
      confirmation_string = g_strdup_printf(msgid, service_name);
  }
  else if (show_contact_name)
    confirmation_string = g_strdup_printf(msgid, display_name);
  else
    confirmation_string = g_strdup(msgid);

  g_free(service_name);

  return confirmation_string;
}

__attribute__ ((visibility("hidden"))) gboolean
_osso_abook_tp_protocol_has_rosters(TpProtocol *protocol)
{
  GList *l;
  GList *next;
  GList *accounts;

  if (!(osso_abook_caps_from_tp_protocol(protocol) &
        OSSO_ABOOK_CAPS_ADDRESSBOOK))
  {
    return FALSE;
  }

  accounts = osso_abook_account_manager_list_by_protocol(
    NULL, tp_protocol_get_name(protocol));

  for (l = accounts; l; l = next)
  {
    next = l->next;

    if (!tp_account_is_enabled(l->data))
      accounts = g_list_delete_link(accounts, l);
  }

  g_list_free(accounts);

  return !!accounts;
}

__attribute__ ((visibility("hidden"))) gchar *
_osso_abook_tp_account_get_vcard_field(TpAccount *account)
{
  TpProtocol *protocol =
    osso_abook_account_manager_get_account_protocol_object(NULL, account);

  if (!protocol)
    return NULL;

  return g_strdup(tp_protocol_get_vcard_field(protocol));
}

static gboolean
_is_regular_file(const gchar *fname)
{
  if (!fname)
    return FALSE;

  return g_file_test(fname, G_FILE_TEST_IS_REGULAR);
}

__attribute__ ((visibility("hidden"))) gboolean
_is_local_file(const gchar *uri)
{
  const gchar *fname;

  if (!uri || !*uri)
    return FALSE;

  if (g_str_has_prefix(uri, FILE_SCHEME))
    fname = uri + strlen(FILE_SCHEME);
  else if (*uri == '/')
    fname = uri;
  else
    return FALSE;

  return _is_regular_file(fname);
}

/*
   don't really like it, but I was not able to find a way to create
   in-memory ESource
 */
__attribute__ ((visibility("hidden"))) ESource *
_osso_abook_create_source(const gchar *uid, const gchar *backend_name)
{
  gchar *_uid = g_strdup(uid);
  GError *error = NULL;
  ESourceRegistry *registry;
  ESource *source = NULL;

  registry = e_source_registry_new_sync(NULL, &error);

  if (error)
  {
    OSSO_ABOOK_WARN("Creating ESourceRegistry for uid %s failed - %s", uid,
                    error->message);
    g_clear_error(&error);
    goto err_out;
  }

  e_filename_make_safe(_uid);
  source = e_source_registry_ref_source(registry, _uid);

  if (source)
  {
    g_warn_if_fail(e_source_has_extension(source,
                                          E_SOURCE_EXTENSION_ADDRESS_BOOK));
    g_warn_if_fail(e_source_has_extension(source, E_SOURCE_EXTENSION_RESOURCE));
  }
  else
  {
    OSSO_ABOOK_NOTE(TP, "creating new EDS source %s for %s", _uid, uid);
    source = e_source_new_with_uid(_uid, NULL, &error);

    if (source)
    {
      ESourceBackend *backend =
        e_source_get_extension(source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
      ESourceResource *resource =
        e_source_get_extension(source, E_SOURCE_EXTENSION_RESOURCE);
      GList *sources = NULL;

      e_source_resource_set_identity(resource, uid);
      e_source_backend_set_backend_name(backend, backend_name);
      e_source_set_display_name(source, uid);

      sources = g_list_append(sources, source);
      e_source_registry_create_sources_sync(registry, sources, NULL, &error);

      g_list_free(sources);
    }

    if (error)
    {
      OSSO_ABOOK_WARN("Creating ESource for uid %s failed - %s", uid,
                      error->message);
      g_clear_error(&error);

      if (source)
      {
        g_object_unref(source);
        source = NULL;
      }
    }
  }

  _osso_abook_unref_registry_idle(registry);

err_out:

  g_free(_uid);

  return source;
}

static gboolean
_unref_registry_idle(gpointer data)
{
  g_object_unref(data);

  return FALSE;
}

/* This is ugly workaround for ESourceRegistry unref hang with gdk_threads */
void
_osso_abook_unref_registry_idle(ESourceRegistry *registry)
{
  g_idle_add(_unref_registry_idle, registry);
}
