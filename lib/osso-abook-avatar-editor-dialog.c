/*
 * osso-abook-avatar-editor-dialog.c
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

#include <math.h>
#include <libintl.h>

#include "config.h"

#include "osso-abook-avatar-editor-dialog.h"
#include "osso-abook-avatar-editor.h"
#include "osso-abook-avatar-image.h"
#include "osso-abook-icon-sizes.h"
#include "osso-abook-util.h"

struct _OssoABookAvatarEditorDialogPrivate
{
  GtkWidget *avatar_editor;
  GtkWidget *vscale;
  int zoom_steps;
};

typedef struct _OssoABookAvatarEditorDialogPrivate OssoABookAvatarEditorDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookAvatarEditorDialog,
  osso_abook_avatar_editor_dialog,
  GTK_TYPE_DIALOG
);

#define PRIVATE(dialog) \
  ((OssoABookAvatarEditorDialogPrivate *)osso_abook_avatar_editor_dialog_get_instance_private(((OssoABookAvatarEditorDialog *)dialog)))

enum
{
  PROP_PIXBUF = 1,
  PROP_AVATAR_SIZE,
  PROP_MAXIMUM_ZOOM,
  PROP_ZOOM_STEPS,
};

static void
change_adjustment(GtkAdjustment *adjustment, gpointer user_data)
{
  gdouble step = (adjustment->upper - adjustment->lower) /
      (gdouble)PRIVATE(user_data)->zoom_steps;

  if (fabs(step - adjustment->step_increment) > step / 1000.0f)
  {
    adjustment->step_increment = step;
    adjustment->page_increment = step * 10.0f;
    gtk_adjustment_changed(adjustment);
  }
}

static OssoABookAvatarImage *
get_avatar_image(OssoABookAvatarEditorDialogPrivate *priv)
{
  return osso_abook_avatar_editor_get_image(
        OSSO_ABOOK_AVATAR_EDITOR(priv->avatar_editor));
}

static GtkAdjustment *
get_xadjustment(OssoABookAvatarEditorDialogPrivate *priv)
{
  return osso_abook_avatar_image_get_xadjustment(get_avatar_image(priv));
}

static GtkAdjustment *
get_yadjustment(OssoABookAvatarEditorDialogPrivate *priv)
{
  return osso_abook_avatar_image_get_yadjustment(get_avatar_image(priv));
}

static GtkAdjustment *
get_zadjustment(OssoABookAvatarEditorDialogPrivate *priv)
{
  return osso_abook_avatar_image_get_zadjustment(get_avatar_image(priv));
}

static GdkPixbuf *
get_pixbuf(OssoABookAvatarEditorDialogPrivate *priv)
{
  return osso_abook_avatar_image_get_pixbuf(get_avatar_image(priv));
}

static int
get_size(OssoABookAvatarEditorDialogPrivate *priv)
{
  return osso_abook_avatar_image_get_size(get_avatar_image(priv));
}

static double
get_maximum_zoom(OssoABookAvatarEditorDialogPrivate *priv)
{
  return osso_abook_avatar_image_get_maximum_zoom(get_avatar_image(priv));
}

static void
adjustment_set_value(GtkAdjustment *adjustment, double steps)
{
  gtk_adjustment_set_value(
        adjustment, adjustment->value + steps * adjustment->step_increment);
}

static void
osso_abook_avatar_editor_dialog_set_property(GObject *object, guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec)
{
  OssoABookAvatarEditorDialogPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_PIXBUF:
    {
      osso_abook_avatar_image_set_pixbuf(get_avatar_image(priv),
                                         g_value_get_object(value));
      break;
    }
    case PROP_AVATAR_SIZE:
    {
      osso_abook_avatar_image_set_size(get_avatar_image(priv),
                                       g_value_get_int(value));
      break;
    }
    case PROP_MAXIMUM_ZOOM:
    {
      osso_abook_avatar_image_set_maximum_zoom(get_avatar_image(priv),
                                               g_value_get_double(value));
      break;
    }
    case PROP_ZOOM_STEPS:
    {
      priv->zoom_steps = g_value_get_int(value);
      change_adjustment(get_xadjustment(priv), object);
      change_adjustment(get_yadjustment(priv), object);
      change_adjustment(get_zadjustment(priv), object);
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
osso_abook_avatar_editor_dialog_get_property(GObject *object, guint property_id,
                                             GValue *value, GParamSpec *pspec)
{
  OssoABookAvatarEditorDialogPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_PIXBUF:
    {
      g_value_set_object(value, get_pixbuf(priv));
      break;
    }
    case PROP_AVATAR_SIZE:
    {
      g_value_set_int(value, get_size(priv));
      break;
    }
    case PROP_MAXIMUM_ZOOM:
    {
      g_value_set_double(value, get_maximum_zoom(priv));
      break;
    }
    case PROP_ZOOM_STEPS:
    {
      g_value_set_int(value, priv->zoom_steps);
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
osso_abook_avatar_editor_dialog_class_init(
    OssoABookAvatarEditorDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_avatar_editor_dialog_set_property;
  object_class->get_property = osso_abook_avatar_editor_dialog_get_property;

  g_object_class_install_property(
        object_class, PROP_PIXBUF,
        g_param_spec_object(
          "pixbuf", "Pixbuf", "The image to edit",
          GDK_TYPE_PIXBUF,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_AVATAR_SIZE,
        g_param_spec_int(
          "avatar-size", "Avatar Size", "The size of the avatar image",
          1, G_MAXINT, OSSO_ABOOK_PIXEL_SIZE_AVATAR_LARGE,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_ZOOM_STEPS,
        g_param_spec_int(
          "zoom-steps", "Zoom Steps", "The number of zooming steps",
          1, G_MAXINT, 20,
          GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_MAXIMUM_ZOOM,
        g_param_spec_double(
          "maximum-zoom", "Maximum Zoom", "The maximum allowed zoom level",
          1.0f, G_MAXDOUBLE, 2.0f,
          GTK_PARAM_READWRITE));
}

static void
_zoom_changed_cb(OssoABookAvatarImage *image, GParamSpec *arg1,
                 gpointer user_data)
{
  OssoABookAvatarEditorDialogPrivate *priv = PRIVATE(user_data);

  if (osso_abook_avatar_image_get_maximum_zoom(image) -
      osso_abook_avatar_image_get_minimum_zoom(image) <= 0.0f)
  {
    gtk_widget_hide(priv->vscale);
  }
  else
    gtk_widget_show(priv->vscale);
}

static gboolean
_key_press_event_cb(GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
  OssoABookAvatarEditorDialogPrivate *priv = PRIVATE(widget);

  switch (event->key.keyval)
  {
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
    {
      gtk_widget_grab_focus(priv->vscale);
      break;
    }
    case GDK_KEY_Left:
    {
      adjustment_set_value(get_xadjustment(priv), -1.0);
      break;
    }
    case GDK_KEY_Right:
    {
      adjustment_set_value(get_xadjustment(priv), 1.0);
      break;
    }
    case GDK_KEY_Up:
    {
      adjustment_set_value(get_yadjustment(priv), -1.0);
      break;
    }
    case GDK_KEY_Down:
    {
      adjustment_set_value(get_yadjustment(priv), 1.0);
      break;
    }
    case GDK_KEY_F7:
    {
      adjustment_set_value(get_zadjustment(priv), 1.0);
      break;
    }
    case GDK_KEY_F8:
    {
      adjustment_set_value(get_zadjustment(priv), -1.0);
      break;
    }
    default:
      return FALSE;
  }

  return TRUE;
}

static void
osso_abook_avatar_editor_dialog_init(OssoABookAvatarEditorDialog *dialog)
{
  OssoABookAvatarEditorDialogPrivate *priv = PRIVATE(dialog);
  GtkAdjustment *xadj;
  GtkAdjustment *yadj;
  GtkAdjustment *zadj;
  GtkWidget *hbox;

  priv->zoom_steps = 20;
  priv->avatar_editor = osso_abook_avatar_editor_new();

  xadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -1.0, 1.0, 0.0, 0.0, 0.0));
  yadj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0, -1.0, 1.0, 0.0, 0.0, 0.0));
  zadj = get_zadjustment(priv);
  g_signal_connect(xadj, "changed", G_CALLBACK(change_adjustment), dialog);
  g_signal_connect(yadj, "changed", G_CALLBACK(change_adjustment), dialog);
  g_signal_connect(zadj, "changed", G_CALLBACK(change_adjustment), dialog);
  change_adjustment(xadj, dialog);
  change_adjustment(yadj, dialog);
  change_adjustment(zadj, dialog);
  osso_abook_avatar_image_set_xadjustment(get_avatar_image(priv), xadj);
  osso_abook_avatar_image_set_yadjustment(get_avatar_image(priv), yadj);

  priv->vscale = hildon_gtk_vscale_new();

  gtk_range_set_adjustment(GTK_RANGE(priv->vscale), zadj);
  gtk_widget_set_no_show_all(priv->vscale, TRUE);
  g_signal_connect(get_avatar_image(priv), "notify::minimum-zoom",
                   G_CALLBACK(_zoom_changed_cb), dialog);
  g_signal_connect(get_avatar_image(priv), "notify::maximum-zoom",
                   G_CALLBACK(_zoom_changed_cb), dialog);
  _zoom_changed_cb(get_avatar_image(priv), NULL, dialog);

  hbox = gtk_hbox_new(TRUE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);
  gtk_box_pack_start(GTK_BOX(hbox), priv->avatar_editor, TRUE, TRUE, 0);
  gtk_box_pack_start(GTK_BOX(hbox), priv->vscale, FALSE, TRUE, 0);
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), hbox, TRUE, TRUE, 0);
  gtk_dialog_add_button(GTK_DIALOG(dialog),
                        dgettext("hildon-libs", "wdgt_bd_save"),
                        GTK_RESPONSE_OK);
  osso_abook_attach_screen_size_handler(GTK_WINDOW(dialog));
  osso_abook_set_zoom_key_used(GTK_WINDOW(dialog), TRUE);
  g_signal_connect(dialog, "key-press-event",
                   G_CALLBACK(_key_press_event_cb), NULL);
}

GtkWidget *
osso_abook_avatar_editor_dialog_new(GtkWindow *parent, GdkPixbuf *pixbuf)
{
  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), NULL);
  g_return_val_if_fail(GDK_IS_PIXBUF(pixbuf), NULL);

  return g_object_new(
        OSSO_ABOOK_TYPE_AVATAR_EDITOR_DIALOG,
        "title", g_dgettext("osso-addressbook", "addr_ti_crop_avatar_title"),
        "modal", TRUE,
        "transient-for", parent,
        "destroy-with-parent", TRUE,
        "has-separator", FALSE,
        "pixbuf", pixbuf,
        NULL);
}

void
osso_abook_avatar_editor_dialog_set_pixbuf(OssoABookAvatarEditorDialog *dialog,
                                           GdkPixbuf *pixbuf)
{
  g_return_if_fail(GDK_IS_PIXBUF(pixbuf));
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(dialog));

  g_object_set(dialog, "pixbuf", pixbuf, NULL);
}

GdkPixbuf *
osso_abook_avatar_editor_dialog_get_pixbuf(OssoABookAvatarEditorDialog *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(dialog), NULL);

  return get_pixbuf(PRIVATE(dialog));
}

GdkPixbuf *
osso_abook_avatar_editor_dialog_get_scaled_pixbuf(
    OssoABookAvatarEditorDialog *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(dialog), NULL);

  return osso_abook_avatar_image_get_scaled_pixbuf(
        get_avatar_image(PRIVATE(dialog)));
}

void
osso_abook_avatar_editor_dialog_set_avatar_size(
    OssoABookAvatarEditorDialog *dialog, int size)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(dialog));

  g_object_set(dialog, "avatar-size", size, NULL);
}

int
osso_abook_avatar_editor_dialog_get_avatar_size(OssoABookAvatarEditorDialog *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(dialog),
                       OSSO_ABOOK_PIXEL_SIZE_AVATAR_LARGE);

  return get_size(PRIVATE(dialog));
}

void
osso_abook_avatar_editor_dialog_set_zoom_steps(
    OssoABookAvatarEditorDialog *dialog, int steps)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(dialog));

  g_object_set(dialog, "zoom-steps", steps, NULL);
}

int
osso_abook_avatar_editor_dialog_get_zoom_steps(
    OssoABookAvatarEditorDialog *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(dialog), 20);

  return PRIVATE(dialog)->zoom_steps;
}

void
osso_abook_avatar_editor_dialog_set_maximum_zoom(
    OssoABookAvatarEditorDialog *dialog, double zoom)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(dialog));

  g_object_set(dialog, "maximum-zoom", zoom, NULL);
}

double
osso_abook_avatar_editor_dialog_get_maximum_zoom(
    OssoABookAvatarEditorDialog *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(dialog), 2.0f);

  return get_maximum_zoom(PRIVATE(dialog));
}
