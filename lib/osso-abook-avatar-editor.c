/*
 * osso-abook-avatar-editor.c
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

#include "osso-abook-avatar-editor.h"
#include "osso-abook-avatar-image.h"

struct _OssoABookAvatarEditorPrivate
{
  int x;
  int y;
};

typedef struct _OssoABookAvatarEditorPrivate OssoABookAvatarEditorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookAvatarEditor,
  osso_abook_avatar_editor,
  GTK_TYPE_EVENT_BOX
);

#define PRIVATE(editor) \
  ((OssoABookAvatarEditorPrivate *)osso_abook_avatar_editor_get_instance_private(((OssoABookAvatarEditor *)editor)))

static gboolean
osso_abook_avatar_editor_button_press_event(GtkWidget *widget,
                                            GdkEventButton *event)
{
  OssoABookAvatarEditorPrivate *priv = PRIVATE(widget);

  if (event->type != GDK_BUTTON_PRESS)
    return FALSE;

  gtk_widget_grab_focus(widget);
  priv->x = event->x;
  priv->y = event->y;

  return TRUE;
}

static gboolean
osso_abook_avatar_editor_motion_notify_event(GtkWidget *widget,
                                             GdkEventMotion *event)
{
  OssoABookAvatarEditor *editor = OSSO_ABOOK_AVATAR_EDITOR(widget);
  OssoABookAvatarEditorPrivate *priv = PRIVATE(editor);
  OssoABookAvatarImage *image = osso_abook_avatar_editor_get_image(editor);
  GdkPixbuf *image_pixbuf = osso_abook_avatar_image_get_pixbuf(image);
  gdouble size = osso_abook_avatar_image_get_size(image);
  GtkAdjustment *xadj = osso_abook_avatar_image_get_xadjustment(image);
  GtkAdjustment *yadj = osso_abook_avatar_image_get_yadjustment(image);
  gdouble zoom = osso_abook_avatar_image_get_current_zoom(image);

  if (image_pixbuf)
  {
    gdouble c_x = zoom * (gdouble)gdk_pixbuf_get_width(image_pixbuf) - size;
    gdouble c_y = zoom * (gdouble)gdk_pixbuf_get_height(image_pixbuf) - size;
    gdouble xval = 0.0f;
    gdouble yval = 0.0f;

    if (c_x > 0.0f)
      xval = 2.0f * (event->x - (gdouble)priv->x) / c_x;

    if (c_y > 0.0f)
      yval = 2.0f * (event->y - (gdouble)priv->y) / c_y;

    gtk_adjustment_set_value(xadj, xval + xadj->value);
    gtk_adjustment_set_value(yadj, yval + yadj->value);
  }

  priv->x = event->x;
  priv->y = event->y;
  gdk_event_request_motions(event);

  return TRUE;
}

static void
osso_abook_avatar_editor_class_init(OssoABookAvatarEditorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  widget_class->button_press_event =
      osso_abook_avatar_editor_button_press_event;
  widget_class->motion_notify_event =
      osso_abook_avatar_editor_motion_notify_event;
}

static void
osso_abook_avatar_editor_init(OssoABookAvatarEditor *editor)
{
  gtk_container_add(GTK_CONTAINER(editor), osso_abook_avatar_image_new());
  gtk_widget_set_events(GTK_WIDGET(editor), GDK_POINTER_MOTION_HINT_MASK |
                        GDK_BUTTON1_MOTION_MASK | GDK_BUTTON_PRESS_MASK);
}

GtkWidget *
osso_abook_avatar_editor_new()
{
  return g_object_new(OSSO_ABOOK_TYPE_AVATAR_EDITOR, NULL);
}

OssoABookAvatarImage *
osso_abook_avatar_editor_get_image(OssoABookAvatarEditor *editor)
{
  return OSSO_ABOOK_AVATAR_IMAGE(gtk_bin_get_child(GTK_BIN(editor)));
}
