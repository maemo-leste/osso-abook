/*
 * osso-abook-voicemail-selector.c
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

#include "osso-abook-log.h"
#include "osso-abook-settings.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-voicemail-contact.h"
#include "osso-abook-voicemail-selector.h"

#include "config.h"

struct _OssoABookVoicemailSelectorPrivate
{
  gulong dialog_response_hook;
};

typedef struct _OssoABookVoicemailSelectorPrivate
  OssoABookVoicemailSelectorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookVoicemailSelector,
  osso_abook_voicemail_selector,
  HILDON_TYPE_TOUCH_SELECTOR_ENTRY
);

#define PRIVATE(selector) \
  ((OssoABookVoicemailSelectorPrivate *) \
   osso_abook_voicemail_selector_get_instance_private(( \
                                                        OssoABookVoicemailSelector \
                                                        *)selector))

enum
{
  COL_PHONE_NUMBER,
  COL_OPERATOR_ID,
  COL_OPERATOR_NAME
};
static void
osso_abook_voicemail_selector_dispose(GObject *object)
{
  OssoABookVoicemailSelectorPrivate *priv = PRIVATE(object);

  if (priv->dialog_response_hook)
  {
    g_signal_remove_emission_hook(g_signal_lookup("response", GTK_TYPE_DIALOG),
                                  priv->dialog_response_hook);
    priv->dialog_response_hook = 0;
  }

  G_OBJECT_CLASS(osso_abook_voicemail_selector_parent_class)->dispose(object);
}

static void
osso_abook_voicemail_selector_size_request(GtkWidget *widget,
                                           GtkRequisition *requisition)
{
  GtkRequisition r;

  GTK_WIDGET_CLASS(osso_abook_voicemail_selector_parent_class)->size_request(
    widget, requisition);

  hildon_touch_selector_optimal_size_request(HILDON_TOUCH_SELECTOR(widget), &r);

  if (70 - r.height >= 0)
    requisition->height += 70 - r.height;
}

static void
osso_abook_voicemail_selector_style_set(GtkWidget *widget,
                                        GtkStyle *previous_style)
{
  GList *cells;
  GList *last_cell;

  GTK_WIDGET_CLASS(osso_abook_voicemail_selector_parent_class)->style_set(
    widget, previous_style);

  cells = gtk_cell_layout_get_cells(
    GTK_CELL_LAYOUT(hildon_touch_selector_get_column(
                      HILDON_TOUCH_SELECTOR(widget), 0)));
  last_cell = g_list_last(cells);

  if (last_cell)
  {
    GdkColor color;
    GtkStyle *style;

    if (!gtk_style_lookup_color(widget->style, "SecondaryTextColor", &color))
      color = widget->style->fg[GTK_STATE_INSENSITIVE];

    style = gtk_rc_get_style_by_paths(gtk_widget_get_settings(widget),
                                      "SmallSystemFont", NULL, G_TYPE_NONE);

    g_object_set(last_cell->data,
                 "foreground-gdk", &color,
                 "font-desc", style ? style->font_desc : NULL,
                 NULL);
  }

  g_list_free(cells);
}

static void
osso_abook_voicemail_selector_class_init(OssoABookVoicemailSelectorClass *klass)
{
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = osso_abook_voicemail_selector_dispose;

  widget_class->size_request = osso_abook_voicemail_selector_size_request;
  widget_class->style_set = osso_abook_voicemail_selector_style_set;
}

static GtkTreeModel *
create_numbers_model(GSList *numbers)
{
  GType types[] = { G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING };
  GtkListStore *store = gtk_list_store_newv(G_N_ELEMENTS(types), types);
  GtkTreeIter iter;
  GSList *l;

  for (l = numbers; l; l = l->next)
  {
    OssoABookVoicemailNumber *number = l->data;

    gtk_list_store_insert_with_values(store, &iter, -1,
                                      COL_PHONE_NUMBER, number->phone_number,
                                      COL_OPERATOR_ID, number->operator_id,
                                      COL_OPERATOR_NAME, number->operator_name,
                                      -1);
  }

  return GTK_TREE_MODEL(store);
}

static gboolean
remove_dialog_response_hook(gpointer user_data)
{
  HildonTouchSelector *selector = user_data;

  g_warning("FIXME: Remove workaround for NB#147342");
  hildon_touch_selector_set_hildon_ui_mode(selector, HILDON_UI_MODE_EDIT);

  return FALSE;
}

static gboolean
dialog_response_hook(GSignalInvocationHint *ihint, guint n_params,
                     const GValue *param_values, gpointer data)
{
  HildonPickerDialog *dialog;

  if (ihint->run_type == G_SIGNAL_RUN_FIRST)
  {
    g_return_val_if_fail(2 == n_params, TRUE);

    dialog = g_value_get_object(param_values);

    if (g_value_get_int(param_values + 1) == GTK_RESPONSE_OK)
    {
      if (HILDON_IS_PICKER_DIALOG(dialog))
      {
        HildonTouchSelector *selector =
          hildon_picker_dialog_get_selector(dialog);
        GtkTreeModel *model =
          hildon_touch_selector_get_model(selector, COL_PHONE_NUMBER);

        if ((selector == data) &&
            (gtk_tree_model_iter_n_children(model, NULL) <= 0))
        {
          gchar *text = hildon_touch_selector_get_current_text(selector);

          if (text)
          {
            gchar *tmp = g_strchomp(g_strchug(text));

            if (!IS_EMPTY(tmp))
            {
              /* NB#147342 (Done button of HildonPickerDialog doesn't work for
               * empty models)
               */
              g_warning("FIXME: Working arround NB#147342");
              hildon_touch_selector_set_hildon_ui_mode(selector,
                                                       HILDON_UI_MODE_NORMAL);

              g_idle_add_full(-100, remove_dialog_response_hook,
                              g_object_ref(selector),
                              (GDestroyNotify)g_object_unref);
            }
          }

          g_free(text);
        }
      }
    }
  }

  return TRUE;
}

