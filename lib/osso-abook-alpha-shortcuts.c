/*
 * osso-abook-alpha-shortcuts.c
 *
 * Copyright (C) 2022 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
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

#include "osso-abook-alpha-shortcuts.h"
#include "osso-abook-contact-view.h"
#include "osso-abook-util.h"

enum
{
  COLUMN_LABEL,
  COLUMN_NUM_LETTERS,
  COLUMN_INDEX,
  N_COLUMNS
};

#define N_LETTER_GROUPS 9

struct _OssoABookAlphaShortcutsPrivate
{
  OssoABookContactView *contact_view;
  GtkWidget *pannable_area;
  GtkListStore *list_store;
  GtkWidget *tree_view;
  GtkCellRenderer *cell_renderer;
  GtkTreeViewColumn *column;
  int current_row;
  int tap_count;
};

typedef struct _OssoABookAlphaShortcutsPrivate OssoABookAlphaShortcutsPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookAlphaShortcuts,
  osso_abook_alpha_shortcuts,
  GTK_TYPE_VBOX
);

#define PRIVATE(shortcuts) \
  ((OssoABookAlphaShortcutsPrivate *) \
   osso_abook_alpha_shortcuts_get_instance_private(shortcuts))

static void
osso_abook_alpha_shortcuts_dispose(GObject *object)
{
  osso_abook_alpha_shortcuts_widget_unhook(
    OSSO_ABOOK_ALPHA_SHORTCUTS(object));

  G_OBJECT_CLASS(osso_abook_alpha_shortcuts_parent_class)->dispose(object);
}

static void
osso_abook_alpha_shortcuts_class_init(OssoABookAlphaShortcutsClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = osso_abook_alpha_shortcuts_dispose;
}

static void
style_set_cb(GtkWidget *widget, GtkStyle *previous_style, gpointer user_data)
{
  OssoABookAlphaShortcutsPrivate *priv = PRIVATE(user_data);
  gint spacing;
  gint height;
  int focus_line_w;
  int h_sep;

  gtk_widget_style_get(widget,
                       "horizontal-separator", &h_sep,
                       "focus-line-width", &focus_line_w,
                       NULL);
  spacing = 2 * focus_line_w + h_sep;

  gtk_icon_size_lookup_for_settings(gtk_widget_get_settings(GTK_WIDGET(widget)),
                                    HILDON_ICON_SIZE_THUMB, NULL, &height);
  gtk_cell_renderer_set_fixed_size(priv->cell_renderer, -1, height);
  gtk_tree_view_column_set_spacing(priv->column, spacing);
}

static void
jump_after_last(GtkTreeView *view, GtkWidget *are)
{
  GtkTreeModel *model = gtk_tree_view_get_model(view);
  GtkTreeIter iter;
  GtkTreeIter last;
  GtkTreePath *path;
  GdkRectangle rect;
  gint y;

  if (gtk_tree_model_get_iter_first(model, &iter))
  {
    do
    {
      last = iter;
    }
    while (gtk_tree_model_iter_next(model, &iter));
  }

  path = gtk_tree_model_get_path(model, &last);
  gtk_tree_view_get_background_area(GTK_TREE_VIEW(view), path, NULL, &rect);
  gtk_tree_view_convert_bin_window_to_tree_coords(
    GTK_TREE_VIEW(view), 0, rect.y, NULL, &y);
  hildon_pannable_area_jump_to(HILDON_PANNABLE_AREA(are), -1, y);
  gtk_tree_path_free(path);
}

static void
hildon_row_tapped_cb(GtkTreeView *tree_view, GtkTreePath *path,
                     gpointer user_data)
{
  OssoABookAlphaShortcutsPrivate *priv = PRIVATE(user_data);
  GtkTreeView *contact_tree_view;
  GtkWidget *pannable_area;
  gboolean not_first_time = FALSE;
  GtkTreeIter iter;
  gint num_letters;
  guint index;

  if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->list_store), &iter, path))
    return;

  gtk_tree_model_get(GTK_TREE_MODEL(priv->list_store), &iter,
                     COLUMN_INDEX, &index,
                     COLUMN_NUM_LETTERS, &num_letters,
                     -1);

  g_return_if_fail((index >= 0) && (index < N_LETTER_GROUPS));

  contact_tree_view = osso_abook_tree_view_get_tree_view(
      OSSO_ABOOK_TREE_VIEW(priv->contact_view));
  pannable_area = osso_abook_tree_view_get_pannable_area(
      OSSO_ABOOK_TREE_VIEW(priv->contact_view));

  if (priv->current_row == index)
  {
    priv->tap_count++;

    if (priv->tap_count > num_letters)
      priv->tap_count = 1;
  }
  else
  {
    priv->current_row = index;
    priv->tap_count = 1;
  }

  while (index < N_LETTER_GROUPS)
  {
    const gchar *c;
    gchar *msg_id_idx = g_strdup_printf("addr_li_alpha_%d%d", index, index);

    for (c = _(msg_id_idx); *c == '!'; c++)
      ;

    if (!not_first_time)
    {
      int i;

      for (i = 1; i < priv->tap_count; i++)
        c = g_utf8_next_char(c);
    }

    while (*c)
    {
      gunichar letter = g_unichar_tolower(g_utf8_get_char_validated(c, -1));
      GtkTreeModel *model = gtk_tree_view_get_model(contact_tree_view);
      GtkTreeIter contact_iter;

      if (gtk_tree_model_get_iter_first(model, &contact_iter))
      {
        do
        {
          const char *display_name;
          OssoABookContact *contact;

          gtk_tree_model_get(model, &contact_iter,
                             OSSO_ABOOK_LIST_STORE_COLUMN_CONTACT, &contact,
                             -1);
          display_name = osso_abook_contact_get_display_name(contact);

          if (display_name)
          {
            if (letter == g_unichar_tolower(g_utf8_get_char(display_name)))
            {
              GdkRectangle rect;
              gint y;
              GtkTreePath *contact_path =
                gtk_tree_model_get_path(model, &contact_iter);

              gtk_tree_view_get_background_area(
                GTK_TREE_VIEW(contact_tree_view), contact_path, NULL, &rect);

              gtk_tree_view_convert_bin_window_to_tree_coords(
                GTK_TREE_VIEW(contact_tree_view), 0, rect.y, 0, &y);
              y += pannable_area->allocation.height / 2;

              if (not_first_time)
                y -= rect.height;

              hildon_pannable_area_jump_to(
                HILDON_PANNABLE_AREA(pannable_area), -1, y);
              gtk_tree_path_free(contact_path);
              g_object_unref(contact);
              g_free(msg_id_idx);
              return;
            }
          }

          g_object_unref(contact);
        }
        while (gtk_tree_model_iter_next(model, &contact_iter));
      }

      c = g_utf8_next_char(c);
      not_first_time = TRUE;
    }

    g_free(msg_id_idx);
  }

  jump_after_last(contact_tree_view, pannable_area);
}

static void
osso_abook_alpha_shortcuts_init(OssoABookAlphaShortcuts *shortcuts)
{
  int index;
  OssoABookAlphaShortcutsPrivate *priv = PRIVATE(shortcuts);
  GtkWidget *pannable_area;

  priv->contact_view = NULL;
  priv->current_row = -1;
  priv->list_store = gtk_list_store_new(
      N_COLUMNS, G_TYPE_STRING, G_TYPE_LONG, G_TYPE_INT);

  priv->tree_view =
    hildon_gtk_tree_view_new_with_model(HILDON_UI_MODE_NORMAL,
                                        GTK_TREE_MODEL(priv->list_store));
  g_object_unref(priv->list_store);

  gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(priv->tree_view), FALSE);
  gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(priv->tree_view), TRUE);
  gtk_tree_view_set_enable_search(GTK_TREE_VIEW(priv->tree_view), FALSE);
  gtk_tree_view_set_search_column(GTK_TREE_VIEW(priv->tree_view), -1);

  priv->cell_renderer = gtk_cell_renderer_text_new();
  g_object_set(priv->cell_renderer,
               "xpad", 8,
               "xalign", 0.5,
               NULL);
  priv->column = gtk_tree_view_column_new_with_attributes(
      NULL, priv->cell_renderer, "text", NULL, NULL);
  gtk_tree_view_append_column(GTK_TREE_VIEW(priv->tree_view), priv->column);

  /* FIXME - this is really not very usable for contacts in different alphabets.
   * A better algo shall be implemented - like gathering all contact names first
   * letters and grouping them. Will do it, someday...
   */
  for (index = 0; index < N_LETTER_GROUPS; index++)
  {
    gchar *msg_id = g_strdup_printf("addr_li_alpha_%d", index);
    gchar *msg_id_idx = g_strdup_printf("addr_li_alpha_%d%d", index, index);
    GtkTreeIter iter;

    gtk_list_store_append(priv->list_store, &iter);
    gtk_list_store_set(priv->list_store, &iter,
                       COLUMN_LABEL, _(msg_id),
                       COLUMN_NUM_LETTERS, g_utf8_strlen(_(msg_id_idx), -1),
                       COLUMN_INDEX, index,
                       -1);
    g_free(msg_id_idx);
    g_free(msg_id);
  }

  pannable_area = osso_abook_pannable_area_new();
  g_object_set(pannable_area,
               "vscrollbar-policy", GTK_POLICY_NEVER,
               NULL);
  gtk_container_add(GTK_CONTAINER(pannable_area), priv->tree_view);
  gtk_box_pack_start(GTK_BOX(shortcuts), pannable_area, TRUE, TRUE, 0);
  g_signal_connect(priv->tree_view, "style-set",
                   G_CALLBACK(style_set_cb), shortcuts);
  g_signal_connect(priv->tree_view, "hildon-row-tapped",
                   G_CALLBACK(hildon_row_tapped_cb), shortcuts);
  gtk_widget_show_all(GTK_WIDGET(shortcuts));
}

