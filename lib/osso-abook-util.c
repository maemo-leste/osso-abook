/*
 * osso-abook-util.c
 *
 * Copyright (C) 2020 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

/**
 * SECTION:osso-abook-util
 * @short_description: Various utility functions
 *
 * This module provides convenience functions for performing common tasks in
 * libosso-abook.
 *
 */

#include "config.h"

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <errno.h>
#include <gdk/gdk.h>
#include <gdk/gdkx.h>
#include <glib/gstdio.h>
#include <math.h>

#include "osso-abook-account-manager.h"
#include "osso-abook-avatar-image.h"
#include "osso-abook-debug.h"
#include "osso-abook-filter-model.h"
#include "osso-abook-util.h"
#include "osso-abook-utils-private.h"

#define OSSO_ABOOK_TEL_DIGITS ("0123456789")
#define OSSO_ABOOK_TEL_CHARS ("0123456789" OSSO_ABOOK_DTMF_CHARS)

struct OssoABookAsyncPixbufData
{
  int width;
  int height;
  gboolean preserve_aspect_ratio;
  gsize maximum_size;
  int io_priority;
  GCancellable *cancellable;
  guint8 buf[8192];
  GdkPixbufLoader *pixbuf_loader;
  GSimpleAsyncResult *async_result;
};

gboolean
osso_abook_screen_is_landscape_mode(GdkScreen *screen)
{
  g_return_val_if_fail(GDK_IS_SCREEN(screen), TRUE);

  return gdk_screen_get_width(screen) > gdk_screen_get_height(screen);
}

static void
osso_abook_pannable_area_size_request(GtkWidget *widget,
                                      GtkRequisition *requisition,
                                      gpointer user_data)
{
  GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));

  if (!child)
    return;

  if (gtk_widget_get_sensitive(child))
  {
    GdkScreen *screen = gtk_widget_get_screen(widget);

    if (screen)
    {
      gint screen_height = gdk_screen_get_height(screen);
      GtkRequisition child_requisition;

      gtk_widget_size_request(child, &child_requisition);

      if (screen_height > child_requisition.height + requisition->height)
        requisition->height = child_requisition.height + requisition->height;
      else
        requisition->height = screen_height;
    }
  }
}

GtkWidget *
osso_abook_pannable_area_new(void)
{
  GtkWidget *area = hildon_pannable_area_new();

  g_object_set(area,
               "hscrollbar-policy", GTK_POLICY_NEVER,
               "mov-mode", HILDON_MOVEMENT_MODE_VERT,
               NULL);
  g_signal_connect(area, "size-request",
                   G_CALLBACK(osso_abook_pannable_area_size_request), NULL);

  return area;
}

static gboolean
_live_search_refilter_cb(HildonLiveSearch *live_search)
{
  GtkTreeModelFilter *filter = hildon_live_search_get_filter(live_search);

  g_return_val_if_fail(OSSO_ABOOK_IS_FILTER_MODEL(filter), FALSE);

  osso_abook_filter_model_set_prefix(OSSO_ABOOK_FILTER_MODEL(filter),
                                     hildon_live_search_get_text(live_search));

  return TRUE;
}

GtkWidget *
osso_abook_live_search_new_with_filter(OssoABookFilterModel *filter)
{
  GtkWidget *live_search = hildon_live_search_new();

  hildon_live_search_set_filter(HILDON_LIVE_SEARCH(live_search),
                                GTK_TREE_MODEL_FILTER(filter));
  g_signal_connect(live_search, "refilter",
                   G_CALLBACK(_live_search_refilter_cb), 0);

  return live_search;
}

char *
osso_abook_concat_names(OssoABookNameOrder order, const gchar *primary,
                        const gchar *secondary)
{
  const char *sep;

  if (!primary || !*primary)
    return g_strdup(secondary);

  if (!secondary || !*secondary)
    return g_strdup(primary);

  if (order == OSSO_ABOOK_NAME_ORDER_LAST)
    sep = ", ";
  else
    sep = " ";

  return g_strchomp(g_strchug(g_strconcat(primary, sep, secondary, NULL)));
}

const char *
osso_abook_get_work_dir()
{
  static gchar *work_dir = NULL;

  if (!work_dir)
    work_dir = g_build_filename(g_get_home_dir(), ".osso-abook", NULL);

  return work_dir;
}

const char *
osso_abook_get_photos_dir()
{
  static gchar *photos_dir = NULL;

  if (!photos_dir)
    photos_dir = g_build_filename(osso_abook_get_work_dir(), "photos", NULL);

  return photos_dir;
}

