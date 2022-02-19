/*
 * osso-abook-button.c
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

#include "osso-abook-button.h"
#include "osso-abook-enums.h"

struct _OssoABookButtonPrivate
{
  OssoABookButtonStyle style;
  PangoLayout *title_layout;
  gint title_layout_width;
  PangoLayout *value_layout;
  gint value_layout_width;
  gchar *icon_name;
  GdkPixbuf *icon;
  OssoABookPresence *presence;
  GdkPixbuf *presence_icon;
  gboolean icon_visible : 1; /* 0x1 */
  gboolean visible : 1; /* = 0x2 */
  gboolean presence_invalid : 1; /* = 0x4 */
  gboolean highlighted : 1; /* = 0x8 */
  gboolean resized : 1; /* = 0x10 */
};

typedef struct _OssoABookButtonPrivate OssoABookButtonPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookButton,
  osso_abook_button,
  GTK_TYPE_BUTTON
);

#define OSSO_ABOOK_BUTTON_PRIVATE(button) \
  ((OssoABookButtonPrivate *)osso_abook_button_get_instance_private((( \
                                                                       OssoABookButton \
                                                                       *)button)))

struct _OssoABookButtonStyleHints
{
  GtkBorder default_border;
  GtkBorder inner_border;
  guint child_displacement_x;
  guint child_displacement_y;
  guint focus_line_width;
  guint focus_padding;
  guint image_spacing;
  GtkStyle *style[5];
  gint line_spacing;
  gint finger_width;
  gint finger_height;
  gint stylus_width;
  gint stylus_height;
  gint xsmall_width;
  gint xsmall_height;
};

typedef struct _OssoABookButtonStyleHints OssoABookButtonStyleHints;

enum
{
  PROP_SIZE = 1,
  PROP_STYLE,
  PROP_TITLE,
  PROP_VALUE,
  PROP_ICON_NAME,
  PROP_ICON_VISIBLE,
  PROP_PRESENCE,
  PROP_PRESENCE_VISIBLE,
  PROP_HIGHLIGHTED,
};

enum
{
  STYLE_WIDGET_DEFAULT,
  STYLE_TITLE_HIGHLIGHTED,
  STYLE_VALUE_DEFAULT,
  STYLE_VALUE_PICKER,
  STYLE_VALUE_INVALID_PRESENCE
};
static GQuark style_hints_quark;

static PangoAlignment
_get_alignment(GtkButton *button)
{
  gfloat xalign;

  gtk_button_get_alignment(button, &xalign, NULL);

  if (xalign < 0.25f)
    return PANGO_ALIGN_LEFT;
  else if (xalign > 0.75f)
    return PANGO_ALIGN_RIGHT;
  else
    return PANGO_ALIGN_CENTER;
}

static void
_notify_xalign_cb(GtkButton *button, GParamSpec *pspec, gpointer user_data)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(button);
  PangoAlignment align = _get_alignment(button);

  if (priv->title_layout)
    pango_layout_set_alignment(priv->title_layout, align);

  if (priv->value_layout)
    pango_layout_set_alignment(priv->value_layout, align);
}

static void
osso_abook_button_init(OssoABookButton *button)
{
  gtk_button_set_alignment(GTK_BUTTON(button), 0.0f, 0.5f);
  g_signal_connect(button, "notify::xalign",
                   G_CALLBACK(_notify_xalign_cb), NULL);
}

static void
osso_abook_button_add(GtkContainer *container, GtkWidget *widget)
{
  g_warning("%s: Adding widgets is not supported", __FUNCTION__);
}

static void
osso_abook_button_remove(GtkContainer *container, GtkWidget *widget)
{
  g_warning("%s: Removing widgets is not supported", __FUNCTION__);
}

static void
_presence_status_changed_cb(OssoABookButton *button)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(button);

  if (priv->presence_icon)
  {
    g_object_unref(priv->presence_icon);
    priv->presence_icon = NULL;
  }

  gtk_widget_queue_draw(GTK_WIDGET(button));
}

static void
_remove_presence(GObject *button)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(button);

  if (priv->presence)
  {
    g_signal_handlers_disconnect_matched(
      priv->presence, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, _presence_status_changed_cb, button);
    g_object_unref(priv->presence);
    priv->presence = NULL;
  }
}

static const char *
_get_layout_text(PangoLayout *layout)
{
  if (layout)
    return pango_layout_get_text(layout);

  return NULL;
}