static void
pannable_area_finalize(gpointer data, GObject *where_the_object_was)
{
  PRIVATE(data)->pannable_area = NULL;
}

static void
vertical_movement_cb(HildonPannableArea *hildonpannablearea, gint arg1,
                     gdouble arg2, gdouble arg3, gpointer user_data)
{
  OssoABookAlphaShortcutsPrivate *priv = PRIVATE(user_data);

  priv->current_row = -1;
  priv->tap_count = 0;
}

static void
contact_view_finalize(gpointer data, GObject *where_the_object_was)
{
  OssoABookAlphaShortcuts *shortcuts = OSSO_ABOOK_ALPHA_SHORTCUTS(data);
  OssoABookAlphaShortcutsPrivate *priv = PRIVATE(shortcuts);

  priv->contact_view = NULL;

  if (priv->pannable_area)
  {
    g_object_weak_unref(G_OBJECT(priv->pannable_area), pannable_area_finalize,
                        shortcuts);
    g_signal_handlers_disconnect_matched(
      priv->pannable_area, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, vertical_movement_cb, shortcuts);
    priv->pannable_area = NULL;
  }
}

void
osso_abook_alpha_shortcuts_widget_hook(OssoABookAlphaShortcuts *shortcuts,
                                       OssoABookContactView *contact_view)
{
  OssoABookAlphaShortcutsPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_ALPHA_SHORTCUTS(shortcuts));
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_VIEW(contact_view));

  priv = PRIVATE(shortcuts);

  g_return_if_fail(priv->contact_view == NULL);

  g_object_weak_ref(G_OBJECT(contact_view), contact_view_finalize, shortcuts);
  priv->contact_view = contact_view;
  priv->pannable_area = osso_abook_tree_view_get_pannable_area(
      OSSO_ABOOK_TREE_VIEW(contact_view));
  g_signal_connect(priv->pannable_area, "vertical-movement",
                   G_CALLBACK(vertical_movement_cb), shortcuts);
  g_object_weak_ref(G_OBJECT(priv->pannable_area), pannable_area_finalize,
                    shortcuts);
}

