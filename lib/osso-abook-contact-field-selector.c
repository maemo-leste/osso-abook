/*
 * osso-abook-contact-field-selector.c
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

#include "config.h"

#include "osso-abook-contact-field-selector.h"
#include "osso-abook-debug.h"
#include "osso-abook-message-map.h"

struct _OssoABookContactFieldSelectorPrivate
{
  GtkListStore *list_store;
  GHashTable *message_map;
  gboolean show_values : 1;
};

typedef struct _OssoABookContactFieldSelectorPrivate
  OssoABookContactFieldSelectorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookContactFieldSelector,
  osso_abook_contact_field_selector,
  HILDON_TYPE_TOUCH_SELECTOR
);

#define PRIVATE(selector) \
  ((OssoABookContactFieldSelectorPrivate *) \
   osso_abook_contact_field_selector_get_instance_private( \
     ((OssoABookContactFieldSelector *)selector)))

enum
{
  PROP_SHOW_VALUES = 1
};

enum
{
  COL_TEXT,
  COL_ICON,
  COL_FIELD
};

static OssoABookMessageMapping field_map[] =
{
  { "", "addr_va_add_field" },
  { "first_name", "addr_va_add_field_first_name" },
  { "last_name", "addr_va_add_field_last_name" },
  { "nickname", "addr_va_add_field_nickname" },
  { "mobile", "addr_va_add_field_mobile" },
  { "mobile_home", "addr_va_add_field_mobile_home" },
  { "mobile_work", "addr_va_add_field_mobile_work" },
  { "phone", "addr_va_add_field_phone" },
  { "phone_home", "addr_va_add_field_phone_home" },
  { "phone_work", "addr_va_add_field_phone_work" },
  { "phone_other", "addr_va_add_field_phone" },
  { "phone_fax", "addr_va_add_field_phone_fax" },
  { "email", "addr_va_add_field_email" },
  { "email_home", "addr_va_add_field_email_home" },
  { "email_work", "addr_va_add_field_email_work" },
  { "chat_sip", "addr_va_add_field_chat_sip" },
  { "webpage", "addr_va_add_field_webpage" },
  { "birthday", "addr_va_add_field_birthday" },
  { "address", "addr_va_add_field_address" },
  { "address_home", "addr_va_add_field_address_home" },
  { "address_work", "addr_va_add_field_address_work" },
  { "title", "addr_va_add_field_title" },
  { "company", "addr_va_add_field_company" },
  { "note", "addr_va_add_field_note" },
  { "gender", "addr_va_add_field_gender" },
  { "address_detail", "" },
  { "address_home_detail", "" },
  { "address_work_detail", "" },
  { "email_detail", "" },
  { "email_home_detail", "" },
  { "email_work_detail", "" },
  { "mobile_detail", "" },
  { "mobile_home_detail", "" },
  { "mobile_work_detail", "" },
  { "phone_detail", "" },
  { "phone_home_detail", "" },
  { "phone_work_detail", "" },
  { "phone_other_detail", "" },
  { "phone_fax_detail", "" },
  { NULL, NULL }
};

static gchar *
get_field_text(OssoABookContactFieldSelectorPrivate *priv,
               OssoABookContactField *field)
{
  GHashTable *message_map = osso_abook_contact_field_get_message_map(field);
  const char *display_title;
  const char *secondary_title;
  GString *s = NULL;

  if (message_map)
    g_hash_table_ref(message_map);

  osso_abook_contact_field_set_message_map(field, priv->message_map);
  display_title = osso_abook_contact_field_get_display_title(field);
  secondary_title = osso_abook_contact_field_get_secondary_title(field);

  if (display_title)
  {
    s = g_string_new(display_title);

    if (secondary_title)
      g_string_append_printf(s, " (%s)", secondary_title);

    if (priv->show_values)
    {
      const char *display_value =
        osso_abook_contact_field_get_display_value(field);

      if (display_value)
        g_string_append_printf(s, "\n%s", display_value);
    }

    if (OSSO_ABOOK_DEBUG_FLAGS(TREE_VIEW))
    {
      g_string_append_printf(s, "\n%s, weight=%d",
                             osso_abook_contact_field_get_path(field),
                             osso_abook_contact_field_get_sort_weight(field));
    }
  }

  osso_abook_contact_field_set_message_map(field, message_map);

  if (message_map)
    g_hash_table_unref(message_map);

  if (s)
    return g_string_free(s, FALSE);

  return NULL;
}

static void
update(OssoABookContactFieldSelectorPrivate *priv)
{
  GtkTreeModel *model = GTK_TREE_MODEL(priv->list_store);
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first(model, &iter))
  {
    OssoABookContactField *field;
    gchar *text;

    do
    {
      gtk_tree_model_get(model, &iter,
                         COL_FIELD, &field,
                         -1);
      text = get_field_text(priv, field);
      gtk_list_store_set(priv->list_store, &iter,
                         COL_TEXT, text,
                         -1);
      g_object_unref(field);
      g_free(text);
    }
    while (gtk_tree_model_iter_next(model, &iter));
  }
}

static void
osso_abook_contact_field_selector_set_property(GObject *object,
                                               guint property_id,
                                               const GValue *value,
                                               GParamSpec *pspec)
{
  OssoABookContactFieldSelectorPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_SHOW_VALUES:
    {
      priv->show_values = g_value_get_boolean(value);
      update(priv);
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
osso_abook_contact_field_selector_get_property(GObject *object,
                                               guint property_id, GValue *value,
                                               GParamSpec *pspec)
{
  OssoABookContactFieldSelectorPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_SHOW_VALUES:
    {
      g_value_set_boolean(value, priv->show_values);
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
osso_abook_contact_field_selector_dispose(GObject *object)
{
  OssoABookContactFieldSelectorPrivate *priv = PRIVATE(object);

  if (priv->list_store)
  {
    g_object_unref(priv->list_store);
    priv->list_store = NULL;
  }

  if (priv->message_map)
  {
    g_hash_table_unref(priv->message_map);
    priv->message_map = NULL;
  }

  G_OBJECT_CLASS(osso_abook_contact_field_selector_parent_class)->
  dispose(object);
}

static void
osso_abook_contact_field_selector_class_init(
  OssoABookContactFieldSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_contact_field_selector_set_property;
  object_class->get_property = osso_abook_contact_field_selector_get_property;
  object_class->dispose = osso_abook_contact_field_selector_dispose;

  g_object_class_install_property(
    object_class, PROP_SHOW_VALUES,
    g_param_spec_boolean(
      "show-values", "Show Values", "Show contact field values",
      FALSE, GTK_PARAM_READWRITE));
}

static gint
compare_fields(GtkTreeModel *model, GtkTreeIter *a, GtkTreeIter *b,
               gpointer user_data)
{
  gint rv;
  OssoABookContactField *field_b;
  OssoABookContactField *field_a;

  gtk_tree_model_get(model, a, COL_FIELD, &field_a, -1);
  gtk_tree_model_get(model, b, COL_FIELD, &field_b, -1);
  rv = osso_abook_contact_field_cmp(field_a, field_b);
  g_object_unref(field_a);
  g_object_unref(field_b);

  return rv;
}

static void
osso_abook_contact_field_selector_init(OssoABookContactFieldSelector *selector)
{
  OssoABookContactFieldSelectorPrivate *priv = PRIVATE(selector);
  HildonTouchSelectorColumn *col;
  GList *cells;
  GtkCellRenderer *cell;

  priv->message_map = osso_abook_message_map_new(field_map);

  priv->list_store = gtk_list_store_new(3, G_TYPE_STRING, G_TYPE_STRING,
                                        OSSO_ABOOK_TYPE_CONTACT_FIELD);

  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(priv->list_store),
                                       COL_TEXT, GTK_SORT_ASCENDING);

  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(priv->list_store), COL_TEXT,
                                  compare_fields, NULL, NULL);
  col = hildon_touch_selector_append_text_column(
    HILDON_TOUCH_SELECTOR(selector), GTK_TREE_MODEL(priv->list_store), 0);

  for (cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(col)); cells;
       cells = g_list_delete_link(cells, cells))
  {
    if (GTK_IS_CELL_RENDERER_TEXT(cells->data))
      g_object_set(cells->data, "ellipsize", 3, NULL);
  }

  cell = gtk_cell_renderer_pixbuf_new();
  gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(col), cell, FALSE);
  gtk_cell_layout_reorder(GTK_CELL_LAYOUT(col), cell, 0);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(col), cell,
                                 "icon-name", COL_ICON,
                                 NULL);
  g_object_set(cell, "stock-size", HILDON_ICON_SIZE_FINGER, NULL);
}

static void
append_field(OssoABookContactFieldSelectorPrivate *priv,
             OssoABookContactField *field, GtkIconTheme *icon_theme)
{
  gchar *text = get_field_text(priv, field);
  const char *icon_name = osso_abook_contact_field_get_icon_name(field);
  GtkTreeIter iter;

  if (text)
  {
    if (icon_name && !gtk_icon_theme_has_icon(icon_theme, icon_name))
      icon_name = NULL;

    gtk_list_store_append(priv->list_store, &iter);
    gtk_list_store_set(priv->list_store, &iter,
                       COL_TEXT, text,
                       COL_ICON, icon_name,
                       COL_FIELD, field,
                       -1);
    g_free(text);
  }
}

GtkWidget *
osso_abook_contact_field_selector_new(void)
{
  return g_object_new(OSSO_ABOOK_TYPE_CONTACT_FIELD_SELECTOR, NULL);
}

void
osso_abook_contact_field_selector_load(OssoABookContactFieldSelector *selector,
                                       OssoABookContact *contact,
                                       OssoABookContactFieldPredicate accept_field,
                                       gpointer user_data)
{
  OssoABookContactFieldSelectorPrivate *priv;
  GList *fields;
  GtkIconTheme *icon_theme;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD_SELECTOR(selector));
  g_return_if_fail(NULL == contact || OSSO_ABOOK_IS_CONTACT(contact));
  g_return_if_fail(NULL != accept_field);

  priv = PRIVATE(selector);
  gtk_list_store_clear(priv->list_store);
  fields = osso_abook_contact_field_list_supported_fields(priv->message_map,
                                                          contact, accept_field,
                                                          user_data);
  icon_theme = gtk_icon_theme_get_for_screen(
    gtk_widget_get_screen(GTK_WIDGET(selector)));

  while (fields)
  {
    OssoABookContactField *field = fields->data;

    append_field(priv, field, icon_theme);
    fields = g_list_delete_link(fields, fields);
    g_object_unref(field);
  }
}

void
osso_abook_contact_field_selector_append(
  OssoABookContactFieldSelector *selector,
  OssoABookContactField *field)
{
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD_SELECTOR(selector));
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field));

  append_field(PRIVATE(selector), field,
               gtk_icon_theme_get_for_screen(
                 gtk_widget_get_screen(GTK_WIDGET(selector))));
}

OssoABookContactField *
osso_abook_contact_field_selector_get_selected(
  OssoABookContactFieldSelector *selector)
{
  OssoABookContactFieldSelectorPrivate *priv;
  HildonTouchSelector *ts;
  GtkTreeIter iter;
  OssoABookContactField *field = NULL;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD_SELECTOR(selector), NULL);

  priv = PRIVATE(selector);
  ts = HILDON_TOUCH_SELECTOR(selector);

  if (hildon_touch_selector_get_hildon_ui_mode(ts))
  {
    if (hildon_touch_selector_get_selected(ts, COL_TEXT, &iter))
    {
      gtk_tree_model_get(GTK_TREE_MODEL(priv->list_store), &iter,
                         COL_FIELD, &field,
                         -1);
    }
  }
  else
  {
    GtkTreePath *path =
      hildon_touch_selector_get_last_activated_row(ts, COL_TEXT);

    if (path)
    {
      gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->list_store), &iter, path);
      gtk_tree_model_get(GTK_TREE_MODEL(priv->list_store), &iter,
                         COL_FIELD, &field,
                         -1);
      gtk_tree_path_free(path);
    }
  }

  return field;
}

int
osso_abook_contact_field_selector_find_custom(
  OssoABookContactFieldSelector *selector,
  OssoABookContactFieldPredicate accept_field,
  gpointer user_data)
{
  GtkTreeModel *model;
  GtkTreeIter iter;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD_SELECTOR(selector), -1);
  g_return_val_if_fail(NULL != accept_field, -1);

  model = hildon_touch_selector_get_model(HILDON_TOUCH_SELECTOR(selector),
                                          COL_TEXT);

  if (gtk_tree_model_get_iter_first(model, &iter))
  {
    int i = 0;

    do
    {
      OssoABookContactField *field;

      gtk_tree_model_get(model, &iter,
                         COL_FIELD, &field,
                         -1);
      g_object_unref(field);

      if (accept_field(field, user_data))
        return i;
    }
    while (gtk_tree_model_iter_next(model, &iter));
  }

  return -1;
}

GList *
osso_abook_contact_field_selector_get_selected_fields(
  OssoABookContactFieldSelector *selector)
{
  OssoABookContactFieldSelectorPrivate *priv;
  GList *rows;
  GList *fields = NULL;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD_SELECTOR(selector), NULL);

  priv = PRIVATE(selector);

  for (rows = hildon_touch_selector_get_selected_rows(
         HILDON_TOUCH_SELECTOR(selector), COL_TEXT); rows;
       rows = g_list_delete_link(rows, rows))
  {
    GtkTreeIter iter;

    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->list_store), &iter,
                                rows->data))
    {
      OssoABookContactField *field;

      gtk_tree_model_get(GTK_TREE_MODEL(priv->list_store), &iter,
                         COL_FIELD, &field,
                         -1);
      fields = g_list_prepend(fields, field);
    }

    gtk_tree_path_free(rows->data);
  }

  return fields;
}

void
osso_abook_contact_field_selector_set_show_values(
  OssoABookContactFieldSelector *selector,
  gboolean value)
{
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD_SELECTOR(selector));

  g_object_set(selector,
               "show-values", value,
               NULL);
}

gboolean
osso_abook_contact_field_selector_get_show_values(
  OssoABookContactFieldSelector *selector)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD_SELECTOR(selector), FALSE);

  return PRIVATE(selector)->show_values;
}