EBook *
osso_abook_system_book_dup_singleton(gboolean open, GError **error)
{
  static EBook *book;
  static gboolean is_opened;

  if (!book)
  {
    ESourceRegistry *registry = e_source_registry_new_sync(NULL, error);
    ESource *source;

    if (!registry)
      return NULL;

    source = e_source_registry_ref_default_address_book(registry);

    /* Do not allow non-addressbook, telepathy or sim sources */
    if (e_source_has_extension(source, E_SOURCE_EXTENSION_ADDRESS_BOOK))
    {
      ESourceBackend *backend;
      const gchar *backend_name;

      backend = e_source_get_extension(source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
      backend_name = e_source_backend_get_backend_name(backend);

      if (backend_name &&
          (!strcmp(backend_name, OSSO_ABOOK_BACKEND_TELEPATHY) ||
           !strcmp(backend_name, OSSO_ABOOK_BACKEND_SIM)))
      {
        g_clear_object(&source);
      }
      else
      {
        OSSO_ABOOK_NOTE(EDS, "Using default addressbook backend %s",
                        backend_name);
      }
    }
    else
      g_clear_object(&source);

    if (!source)
    {
      ESourceBackend *backend;
      const gchar *backend_name;

      source = e_source_registry_ref_builtin_address_book(registry);
      backend = e_source_get_extension(source, E_SOURCE_EXTENSION_ADDRESS_BOOK);
      backend_name = e_source_backend_get_backend_name(backend);

      OSSO_ABOOK_NOTE(EDS, "Using built in addressbook backend %s",
                      backend_name);
    }

    book = e_book_new(source, error);

    g_object_unref(source);

    if (!book)
      return NULL;

    g_object_add_weak_pointer(G_OBJECT(book), (gpointer *)&book);
    is_opened = FALSE;
  }
  else
    g_object_ref(book);

  if (!open || is_opened)
    return book;

  if (e_book_open(book, FALSE, error))
  {
    g_object_set_data(G_OBJECT(book), "opened", GINT_TO_POINTER(1));
    is_opened = TRUE;
    return book;
  }

  g_object_unref(book);

  return NULL;
}

gboolean
osso_abook_is_fax_attribute(EVCardAttribute *attribute)
{
  GList *p;
  gboolean is_fax = FALSE;
  const gchar *name;

  g_return_val_if_fail(attribute != NULL, FALSE);

  name = e_vcard_attribute_get_name(attribute);

  if (!name || g_ascii_strcasecmp(name, EVC_TEL))
    return FALSE;

  for (p = e_vcard_attribute_get_params(attribute); p; p = p->next)
  {
    const gchar *pname = e_vcard_attribute_param_get_name(p->data);

    if (pname && !g_ascii_strcasecmp(pname, EVC_TYPE))
    {
      GList *l;

      for (l = e_vcard_attribute_param_get_values(p->data); l; l = l->next)
      {
        if (l->data)
        {
          if (!g_ascii_strcasecmp(l->data, "CELL") ||
              !g_ascii_strcasecmp(l->data, "CAR") ||
              !g_ascii_strcasecmp(l->data, "VOICE"))
          {
            return FALSE;
          }

          if (!g_ascii_strcasecmp(l->data, "FAX"))
            is_fax = TRUE;
        }
      }
    }
  }

  return is_fax;
}

gboolean
osso_abook_is_mobile_attribute(EVCardAttribute *attribute)
{
  GList *p;
  const gchar *name;

  g_return_val_if_fail(attribute != NULL, FALSE);

  name = e_vcard_attribute_get_name(attribute);

  if (!name || g_ascii_strcasecmp(name, EVC_TEL))
    return FALSE;

  for (p = e_vcard_attribute_get_params(attribute); p; p = p->next)
  {
    name = e_vcard_attribute_param_get_name(p->data);

    if (name && !g_ascii_strcasecmp(name, EVC_TYPE))
    {
      GList *v;

      for (v = e_vcard_attribute_param_get_values(p->data); v; v = v->next)
      {
        if (v->data)
        {
          if (!g_ascii_strcasecmp(v->data, "CELL") ||
              !g_ascii_strcasecmp(v->data, "CAR"))
          {
            return TRUE;
          }
        }
      }
    }
  }

  return FALSE;
}

const gchar *
osso_abook_tp_account_get_bound_name(TpAccount *account)
{
  const gchar *bound_name;

  g_return_val_if_fail(TP_IS_ACCOUNT(account), NULL);

  bound_name = tp_account_get_normalized_name(account);

  if (IS_EMPTY(bound_name))
  {
    bound_name = tp_asv_get_string(tp_account_get_parameters(account),
                                   "account");
  }

  return bound_name;
}

/* move to EDS if needed */
/**
 * e_normalize_phone_number:
 * @phone_number: the phone number
 *
 * Normalizes @phone_number. Comma, period, parentheses, hyphen, space,
 * tab and forward-slash characters are stripped. The characters 'p',
 * 'w' and 'x' are copied in uppercase. A plus only is valid at the
 * beginning, or after a number suppression prefix ("*31#", "#31#"). All
 * other characters are copied verbatim.
 *
 * Returns: A newly allocated string containing the normalized phone
 * number.
 */
static char *
e_normalize_phone_number (const char *phone_number)
{
  GString *result;
  const char *p;

  /* see cui_utils_normalize_number() of rtcom-call-ui for reference */
  g_return_val_if_fail(NULL != phone_number, NULL);

  result = g_string_new(NULL);

  for (p = phone_number; *p; ++p)
  {
    switch (*p)
    {
      case 'p': case 'P':
      {
        /* Normalize this characters to P -
         * pause for one second. */
        g_string_append_c(result, 'P');
        break;
      }

      case 'w': case 'W':
      {
        /* Normalize this characters to W -
         * wait until confirmation. */
        g_string_append_c(result, 'W');
        break;
      }

      case 'x': case 'X':
      {
        /* Normalize this characters to X -
         * alias for 'W' it seems */
        g_string_append_c(result, 'X');
        break;
      }

      case '+':
      {
        /* Plus only is valid on begin of phone numbers and
         * after number suppression prefix */
        if ((0 == result->len) ||
            strcmp(result->str, "*31#") ||
            strcmp(result->str, "#31#"))
          g_string_append_c(result, *p);

        break;
      }

      case ',': case '.': case '(': case ')':
      case '-': case ' ': case '\t': case '/':
      {
        /* Skip commonly used delimiters */
        break;
      }

      case '#': case '*':
      case '0': case '1': case '2': case '3': case '4':
      case '5': case '6': case '7': case '8': case '9':
      default:
      {
        /* Directly accept all other characters. */
        g_string_append_c(result, *p);
        break;
      }
    }
  }

  return g_string_free(result, FALSE);
}

EBookQuery *
osso_abook_query_phone_number(const char *phone_number, gboolean fuzzy_match)
{
  gsize len;
  EBookQuery *query;
  gchar *phone_num = NULL;
  int nqs = 1;
  EBookQuery *qs[3];

  g_return_val_if_fail(!IS_EMPTY(phone_number), NULL);

  qs[0] = e_book_query_vcard_field_test(EVC_TEL, E_BOOK_QUERY_IS, phone_number);
  len = strcspn(phone_number, OSSO_ABOOK_DTMF_CHARS);

  if (len < strlen(phone_number))
  {
    phone_num = g_strndup(phone_number, len);

    if (phone_num)
    {
      qs[1] = e_book_query_vcard_field_test(EVC_TEL, E_BOOK_QUERY_IS, phone_num);
      nqs = 2;
    }
  }

  if (fuzzy_match)
  {
    EBookQueryTest test;
    char *normalized;
    size_t normalized_len;
    const char *phone_num_norm;

    if (phone_num)
      phone_number = phone_num;

    normalized = e_normalize_phone_number(phone_number);
    normalized_len = strlen(normalized);

    if (normalized_len <= 6)
    {
      phone_num_norm = normalized;
      test = E_BOOK_QUERY_IS;
    }
    else
    {
      phone_num_norm = &normalized[normalized_len - 7];
      test = E_BOOK_QUERY_ENDS_WITH;
    }

    qs[nqs++] = e_book_query_vcard_field_test(EVC_TEL, test, phone_num_norm);
    g_free(normalized);
  }

  g_free(phone_num);

  if (nqs == 1)
    query = qs[0];
  else
    query = e_book_query_or(nqs, qs, TRUE);

  return query;
}

struct phone_sort_data
{
  OssoABookContact *contact;
  int weigth;
};

static gint
sort_phone_number_matches(struct phone_sort_data *a, struct phone_sort_data *b)
{
  int w1 = a->weigth;
  int w2 = b->weigth;

  if (w1 > w2)
    return -1;

  if (w1 < w2)
    return 1;

  return osso_abook_contact_uid_compare(a->contact, b->contact);
}

GList *
osso_abook_sort_phone_number_matches(GList *matches, const char *phone_number)
{
  char *norm_phone_num;
  GList *l;
  char *norm_phone_num_unprefixed;

  g_return_val_if_fail(NULL != phone_number, matches);
  g_return_val_if_fail(0 != *phone_number, matches);

  norm_phone_num = e_normalize_phone_number(phone_number);
  norm_phone_num_unprefixed =
    &norm_phone_num[strcspn(norm_phone_num, OSSO_ABOOK_DTMF_CHARS) - 1];

  for (l = matches; l; l = l->next)
  {
    struct phone_sort_data *data = g_slice_new0(struct phone_sort_data);
    const char *val = NULL;
    GList *attr;
    gboolean a, b;
    char *p, *q;
    char *norm_val;
    size_t norm_val_len;
    char *norm_val_unprefixed;

    data->contact = l->data;
    l->data = data;

    for (attr = e_vcard_get_attributes(E_VCARD(data->contact)); attr;
         attr = attr->next)
    {
      const gchar *name = e_vcard_attribute_get_name(attr->data);

      if (name && !g_ascii_strcasecmp(name, EVC_TEL))
      {
        GList *vals = e_vcard_attribute_get_values(attr->data);

        if (vals)
        {
          val = vals->data;

          if (val && *val)
            break;
        }
      }
    }

    if (!val || !*val)
      continue;

    if (!strcmp(val, phone_number))
    {
      data->weigth = G_MAXINT;
      break;
    }

    norm_val = e_normalize_phone_number(val);
    norm_val_len = strcspn(norm_val, OSSO_ABOOK_DTMF_CHARS);
    p = norm_phone_num_unprefixed;
    norm_val_unprefixed = &norm_val[norm_val_len - 1];
    q = &norm_val[norm_val_len - 1];

    while (1)
    {
      a = q >= norm_val;
      b = p >= norm_phone_num;

      if (b)
        b = q >= norm_val;

      if (!b)
        break;

      if (*(p-- - 1) != *(q-- - 1))
      {
        a = norm_val <= q;
        break;
      }
    }

    if (p < norm_phone_num)
      a = TRUE;

    if (a)
    {
      gint w1 = data->weigth;
      gint w2 = norm_val_unprefixed - q - 1;

      if (w1 < w2)
        data->weigth = w2;
      else
        data->weigth = w1;
    }

    g_free(norm_val);
  }

  g_free(norm_phone_num);
  matches = g_list_sort(matches, (GCompareFunc)sort_phone_number_matches);

  for (l = matches; l; l = l->next)
  {
    struct phone_sort_data *data = l->data;

    l->data = data->contact;
    g_slice_free(struct phone_sort_data, data);
  }

  return matches;
}

GtkWidget *
osso_abook_avatar_button_new(OssoABookAvatar *avatar, int size)
{
  GtkWidget *avatar_image =
    osso_abook_avatar_image_new_with_avatar(avatar, size);
  GtkWidget *button = hildon_gtk_button_new(HILDON_SIZE_AUTO_WIDTH);

  gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
  gtk_widget_set_name(button, "osso-abook-avatar-button");
  gtk_container_add(GTK_CONTAINER(button), avatar_image);
  gtk_widget_show(avatar_image);

  return button;
}

/*
   fmg - GSimpleAsyncResult is deprecated in favor of GTask, but I have better
   things to do now
 */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
static void
_size_prepared_cb(GdkPixbufLoader *loader, gint width, gint height,
                  gpointer user_data)
{
  struct OssoABookAsyncPixbufData *data = user_data;
  int req_width = data->width;
  int req_height = data->height;

  if ((req_width > 0) && (req_height > 0))
  {
    if (data->preserve_aspect_ratio)
    {
      double ratio = MIN((double)req_width / (double)width,
                         (double)req_height / (double)height);

      height = ratio * (double)height;
      width = ratio * (double)width;
    }
    else
    {
      width = req_width;
      width = req_height;
    }

    gdk_pixbuf_loader_set_size(loader, width, height);
  }

  if (!data->maximum_size)
    return;

  if (width * height > data->maximum_size)
  {
    GdkPixbufFormat *format = gdk_pixbuf_loader_get_format(data->pixbuf_loader);

    if (format && gdk_pixbuf_format_is_scalable(format))
    {
      double coeff =
        sqrt((double)(width * height) / (double)data->maximum_size);

      gdk_pixbuf_loader_set_size(loader, coeff * (double)width,
                                 coeff * (double)height);
    }
    else
    {
      g_simple_async_result_set_handle_cancellation(data->async_result, FALSE);
      g_simple_async_result_set_error(
        data->async_result,
        gdk_pixbuf_error_quark(),
        gdk_pixbuf_error_quark(),
        "Image exceeds arbitary size limit of %.1f mebipixels",
        (((double)data->maximum_size) / 1024.f) / 1024.f);
      g_cancellable_cancel(data->cancellable);
    }
  }
}

static void
async_pixbuf_data_destroy(struct OssoABookAsyncPixbufData *data)
{
  if (data->async_result)
  {
    g_simple_async_result_complete(data->async_result);
    g_object_unref(data->async_result);
  }

  if (data->cancellable)
    g_object_unref(data->cancellable);

  if (data->pixbuf_loader)
  {
    g_signal_handlers_disconnect_matched(
      data->pixbuf_loader, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0,
      NULL, _size_prepared_cb, data);
    gdk_pixbuf_loader_close(data->pixbuf_loader, NULL);
    g_object_unref(data->pixbuf_loader);
  }

  g_free(data);
}

static void
_async_pixbuf_stream_closed_cb(GObject *source_object, GAsyncResult *res,
                               gpointer user_data)
{
  struct OssoABookAsyncPixbufData *data = user_data;
  GError *error = NULL;

  if (g_input_stream_close_finish(G_INPUT_STREAM(source_object), res, &error))
  {
    if (gdk_pixbuf_loader_close(data->pixbuf_loader, &error))
    {
      GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(data->pixbuf_loader);

      if (pixbuf)
      {
        g_simple_async_result_set_op_res_gpointer(
          data->async_result, g_object_ref(pixbuf),
          (GDestroyNotify)&g_object_unref);
      }
    }
  }

  if (error)
  {
    if (!g_cancellable_is_cancelled(data->cancellable))
    {
      if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_simple_async_result_set_from_error(data->async_result, error);
    }

    g_clear_error(&error);
  }

  async_pixbuf_data_destroy(data);
}

static void
async_pixbuf_finish(struct OssoABookAsyncPixbufData *data, GInputStream *is,
                    GError *error)
{
  if (error)
  {
    if (!g_cancellable_is_cancelled(data->cancellable))
    {
      if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_simple_async_result_set_from_error(data->async_result, error);
    }

    g_clear_error(&error);
  }

  g_input_stream_close_async(is, data->io_priority, data->cancellable,
                             _async_pixbuf_stream_closed_cb, data);
}

static void
_async_pixbuf_bytes_read_cb(GObject *source_object, GAsyncResult *res,
                            gpointer user_data)
{
  struct OssoABookAsyncPixbufData *data = user_data;
  GInputStream *in = G_INPUT_STREAM(source_object);
  GError *error = NULL;
  gssize size = g_input_stream_read_finish(in, res, &error);

  if ((size > 0) &&
      gdk_pixbuf_loader_write(data->pixbuf_loader, data->buf, size, &error))
  {
    g_input_stream_read_async(in, data->buf, sizeof(data->buf),
                              data->io_priority, data->cancellable,
                              _async_pixbuf_bytes_read_cb, data);
  }
  else
    async_pixbuf_finish(data, in, error);
}

static void
_ready_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  struct OssoABookAsyncPixbufData *data = user_data;
  GError *error = NULL;
  GFileInputStream *in = g_file_read_finish(G_FILE(source_object), res, &error);

  if (in)
  {
    g_input_stream_read_async(G_INPUT_STREAM(in), data->buf, sizeof(data->buf),
                              data->io_priority, data->cancellable,
                              _async_pixbuf_bytes_read_cb, data);
  }
  else
  {
    if (!g_cancellable_is_cancelled(data->cancellable))
    {
      if (!g_error_matches(error, G_IO_ERROR, G_IO_ERROR_CANCELLED))
        g_simple_async_result_set_from_error(data->async_result, error);
    }

    g_clear_error(&error);
    async_pixbuf_data_destroy(data);
  }
}

