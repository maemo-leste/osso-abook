/*
 * osso-abook-presence-label.c
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

#include "config.h"

#include <gtk/gtkprivate.h>
#include <hildon/hildon.h>

#include <libintl.h>

#include "osso-abook-presence-label.h"
#include "osso-abook-presence.h"

struct _OssoABookPresenceLabelPrivate
{
  OssoABookPresence *presence;
  GdkPixbuf *icon;
};

typedef struct _OssoABookPresenceLabelPrivate OssoABookPresenceLabelPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookPresenceLabel,
  osso_abook_presence_label,
  GTK_TYPE_LABEL
);

enum
{
  PRESENCE = 1,
  LABEL
};

#define OSSO_ABOOK_PRESENCE_LABEL_PRIVATE(label) \
  ((OssoABookPresenceLabelPrivate *) \
   osso_abook_presence_label_get_instance_private(label))

static const char *obj = "\xEF\xBF\xBC"; /* (U+FFFC) */

static void
label_set_obj_and_text(GtkLabel *label, const char *text)
{
  gchar *s = g_strconcat(obj, text, NULL);

  gtk_label_set_text(label, s);
  g_free(s);
}

static void
icon_free(OssoABookPresenceLabelPrivate *priv)
{
  if (!priv->icon)
    return;

  g_object_unref(priv->icon);
  priv->icon = NULL;
}

static void
presence_type_changed_cb(OssoABookPresenceLabel *label)
{
  OssoABookPresenceLabelPrivate *priv =
    OSSO_ABOOK_PRESENCE_LABEL_PRIVATE(label);
  const char *status = NULL;

  icon_free(priv);

  if (priv->presence)
  {
    status = osso_abook_presence_get_presence_status_message(priv->presence);

    if (!status)
      status = osso_abook_presence_get_display_status(priv->presence);
  }

  if (!status)
    status = dgettext("osso-statusbar-presence", "pres_fi_status_offline");

  label_set_obj_and_text(GTK_LABEL(label), status);
}

static void
presence_free(gpointer data, OssoABookPresenceLabelPrivate *priv)
{
  if (!priv->presence)
    return;

  g_signal_handlers_disconnect_matched(
    priv->presence, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
    0, 0, NULL, presence_type_changed_cb, data);
  g_object_unref(priv->presence);
  priv->presence = NULL;
}