static void
osso_abook_voicemail_selector_init(OssoABookVoicemailSelector *selector)
{
  OssoABookVoicemailSelectorPrivate *priv = PRIVATE(selector);
  GtkTreeModel *model = create_numbers_model(NULL);
  HildonTouchSelectorColumn *col = hildon_touch_selector_append_text_column(
    HILDON_TOUCH_SELECTOR(selector), model, 0);
  GtkCellRenderer *cell_renderer = gtk_cell_renderer_text_new();
  OssoABookVoicemailNumber *number;
  GSList *numbers;
  GHashTable *operators;
  GSList *l, *next;
  GHashTableIter iter;
  gchar *operator_id;

  g_object_set(cell_renderer, "xalign", 1.0f, NULL);
  gtk_cell_layout_pack_end(GTK_CELL_LAYOUT(col), cell_renderer, FALSE);
  gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(col), cell_renderer,
                                 "text", 2,
                                 NULL);
  hildon_touch_selector_entry_set_input_mode(
    HILDON_TOUCH_SELECTOR_ENTRY(selector), HILDON_GTK_INPUT_MODE_TELE);
  hildon_touch_selector_entry_set_text_column(
    HILDON_TOUCH_SELECTOR_ENTRY(selector), 0);

  number = osso_abook_voicemail_number_new(NULL, NULL, NULL);
  number->phone_number =
    osso_abook_voicemail_contact_get_preferred_number(NULL);

  if (IS_EMPTY(number->phone_number))
  {
    number->operator_id = _osso_abook_get_operator_id(NULL, NULL);
    number->operator_name =
      _osso_abook_get_operator_name(NULL, number->operator_id, NULL);
    g_free(number->phone_number);
    number->phone_number = NULL;
  }

  numbers = osso_abook_settings_get_voicemail_numbers();

  for (l = numbers; l; l = next)
  {
    OssoABookVoicemailNumber *candidate = l->data;

    next = l->next;

    if (number->phone_number)
    {
      if (!g_strcmp0(candidate->phone_number, number->phone_number))
      {
        if (!IS_EMPTY(candidate->operator_id) && !number->operator_id)
        {
          osso_abook_voicemail_number_free(number);
          number = candidate;
        }
        else
          osso_abook_voicemail_number_free(candidate);

        numbers = g_slist_delete_link(numbers, l);
      }
    }
    else
    {
      if (number->operator_id &&
          !g_strcmp0(candidate->operator_id, number->operator_id) &&
          !IS_EMPTY(candidate->phone_number))
      {
        numbers = g_slist_delete_link(numbers, l);
        osso_abook_voicemail_number_free(number);
        number = candidate;
      }
    }
  }

  if (!number->operator_id)
    number->operator_id = _osso_abook_get_operator_id(NULL, NULL);

  if (!IS_EMPTY(number->phone_number))
  {
    numbers = g_slist_prepend(numbers, number);
    number = NULL;
  }

  operators = g_hash_table_new_full(g_str_hash, g_str_equal, NULL,
                                    (GDestroyNotify)g_free);

  for (l = numbers; l; l = l->next)
  {
    OssoABookVoicemailNumber *candidate = l->data;

    if (!IS_EMPTY(candidate->operator_id) &&
        !g_hash_table_lookup(operators, candidate->operator_id))
    {
      g_hash_table_insert(operators, candidate->operator_id,
                          g_strdup(candidate->operator_name));
    }
  }

  g_hash_table_iter_init(&iter, operators);

  while (g_hash_table_iter_next(&iter, (gpointer *)&operator_id, 0))
  {
    gchar *operator_name =
      _osso_abook_get_operator_name(NULL, operator_id, NULL);

    if (operator_name)
      g_hash_table_insert(operators, operator_id, operator_name);
  }

  if (numbers)
  {
    for (l = numbers; l; l = l->next)
    {
      OssoABookVoicemailNumber *num = l->data;

      if (num->operator_id)
      {
        gchar *operator_name = g_hash_table_lookup(operators, num->operator_id);

        if (operator_name)
        {
          g_free(num->operator_name);
          num->operator_name = g_strdup(operator_name);
        }
      }
    }

    g_hash_table_unref(operators);
    hildon_touch_selector_set_model(HILDON_TOUCH_SELECTOR(selector), 0,
                                    create_numbers_model(numbers));
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(selector), 0, 0);
  }
  else
  {
    g_hash_table_unref(operators);
    hildon_touch_selector_set_model(HILDON_TOUCH_SELECTOR(selector), 0,
                                    create_numbers_model(NULL));
  }

  osso_abook_voicemail_number_list_free(numbers);
  osso_abook_voicemail_number_free(number);
  priv->dialog_response_hook =
    g_signal_add_emission_hook(g_signal_lookup("response", GTK_TYPE_DIALOG),
                               0, dialog_response_hook, selector, NULL);
}