void
osso_abook_load_pixbuf_at_scale_async(GFile *file, int width, int height,
                                      gboolean preserve_aspect_ratio,
                                      gsize maximum_size, int io_priority,
                                      GCancellable *cancellable,
                                      GAsyncReadyCallback callback,
                                      gpointer user_data)
{
  struct OssoABookAsyncPixbufData *data =
    g_new0(struct OssoABookAsyncPixbufData, 1);

  g_return_if_fail(G_IS_FILE(file));
  g_return_if_fail(!cancellable || G_IS_CANCELLABLE(cancellable));
  g_return_if_fail(callback);

  data->maximum_size = maximum_size;
  data->width = width;
  data->io_priority = io_priority;
  data->height = height;
  data->preserve_aspect_ratio = preserve_aspect_ratio;
  data->async_result = g_simple_async_result_new(G_OBJECT(file), callback,
                                                 user_data,
                                                 osso_abook_load_pixbuf_async);
  data->pixbuf_loader = gdk_pixbuf_loader_new();
  g_signal_connect(data->pixbuf_loader, "size-prepared",
                   G_CALLBACK(_size_prepared_cb), data);

  if (cancellable)
    data->cancellable = g_object_ref(cancellable);
  else
    data->cancellable = g_cancellable_new();

  g_file_read_async(file, io_priority, cancellable, _ready_cb, data);
}