static void
osso_abook_presence_label_set_property(GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec)
{
  OssoABookPresenceLabel *label = OSSO_ABOOK_PRESENCE_LABEL(object);
  OssoABookPresenceLabelPrivate *priv =
    OSSO_ABOOK_PRESENCE_LABEL_PRIVATE(label);

  switch (property_id)
  {
    case PRESENCE:
    {
      OssoABookPresence *presence = g_value_get_object(value);

      if (presence != priv->presence)
      {
        presence_free(label, priv);

        if (presence)
        {
          priv->presence = g_object_ref(presence);
          g_signal_connect_swapped(presence, "notify::presence-type",
                                   G_CALLBACK(presence_type_changed_cb), label);
        }

        presence_type_changed_cb(label);
      }

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
osso_abook_presence_label_get_property(GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec)
{
  OssoABookPresenceLabel *label = OSSO_ABOOK_PRESENCE_LABEL(object);
  OssoABookPresenceLabelPrivate *priv =
    OSSO_ABOOK_PRESENCE_LABEL_PRIVATE(label);

  switch (property_id)
  {
    case PRESENCE:
    {
      g_value_set_object(value, priv->presence);
      break;
    }
    case LABEL:
    {
      G_OBJECT_CLASS(g_type_class_peek(pspec->owner_type))->
      get_property(object, pspec->param_id, value, pspec);
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
osso_abook_presence_label_dispose(GObject *object)
{
  OssoABookPresenceLabel *label = OSSO_ABOOK_PRESENCE_LABEL(object);
  OssoABookPresenceLabelPrivate *priv =
    OSSO_ABOOK_PRESENCE_LABEL_PRIVATE(label);

  presence_free(object, priv);
  icon_free(priv);

  G_OBJECT_CLASS(osso_abook_presence_label_parent_class)->dispose(object);
}

static gboolean
osso_abook_presence_label_expose_event(GtkWidget *widget, GdkEventExpose *event)
{
  OssoABookPresenceLabel *label = OSSO_ABOOK_PRESENCE_LABEL(widget);
  OssoABookPresenceLabelPrivate *priv =
    OSSO_ABOOK_PRESENCE_LABEL_PRIVATE(label);
  GtkWidgetClass *widget_class =
    GTK_WIDGET_CLASS(osso_abook_presence_label_parent_class);
  PangoAttrList *attrs = gtk_label_get_attributes(GTK_LABEL(widget));
  PangoAttrIterator *iter;
  PangoAttribute *attr;
  const char *text;
  size_t len;
  unsigned int idx;

  if (!attrs)
  {
    attrs = pango_attr_list_new();
    gtk_label_set_attributes(GTK_LABEL(widget), attrs);
    pango_attr_list_unref(attrs);
  }

  iter = pango_attr_list_get_iterator(attrs);

  do
  {
    attr = pango_attr_iterator_get(iter, PANGO_ATTR_SHAPE);

    if (attr)
    {
      pango_attr_iterator_destroy(iter);
      break;
    }
  }
  while (pango_attr_iterator_next(iter));

  if (!attr)
  {
    gint h = HILDON_ICON_PIXEL_SIZE_XSMALL;
    PangoRectangle rect;

    pango_attr_iterator_destroy(iter);

    rect.x = 0;
    rect.y = -PANGO_SCALE * h;
    rect.width = PANGO_SCALE * (HILDON_ICON_PIXEL_SIZE_XSMALL + 1);
    rect.height = PANGO_SCALE * h;

    attr = pango_attr_shape_new_with_data(&rect, &rect, &priv, NULL, NULL);
    pango_attr_list_insert(attrs, attr);
  }

  text = gtk_label_get_text(GTK_LABEL(widget));
  len = strlen(text);
  idx = attr->start_index;

  if ((idx >= len) || strncmp(&text[idx], obj, 3))
  {
    gchar *s = strstr(text, obj);

    if (s)
    {
      idx = s - text;
      attr->start_index = s - text;
    }
    else
    {
      idx = 0;
      label_set_obj_and_text(GTK_LABEL(label), text);
      attr->start_index = idx;
    }
  }

  attr->end_index = idx + 3;

  if (!priv->icon)
  {
    const char *icon_name = NULL;
    GtkIconTheme *theme =
      gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget));

    if (priv->presence)
      icon_name = osso_abook_presence_get_icon_name(priv->presence);

    if (!icon_name)
      icon_name = "general_presence_offline";

    priv->icon = gtk_icon_theme_load_icon(
      theme, icon_name, HILDON_ICON_PIXEL_SIZE_XSMALL, 0, NULL);
  }

  widget_class->expose_event(widget, event);

  if (priv->icon)
  {
    int width = gdk_pixbuf_get_width(priv->icon);
    int height = gdk_pixbuf_get_height(priv->icon);
    PangoLayout *layout = gtk_label_get_layout(GTK_LABEL(widget));
    PangoRectangle pos;
    int baseline;
    gint y;
    gint x;

    pango_layout_index_to_pos(layout, attr->start_index, &pos);
    baseline = pango_layout_get_baseline(layout);

    gtk_label_get_layout_offsets(GTK_LABEL(widget), &x, &y);

#define PANGO_SCALE_2 (PANGO_SCALE / 2)
    x += (pos.x + PANGO_SCALE_2) / PANGO_SCALE;

    y += (pos.y + PANGO_SCALE_2 + baseline - (height * PANGO_SCALE))
      / PANGO_SCALE;

    if (pos.x > 0)
    {
      x += ((pos.width - (width * PANGO_SCALE)) / 2 + PANGO_SCALE_2)
        / PANGO_SCALE;
    }

    gdk_draw_pixbuf(widget->window, 0, priv->icon, 0, 0, x, y, width, height,
                    GDK_RGB_DITHER_NONE, 0, 0);
  }

  return FALSE;
}

static gboolean
attr_filter_func(PangoAttribute *attribute, gpointer user_data)
{
  if (attribute->klass->type == PANGO_ATTR_SHAPE)
    return ((PangoAttrShape *)attribute)->data == user_data;

  return FALSE;
}

static void
osso_abook_presence_label_style_set(GtkWidget *widget, GtkStyle *previous_style)
{
  OssoABookPresenceLabel *label = OSSO_ABOOK_PRESENCE_LABEL(widget);
  OssoABookPresenceLabelPrivate *priv =
    OSSO_ABOOK_PRESENCE_LABEL_PRIVATE(label);
  GtkWidgetClass *widget_class =
    GTK_WIDGET_CLASS(osso_abook_presence_label_parent_class);
  PangoAttrList *attr;

  widget_class->style_set(widget, previous_style);
  icon_free(priv);
  attr = gtk_label_get_attributes(GTK_LABEL(widget));

  if (attr)
    pango_attr_list_unref(pango_attr_list_filter(attr, attr_filter_func, priv));
}

static void
osso_abook_presence_label_class_init(OssoABookPresenceLabelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->set_property = osso_abook_presence_label_set_property;
  object_class->get_property = osso_abook_presence_label_get_property;
  object_class->dispose = osso_abook_presence_label_dispose;

  widget_class->expose_event = osso_abook_presence_label_expose_event;
  widget_class->style_set = osso_abook_presence_label_style_set;

  g_object_class_install_property(
    object_class, PRESENCE,
    g_param_spec_object(
      "presence",
      "Presence",
      "The presence to display",
      OSSO_ABOOK_TYPE_PRESENCE,
      GTK_PARAM_READWRITE));
  g_object_class_override_property(object_class, LABEL, "label");
}

static void
osso_abook_presence_label_init(OssoABookPresenceLabel *label)
{
  presence_type_changed_cb(label);
}

GtkWidget *
osso_abook_presence_label_new(OssoABookPresence *presence)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE(presence) || !presence, NULL);

  return g_object_new(OSSO_ABOOK_TYPE_PRESENCE_LABEL,
                      "presence", presence,
                      NULL);
}

void
osso_abook_presence_label_set_presence(OssoABookPresenceLabel *label,
                                       OssoABookPresence *presence)
{
  g_return_if_fail(OSSO_ABOOK_IS_PRESENCE_LABEL(label));
  g_return_if_fail(OSSO_ABOOK_IS_PRESENCE(presence) || !presence);

  g_object_set(label, "presence", presence, NULL);
}

OssoABookPresence *
osso_abook_presence_label_get_presence(OssoABookPresenceLabel *label)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_PRESENCE_LABEL(label), NULL);

  return OSSO_ABOOK_PRESENCE_LABEL_PRIVATE(label)->presence;
}