static gboolean
_is_style_text(OssoABookButtonStyle style)
{
  return
    style == OSSO_ABOOK_BUTTON_STYLE_LABEL ||
    style == OSSO_ABOOK_BUTTON_STYLE_NOTE;
}

static void
_resize(GtkWidget *button, OssoABookButtonPrivate *priv)
{
  if (gtk_widget_get_realized(button))
    gtk_widget_queue_resize(button);
  else
    priv->resized = TRUE;
}

static void
_styles_attach(OssoABookButtonStyleHints *hints, GtkWidget *widget)
{
  GdkWindow *window = widget->window;

  hints->style[STYLE_TITLE_HIGHLIGHTED] =
    gtk_style_attach(hints->style[STYLE_TITLE_HIGHLIGHTED], window);
  hints->style[STYLE_VALUE_DEFAULT] =
    gtk_style_attach(hints->style[STYLE_VALUE_DEFAULT], window);
  hints->style[STYLE_VALUE_PICKER] =
    gtk_style_attach(hints->style[STYLE_VALUE_PICKER], window);
  hints->style[STYLE_VALUE_INVALID_PRESENCE] =
    gtk_style_attach(hints->style[STYLE_VALUE_INVALID_PRESENCE], window);
}

static void
_get_inner_border(GtkWidget *button, OssoABookButtonPrivate *priv,
                  OssoABookButtonStyleHints *hints, GtkBorder *border)
{
  GtkStyle *style = button->style;
  gint border_width = GTK_CONTAINER(button)->border_width;
  gint xthickness = style->xthickness;
  gint ythickness = style->ythickness;

  border->left = border_width + hints->inner_border.left + xthickness;
  border->right = border_width + hints->inner_border.right + xthickness;
  border->top = border_width + hints->inner_border.top + ythickness;
  border->bottom = border_width + hints->inner_border.bottom + ythickness;

  if (gtk_widget_get_can_default(button))
  {
    border->left += hints->default_border.left;
    border->right += hints->default_border.right;
    border->top += hints->default_border.top;
    border->bottom += hints->default_border.bottom;
  }

  if (gtk_widget_get_can_focus(button))
  {
    border->left += hints->focus_padding + hints->focus_line_width;
    border->right += hints->focus_padding + hints->focus_line_width;
    border->top += hints->focus_padding + hints->focus_line_width;
    border->bottom += hints->focus_padding + hints->focus_line_width;
  }

  if (priv->icon_visible)
    border->left += hints->image_spacing + hints->finger_width;

  if (priv->visible)
  {
    gint width;

    if (priv->presence_invalid)
      width = hints->stylus_width;
    else
      width = hints->xsmall_width;

    border->right += hints->image_spacing + width;
  }
}

static void
_get_full_border(GtkWidget *button, GtkAllocation *allocation,
                 OssoABookButtonStyleHints *hints, GtkBorder *inner_border,
                 GtkBorder *full_border)
{
  full_border->left = inner_border->left + allocation->x;
  full_border->right = inner_border->top + allocation->y;
  full_border->top =
    allocation->width - inner_border->left - inner_border->right;
  full_border->bottom =
    allocation->height - inner_border->top - inner_border->bottom;

  if (GTK_BUTTON(button)->focus_on_click)
  {
    full_border->left += hints->child_displacement_x;
    full_border->right += hints->child_displacement_y;
  }
}

static gboolean
_is_not_note(OssoABookButtonStyle style)
{
  return style != OSSO_ABOOK_BUTTON_STYLE_NOTE;
}

static gint
_get_line_count(GtkWidget *button)
{
  gint count;
  PangoLayout *layout;
  gint h;

  gtk_widget_get_size_request(button, NULL, &h);

  if ((h == -1) && (layout = OSSO_ABOOK_BUTTON_PRIVATE(button)->value_layout))
  {
    count = pango_layout_get_line_count(layout);

    if (count >= 2)
      count = 2;
  }
  else
    count = 1;

  if (h > 70)
    count = 2;

  return count;
}

static gint
_get_button_style_height(GtkWidget *button)
{
  if (_get_line_count(button) > 1)
    return 105;
  else
    return 70;
}