GdkPixbuf *
osso_abook_load_pixbuf_finish(GFile *file, GAsyncResult *result, GError **error)
{
  GdkPixbuf *pixbuf = NULL;
  gpointer source_tag;

  g_return_val_if_fail(G_IS_FILE(file), NULL);

  source_tag = g_simple_async_result_get_source_tag(
      G_SIMPLE_ASYNC_RESULT(result));

  g_return_val_if_fail(osso_abook_load_pixbuf_async == source_tag, NULL);

  if (!g_simple_async_result_propagate_error(G_SIMPLE_ASYNC_RESULT(result),
                                             error))
  {
    pixbuf = g_simple_async_result_get_op_res_gpointer(
        G_SIMPLE_ASYNC_RESULT(result));
  }

  if (pixbuf)
    pixbuf = g_object_ref(pixbuf);

  return pixbuf;
}

G_GNUC_END_IGNORE_DEPRECATIONS

void
osso_abook_load_pixbuf_async(GFile *file, gsize maximum_size, int io_priority,
                             GCancellable *cancellable,
                             GAsyncReadyCallback callback, gpointer user_data)
{
  osso_abook_load_pixbuf_at_scale_async(file, -1, -1, TRUE, maximum_size,
                                        io_priority, cancellable, callback,
                                        user_data);
}

