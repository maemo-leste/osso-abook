/*
 * osso-abook-avatar-image.c
 *
 * Copyright (C) 2021 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
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
#include <gtk/gtkprivate.h>
#include <hildon/hildon.h>
#include <cairo-xlib.h>

#include <math.h>

#include "config.h"

#include "osso-abook-avatar-image.h"
#include "osso-abook-avatar.h"
#include "osso-abook-debug.h"
#include "osso-abook-icon-sizes.h"

struct _OssoABookAvatarImagePrivate
{
  gchar *fallback_icon;
  gdouble minimum_zoom;
  gdouble maximum_zoom;
  OssoABookAvatar *avatar;
  int size;
  GdkPixbuf *scaled_pixbuf;
  GdkPixbuf *pixbuf;
  GdkPixbuf *default_avatar_pixbuf;
  cairo_surface_t *surface;
  gdouble background_opacity;
  int border_width;
  int border_radius;
  gdouble shadow_opacity;
  gdouble red;
  gdouble green;
  gdouble blue;
  int shadow_size;
  GtkAdjustment *xadjustment;
  GtkAdjustment *yadjustment;
  GtkAdjustment *zadjustment;
  guint redraw_timeout_id;
  gboolean pending_redraw : 1;
  gboolean redraw_now : 1;
};

typedef struct _OssoABookAvatarImagePrivate OssoABookAvatarImagePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookAvatarImage,
  osso_abook_avatar_image,
  GTK_TYPE_WIDGET
);

#define OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image) \
  ((OssoABookAvatarImagePrivate *)osso_abook_avatar_image_get_instance_private(((OssoABookAvatarImage *)image)))

enum
{
  PROP_AVATAR = 1,
  PROP_PIXBUF,
  PROP_SIZE,
  PROP_FALLBACK_ICON,
  PROP_MINIMUM_ZOOM,
  PROP_CURRENT_ZOOM,
  PROP_MAXIMUM_ZOOM,
  PROP_XADJUSTMENT,
  PROP_YADJUSTMENT,
  PROP_ZADJUSTMENT
};

static void
osso_abook_avatar_image_init(OssoABookAvatarImage *image)
{
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);

  priv->maximum_zoom = 2.0f;
  priv->size = OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT;
  priv->minimum_zoom = 1.0f;
  gtk_widget_set_events(GTK_WIDGET(image),
                        GDK_EXPOSURE_MASK | GDK_POINTER_MOTION_HINT_MASK |
                        GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK);
  gtk_widget_set_has_window(GTK_WIDGET(image), FALSE);

  osso_abook_avatar_image_set_zadjustment(
        image,
        GTK_ADJUSTMENT(gtk_adjustment_new(0.0, 0.0, 1.0, 0.0, 0.0, 0.0)));
  hildon_helper_set_logical_color(GTK_WIDGET(image), GTK_RC_BG,
                                  GTK_STATE_NORMAL, "AvatarBgColor");
  hildon_helper_set_logical_color(GTK_WIDGET(image), GTK_RC_FG,
                                  GTK_STATE_NORMAL, "AvatarStrokeColor");
  priv->pending_redraw = FALSE;
}

static gboolean
redraw_widget(gpointer user_data)
{
  GtkWidget *image = user_data;
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);

  if (gtk_widget_get_mapped(image))
  {
    gtk_widget_queue_draw(image);

    if (priv->pending_redraw)
      return TRUE;
  }

  priv->redraw_timeout_id = 0;
  priv->pending_redraw = FALSE;

  return FALSE;
}

static void
redraw(OssoABookAvatarImage *image)
{
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);

  priv->pending_redraw = FALSE;
  priv->redraw_now = TRUE;

  if (priv->redraw_timeout_id)
  {
    g_source_remove(priv->redraw_timeout_id);
    priv->redraw_timeout_id = 0;
  }

  redraw_widget(GTK_WIDGET(image));
}

static GdkPixbuf *
get_fallback_pixbuf(OssoABookAvatarImage *image)
{
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);
  GError *error = NULL;
  GdkPixbuf *pixbuf = priv->default_avatar_pixbuf;

  if (!pixbuf)
  {
    GtkIconTheme *icon_theme =
        gtk_icon_theme_get_for_screen(gtk_widget_get_screen(GTK_WIDGET(image)));
    const char *icon = priv->fallback_icon;

    if (!icon && priv->avatar)
      icon = osso_abook_avatar_get_fallback_icon_name(priv->avatar);

    if (!icon)
      icon = "general_default_avatar";

    pixbuf = gtk_icon_theme_load_icon(icon_theme, icon, priv->size, 0, &error);
    priv->default_avatar_pixbuf = pixbuf;

    if (!pixbuf)
    {
      g_warning("%s: Cannot load fallback icon \"%s\": %s", __FUNCTION__, icon,
                error->message);
      g_clear_error(&error);
    }
  }

  return pixbuf;
}
static GdkPixbuf *
get_pixbuf(OssoABookAvatarImage *image)
{
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);
  GdkPixbuf *pixbuf = priv->pixbuf;

  if (pixbuf ||
      (priv->avatar && (pixbuf = osso_abook_avatar_get_image(priv->avatar))))
  {
    return pixbuf;
  }

  return get_fallback_pixbuf(image);
}

static void
calculate_zoom_and_redraw(OssoABookAvatarImage *image)
{
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);
  double old_minimum_zoom = priv->minimum_zoom;
  GdkPixbuf *pixbuf = get_pixbuf(image);
  gdouble diff;

  if (pixbuf)
  {
    gint w = gdk_pixbuf_get_width(pixbuf);
    gint size = gdk_pixbuf_get_height(pixbuf);

    if (size >= w)
      size = w;

    priv->minimum_zoom = (double)priv->size / (double)size;
  }

  g_object_notify(G_OBJECT(image), "current-zoom");

  diff = fabs(old_minimum_zoom - priv->minimum_zoom);

  if (diff > 1.0e-10)
    g_object_notify(G_OBJECT(image), "minimum-zoom");

  redraw(image);
}

static void
notify_image_cb(OssoABookAvatar *avatar, GParamSpec *arg1,
                OssoABookAvatarImage *image)
{
  OSSO_ABOOK_NOTE(AVATAR, "avatar notified a new image, done loading: %s",
                  osso_abook_avatar_is_done_loading(avatar) ? "yes" : "no");
  calculate_zoom_and_redraw(image);
}

static void
destroy_avatar(OssoABookAvatarImage *image)
{
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);

  if (priv->avatar)
  {
    g_signal_handlers_disconnect_matched(
          priv->avatar, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
          notify_image_cb, image);
    g_object_unref(priv->avatar);
    priv->avatar = NULL;
  }
}

static void
destroy_pixbuf(OssoABookAvatarImagePrivate *priv)
{
  if (priv->pixbuf)
  {
    g_object_unref(priv->pixbuf);
    priv->pixbuf = NULL;
  }
}

static void
destroy_default_avatar_pixbuf(OssoABookAvatarImagePrivate *priv)
{
  if (priv->default_avatar_pixbuf)
  {
    g_object_unref(priv->default_avatar_pixbuf);
    priv->default_avatar_pixbuf = NULL;
  }
}

static void
destroy_scaled_pixbuf(OssoABookAvatarImagePrivate *priv)
{
  if (priv->scaled_pixbuf)
  {
    g_object_unref(priv->scaled_pixbuf);
    priv->scaled_pixbuf = NULL;
  }
}

static void
destroy_cairo_surface(OssoABookAvatarImagePrivate *priv)
{
  if (priv->surface)
  {
    cairo_surface_destroy(priv->surface);
    priv->surface = NULL;
  }

  priv->redraw_now = TRUE;
}

static void
replace_adjustment(OssoABookAvatarImage *image, GtkAdjustment **adjustment,
                   GtkAdjustment *new_adjustment, gpointer value_change_func)
{
  if (*adjustment == new_adjustment)
    return;

  if (*adjustment)
  {
    g_signal_handlers_disconnect_matched(
          *adjustment, G_SIGNAL_MATCH_DATA|G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
          value_change_func, image);
    g_object_unref(*adjustment);
    *adjustment = NULL;
  }

  if (new_adjustment)
  {
    *adjustment = g_object_ref(new_adjustment);
    g_signal_connect_swapped(new_adjustment, "value-changed",
                             G_CALLBACK(value_change_func), image);
  }
}

static void
adjustment_value_changed_cb(OssoABookAvatarImage *image)
{
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);

  priv->pending_redraw = TRUE;
  priv->redraw_now = TRUE;

  if (priv->redraw_timeout_id)
    return;

  priv->redraw_timeout_id = gdk_threads_add_timeout(120, redraw_widget, image);
}

static void
zadjustment_value_changed_cb(OssoABookAvatarImage *image)
{
  g_object_notify(G_OBJECT(image), "current-zoom");
  adjustment_value_changed_cb(image);
}

static gboolean
has_adjustments(OssoABookAvatarImagePrivate *priv)
{
  return priv->xadjustment && priv->yadjustment;
}

static gdouble
get_current_zoom(OssoABookAvatarImagePrivate *priv)
{
  GtkAdjustment *zadjustment = priv->zadjustment;
  gdouble zoom = priv->minimum_zoom;

  if (zadjustment)
  {
    gdouble diff = priv->maximum_zoom - zoom;

    if (diff < 0.0f)
      diff = 0.f;

    zoom = zoom + diff * (zadjustment->value * zadjustment->value);
  }

  return zoom;
}

static void
scale_pixbuf(const GdkPixbuf *src, GdkPixbuf *dst, gint size, double zoom,
             GtkAdjustment *xadjustment, GtkAdjustment *yadjustment,
             GdkInterpType interp_type)
{
  gint dst_w = gdk_pixbuf_get_width(dst);
  gint dst_h = gdk_pixbuf_get_height(dst);
  gint src_w = ceil(zoom * gdk_pixbuf_get_width(src));
  gint src_h = ceil(zoom * gdk_pixbuf_get_height(src));
  gdouble xadj = 0.0f;
  gdouble yadj = 0.0f;
  gint dst_x = 0;
  gint dst_y = 0;
  gint offset_y;
  gint offset_x;

  if (xadjustment)
    xadj = xadjustment->value;

  if (yadjustment)
    yadj = yadjustment->value;

  offset_x =  0.5f * (xadj * (src_w - size) + (dst_w - src_w) + 1.0f);
  offset_y =  0.5f * (yadj * (src_h - size) + (dst_h - src_h) + 1.0f);

  if (offset_x < 0)
    src_w += offset_x;
  else
    dst_x = offset_x;

  if (offset_y < 0)
    src_h += offset_y;
  else
    dst_y = offset_y;

  gdk_pixbuf_fill(dst, 0);

  dst_h = dst_h - dst_y;

  if (dst_h > src_h)
    dst_h = src_h;

  dst_w = dst_w - dst_x;

  if (dst_w > src_w)
    dst_w = src_w;

  gdk_pixbuf_scale(src, dst, dst_x, dst_y, dst_w, dst_h, offset_x, offset_y,
                   zoom, zoom, interp_type);
}

static void
draw_shadow(cairo_t *cr, double x, double y, double w, double h, double radius)
{
  gdouble r = x + w;
  gdouble b = y + h;
  gdouble y1 = y + h - radius;
  gdouble x1 = x + radius;
  gdouble x2 = x + w - radius;

  cairo_move_to(cr, x + radius, y);
  cairo_line_to(cr, x2, y);
  cairo_curve_to(cr, r, y, r, y, r, y + radius);
  cairo_line_to(cr, r, y1);
  cairo_curve_to(cr, r, b, r, b, x2, b);
  cairo_line_to(cr, x1, y + h);
  cairo_curve_to(cr, x, b, x, b, x, y1);
  cairo_line_to(cr, x, y + radius);
  cairo_curve_to(cr, x, y, x, y, x1, y);
}

static void
osso_abook_avatar_image_set_property(GObject *object, guint property_id,
                                     const GValue *value, GParamSpec *pspec)
{
  OssoABookAvatarImage *image = OSSO_ABOOK_AVATAR_IMAGE(object);
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);

  switch (property_id)
  {
    case PROP_AVATAR:
    {
      OssoABookAvatar * avatar = g_value_get_object(value);

      if (!avatar || avatar != priv->avatar)
      {
        destroy_avatar(image);
        destroy_pixbuf(priv);
        destroy_default_avatar_pixbuf(priv);

        if (avatar && avatar != priv->avatar)
        {
          priv->avatar  = g_object_ref(avatar);
          g_signal_connect(priv->avatar, "notify::avatar-image",
                           G_CALLBACK(notify_image_cb), image);
        }

        calculate_zoom_and_redraw(image);
      }

      break;
    }
    case PROP_PIXBUF:
    {
      GdkPixbuf *pixbuf = g_value_get_object(value);

      if (!pixbuf || pixbuf != priv->pixbuf)
      {
        destroy_avatar(image);
        destroy_pixbuf(priv);
        destroy_default_avatar_pixbuf(priv);

        if (pixbuf && pixbuf != priv->pixbuf)
          priv->pixbuf = gdk_pixbuf_apply_embedded_orientation(pixbuf);

        calculate_zoom_and_redraw(image);
      }

      break;
    }
    case PROP_SIZE:
    {
      gint size = g_value_get_int(value);

      if (size != priv->size)
      {
        priv->size = size;
        destroy_pixbuf(priv);
        destroy_default_avatar_pixbuf(priv);
        calculate_zoom_and_redraw(image);
        gtk_widget_queue_resize(GTK_WIDGET(image));
      }

      break;
    }
    case PROP_FALLBACK_ICON:
    {
      const gchar *icon = g_value_get_string(value);

      if (g_strcmp0(priv->fallback_icon, icon))
      {
        g_free(priv->fallback_icon);
        priv->fallback_icon = g_strdup(icon);
        destroy_default_avatar_pixbuf(priv);
        redraw(image);
      }

      break;
    }
    case PROP_MAXIMUM_ZOOM:
    {
      priv->maximum_zoom = g_value_get_double(value);
      calculate_zoom_and_redraw(image);
      break;
    }
    case PROP_XADJUSTMENT:
    {
      replace_adjustment(image, &priv->xadjustment, g_value_get_object(value),
                         adjustment_value_changed_cb);
      redraw(image);
      break;
    }
    case PROP_YADJUSTMENT:
    {
      replace_adjustment(image, &priv->yadjustment, g_value_get_object(value),
                         adjustment_value_changed_cb);
      redraw(image);
      break;
    }
    case PROP_ZADJUSTMENT:
    {
      replace_adjustment(image, &priv->zadjustment, g_value_get_object(value),
                         zadjustment_value_changed_cb);
      zadjustment_value_changed_cb(image);
      redraw(image);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_avatar_image_get_property(GObject *object, guint property_id,
                                     GValue *value, GParamSpec *pspec)
{
  OssoABookAvatarImage *image = OSSO_ABOOK_AVATAR_IMAGE(object);

  switch (property_id)
  {
    case PROP_AVATAR:
    {
      g_value_set_object(value, osso_abook_avatar_image_get_avatar(image));
      break;
    }
    case PROP_PIXBUF:
    {
      g_value_set_object(value, osso_abook_avatar_image_get_pixbuf(image));
      break;
    }
    case PROP_SIZE:
    {
      g_value_set_int(value, osso_abook_avatar_image_get_size(image));
      break;
    }
    case PROP_FALLBACK_ICON:
    {
      g_value_set_string(value,
                         osso_abook_avatar_image_get_fallback_icon(image));
      break;
    }
    case PROP_MINIMUM_ZOOM:
    {
      g_value_set_double(value,
                         osso_abook_avatar_image_get_minimum_zoom(image));
      break;
    }
    case PROP_CURRENT_ZOOM:
    {
      g_value_set_double(value,
                         osso_abook_avatar_image_get_current_zoom(image));
      break;
    }
    case PROP_MAXIMUM_ZOOM:
    {
      g_value_set_double(value,
                         osso_abook_avatar_image_get_maximum_zoom(image));
      break;
    }
    case PROP_XADJUSTMENT:
    {
      g_value_set_object(value, osso_abook_avatar_image_get_xadjustment(image));
      break;
    }
    case PROP_YADJUSTMENT:
    {
      g_value_set_object(value, osso_abook_avatar_image_get_yadjustment(image));
      break;
    }
    case PROP_ZADJUSTMENT:
    {
      g_value_set_object(value, osso_abook_avatar_image_get_zadjustment(image));
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_avatar_image_dispose(GObject *object)
{
  OssoABookAvatarImage *image = OSSO_ABOOK_AVATAR_IMAGE(object);
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);

  if (priv->redraw_timeout_id)
  {
    g_source_remove(priv->redraw_timeout_id);
    priv->redraw_timeout_id = 0;
  }

  replace_adjustment(image, &priv->xadjustment, NULL, NULL);
  replace_adjustment(image, &priv->yadjustment, NULL, NULL);
  replace_adjustment(image, &priv->zadjustment, NULL, NULL);
  destroy_avatar(image);
  destroy_pixbuf(priv);
  destroy_scaled_pixbuf(priv);
  destroy_cairo_surface(priv);
  destroy_default_avatar_pixbuf(priv);

  G_OBJECT_CLASS(osso_abook_avatar_image_parent_class)->dispose(object);
}

static gboolean
osso_abook_avatar_image_expose_event(GtkWidget *widget, GdkEventExpose *event)
{
  OssoABookAvatarImage *image;
  OssoABookAvatarImagePrivate *priv;
  GtkStyle *style;
  cairo_t *window_cr;
  gint h;
  gint w;

  OSSO_ABOOK_LOCAL_TIMER_START(GTK, __FUNCTION__);

  image = OSSO_ABOOK_AVATAR_IMAGE(widget);
  priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);
  style = widget->style;

  if (priv->scaled_pixbuf)
  {
    w = gdk_pixbuf_get_width(priv->scaled_pixbuf);
    h = gdk_pixbuf_get_height(priv->scaled_pixbuf);
  }
  else
  {
    w = 0;
    h = 0;
  }

  if (widget->allocation.width != w || widget->allocation.height != h)
  {
    destroy_scaled_pixbuf(priv);
    priv->scaled_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 1, 8,
                                         widget->allocation.width,
                                         widget->allocation.height);
  }

  if (priv->pending_redraw)
  {
    scale_pixbuf(get_pixbuf(image), priv->scaled_pixbuf, priv->size,
                 get_current_zoom(priv), priv->xadjustment, priv->yadjustment,
                 GDK_INTERP_NEAREST);
  }
  else
  {
    scale_pixbuf(get_pixbuf(image), priv->scaled_pixbuf, priv->size,
                 get_current_zoom(priv), priv->xadjustment, priv->yadjustment,
                 GDK_INTERP_HYPER);
  }

  priv->pending_redraw = FALSE;

  if (has_adjustments(priv))
  {
    GdkGC *gc = gdk_gc_new(event->window);
    gdk_gc_set_clip_region(gc, event->region);
    gdk_gc_set_foreground(gc, &style->bg[gtk_widget_get_state(widget)]);
    gdk_draw_rectangle(event->window, gc, TRUE,
                       widget->allocation.x, widget->allocation.y,
                       widget->allocation.width, widget->allocation.height);
    gdk_draw_pixbuf(event->window, gc, priv->scaled_pixbuf,
                    0, 0, widget->allocation.x, widget->allocation.y,
                    widget->allocation.width, widget->allocation.height,
                    GDK_RGB_DITHER_NORMAL, 0, 0);
    g_object_unref(gc);
  }

  window_cr = gdk_cairo_create(event->window);
  gdk_cairo_region(window_cr, event->region);
  cairo_clip(window_cr);

  if (priv->surface)
  {
    w = cairo_xlib_surface_get_width(priv->surface);
    h = cairo_xlib_surface_get_height(priv->surface);
  }
  else
  {
    w = 0;
    h = 0;
  }

  if (widget->allocation.width != w || widget->allocation.height != h)
  {
    destroy_cairo_surface(priv);
    priv->surface = cairo_surface_create_similar(cairo_get_target(window_cr),
                                                 CAIRO_CONTENT_COLOR_ALPHA,
                                                 widget->allocation.width,
                                                 widget->allocation.height);
  }

  if (priv->redraw_now)
  {
    cairo_t *surface_cr = cairo_create(priv->surface);

    if (has_adjustments(priv))
    {
      GtkStateType state = gtk_widget_get_state(widget);

      cairo_set_source_rgba(surface_cr,
                            (gdouble)style->bg[state].red / 65535.0,
                            (gdouble)style->bg[state].green / 65535.0,
                            (gdouble)style->bg[state].blue / 65535.0,
                            priv->background_opacity);
    }
    else
      cairo_set_source_rgba(surface_cr, 0.0, 0.0, 0.0, 0.0);

    cairo_set_operator(surface_cr, CAIRO_OPERATOR_SOURCE);
    cairo_paint(surface_cr);
    cairo_set_operator(surface_cr, CAIRO_OPERATOR_OVER);
    cairo_translate(surface_cr,
                    rint(0.5f * (widget->allocation.width - priv->size)),
                    rint(0.5f * (widget->allocation.height - priv->size)));

    if (priv->shadow_size > 0)
    {
      int i;

      cairo_set_source_rgba(surface_cr, priv->red, priv->green, priv->blue,
                            priv->shadow_opacity / (gdouble)priv->shadow_size);

      for (i = priv->shadow_size + 1; i > 1; i--)
      {
        gint shadow_size = priv->shadow_size;
        gint size = 2 * i + priv->size - shadow_size;
        draw_shadow(surface_cr, shadow_size - i, shadow_size - i, size, size,
                    2 * (priv->border_radius + i));
        cairo_fill(surface_cr);
      }
    }

    draw_shadow(surface_cr, -0.5, -0.5, priv->size + 1, priv->size + 1,
                2 * priv->border_radius);

    if (has_adjustments(priv))
    {
      cairo_set_operator(surface_cr, CAIRO_OPERATOR_SOURCE);
      cairo_set_source_rgba(surface_cr, 0.0, 0.0, 0.0, 0.0);
      cairo_fill_preserve(surface_cr);
      cairo_set_operator(surface_cr, CAIRO_OPERATOR_OVER);
    }
    else
    {
      GtkStateType state = gtk_widget_get_state(widget);

      gdk_cairo_set_source_color(surface_cr,
                                 &style->bg[state]);
      cairo_fill_preserve(surface_cr);
      gdk_cairo_set_source_pixbuf(
            surface_cr, priv->scaled_pixbuf,
            floor(0.5f * (priv->size -
                          gdk_pixbuf_get_width(priv->scaled_pixbuf))),
            floor(0.5f * (priv->size -
                          gdk_pixbuf_get_height(priv->scaled_pixbuf))));
      cairo_fill_preserve(surface_cr);
      cairo_fill_preserve(surface_cr);

      if (state == GTK_STATE_INSENSITIVE)
      {
        cairo_set_source_rgba(surface_cr,
                              (gdouble)style->bg[state].red / 65535.0,
                              (gdouble)style->bg[state].green / 65535.0,
                              (gdouble)style->bg[state].blue / 65535.0,
                              priv->background_opacity);
        cairo_fill_preserve(surface_cr);
      }
    }

    if (priv->border_width > 0)
    {
      cairo_set_line_width(surface_cr, priv->border_width);
      gdk_cairo_set_source_color(surface_cr,
                                 &style->fg[GTK_WIDGET(widget)->state]);
      cairo_stroke(surface_cr);
    }

    cairo_destroy(surface_cr);
    priv->redraw_now = FALSE;
  }

  cairo_set_source_surface(window_cr, priv->surface,
                           widget->allocation.x, widget->allocation.y);
  cairo_paint(window_cr);
  cairo_destroy(window_cr);

  OSSO_ABOOK_LOCAL_TIMER_END();

  return TRUE;
}

static void
osso_abook_avatar_image_size_request(GtkWidget *widget,
                                     GtkRequisition *requisition)
{
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(widget);
  gint size  = 2 * (priv->border_width + priv->shadow_size) + priv->size;

  requisition->width = size;
  requisition->height = size;
}

static void
osso_abook_avatar_image_state_changed(GtkWidget *widget,
                                      GtkStateType previous_state)
{
  redraw(OSSO_ABOOK_AVATAR_IMAGE(widget));
}

static void
osso_abook_avatar_image_style_set(GtkWidget *widget, GtkStyle *previous_style)
{
  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(widget);
  GdkColor *color = NULL;

  GTK_WIDGET_CLASS(osso_abook_avatar_image_parent_class)->
      style_set(widget, previous_style);

  gtk_widget_style_get(widget,
                       "background-opacity", &priv->background_opacity,
                       "border-width", &priv->border_width,
                       "border-radius", &priv->border_radius,
                       "shadow-opacity", &priv->shadow_opacity,
                       "shadow-size", &priv->shadow_size,
                       "shadow-color", &color,
                       NULL);
  if (color)
  {
    priv->red = (double)color->red / 65535.0f;
    priv->blue = (double)color->blue / 65535.0f;
    priv->green = (double)color->green / 65535.0f;
    gdk_color_free(color);
  }
  else
  {
    priv->red = 0.0f;
    priv->blue = 0.0f;
    priv->green = 0.0f;
  }

  redraw(OSSO_ABOOK_AVATAR_IMAGE(widget));
  gtk_widget_queue_resize(widget);
}

static void
osso_abook_avatar_image_class_init(OssoABookAvatarImageClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_avatar_image_set_property;
  object_class->get_property = osso_abook_avatar_image_get_property;
  object_class->dispose = osso_abook_avatar_image_dispose;

  widget_class->expose_event = osso_abook_avatar_image_expose_event;
  widget_class->size_request = osso_abook_avatar_image_size_request;
  widget_class->state_changed = osso_abook_avatar_image_state_changed;
  widget_class->style_set = osso_abook_avatar_image_style_set;

  g_object_class_install_property(
        object_class, PROP_AVATAR,
        g_param_spec_object(
          "avatar", "Avatar", "Avatar object",
          OSSO_ABOOK_TYPE_AVATAR,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_PIXBUF,
        g_param_spec_object(
          "pixbuf", "Pixbuf", "Pixbuf object of the avatar",
          GDK_TYPE_PIXBUF,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_SIZE,
        g_param_spec_int(
          "size", "Size", "Size of the avatar in pixels",
          16, 800, 144,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_MINIMUM_ZOOM,
        g_param_spec_double(
          "minimum-zoom", "Minimum Zoom", "The minimal allowed zoom level",
          0.0f, G_MAXDOUBLE, 1.0f,
          GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_CURRENT_ZOOM,
        g_param_spec_double(
          "current-zoom", "Current Zoom", "The current zoom level",
          0.0, G_MAXDOUBLE, 1.0,
          GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_MAXIMUM_ZOOM,
        g_param_spec_double(
          "maximum-zoom", "Maximum Zoom", "The maximal allowed zoom level",
          0.0, G_MAXDOUBLE, 2.0,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_FALLBACK_ICON,
        g_param_spec_string(
          "fallback-icon", "Fallback Icon", "The name of the fallback icon",
          "general_default_avatar",
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_XADJUSTMENT,
        g_param_spec_object(
          "xadjustment", "X-Adjustment",
          "The GtkAdjustment for the horizontal avatar position",
          GTK_TYPE_ADJUSTMENT,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_YADJUSTMENT,
        g_param_spec_object(
          "yadjustment", "Y-Adjustment",
          "The GtkAdjustment for the vertical avatar position",
          GTK_TYPE_ADJUSTMENT,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_ZADJUSTMENT,
        g_param_spec_object(
          "zadjustment", "Z-Adjustment",
          "The GtkAdjustment for the avatar zoom",
          GTK_TYPE_ADJUSTMENT,
          GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property(
        widget_class,
        g_param_spec_double(
          "background-opacity", "Background Opacity",
          "Opacity of the background shading",
          0.0f, 1.0f, 0.7f,
          GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property(
        widget_class,
        g_param_spec_int(
          "border-width", "Border Width",
          "Width of the avatar border in pixels",
          0, G_MAXINT, 0,
          GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property(
        widget_class,
        g_param_spec_int(
          "border-radius", "Border Radius",
          "Corner radius of the avatar border in pixels",
          0,
          G_MAXINT,
          10,
          GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property(
        widget_class,
        g_param_spec_double(
          "shadow-opacity", "Shadow Opacity", "Opacity of the avatar shadow",
          0.0f, 1.0f, 0.7f,
          GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property(
        widget_class,
        g_param_spec_int(
          "shadow-size", "Shadow Size", "Size of the avatar shadow in pixels",
          0, G_MAXINT, 0,
          GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property(
        widget_class,
        g_param_spec_boxed(
          "shadow-color", "Shadow Color", "Color of the avatar shadow",
          GDK_TYPE_COLOR,
          GTK_PARAM_READWRITE));
}

GtkWidget *
osso_abook_avatar_image_new(void)
{
  return g_object_new(OSSO_ABOOK_TYPE_AVATAR_IMAGE, NULL);
}

GtkWidget *
osso_abook_avatar_image_new_with_avatar(OssoABookAvatar *avatar, int size)
{
  g_return_val_if_fail(!avatar || OSSO_ABOOK_IS_AVATAR(avatar), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_AVATAR_IMAGE,
                      "avatar", avatar,
                      "size", size,
                      NULL);
}

void
osso_abook_avatar_image_set_avatar(OssoABookAvatarImage *image,
                                   OssoABookAvatar *avatar)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image));
  g_return_if_fail(!avatar || OSSO_ABOOK_IS_AVATAR(avatar));

  g_object_set(image, "avatar", avatar, NULL);
}

OssoABookAvatar *
osso_abook_avatar_image_get_avatar(OssoABookAvatarImage *image)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image), NULL);

  return OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image)->avatar;
}

void
osso_abook_avatar_image_set_pixbuf(OssoABookAvatarImage *image,
                                   GdkPixbuf *pixbuf)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image));
  g_return_if_fail(!pixbuf || GDK_IS_PIXBUF(pixbuf));

  g_object_set(image, "pixbuf", pixbuf, NULL);
}

GdkPixbuf *
osso_abook_avatar_image_get_pixbuf(OssoABookAvatarImage *image)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image), NULL);

  OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);

  if (priv->pixbuf)
    return priv->pixbuf;

  if (priv->avatar)
    return osso_abook_avatar_get_image(priv->avatar);

  return NULL;
}

GdkPixbuf *
osso_abook_avatar_image_get_scaled_pixbuf(OssoABookAvatarImage *image)
{
  GdkPixbuf *scaled_pixbuf = NULL;
  GdkPixbuf *pixbuf;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image), NULL);

  pixbuf = osso_abook_avatar_image_get_pixbuf(image);

  if (pixbuf)
  {
    OssoABookAvatarImagePrivate *priv = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image);

    scaled_pixbuf = gdk_pixbuf_new(GDK_COLORSPACE_RGB, 1, 8, priv->size,
                                   priv->size);
    scale_pixbuf(pixbuf, scaled_pixbuf, priv->size, get_current_zoom(priv),
                 priv->xadjustment, priv->yadjustment, GDK_INTERP_HYPER);
  }

  return scaled_pixbuf;
}

void
osso_abook_avatar_image_set_fallback_icon(OssoABookAvatarImage *image,
                                          const char *icon_name)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image));

  g_object_set(image, "fallback-icon", icon_name, NULL);
}

const char *
osso_abook_avatar_image_get_fallback_icon(OssoABookAvatarImage *image)
{
  const char *icon;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image),
                       "general_default_avatar");

  icon = OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image)->fallback_icon;

  if (!icon)
    icon = "general_default_avatar";

  return icon;
}

void
osso_abook_avatar_image_set_size(OssoABookAvatarImage *image, int size)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image));

  g_object_set(image, "size", size, NULL);
}

int
osso_abook_avatar_image_get_size(OssoABookAvatarImage *image)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image),
                       OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT);

  return OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image)->size;
}

double
osso_abook_avatar_image_get_minimum_zoom(OssoABookAvatarImage *image)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image),
                       OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT);

  return OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image)->minimum_zoom;
}

double
osso_abook_avatar_image_get_current_zoom(OssoABookAvatarImage *image)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image),
                       OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT);

  return get_current_zoom(OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image));
}

void
osso_abook_avatar_image_set_maximum_zoom(OssoABookAvatarImage *image,
                                         double limit)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image));

  g_object_set(image, "maximum-zoom", limit, NULL);
}

double
osso_abook_avatar_image_get_maximum_zoom(OssoABookAvatarImage *image)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image),
                       OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT);

  return OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image)->maximum_zoom;
}

void
osso_abook_avatar_image_set_xadjustment(OssoABookAvatarImage *image,
                                        GtkAdjustment *adjustment)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image));
  g_return_if_fail(!adjustment || GTK_IS_ADJUSTMENT(adjustment));

  g_object_set(image, "xadjustment", adjustment, NULL);
}

GtkAdjustment *
osso_abook_avatar_image_get_xadjustment(OssoABookAvatarImage *image)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image), NULL);

  return OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image)->xadjustment;
}

void
osso_abook_avatar_image_set_yadjustment(OssoABookAvatarImage *image,
                                        GtkAdjustment *adjustment)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image));
  g_return_if_fail(!adjustment || GTK_IS_ADJUSTMENT(adjustment));

  g_object_set(image, "yadjustment", adjustment, NULL);
}

GtkAdjustment *
osso_abook_avatar_image_get_yadjustment(OssoABookAvatarImage *image)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image), NULL);

  return OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image)->yadjustment;
}

void
osso_abook_avatar_image_set_zadjustment(OssoABookAvatarImage *image,
                                        GtkAdjustment *adjustment)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image));
  g_return_if_fail(!adjustment || GTK_IS_ADJUSTMENT(adjustment));

  g_object_set(image, "zadjustment", adjustment, NULL);
}

GtkAdjustment *
osso_abook_avatar_image_get_zadjustment(OssoABookAvatarImage *image)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_IMAGE(image), NULL);

  return OSSO_ABOOK_AVATAR_IMAGE_PRIVATE(image)->zadjustment;
}