static gint
_get_note_style_height(GtkWidget *button)
{
  OssoABookButtonStyleHints *hints;
  OssoABookButtonPrivate *priv;
  GtkBorder inner_border;
  gint height;
  gint value_height = 0;
  gint title_height = 0;

  gtk_widget_get_size_request(button, NULL, &height);

  if (height >= 0)
    return height;

  hints = g_object_get_qdata(G_OBJECT(button), style_hints_quark);
  priv = OSSO_ABOOK_BUTTON_PRIVATE(button);
  _get_inner_border(button, priv, hints, &inner_border);

  if (priv->title_layout)
    pango_layout_get_pixel_size(priv->title_layout, NULL, &title_height);

  if (priv->value_layout)
    pango_layout_get_pixel_size(priv->value_layout, NULL, &value_height);

  height = value_height + title_height + inner_border.bottom + inner_border.top;

  if (priv->icon_visible)
  {
    if (height < hints->finger_height)
      height = hints->finger_height;
  }

  if (priv->visible)
  {
    if (height < hints->xsmall_height)
      height = hints->xsmall_height;
  }

  if (height >= 70)
  {
    if (height != 35 * (height / 35))
      height = 35 * (height / 35 + 1);
  }
  else
    height = 70;

  return height;
}

static void
_style_hints_destroy(OssoABookButtonStyleHints *hints)
{
  if (hints->style[STYLE_TITLE_HIGHLIGHTED])
    g_object_unref(hints->style[STYLE_TITLE_HIGHLIGHTED]);

  if (hints->style[STYLE_VALUE_DEFAULT])
    g_object_unref(hints->style[STYLE_VALUE_DEFAULT]);

  if (hints->style[STYLE_VALUE_PICKER])
    g_object_unref(hints->style[STYLE_VALUE_PICKER]);

  if (hints->style[STYLE_VALUE_INVALID_PRESENCE])
    g_object_unref(hints->style[STYLE_VALUE_INVALID_PRESENCE]);

  g_free(hints);
}

static void
osso_abook_button_size_request(GtkWidget *widget, GtkRequisition *requisition)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(widget);
  gint screen_width = gdk_screen_get_width(gtk_widget_get_screen(widget));
  OssoABookButtonStyleHints *hints =
    g_object_get_qdata(G_OBJECT(widget), style_hints_quark);
  gint width;
  GtkBorder inner_border;

  _get_inner_border(widget, priv, hints, &inner_border);

  if (priv->title_layout_width < priv->value_layout_width)
    width = inner_border.right + inner_border.left + priv->value_layout_width;
  else
    width = inner_border.right + inner_border.left + priv->title_layout_width;

  if (width < screen_width / 3)
    requisition->width = width;
  else
    requisition->width = screen_width / 3;

  if (_is_not_note(priv->style))
    requisition->height = _get_button_style_height(widget);
  else
    requisition->height = _get_note_style_height(widget);

  if (requisition->height != widget->requisition.height)
  {
    gint height = widget->requisition.height;

    /* fmg - no idea where those values come from */
    if (height == 70)
      gtk_widget_set_name(widget, "OssoABookButton-finger");
    else if (height == 105)
      gtk_widget_set_name(widget, "OssoABookButton-thumb");
    else
      gtk_widget_set_name(widget, "OssoABookButton");
  }
}

static void
osso_abook_button_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(widget);
  OssoABookButtonStyleHints *hints = g_object_get_qdata(G_OBJECT(widget),
                                                        style_hints_quark);
  gint height;
  GtkBorder inner_border;
  GtkBorder full_border;

  _get_inner_border(widget, priv, hints, &inner_border);
  _get_full_border(widget, allocation, hints, &inner_border, &full_border);

  if (priv->value_layout)
    pango_layout_set_width(priv->value_layout, full_border.top * PANGO_SCALE);

  if (_is_not_note(priv->style))
    height = _get_button_style_height(widget);
  else
    height = _get_note_style_height(widget);

  if (widget->requisition.height != height)
    _resize(widget, priv);

  GTK_WIDGET_CLASS(osso_abook_button_parent_class)->
  size_allocate(widget, allocation);
}