static void
osso_abook_apply_window_flag(GtkWidget *window, Atom type, const gchar *name,
                             const gchar *flag)
{
  GdkAtom name_atom;
  const gchar *flag_value;

  g_return_if_fail(GTK_IS_WINDOW(window));

  name_atom = gdk_atom_intern_static_string(name);
  flag_value = g_object_get_data(G_OBJECT(window), flag);

  if (flag_value)
  {
    long val = strcmp(flag_value, "0") ? 1 : 0;

    gdk_property_change(GTK_WIDGET(window)->window, name_atom,
                        gdk_x11_xatom_to_atom(type), 32, GDK_PROP_MODE_REPLACE,
                        (const guchar *)&val, 1);
  }
  else
    gdk_property_delete(GTK_WIDGET(window)->window, name_atom);
}

static void
realized_cb(GtkWidget *widget, gpointer user_data)
{
  osso_abook_apply_window_flag(widget, XA_CARDINAL,
                               "_HILDON_PORTRAIT_MODE_SUPPORT",
                               "osso-abook-portrait-mode-supported");
  osso_abook_apply_window_flag(widget, XA_CARDINAL,
                               "_HILDON_PORTRAIT_MODE_REQUEST",
                               "osso-abook-portrait-mode-requested");
  osso_abook_apply_window_flag(widget, 19u,
                               "_HILDON_ZOOM_KEY_ATOM",
                               "osso-abook-zoom-key-used");
}

