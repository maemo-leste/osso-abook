/*
 * osso-abook-entry.c
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

#include <hildon/hildon.h>

#include "config.h"

#include "osso-abook-entry.h"

struct icon_data
{
  GdkPixbuf *icon;
  gpointer icon_name;
  gint size;
};

static void
_style_set_cb(GtkWidget *widget, GtkStyle *previous_style, gpointer user_data)
{
  struct icon_data *data = user_data;
  gint size;
  GtkBorder *border;

  if (data->icon)
  {
    g_object_unref(data->icon);
    data->icon = NULL;
  }

  gtk_widget_style_get(widget, "inner-border", &border, NULL);

  if (!border)
    border = gtk_border_new();

  size = hildon_get_icon_pixel_size(gtk_icon_size_from_name("hildon-stylus"));
  data->size = size;
  border->right += size;
  gtk_entry_set_inner_border(GTK_ENTRY(widget), border);
  gtk_border_free(border);
}

static gboolean
_expose_event_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  struct icon_data *data = user_data;

  gint icon_height;
  gint icon_width;
  GdkGC *gc;
  gint height;
  gint width;

  if (GTK_ENTRY(widget)->text_area != event->any.window )
    return FALSE;

  if ( !data->icon )
  {
    GtkIconTheme *theme =
        gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget));
    gint size = hildon_get_icon_pixel_size(
          gtk_icon_size_from_name("hildon-stylus"));
    data->icon =
        gtk_icon_theme_load_icon(theme, data->icon_name, size, 0, NULL);

    if (!data->icon)
      return FALSE;
  }

  gc = gdk_gc_new(event->any.window);
  gdk_gc_set_clip_region(gc, event->expose.region);
  gdk_drawable_get_size(event->any.window, &width, &height);
  icon_height = gdk_pixbuf_get_height(data->icon);
  icon_width = gdk_pixbuf_get_width(data->icon);

  gdk_draw_pixbuf(event->any.window, gc, data->icon, 0, 0,
                  (data->size - icon_width) / 2 + width - data->size,
                  (height - icon_height) / 2, icon_width, icon_height,
                  GDK_RGB_DITHER_NORMAL, 0, 0);
  g_object_unref(gc);

  return FALSE;
}

static void
_icon_data_destroy(gpointer user_data)
{
  struct icon_data *data = user_data;

  if (data->icon)
    g_object_unref(data->icon);

  g_free(data->icon_name);
  g_free(data);
}

void
_osso_abook_entry_add_icon(GtkEntry *entry, gchar *icon_name)
{
  struct icon_data *data;

  data = g_object_get_data(G_OBJECT(entry), "icon-data");

  if (!data)
  {
    data = g_new0(struct icon_data, 1);
    g_signal_connect_after(entry, "expose-event",
                           G_CALLBACK(_expose_event_cb), data);
    g_signal_connect(entry, "style-set",
                     G_CALLBACK(_style_set_cb), data);
    g_object_set_data_full(G_OBJECT(entry), "icon-data", data,
                           _icon_data_destroy);
  }

  g_free(data->icon_name);
  data->icon_name = g_strdup(icon_name);
  _style_set_cb(GTK_WIDGET(entry), NULL, data);
}

void
_osso_abook_entry_set_error_style(GtkEntry *entry)
{
  _osso_abook_entry_add_icon(entry, "app_install_error");
  hildon_helper_set_logical_color(GTK_WIDGET(entry), GTK_RC_TEXT,
                                  GTK_STATE_NORMAL, "AttentionColor");
}