static gboolean
osso_abook_button_expose_event(GtkWidget *widget, GdkEventExpose *event)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(widget);
  OssoABookButtonStyleHints *hints =
    g_object_get_qdata(G_OBJECT(widget), style_hints_quark);
  gint height = 0;
  gint width = 0;
  gint value_height = 0;
  cairo_t *cr;
  GtkBorder inner_border;
  GtkBorder full_border;
  gfloat yalign;
  gfloat xalign;

  if (!_is_style_text(priv->style))
  {
    if (GTK_WIDGET_CLASS(osso_abook_button_parent_class)->expose_event(widget,
                                                                       event))
    {
      return TRUE;
    }
  }

  cr = gdk_cairo_create(widget->window);
  gdk_cairo_region(cr, event->region);
  cairo_clip(cr);
  _get_inner_border(widget, priv, hints, &inner_border);
  _get_full_border(widget, &widget->allocation, hints, &inner_border,
                   &full_border);
  gtk_button_get_alignment(GTK_BUTTON(widget), &xalign, &yalign);

  if (priv->title_layout)
  {
    pango_layout_get_pixel_size(priv->title_layout, &width, &height);

    if (width > full_border.top)
      width = full_border.top;
  }

  if (priv->value_layout)
  {
    gint line_count = pango_layout_get_line_count(priv->value_layout);

    if (_is_not_note(priv->style) && (line_count >= _get_line_count(widget)))
      line_count = _get_line_count(widget);

    if (line_count > 0)
    {
      PangoRectangle rect;
      gint _h = 0;
      gint line;

      for (line = 0; line < line_count; line++)
      {
        pango_layout_line_get_extents(
          pango_layout_get_line_readonly(priv->value_layout, line),
          NULL, &rect);
        _h += rect.height;
      }

      value_height = (_h + 1023) / PANGO_SCALE;
    }
  }

  if (priv->title_layout)
  {
    GtkStyle *style;
    GdkRectangle area;

    area.x = ((float)(full_border.top - width) * xalign +
              (float)full_border.left);
    area.y = ((float)(full_border.bottom - height - value_height) * yalign +
              (float)full_border.right);
    area.width = width;
    area.height = height;

    if (priv->highlighted)
      style = hints->style[STYLE_TITLE_HIGHLIGHTED];
    else
      style = widget->style;

    gtk_paint_layout(style, widget->window, widget->state, FALSE, &area, widget,
                     NULL, area.x, area.y, priv->title_layout);
  }

  if (priv->value_layout)
  {
    gint style_idx;
    GdkRectangle area;

    area.x = full_border.left;
    area.y = (float)(full_border.bottom - height - value_height) * yalign +
      (float)(full_border.right + height);
    area.width = full_border.top;
    area.height = value_height;

    if (priv->presence_invalid)
      style_idx = STYLE_VALUE_INVALID_PRESENCE;
    else if (priv->style == OSSO_ABOOK_BUTTON_STYLE_PICKER)
      style_idx = STYLE_VALUE_PICKER;
    else
      style_idx = STYLE_VALUE_DEFAULT;

    gtk_paint_layout(
      hints->style[style_idx], widget->window, widget->state, FALSE,
      &area, widget, NULL, area.x, area.y, priv->value_layout);
  }

  if (priv->icon_visible)
  {
    if (!priv->icon)
    {
      if (priv->icon_name)
      {
        gint size = hints->finger_width;

        if (size > hints->finger_height)
          size = hints->finger_height;

        priv->icon = gtk_icon_theme_load_icon(
          gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget)),
          priv->icon_name, size, 0, NULL);
      }
    }

    if (priv->icon)
    {
      gint w = gdk_pixbuf_get_width(priv->icon);
      gint h = gdk_pixbuf_get_height(priv->icon);
      gint bottom = full_border.bottom;

      if (widget->allocation.height > 70)
        bottom = 70 - inner_border.top - inner_border.bottom;

      gdk_cairo_set_source_pixbuf(
        cr, priv->icon,
        full_border.left - hints->image_spacing -
        (hints->finger_width + w) / 2,
        full_border.right + (bottom - h) / 2);
      cairo_paint(cr);
    }
  }

  if (priv->visible)
  {
    const char *icon_name;
    gint icon_width;
    gint icon_height;

    if (priv->presence_invalid)
    {
      icon_width = hints->stylus_width;
      icon_height = hints->stylus_height;
      icon_name = "app_install_error";
    }
    else
    {
      icon_width = hints->xsmall_width;
      icon_height = hints->xsmall_height;

      if (priv->presence)
        icon_name = osso_abook_presence_get_icon_name(priv->presence);
      else
        icon_name = "general_presence_offline";
    }

    if (!priv->presence_icon && icon_name)
    {
      gint size = icon_height;

      if (icon_height < icon_width)
        size = icon_width;

      priv->presence_icon = gtk_icon_theme_load_icon(
        gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget)),
        icon_name, size, 0, NULL);
    }

    if (priv->presence_icon)
    {
      gint w = gdk_pixbuf_get_width(priv->presence_icon);
      gint h = gdk_pixbuf_get_height(priv->presence_icon);

      gdk_cairo_set_source_pixbuf(
        cr, priv->presence_icon,
        (icon_width - w) / 2 + hints->image_spacing + full_border.top +
        full_border.left,
        (int)((float)(full_border.bottom - height - value_height) * yalign +
              (float)full_border.right + (float)((height - h) / 2)));
      cairo_paint(cr);
    }
  }

  cairo_destroy(cr);

  return FALSE;
}