static void
osso_abook_set_window_flag(GtkWindow *window, const char *flag, gboolean value)
{
  g_return_if_fail(GTK_IS_WINDOW(window));

  g_object_set_data(G_OBJECT(window), flag, value ? "1" : "0");
  g_signal_handlers_disconnect_matched(
    window, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
    realized_cb, NULL);
  g_signal_connect_after(window, "realize",
                         G_CALLBACK(realized_cb), NULL);

  if (gtk_widget_get_realized(GTK_WIDGET(window)))
    realized_cb(NULL, 0);
}

void
osso_abook_set_portrait_mode_supported(GtkWindow *window, gboolean flag)
{
  osso_abook_set_window_flag(window, "osso-abook-portrait-mode-supported",
                             flag);
}

void
osso_abook_set_portrait_mode_requested(GtkWindow *window, gboolean flag)
{
  osso_abook_set_window_flag(window, "osso-abook-portrait-mode-requested",
                             flag);
}

void
osso_abook_set_zoom_key_used(GtkWindow *window, gboolean flag)
{
  osso_abook_set_window_flag(window, "osso-abook-zoom-key-used", flag);
}

static void
screen_size_changed_cb(GdkScreen *screen, gpointer user_data)
{
  gint screen_height = gdk_screen_get_height(screen);
  gint height;

  gtk_widget_get_size_request(user_data, NULL, &height);

  if (screen_height != height)
  {
    gtk_widget_set_size_request(user_data, -1, screen_height);
    gtk_window_resize(GTK_WINDOW(user_data), gdk_screen_get_width(screen),
                      screen_height);
  }
}

static void
destroyed_cb(GtkWidget *widget, gpointer user_data)
{
  g_signal_handlers_disconnect_matched(
    user_data, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
    screen_size_changed_cb, widget);
}

void
osso_abook_attach_screen_size_handler(GtkWindow *window)
{
  GtkWidget *widget;
  GdkScreen *screen;

  g_return_if_fail(GTK_IS_WINDOW(window));

  widget = GTK_WIDGET(window);
  screen = gtk_widget_get_screen(widget);

  if (screen)
  {
    g_signal_connect(screen, "size-changed",
                     G_CALLBACK(screen_size_changed_cb), widget);
    g_signal_connect(widget, "destroy", G_CALLBACK(destroyed_cb), screen);
    screen_size_changed_cb(screen, widget);
  }
}

gboolean
osso_abook_file_set_contents(const char *filename, const void *contents,
                             gssize length, GError **error)
{
  gboolean rv = FALSE;
  gchar *tmp_name = g_strdup_printf("%s.XXXXXX", filename);
  gint fd = g_mkstemp(tmp_name);

#define SET_ERROR(msg) \
  { \
    int _errno = errno; \
    g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(_errno), \
                msg " '%s': %s", tmp_name, g_strerror(_errno)); \
  }

  if (fd != -1)
  {
    if (write(fd, contents, length) == length)
    {
      if (fdatasync(fd) != -1)
      {
        if (close(fd) != -1)
        {
          fd = -1;

          if (rename(tmp_name, filename) != -1)
            rv = TRUE;
          else
            SET_ERROR("Failed to rename");
        }
        else
          SET_ERROR("Failed to close");
      }
      else
        SET_ERROR("Failed to sync");
    }
    else
      SET_ERROR("Failed to write");
  }
  else
    SET_ERROR("Failed to create");

#undef SET_ERROR

  if (fd != -1)
    close(fd);

  if (rv)
    g_unlink(tmp_name);

  g_free(tmp_name);

  return rv;
}

static char *
account_get_display_string(TpAccount *account, const char *username,
                           const char *format, gboolean markup)
{
  const char *display_name;
  char *display_string;

  g_return_val_if_fail(TP_IS_ACCOUNT(account), NULL);

  if (!format)
    format = "%s\n%s";

  display_name = tp_account_get_display_name(account);

  if (IS_EMPTY(display_name))
  {
    TpProtocol *protocol_object =
      osso_abook_account_manager_get_account_protocol_object(NULL, account);

    if (protocol_object)
      display_name = tp_protocol_get_english_name(protocol_object);

    if (IS_EMPTY(display_name))
      display_name = tp_account_get_path_suffix(account);
  }

  if (IS_EMPTY(username))
    username = osso_abook_tp_account_get_bound_name(account);

  if (markup)
  {
    char foreground[40];
    GdkColor color;
    GtkStyle *style = gtk_rc_get_style_by_paths(gtk_settings_get_default(),
                                                NULL, NULL, GTK_TYPE_LABEL);

    if (gtk_style_lookup_color(style, "SecondaryTextColor", &color))
      snprintf(foreground, sizeof(foreground), "#%02x%02x%02x",
               color.red >> 8, color.green >> 8, color.blue >> 8);

    display_string = g_markup_printf_escaped(
        "%s\n<span size=\"x-small\" foreground=\"%s\">%s</span>",
        display_name, foreground, username);
  }
  else
    display_string = g_strdup_printf(format, display_name, username);

  return display_string;
}