void
osso_abook_alpha_shortcuts_widget_unhook(OssoABookAlphaShortcuts *shortcuts)
{
  OssoABookAlphaShortcutsPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_ALPHA_SHORTCUTS(shortcuts));

  priv = PRIVATE(shortcuts);

  if (!priv->contact_view)
    return;

  if (priv->pannable_area)
  {
    g_object_weak_unref(G_OBJECT(priv->pannable_area),
                        pannable_area_finalize, shortcuts);
    g_signal_handlers_disconnect_matched(
      priv->pannable_area, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, vertical_movement_cb, shortcuts);

    priv->pannable_area = NULL;
  }

  g_object_weak_unref(G_OBJECT(priv->contact_view), contact_view_finalize,
                      shortcuts);
  priv->contact_view = NULL;
}

GtkWidget *
osso_abook_alpha_shortcuts_new()
{
  return g_object_new(OSSO_ABOOK_TYPE_ALPHA_SHORTCUTS, NULL);
}

void
osso_abook_alpha_shortcuts_show(OssoABookAlphaShortcuts *shortcuts)
{
  g_return_if_fail(OSSO_ABOOK_IS_ALPHA_SHORTCUTS(shortcuts));

  if (gtk_widget_get_mapped(GTK_WIDGET(shortcuts)))
  {
    gtk_widget_set_no_show_all(GTK_WIDGET(shortcuts), FALSE);
    gtk_widget_show_all(GTK_WIDGET(shortcuts));
  }
}

void
osso_abook_alpha_shortcuts_hide(OssoABookAlphaShortcuts *shortcuts)
{
  g_return_if_fail(OSSO_ABOOK_IS_ALPHA_SHORTCUTS(shortcuts));

  gtk_widget_hide_all(GTK_WIDGET(shortcuts));
  gtk_widget_set_no_show_all(GTK_WIDGET(shortcuts), TRUE);
}