static void
osso_abook_button_style_set(GtkWidget *widget, GtkStyle *previous_style)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(widget);
  OssoABookButtonStyleHints *hints = g_object_get_qdata(G_OBJECT(widget),
                                                        style_hints_quark);
  GtkStyle *style;
  PangoFontMetrics *metrics;
  gint width;
  GtkSettings *settings;
  GdkColor color;
  GtkBorder *border;

  GTK_WIDGET_CLASS(osso_abook_button_parent_class)->style_set(widget,
                                                              previous_style);

  if (!hints || (hints->style[STYLE_WIDGET_DEFAULT] != widget->style))
  {
    settings = gtk_widget_get_settings(widget);
    hints = g_new0(OssoABookButtonStyleHints, 1);
    hints->style[STYLE_WIDGET_DEFAULT] = widget->style;
    gtk_widget_style_get(widget,
                         "child-displacement-x", &hints->child_displacement_x,
                         "child-displacement-y", &hints->child_displacement_y,
                         "focus-line-width", &hints->focus_line_width,
                         "focus-padding", &hints->focus_padding,
                         "image-spacing", &hints->image_spacing,
                         NULL);

    gtk_widget_style_get(widget, "default-border", &border, NULL);

    if (border)
    {
      hints->default_border = *border;
      gtk_border_free(border);
    }
    else
    {
      hints->default_border.left = 1;
      hints->default_border.right = 1;
      hints->default_border.top = 1;
      hints->default_border.bottom = 1;
    }

    gtk_widget_style_get(widget, "inner-border", &border, NULL);

    if (border)
    {
      hints->inner_border = *border;
      gtk_border_free(border);
    }
    else
    {
      hints->inner_border.left = 1;
      hints->inner_border.right = 1;
      hints->inner_border.top = 1;
      hints->inner_border.bottom = 1;
    }

    hints->style[STYLE_TITLE_HIGHLIGHTED] = gtk_style_copy(widget->style);

    if (gtk_style_lookup_color(widget->style, "ActiveTextColor", &color))
    {
      hints->style[STYLE_TITLE_HIGHLIGHTED]->fg[GTK_STATE_NORMAL] = color;
      hints->style[STYLE_TITLE_HIGHLIGHTED]->fg[GTK_STATE_PRELIGHT] = color;
    }

    hints->style[STYLE_VALUE_DEFAULT] = gtk_style_copy(widget->style);
    style = gtk_rc_get_style_by_paths(settings, "SmallSystemFont", NULL,
                                      G_TYPE_NONE);

    if (style)
    {
      pango_font_description_free(hints->style[STYLE_VALUE_DEFAULT]->font_desc);
      hints->style[STYLE_VALUE_DEFAULT]->font_desc =
        pango_font_description_copy(style->font_desc);
    }

    metrics = pango_context_get_metrics(gtk_widget_get_pango_context(
                                          widget),
                                        hints->style[STYLE_VALUE_DEFAULT]->font_desc,
                                        NULL);
    hints->line_spacing = pango_font_metrics_get_descent(metrics) +
      pango_font_metrics_get_ascent(metrics);
    pango_font_metrics_unref(metrics);

    hints->style[STYLE_VALUE_PICKER] =
      gtk_style_copy(hints->style[STYLE_VALUE_DEFAULT]);
    hints->style[STYLE_VALUE_INVALID_PRESENCE] =
      gtk_style_copy(hints->style[STYLE_VALUE_DEFAULT]);

    if (gtk_style_lookup_color(widget->style, "SecondaryTextColor", &color))
    {
      hints->style[STYLE_VALUE_DEFAULT]->fg[GTK_STATE_NORMAL] = color;
      hints->style[STYLE_VALUE_DEFAULT]->fg[GTK_STATE_PRELIGHT] = color;
    }

    if (gtk_style_lookup_color(widget->style, "ActiveTextColor", &color))
    {
      hints->style[STYLE_VALUE_PICKER]->fg[GTK_STATE_NORMAL] = color;
      hints->style[STYLE_VALUE_PICKER]->fg[GTK_STATE_PRELIGHT] = color;
    }

    if (gtk_style_lookup_color(widget->style, "AttentionColor", &color))
    {
      hints->style[STYLE_VALUE_INVALID_PRESENCE]->fg[GTK_STATE_NORMAL] = color;
      hints->style[STYLE_VALUE_INVALID_PRESENCE]->fg[GTK_STATE_PRELIGHT] =
        color;
    }

    if (gtk_widget_get_realized(widget))
      _styles_attach(hints, widget);

    if (!gtk_icon_size_lookup_for_settings(settings, HILDON_ICON_SIZE_FINGER,
                                           &hints->finger_width,
                                           &hints->finger_height))
    {
      hints->finger_height = 48;
      hints->finger_width = 48;
    }

    if (!gtk_icon_size_lookup_for_settings(settings, HILDON_ICON_SIZE_STYLUS,
                                           &hints->stylus_width,
                                           &hints->stylus_height))
    {
      hints->stylus_height = 32;
      hints->stylus_width = 32;
    }

    if (!gtk_icon_size_lookup_for_settings(settings, HILDON_ICON_SIZE_XSMALL,
                                           &hints->xsmall_width,
                                           &hints->xsmall_height))
    {
      hints->xsmall_height = 16;
      hints->xsmall_width = 16;
    }

    g_object_set_qdata_full(G_OBJECT(widget), style_hints_quark, hints,
                            (GDestroyNotify)_style_hints_destroy);
  }

  if (priv->title_layout)
  {
    pango_layout_set_font_description(priv->title_layout,
                                      widget->style->font_desc);
    pango_layout_get_pixel_size(priv->title_layout, &priv->title_layout_width,
                                NULL);
  }

  if (priv->value_layout)
  {
    width = pango_layout_get_width(priv->value_layout);
    pango_layout_set_width(priv->value_layout, -1);
    pango_layout_set_font_description(priv->value_layout,
                                      hints->style[STYLE_VALUE_DEFAULT]->font_desc);
    pango_layout_get_pixel_size(priv->value_layout, &priv->value_layout_width,
                                NULL);
    pango_layout_set_width(priv->value_layout, width);
  }

  if (priv->icon)
  {
    g_object_unref(priv->icon);
    priv->icon = NULL;
  }

  if (priv->presence_icon)
  {
    g_object_unref(priv->presence_icon);
    priv->presence_icon = NULL;
  }
}