char *
osso_abook_tp_account_get_display_string(TpAccount *account,
                                         const char *username,
                                         const char *format)
{
  return account_get_display_string(account, username, format, 0);
}

char *
osso_abook_tp_account_get_display_markup(TpAccount *account)
{
  return account_get_display_string(account, NULL, NULL, TRUE);
}

EVCardAttribute *
osso_abook_convert_to_tel_attribute(EVCardAttribute *attribute)
{
  const char *name;
  GList *values;
  EVCardAttribute *tel_attribute;
  const gchar *value;
  gchar *tel = NULL;
  size_t spn;

  g_return_val_if_fail(attribute != NULL, NULL);

  name = e_vcard_attribute_get_name(attribute);
  values = e_vcard_attribute_get_values(attribute);

  if (!name || !values)
    return NULL;

  value = values->data;

  if (!value || values->next)
    return NULL;

  if (name)
  {
    if (!g_ascii_strcasecmp(name, "X-SKYPE"))
      tel = g_strdup(value);
    else if (!g_ascii_strcasecmp(name, "X-SIP"))
    {
      const char *p = osso_abook_strip_sip_prefix(value);
      const char *q = strchr(p, '@');

      if (q)
        tel = g_strndup(p, q - p);
      else
        tel = g_strdup(p);
    }
  }

  if (!tel || (*tel != '+'))
  {
    g_free(tel);
    return NULL;
  }

  spn = strspn(tel + 1, OSSO_ABOOK_TEL_DIGITS) + 1;

  if ((spn <= 1) || tel[spn + strspn(&tel[spn], OSSO_ABOOK_TEL_CHARS)])
    tel_attribute = NULL;
  else
  {
    tel_attribute = e_vcard_attribute_new(NULL, EVC_TEL);
    e_vcard_attribute_add_value(tel_attribute, tel);
  }

  g_free(tel);

  return tel_attribute;
}

const gchar *
osso_abook_strip_sip_prefix(const gchar *address)
{
  static const char *prefixes[] =
  {
    "sip:",
    "sips:",
    NULL
  };
  const char **prefix;

  for (prefix = prefixes; *prefix; prefix++)
  {
    const char *p = *prefix;
    size_t prefix_len = strlen(p);

    if (!g_ascii_strncasecmp(address, p, prefix_len))
    {
      address += strlen(p);
      break;
    }
  }

  return address;
}

void
osso_abook_enforce_label_width(GtkLabel *label, int width)
{
  gint height;

  gtk_widget_get_size_request(GTK_WIDGET(label), NULL, &height);
  gtk_widget_set_size_request(GTK_WIDGET(label), width, height);
  pango_layout_set_width(gtk_label_get_layout(label), width * PANGO_SCALE);
}

/*
 * Returns TRUE if @photo is changed, FALSE otherwise.
 */
static gboolean
_e_contact_photo_convert_to_uri (EContactPhoto *photo, const char *dir)
{
  GError *err = NULL;
  char *path;
  char *filename;
  char *suffix;
  int fd;

  g_return_val_if_fail(photo, FALSE);
  g_return_val_if_fail(dir, FALSE);

  if (photo->type == E_CONTACT_PHOTO_TYPE_URI)
    return FALSE;

  if (!g_file_test(dir, G_FILE_TEST_IS_DIR))
  {
    if (g_mkdir_with_parents(dir, 0755))
    {
      g_warning("Cannot create directory '%s': %s", dir, g_strerror(errno));
      return FALSE;
    }
  }

  /* guess the suffix */
  if (photo->data.inlined.mime_type &&
      (suffix = strchr(photo->data.inlined.mime_type, '/')))
  {
    filename = g_strdup_printf("XXXXXX.%s", suffix + 1);
  }
  else
    filename = g_strdup("XXXXXX");

  path = g_build_filename(dir, filename, NULL);
  g_free(filename);
  fd = g_mkstemp(path);

  if (fd == -1)
  {
    g_warning("Cannot save file: %s", g_strerror(errno));
    g_free(path);
    return FALSE;
  }

  if (!g_file_set_contents(path, (gchar *)photo->data.inlined.data,
                           photo->data.inlined.length, &err))
  {
    g_warning("Cannot save file '%s': %s", path, err->message);
    g_error_free(err);
    g_free(path);
    close(fd);
    return FALSE;
  }

  /* free photo->data.inlined struct before modifying photo->data */
  g_free(photo->data.inlined.mime_type);
  g_free(photo->data.inlined.data);

  photo->type = E_CONTACT_PHOTO_TYPE_URI;
  photo->data.uri = g_strdup_printf(FILE_SCHEME "%s", path);
  close(fd);
  g_free(path);

  return TRUE;
}

/**
 * e_contact_persist_data:
 * @contact: an #EContact
 * @dir: the directory where the inlined data should be saved.
 *
 * Persist the inlined data from @contact in @dir. Creates @dir if it does not
 * exists.
 *
 * Return value: TRUE if the @contact is changed during the operation, FALSE
 * otherwise.
 */