GtkWidget *
osso_abook_voicemail_selector_new()
{
  return gtk_widget_new(OSSO_ABOOK_TYPE_VOICEMAIL_SELECTOR, NULL);
}

static GSList *
get_voicemail_numbers(OssoABookVoicemailSelector *selector)
{
  GtkTreeModel *model =
    hildon_touch_selector_get_model(HILDON_TOUCH_SELECTOR(selector), 0);
  GSList *numbers = NULL;
  GtkTreeIter iter;

  if (gtk_tree_model_get_iter_first(model, &iter))
  {
    do
    {
      gchar *phone_number = NULL;
      gchar *operator_name = NULL;
      gchar *operator_id = NULL;
      OssoABookVoicemailNumber *number;

      gtk_tree_model_get(model, &iter,
                         COL_PHONE_NUMBER, &phone_number,
                         COL_OPERATOR_ID, &operator_id,
                         COL_OPERATOR_NAME, &operator_name,
                         -1);
      number = osso_abook_voicemail_number_new(phone_number, operator_id,
                                               operator_name);
      g_free(operator_id);
      g_free(operator_name);
      g_free(phone_number);
      numbers = g_slist_prepend(numbers, number);
    }
    while (gtk_tree_model_iter_next(model, &iter));
  }

  return g_slist_reverse(numbers);
}

gboolean
osso_abook_voicemail_selector_save(OssoABookVoicemailSelector *selector)
{
  GSList *numbers;

  g_return_val_if_fail(OSSO_ABOOK_IS_VOICEMAIL_SELECTOR(selector), FALSE);

  numbers = get_voicemail_numbers(selector);

  if (numbers)
  {
    if (osso_abook_settings_set_voicemail_numbers(numbers))
    {
      OssoABookVoicemailNumber *number = numbers->data;
      GList *tel = NULL;
      EContact *contact;

      if (!IS_EMPTY(number->phone_number))
        tel = g_list_prepend(tel, number->phone_number);

      contact = E_CONTACT(osso_abook_voicemail_contact_get_default());
      e_contact_set(contact, E_CONTACT_TEL, tel);
      g_list_free(tel);

      osso_abook_contact_async_commit(
        OSSO_ABOOK_CONTACT(contact), NULL, NULL, NULL);
      g_object_unref(contact);
    }
  }

  osso_abook_voicemail_number_list_free(numbers);

  return TRUE;
}

void
osso_abook_voicemail_selector_apply(OssoABookVoicemailSelector *selector)
{
  HildonTouchSelector *ts;
  GtkTreeModel *model;
  gchar *current_text;
  GSList *numbers;
  gint active = -1;
  GSList *l;
  int i = 0;
  OssoABookVoicemailNumber *current_number = NULL;

  g_return_if_fail(OSSO_ABOOK_IS_VOICEMAIL_SELECTOR(selector));

  ts = HILDON_TOUCH_SELECTOR(selector);
  model = hildon_touch_selector_get_model(ts, 0);
  current_text = hildon_touch_selector_get_current_text(ts);
  numbers = get_voicemail_numbers(selector);

  g_warn_if_fail(!IS_EMPTY(current_text));

  if (gtk_tree_model_iter_n_children(model, NULL) > 0)
    active = hildon_touch_selector_get_active(ts, 0);

  for (l = numbers; l; l = l->next)
  {
    OssoABookVoicemailNumber *number = l->data;

    if ((i == active) && !g_strcmp0(current_text, number->phone_number))
    {
      numbers = g_slist_remove_link(numbers, l);
      current_number = number;
      break;
    }

    i++;
  }

  if (!current_number && current_text)
    current_number = osso_abook_voicemail_number_new(current_text, NULL, NULL);

  g_warn_if_fail(NULL != current_number);

  if (current_number)
  {
    if (!current_number->operator_id)
      current_number->operator_id = _osso_abook_get_operator_id(NULL, NULL);

    if (!current_number->operator_name)
    {
      current_number->operator_name =
        _osso_abook_get_operator_name(NULL,
                                      current_number->operator_id, NULL);
    }

    numbers = g_slist_prepend(numbers, current_number);
  }

  hildon_touch_selector_set_model(ts, 0, create_numbers_model(numbers));
  osso_abook_voicemail_number_list_free(numbers);
  g_free(current_text);
}