static void
osso_abook_button_realize(GtkWidget *widget)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(widget);
  OssoABookButtonStyleHints *hints = g_object_get_qdata(G_OBJECT(widget),
                                                        style_hints_quark);

  GTK_WIDGET_CLASS(osso_abook_button_parent_class)->realize(widget);

  if (hints)
    _styles_attach(hints, widget);

  if (priv->resized)
  {
    gtk_widget_queue_resize(widget);
    priv->resized = FALSE;
  }
}

static gboolean
osso_abook_button_event(GtkWidget *widget, GdkEvent *event)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(widget);

  if (!_is_style_text(priv->style))
    return FALSE;

  switch (event->type)
  {
    case GDK_MOTION_NOTIFY:
    case GDK_BUTTON_PRESS:
    case GDK_BUTTON_RELEASE:
    case GDK_ENTER_NOTIFY:
    case GDK_LEAVE_NOTIFY:
    {
      return TRUE;
    }
    default:
      return FALSE;
  }
}

static void
osso_abook_button_set_property(GObject *object, guint property_id,
                               const GValue *value, GParamSpec *pspec)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(object);
  GtkWidget *button = GTK_WIDGET(object);

  switch (property_id)
  {
    case PROP_SIZE:
    {
      hildon_gtk_widget_set_theme_size(button, g_value_get_flags(value));
      break;
    }
    case PROP_STYLE:
    {
      OssoABookButtonStyle style = g_value_get_enum(value);

      if (priv->style == style)
        break;

      g_object_set_qdata(object, style_hints_quark, NULL);
      priv->style = style;
      gtk_widget_set_can_focus(button, !_is_style_text(style));

      goto redraw;
    }
    case PROP_TITLE:
    {
      const gchar *title = g_value_get_string(value);

      if (priv->title_layout)
      {
        g_object_unref(priv->title_layout);
        priv->title_layout = NULL;
      }

      if (title)
      {
        priv->title_layout = gtk_widget_create_pango_layout(button, title);
        pango_layout_set_alignment(priv->title_layout,
                                   _get_alignment(GTK_BUTTON(button)));
      }

      goto redraw;
    }
    case PROP_VALUE:
    {
      const gchar *val = g_value_get_string(value);

      if (priv->value_layout)
      {
        g_object_unref(priv->value_layout);
        priv->value_layout = NULL;
      }

      if (val)
      {
        priv->value_layout = gtk_widget_create_pango_layout(button, val);
        pango_layout_set_alignment(priv->value_layout,
                                   _get_alignment(GTK_BUTTON(button)));
        pango_layout_set_wrap(priv->value_layout, PANGO_WRAP_CHAR);
      }

      goto redraw;
    }
    case PROP_ICON_NAME:
    {
      if (priv->icon)
      {
        g_object_unref(priv->icon);
        priv->icon = NULL;
      }

      g_free(priv->icon_name);
      priv->icon_name = g_value_dup_string(value);
      _resize(button, priv);
      break;
    }
    case PROP_ICON_VISIBLE:
    {
      priv->icon_visible = g_value_get_boolean(value);
      _resize(button, priv);
      break;
    }
    case PROP_PRESENCE:
    {
      _remove_presence(object);
      priv->presence = g_value_dup_object(value);

      if (priv->presence)
      {
        g_signal_connect_swapped(
          priv->presence, "notify::presence-status",
          G_CALLBACK(_presence_status_changed_cb), object);
        priv->presence_invalid = osso_abook_presence_is_invalid(priv->presence);
      }
      else
        priv->presence_invalid = FALSE;

      _presence_status_changed_cb(OSSO_ABOOK_BUTTON(button));
      break;
    }
    case PROP_PRESENCE_VISIBLE:
    {
      priv->visible = g_value_get_boolean(value);
      _resize(button, priv);
      break;
    }
    case PROP_HIGHLIGHTED:
    {
      priv->highlighted = g_value_get_boolean(value);
      gtk_widget_queue_draw(button);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }

  return;

redraw:
  osso_abook_button_style_set(button, NULL);
  _resize(button, priv);
}