gboolean
osso_abook_e_contact_persist_data(EContact *contact, const char *dir)
{
  EContactPhoto *photo;
  EContactPhoto *logo;
  gboolean photo_changed = FALSE, logo_changed = FALSE;
  char *photo_dir;

  g_return_val_if_fail(contact && E_IS_CONTACT(contact), FALSE);

  if (!dir)
    photo_dir = g_strdup(osso_abook_get_photos_dir());
  else
    photo_dir = g_strdup(dir);

  photo = e_contact_get(contact, E_CONTACT_PHOTO);

  if (photo)
  {
    photo_changed = _e_contact_photo_convert_to_uri(photo, photo_dir);

    if (photo_changed)
      e_contact_set(contact, E_CONTACT_PHOTO, photo);

    e_contact_photo_free(photo);
  }

  logo = e_contact_get(contact, E_CONTACT_LOGO);

  if (logo)
  {
    logo_changed = _e_contact_photo_convert_to_uri(logo, photo_dir);

    if (logo_changed)
      e_contact_set(contact, E_CONTACT_LOGO, logo);

    e_contact_photo_free(logo);
  }

  g_free(photo_dir);

  return photo_changed || logo_changed;
}

/**
 * e_vcard_util_split_cards:
 * @str: string that will be split
 * @len: out value, the length of the split cards.
 *
 * Splits the input string to separate vcard strings.
 *
 * Return value: a #GList with the newly allocated split cards.
 **/
GList *
osso_abook_e_vcard_util_split_cards(const char *str, gsize *len)
{
  GList *vcards = NULL;
  const char *end, *begin;
  gsize begin_len = strlen("BEGIN:VCARD");
  gsize end_len = strlen("END:VCARD");
  const char *last_pos = str;

  begin = strcasestr(str, "BEGIN:VCARD");

  while (begin != NULL)
  {
    const char *current = begin + begin_len;

    while ((end = strcasestr(current, "END:VCARD")))
    {
      if ((*(end - 1) == '\n') || (*(end - 1) == '\r'))
        break;

      current += end_len;
    }

    if (end == NULL)
    {
      /* BEGIN without END */
      break;
    }

    vcards = g_list_prepend(vcards, g_strndup(begin, (end + end_len) - begin));
    last_pos = end + end_len;

    current = last_pos;

    while ((begin = strcasestr(current, "BEGIN:VCARD")))
    {
      if ((*(begin - 1) == '\n') || (*(begin - 1) == '\r'))
        break;

      current += begin_len;
    }
  }

  if (len)
    *len = last_pos - str;

  return g_list_reverse(vcards);
}

static int
string_list_compare (GList *a, GList *b)
{
  int cmp;

  while (a && b)
  {
    cmp = g_strcmp0(a->data, b->data);

    if (0 != cmp)
      return cmp;

    a = a->next;
    b = b->next;
  }

  return NULL != a ? +1 : NULL != b ? -1 : 0;
}

/**
 * e_vcard_attribute_equal:
 * @attr_a: first #EVCardAttribute
 * @attr_b: second #EVCardAttribute
 *
 * Checks if @attr_a and @attr_b are equal by value. This function considers
 * attibute names, values and parameters. It is save to pass %NULL to its
 * arguments.
 *
 * Return value: %TRUE when both attributes are equal, and %FALSE otherwise.
 **/
gboolean
osso_abook_e_vcard_attribute_equal(EVCardAttribute *attr_a,
                                   EVCardAttribute *attr_b)
{
  EVCardAttributeParam *param_a, *param_b;
  GList *param_list_a, *param_list_b;
  const gchar *s1;
  const gchar *s2;


  if (!attr_a)
    return !attr_b;

  if (!attr_b)
    return FALSE;

  s1 = e_vcard_attribute_get_name(attr_a);
  s2 = e_vcard_attribute_get_name(attr_b);

  if (s1 && s2 && g_ascii_strcasecmp(s1, s2))
    return FALSE;

  if ((s1 && !s2) || (!s1 && s2))
    return FALSE;

  if (string_list_compare(e_vcard_attribute_get_values(attr_a),
                          e_vcard_attribute_get_values(attr_b)))
  {
    return FALSE;
  }

  param_list_a = e_vcard_attribute_get_params(attr_a);
  param_list_b = e_vcard_attribute_get_params(attr_b);

  while (param_list_a && param_list_b)
  {
    param_a = param_list_a->data;
    param_b = param_list_b->data;
    s1 = e_vcard_attribute_param_get_name(param_a);
    s2 = e_vcard_attribute_param_get_name(param_b);

    if (s1 && s2 && g_ascii_strcasecmp(s1, s2))
      return FALSE;

    if ((s1 && !s2) || (!s1 && s2))
      return FALSE;

    if (string_list_compare(e_vcard_attribute_param_get_values(param_a),
                            e_vcard_attribute_param_get_values(param_b)))
    {
      return FALSE;
    }

    param_list_a = param_list_a->next;
    param_list_b = param_list_b->next;
  }

  return !param_list_a && !param_list_b;
}
