/*
 * osso-abook-avatar-chooser-dialog.c
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
#include <hildon/hildon-file-chooser-dialog.h>
#include <hildon/hildon-file-selection.h>

#include "config.h"

#include "osso-abook-avatar-chooser-dialog.h"
#include "osso-abook-contact.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-icon-sizes.h"
#include "osso-abook-util.h"
#include "osso-abook-errors.h"

struct _OssoABookAvatarChooserDialogPrivate
{
  GdkPixbuf *pixbuf;
  gchar *filename;
  gchar *icon_name;
  GtkWidget *icon_view;
  GtkCellRenderer *cell_renderer;
  OssoABookContact *contact;
};

typedef struct _OssoABookAvatarChooserDialogPrivate OssoABookAvatarChooserDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookAvatarChooserDialog,
  osso_abook_avatar_chooser_dialog,
  GTK_TYPE_DIALOG
);

#define PRIVATE(dialog) \
  ((OssoABookAvatarChooserDialogPrivate *)osso_abook_avatar_chooser_dialog_get_instance_private(((OssoABookAvatarChooserDialog *)dialog)))

enum
{
  PROP_PIXBUF = 1,
  PROP_FILENAME,
  PROP_ICON_NAME,
  PROP_CONTACT
};

enum
{
  COLUMN_ICON_NAME,
  COLUMN_ICON,
  COLUMN_IMAGE,
};

struct icon_load_data
{
  GdkPixbuf *pixbuf;
  GMainLoop *mainloop;
  OssoABookAvatarChooserDialog *dialog;
  GtkWidget *progress_bar;
  GtkWidget *note;
  guint progress_timeout;
  GCancellable *cancellable;
};

static void
_avatar_image_changed_cb(OssoABookAvatarChooserDialog *dialog)
{
  OssoABookAvatarChooserDialogPrivate *priv = PRIVATE(dialog);
  GtkTreeModel *tree_model;
  GtkTreeIter iter;

  tree_model = gtk_icon_view_get_model(GTK_ICON_VIEW(priv->icon_view));

  if (gtk_tree_model_get_iter_first(tree_model, &iter))
  {
    if (priv->contact)
    {
      GdkPixbuf *image = osso_abook_avatar_get_server_image_scaled(
            OSSO_ABOOK_AVATAR(priv->contact), 106, 106, 1);

      gtk_list_store_set(GTK_LIST_STORE(tree_model), &iter,
                         COLUMN_IMAGE, image,
                         -1);
      if (image)
        g_object_unref(image);
    }
    else
    {
      gtk_list_store_set(GTK_LIST_STORE(tree_model), &iter,
                         COLUMN_IMAGE, NULL,
                         -1);
    }
  }
}

static void
osso_abook_avatar_chooser_dialog_set_property(GObject *object,
                                              guint property_id,
                                              const GValue *value,
                                              GParamSpec *pspec)
{
  OssoABookAvatarChooserDialog *dialog =
      OSSO_ABOOK_AVATAR_CHOOSER_DIALOG(object);
  OssoABookAvatarChooserDialogPrivate *priv = PRIVATE(dialog);

  switch (property_id)
  {
    case PROP_CONTACT:
    {
      OssoABookContact *contact= g_value_get_object(value);

      if (contact)
        g_object_ref(contact);

      if (priv->contact)
      {
        g_signal_handlers_disconnect_matched(
              priv->contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
              0, 0, NULL, _avatar_image_changed_cb, dialog);
        g_object_unref(priv->contact);
      }

      priv->contact = contact;

      if (contact)
      {
        g_signal_connect_swapped(contact, "notify::avatar-image",
                                 G_CALLBACK(_avatar_image_changed_cb), dialog);
      }

      _avatar_image_changed_cb(dialog);
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
osso_abook_avatar_chooser_dialog_get_property(GObject *object,
                                              guint property_id,
                                              GValue *value,
                                              GParamSpec *pspec)
{
  OssoABookAvatarChooserDialogPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_PIXBUF:
    {
      g_value_set_object(value, priv->pixbuf);
      break;
    }
    case PROP_FILENAME:
    {
      g_value_set_string(value, priv->filename);
      break;
    }
    case PROP_ICON_NAME:
    {
      g_value_set_string(value, priv->icon_name);
      break;
    }
    case PROP_CONTACT:
    {
      g_value_set_object(value, priv->contact);
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
osso_abook_avatar_chooser_dialog_finalize(GObject *object)
{
  OssoABookAvatarChooserDialogPrivate *priv = PRIVATE(object);

  g_free(priv->filename);
  g_free(priv->icon_name);

  G_OBJECT_CLASS(osso_abook_avatar_chooser_dialog_parent_class)->
      finalize(object);
}

static void
dispose(GObject *object)
{
  OssoABookAvatarChooserDialogPrivate *priv = PRIVATE(object);

  if (priv->contact)
  {
    g_signal_handlers_disconnect_matched(
          priv->contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
          _avatar_image_changed_cb, object);
    g_object_unref(priv->contact);
    priv->contact = NULL;
  }

  if (priv->pixbuf)
  {
    g_object_unref(priv->pixbuf);
    priv->pixbuf = NULL;
  }

  G_OBJECT_CLASS(osso_abook_avatar_chooser_dialog_parent_class)->
      dispose(object);
}

static void
osso_abook_avatar_chooser_dialog_style_set(GtkWidget *widget,
                                           GtkStyle *previous_style)
{
  GtkWidget *icon_view = PRIVATE(widget)->icon_view;
  GtkTreeModel *model;
  GtkTreeIter iter;

  GTK_WIDGET_CLASS(osso_abook_avatar_chooser_dialog_parent_class)->
      style_set(widget, previous_style);

  if (!icon_view)
    return;

  model = gtk_icon_view_get_model(GTK_ICON_VIEW(icon_view));

  if (!model)
    return;

  if (gtk_tree_model_get_iter_first(model, &iter))
  {
    do
    {
      gchar *icon_name;

      gtk_tree_model_get(model, &iter, COLUMN_ICON_NAME, &icon_name, -1);

      if (icon_name)
      {
        GdkPixbuf *icon = _osso_abook_get_cached_icon(widget, icon_name, 106);

        gtk_list_store_set(GTK_LIST_STORE(model), &iter, COLUMN_ICON, icon, -1);

        if (icon)
          g_object_unref(icon);

        g_free(icon_name);
      }
    }
    while (gtk_tree_model_iter_next(model, &iter));
  }
}

static void
osso_abook_avatar_chooser_dialog_class_init(
    OssoABookAvatarChooserDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->set_property = osso_abook_avatar_chooser_dialog_set_property;
  object_class->get_property = osso_abook_avatar_chooser_dialog_get_property;
  object_class->finalize = osso_abook_avatar_chooser_dialog_finalize;
  object_class->dispose = dispose;

  widget_class->style_set = osso_abook_avatar_chooser_dialog_style_set;

  g_object_class_install_property(
        object_class, PROP_PIXBUF,
        g_param_spec_object(
          "pixbuf", "Pixbuf", "The GdkPixbuf of the selected avatar",
          GDK_TYPE_PIXBUF,
          GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_FILENAME,
        g_param_spec_string(
          "filename", "Filename", "The filename of the selected avatar",
          NULL,
          GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_ICON_NAME,
        g_param_spec_string(
          "icon-name", "Icon Name", "The icon name of the selected avatar",
          NULL,
          GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_CONTACT,
        g_param_spec_object(
          "contact", "Contact",
          "Contact object providing the default avatar image",
          OSSO_ABOOK_TYPE_CONTACT,
          GTK_PARAM_READWRITE));
}

static void
_cell_renderer_data_func(GtkCellLayout *cell_layout, GtkCellRenderer *cell,
                         GtkTreeModel *tree_model, GtkTreeIter *iter,
                         gpointer data)
{
  gpointer icon_name = NULL;
  gpointer pixbuf = NULL;

  gtk_tree_model_get(tree_model, iter, COLUMN_IMAGE, &pixbuf, -1);

  if (!pixbuf)
    gtk_tree_model_get(tree_model, iter, COLUMN_ICON, &pixbuf, -1);

  if (!pixbuf)
    gtk_tree_model_get(tree_model, iter, COLUMN_ICON_NAME, &icon_name, -1);

  if (pixbuf)
  {
    g_object_set(cell, "pixbuf", pixbuf, NULL);
    g_object_unref(pixbuf);
  }
  else
    g_object_set(cell, "icon-name", icon_name, NULL);

  g_free(icon_name);
}

static void
set_file_and_pixbuf(OssoABookAvatarChooserDialog *dialog, GFile *file,
                    GdkPixbuf *pixbuf)
{
  OssoABookAvatarChooserDialogPrivate *priv = PRIVATE(dialog);

  if (priv->pixbuf)
  {
    g_object_unref(pixbuf);
    priv->pixbuf = NULL;
  }

  g_free(priv->filename);
  priv->filename = NULL;

  if (pixbuf)
    priv->pixbuf = gdk_pixbuf_apply_embedded_orientation(pixbuf);

  if (file)
    priv->filename = g_file_get_path(file);

  g_object_notify(G_OBJECT(dialog), "filename");
  g_object_notify(G_OBJECT(dialog), "pixbuf");
  gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
}

static gboolean
_icon_load_timeout_cb(gpointer user_data)
{
  struct icon_load_data *data = user_data;

  if (data->progress_bar)
    gtk_progress_bar_pulse(GTK_PROGRESS_BAR(data->progress_bar));
  else
  {
    data->progress_bar = gtk_progress_bar_new();
    data->note = hildon_note_new_cancel_with_progress_bar(
          GTK_WINDOW(data->dialog), "", GTK_PROGRESS_BAR(data->progress_bar));
    data->note = data->note;
    g_signal_connect_swapped(data->note, "response",
                             G_CALLBACK(g_cancellable_cancel),
                             data->cancellable);
    gtk_widget_show(data->note);
  }

  return TRUE;
}

static void
_icon_load_ready_cb(GObject *source_object, GAsyncResult *res,
                    gpointer user_data)
{
  struct icon_load_data *data = user_data;
  GError *error = NULL;

  data->pixbuf = osso_abook_load_pixbuf_finish(G_FILE(source_object),
                                               res, &error);

  if (!data->pixbuf && error)
    osso_abook_handle_gerror(NULL, error);

  g_main_loop_quit(data->mainloop);
}

static gboolean
_load_icon(OssoABookAvatarChooserDialog *dialog, GFile *file)
{
  struct icon_load_data data = {};

  data.dialog = dialog;
  data.mainloop = g_main_loop_new(0, TRUE);
  data.progress_timeout =
      gdk_threads_add_timeout(1000, _icon_load_timeout_cb, &data);
  data.cancellable = g_cancellable_new();
  osso_abook_load_pixbuf_async(file, 6 * 1024 * 1024, 0, data.cancellable,
                               _icon_load_ready_cb, &data);

  GDK_THREADS_LEAVE();
  g_main_loop_run(data.mainloop);
  GDK_THREADS_ENTER();

  if (data.pixbuf)
  {
    set_file_and_pixbuf(dialog, file, data.pixbuf);
    g_object_unref(data.pixbuf);
  }

  if (data.note)
    gtk_widget_destroy(data.note);

  if (data.progress_timeout)
    g_source_remove(data.progress_timeout);

  g_object_unref(data.cancellable);
  g_main_loop_unref(data.mainloop);

  return !!data.pixbuf;
}

static void
load_stock_icon(OssoABookAvatarChooserDialog *dialog, const char *icon_name)
{
  GtkIconTheme *icon_theme = gtk_icon_theme_get_for_screen(
        gtk_widget_get_screen(GTK_WIDGET(dialog)));
  GtkIconInfo *icon = gtk_icon_theme_lookup_icon(
        icon_theme, icon_name, OSSO_ABOOK_PIXEL_SIZE_AVATAR_LARGE, 0);
  const char *filename;
  GFile *file;

  g_return_if_fail(NULL != icon);

  filename = gtk_icon_info_get_filename(icon);

  g_return_if_fail(NULL != filename);

  file = g_file_new_for_path(filename);
  _load_icon(dialog, file);
  gtk_icon_info_free(icon);
  g_object_unref(file);
}

static void
_item_activated_cb(GtkIconView *iconview, GtkTreePath *path, gpointer user_data)
{
  OssoABookAvatarChooserDialog *dialog = user_data;
  OssoABookAvatarChooserDialogPrivate *priv = PRIVATE(dialog);
  GtkTreeModel *model = gtk_icon_view_get_model(iconview);
  GtkTreeIter iter;

  if (!gtk_tree_model_get_iter(model, &iter, path))
    return;

  g_object_ref(dialog);
  g_object_freeze_notify(G_OBJECT(dialog));
  g_free(priv->icon_name);
  gtk_tree_model_get(model, &iter, COLUMN_ICON_NAME, &priv->icon_name, -1);
  g_object_notify(G_OBJECT(dialog), "icon-name");

  if (g_strcmp0(priv->icon_name, "general_default_avatar"))
    load_stock_icon(dialog, priv->icon_name);
  else
    set_file_and_pixbuf(dialog, NULL, NULL);

  g_object_thaw_notify(G_OBJECT(dialog));
  g_object_unref(dialog);
}

static void
_size_allocate_cb(GtkWidget *widget, GdkRectangle *allocation,
                  gpointer user_data)
{
  OssoABookAvatarChooserDialogPrivate *priv = PRIVATE(user_data);
  gint margin = gtk_icon_view_get_margin(GTK_ICON_VIEW(widget));
  int width = (allocation->width - 530 - 2 * margin) / 5;

  if (width >= 0)
  {
    guint pad = width / 2;
    gint spacing = width % 2;

    gtk_icon_view_set_row_spacing(GTK_ICON_VIEW(widget), spacing);
    gtk_icon_view_set_column_spacing(GTK_ICON_VIEW(widget), spacing);
    g_object_set(priv->cell_renderer, "xpad", pad, "ypad", pad, NULL);
  }
}

static GtkWidget *
find_child_type(GtkWidget *container, GType type)
{
  GList *child;
  GList *children;
  GtkWidget *widget = NULL;

  if (G_TYPE_FROM_INSTANCE(container) == type)
    return container;

  if (GTK_IS_CONTAINER(container))
    return NULL;

  children = gtk_container_get_children(GTK_CONTAINER(container));

  for (child = children; child; child = child->next)
  {
    widget = find_child_type(child->data, type);

    if (widget)
      break;
  }

  g_list_free(children);

  return widget;
}

static void
_response_cb(OssoABookAvatarChooserDialog *dialog, gint response_id,
             gpointer user_data)
{
  GtkWidget *file_chooser;
  const char *picture_folder;
  GtkWidget *file_selection;
  GtkFileFilter *filter;
  gboolean icon_loaded;

  if (response_id != 1)
    return;

  g_signal_stop_emission_by_name(dialog, "response");

  file_chooser = hildon_file_chooser_dialog_new_with_properties(
        gtk_window_get_transient_for(GTK_WINDOW(dialog)),
        "action", GTK_FILE_CHOOSER_ACTION_OPEN,
        "title", g_dgettext("osso-addressbook", "addr_ti_select_picture_title"),
        "empty-text", g_dgettext("osso-addressbook",
                                 "addr_li_select_picture_none"),
        "destroy-with-parent", TRUE,
        NULL);
  picture_folder = osso_abook_settings_get_picture_folder();

  if (picture_folder)
  {
    gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(file_chooser),
                                        picture_folder);
  }

  file_selection = find_child_type(file_chooser, HILDON_TYPE_FILE_SELECTION);

  if (file_selection)
  {
    hildon_file_selection_set_sort_key(HILDON_FILE_SELECTION(file_selection),
                                       HILDON_FILE_SELECTION_SORT_MODIFIED,
                                       GTK_SORT_DESCENDING);
  }

  filter = gtk_file_filter_new();
  gtk_file_filter_add_pixbuf_formats(filter);
  gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(file_chooser), filter);
  gtk_file_chooser_set_filter(GTK_FILE_CHOOSER(file_chooser), filter);
  g_object_unref(filter);

  do
  {
    GFile *file;
    gchar *uri;

    if (gtk_dialog_run(GTK_DIALOG(file_chooser)) != GTK_RESPONSE_OK)
      break;

    uri = gtk_file_chooser_get_uri(GTK_FILE_CHOOSER(file_chooser));

    if (!uri)
      break;

    file = g_file_new_for_uri(uri);
    icon_loaded = _load_icon(dialog, file);
    g_object_unref(file);
    g_free(uri);
  }
  while (!icon_loaded);

  gtk_widget_destroy(file_chooser);
}

static void
osso_abook_avatar_chooser_dialog_init(OssoABookAvatarChooserDialog *dialog)
{
  OssoABookAvatarChooserDialogPrivate *priv = PRIVATE(dialog);
  GType types[] = {G_TYPE_STRING, GDK_TYPE_PIXBUF, GDK_TYPE_PIXBUF};
  GtkWidget *area;
  GtkIconTheme *icon_theme;
  GtkListStore *store;
  GtkTreeIter iter;

  int i = 1;

  gtk_dialog_add_button(GTK_DIALOG(dialog),
                        g_dgettext("osso-addressbook", "addr_bd_browse"), TRUE);
  icon_theme = gtk_icon_theme_get_for_screen(
        gtk_widget_get_screen(GTK_WIDGET(dialog)));

  store = gtk_list_store_newv(G_N_ELEMENTS(types), types);

  gtk_list_store_insert_with_values(store, &iter, 0,
                                    COLUMN_ICON_NAME, "general_default_avatar",
                                    -1);

  while (1)
  {
    gchar *icon = g_strdup_printf("general_avatar%d", i);

    if (!gtk_icon_theme_has_icon(icon_theme, icon))
    {
      g_free(icon);
      break;
    }

    gtk_list_store_insert_with_values(store, &iter, i++,
                                      COLUMN_ICON_NAME, icon,
                                      -1);
    g_free(icon);
  }

  priv->icon_view = hildon_gtk_icon_view_new_with_model(HILDON_UI_MODE_NORMAL,
                                                        GTK_TREE_MODEL(store));

  priv->cell_renderer = gtk_cell_renderer_pixbuf_new();
  g_object_set(priv->cell_renderer,
               "stock-size", OSSO_ABOOK_ICON_SIZE_AVATAR_CHOOSER,
               NULL);
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(priv->icon_view),
                             priv->cell_renderer, TRUE);
  gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(priv->icon_view),
                                     priv->cell_renderer,
                                     _cell_renderer_data_func, NULL, NULL);

  g_signal_connect(priv->icon_view, "item-activated",
                   G_CALLBACK(_item_activated_cb), dialog);
  g_signal_connect(priv->icon_view, "size-allocate",
                   G_CALLBACK(_size_allocate_cb), dialog);
  area = osso_abook_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(area), priv->icon_view);
  gtk_widget_show_all(area);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), area, TRUE, TRUE, 0);
  g_signal_connect(dialog, "response", G_CALLBACK(_response_cb), NULL);
}

GtkWidget *
osso_abook_avatar_chooser_dialog_new(GtkWindow *parent)
{
  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), NULL);

  return g_object_new(
        OSSO_ABOOK_TYPE_AVATAR_CHOOSER_DIALOG,
        "title", g_dgettext("osso-addressbook", "addr_ti_select_avatar"),
        "transient-for", parent,
        "destroy-with-parent", TRUE,
        "modal", TRUE,
        "has-separator", FALSE,
        NULL);
}

GdkPixbuf *
osso_abook_avatar_chooser_dialog_get_pixbuf(
    OssoABookAvatarChooserDialog *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_CHOOSER_DIALOG(dialog), NULL);

  return PRIVATE(dialog)->pixbuf;
}

const char *
osso_abook_avatar_chooser_dialog_get_filename(
    OssoABookAvatarChooserDialog *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_CHOOSER_DIALOG(dialog), NULL);

  return PRIVATE(dialog)->filename;
}

const char *
osso_abook_avatar_chooser_dialog_get_icon_name(
    OssoABookAvatarChooserDialog *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_CHOOSER_DIALOG(dialog), NULL);

  return PRIVATE(dialog)->icon_name;
}

void
osso_abook_avatar_chooser_dialog_set_contact(
    OssoABookAvatarChooserDialog *dialog, OssoABookContact *contact)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_CHOOSER_DIALOG(dialog));
  g_return_if_fail(!contact || OSSO_ABOOK_IS_CONTACT(contact));

  g_object_set(dialog, "contact", contact, NULL);
}

OssoABookContact *
osso_abook_avatar_chooser_dialog_get_contact(
    OssoABookAvatarChooserDialog *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_CHOOSER_DIALOG(dialog), NULL);

    return PRIVATE(dialog)->contact;
}