static void
osso_abook_button_get_property(GObject *object, guint property_id,
                               GValue *value, GParamSpec *pspec)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(object);

  switch (property_id)
  {
    case PROP_TITLE:
    {
      g_value_set_string(value, _get_layout_text(priv->title_layout));
      break;
    }
    case PROP_VALUE:
    {
      g_value_set_string(value, _get_layout_text(priv->value_layout));
      break;
    }
    case PROP_ICON_NAME:
    {
      g_value_set_string(value, priv->icon_name);
      break;
    }
    case PROP_ICON_VISIBLE:
    {
      g_value_set_boolean(value, priv->icon_visible);
      break;
    }
    case PROP_PRESENCE:
    {
      g_value_set_object(value, priv->presence);
      break;
    }
    case PROP_PRESENCE_VISIBLE:
    {
      g_value_set_boolean(value, priv->visible);
      break;
    }
    case PROP_HIGHLIGHTED:
    {
      g_value_set_boolean(value, priv->highlighted);
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
osso_abook_button_dispose(GObject *object)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(object);

  if (priv->title_layout)
  {
    g_object_unref(priv->title_layout);
    priv->title_layout = NULL;
  }

  if (priv->value_layout)
  {
    g_object_unref(priv->value_layout);
    priv->value_layout = NULL;
  }

  if (priv->icon)
  {
    g_object_unref(priv->icon);
    priv->icon = NULL;
  }

  if (priv->presence_icon)
  {
    g_object_unref(priv->presence_icon);
    priv->presence_icon = NULL;
  }

  _remove_presence(object);

  G_OBJECT_CLASS(osso_abook_button_parent_class)->dispose(object);
}

static void
osso_abook_button_finalize(GObject *object)
{
  OssoABookButtonPrivate *priv = OSSO_ABOOK_BUTTON_PRIVATE(object);

  g_free(priv->icon_name);
  G_OBJECT_CLASS(osso_abook_button_parent_class)->finalize(object);
}

static void
osso_abook_button_class_init(OssoABookButtonClass *klass)
{
  GtkContainerClass *container_class = GTK_CONTAINER_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  container_class->add = osso_abook_button_add;
  container_class->remove = osso_abook_button_remove;

  widget_class->size_request = osso_abook_button_size_request;
  widget_class->size_allocate = osso_abook_button_size_allocate;
  widget_class->expose_event = osso_abook_button_expose_event;
  widget_class->style_set = osso_abook_button_style_set;
  widget_class->realize = osso_abook_button_realize;
  widget_class->event = osso_abook_button_event;

  object_class->set_property = osso_abook_button_set_property;
  object_class->get_property = osso_abook_button_get_property;
  object_class->dispose = osso_abook_button_dispose;
  object_class->finalize = osso_abook_button_finalize;

  g_object_class_install_property(
    object_class, PROP_SIZE,
    g_param_spec_flags(
      "size",
      "Size",
      "Size request for the button",
      HILDON_TYPE_SIZE_TYPE,
      HILDON_SIZE_AUTO,
      GTK_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_STYLE,
    g_param_spec_enum(
      "style",
      "Style",
      "Visual style of the button",
      OSSO_ABOOK_TYPE_BUTTON_STYLE,
      OSSO_ABOOK_BUTTON_STYLE_NORMAL,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_TITLE,
    g_param_spec_string(
      "title",
      "Title",
      "Text of the button's title label",
      NULL,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_VALUE,
    g_param_spec_string(
      "value",
      "Value",
      "Text of the button's value label",
      NULL,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_ICON_NAME,
    g_param_spec_string(
      "icon-name",
      "Icon Name",
      "Name of the button's icon",
      NULL,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_ICON_VISIBLE,
    g_param_spec_boolean(
      "icon-visible",
      "Icon Visible",
      "Weither the icon is visible",
      FALSE,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_PRESENCE,
    g_param_spec_object(
      "presence",
      "Presence",
      "Presence state for this button",
      OSSO_ABOOK_TYPE_PRESENCE,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_PRESENCE_VISIBLE,
    g_param_spec_boolean(
      "presence-visible",
      "Presence Visible",
      "Weither the presence icon is visible",
      FALSE,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_HIGHLIGHTED,
    g_param_spec_boolean(
      "highlighted",
      "Highlighted",
      "Wether to highlight the title text",
      FALSE,
      GTK_PARAM_READWRITE));

  style_hints_quark = g_quark_from_static_string("OssoABookButton-style-hints");

  gtk_rc_parse_string("class 'OssoABookButton' style 'hildon-button-label'");
  gtk_rc_parse_string(
    "widget '*.hildon-dialog-action-area.OssoABookButton-finger' style 'fremantle-dialog-button'");
}

GtkWidget *
osso_abook_button_new(HildonSizeType size)
{
  return gtk_widget_new(OSSO_ABOOK_TYPE_BUTTON, "size", size, NULL);
}

GtkWidget *
osso_abook_button_new_with_text(HildonSizeType size, const char *title,
                                const char *value)
{
  return gtk_widget_new(OSSO_ABOOK_TYPE_BUTTON,
                        "size", size,
                        "title", title,
                        "value", value,
                        NULL);
}

GtkWidget *
osso_abook_button_new_with_presence(HildonSizeType size, const char *icon_name,
                                    const char *title, const char *value,
                                    OssoABookPresence *presence)
{
  g_return_val_if_fail(!presence || OSSO_ABOOK_IS_PRESENCE(presence), NULL);

  return gtk_widget_new(OSSO_ABOOK_TYPE_BUTTON,
                        "size", size,
                        "title", title,
                        "value", value,
                        "icon-name", icon_name,
                        "icon-visible", icon_name ? TRUE : FALSE,
                        "presence", presence,
                        "presence-visible", presence ? TRUE : FALSE,
                        NULL);
}

void
osso_abook_button_set_style(OssoABookButton *button, OssoABookButtonStyle style)
{
  g_return_if_fail(OSSO_ABOOK_IS_BUTTON(button));
  g_object_set(button, "style", style, NULL);
}

void
osso_abook_button_set_title(OssoABookButton *button, const char *title)
{
  g_return_if_fail(OSSO_ABOOK_IS_BUTTON(button));
  g_object_set(button, "title", title, NULL);
}
