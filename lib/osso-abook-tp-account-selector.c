/*
 * osso-abook-tp-account-selector.c
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

#include "osso-abook-tp-account-model.h"
#include "osso-abook-util.h"

#include "osso-abook-tp-account-selector.h"

struct _OssoABookTpAccountSelectorPrivate
{
  OssoABookTpAccountModel *model;
  int selection_mode;
};

typedef struct _OssoABookTpAccountSelectorPrivate OssoABookTpAccountSelectorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookTpAccountSelector,
  osso_abook_tp_account_selector,
  HILDON_TYPE_PICKER_DIALOG
);

#define PRIVATE(selector) \
  ((OssoABookTpAccountSelectorPrivate *)osso_abook_tp_account_selector_get_instance_private(((OssoABookTpAccountSelector *)selector)))

enum
{
  PROP_MODEL = 1,
  PROP_SELECTION_MODE
};


static void
cell_data_func(GtkCellLayout *cell_layout, GtkCellRenderer *cell,
               GtkTreeModel *tree_model, GtkTreeIter *iter, gpointer data)
{
  gchar *markup;
  TpAccount *account;

  gtk_tree_model_get(tree_model, iter,
                     OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ACCOUNT, &account,
                     -1);
  markup = osso_abook_tp_account_get_display_markup(account);
  g_object_unref(account);
  g_object_set(cell,
               "markup", markup,
               NULL);
  g_free(markup);
}

static void
style_set_cb(GtkWidget *widget, GtkStyle *previous_style, gpointer user_data)
{
  GList *cells = gtk_cell_layout_get_cells(GTK_CELL_LAYOUT(
        hildon_touch_selector_get_column(HILDON_TOUCH_SELECTOR(widget), 0)));

  if (cells)
  {
    guint size;

    g_object_get(cells->data,
                 "stock-size", &size,
                 NULL);
    g_object_set(cells->data,
                 "width", hildon_get_icon_pixel_size(size) + 16,
                 NULL);
  }

  g_list_free(cells);
}

static void
osso_abook_tp_account_selector_set_property(GObject *object, guint property_id,
                                            const GValue *value,
                                            GParamSpec *pspec)
{
  OssoABookTpAccountSelector *dialog = OSSO_ABOOK_TP_ACCOUNT_SELECTOR(object);
  OssoABookTpAccountSelectorPrivate *priv = PRIVATE(dialog);

  switch (property_id)
  {
    case PROP_MODEL:
    {
      OssoABookTpAccountModel *model = g_value_get_object(value);
      HildonTouchSelector *selector;
      GtkCellRenderer *cell_text;
      HildonTouchSelectorColumn *col;
      GtkCellRenderer *cell_pixbuf;

      if (priv->model)
        g_object_unref(priv->model);

      if (model)
        priv->model = g_object_ref(model);
      else
        priv->model = osso_abook_tp_account_model_new();

      selector = HILDON_TOUCH_SELECTOR(hildon_touch_selector_new());
      cell_text = gtk_cell_renderer_text_new();
      g_object_set(cell_text,
                   "xalign", (gfloat)0.0f,
                   NULL);
      col = hildon_touch_selector_append_column(selector,
                                                GTK_TREE_MODEL(priv->model),
                                                cell_text, NULL);
      gtk_cell_layout_set_cell_data_func(GTK_CELL_LAYOUT(col), cell_text,
                                         cell_data_func, NULL, NULL);

      cell_pixbuf = gtk_cell_renderer_pixbuf_new();
      g_object_set(cell_pixbuf,
                   "xalign", (gfloat)0.0f,
                   "stock-size", HILDON_ICON_SIZE_FINGER,
                   NULL);

      gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(col), cell_pixbuf, FALSE);
      gtk_cell_layout_reorder(GTK_CELL_LAYOUT(col), cell_pixbuf, 0);
      gtk_cell_layout_set_attributes(
            GTK_CELL_LAYOUT(col), cell_pixbuf,
            "icon-name", OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ICON_NAME,
            NULL);
      hildon_touch_selector_set_column_selection_mode(selector,
                                                      priv->selection_mode);
      hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog), selector);
      g_signal_connect(selector, "style-set",
                       G_CALLBACK(style_set_cb), NULL);
      break;
    }
    case PROP_SELECTION_MODE:
    {
      HildonTouchSelector *selector;

      priv->selection_mode = g_value_get_int(value);

      selector =
          hildon_picker_dialog_get_selector(HILDON_PICKER_DIALOG(dialog));

      if (selector)
      {
        hildon_touch_selector_set_column_selection_mode(selector,
                                                        priv->selection_mode);
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
osso_abook_tp_account_selector_get_property(GObject *object, guint property_id,
                                            GValue *value, GParamSpec *pspec)
{
  switch(property_id)
  {
    case PROP_MODEL:
    {
      g_value_set_object(value, PRIVATE(object)->model);
      break;
    }
    case PROP_SELECTION_MODE:
    {
      HildonTouchSelector *selector =
          hildon_picker_dialog_get_selector(HILDON_PICKER_DIALOG(object));

      g_value_set_int(
            value, hildon_touch_selector_get_column_selection_mode(selector));
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
osso_abook_tp_account_selector_dispose(GObject *object)
{
  OssoABookTpAccountSelectorPrivate *priv = PRIVATE(object);

  if (priv->model)
  {
    g_object_unref(priv->model);
    priv->model = NULL;
  }

  G_OBJECT_CLASS(osso_abook_tp_account_selector_parent_class)->dispose(object);
}

static void
osso_abook_tp_account_selector_class_init(
    OssoABookTpAccountSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_tp_account_selector_set_property;
  object_class->get_property = osso_abook_tp_account_selector_get_property;
  object_class->dispose = osso_abook_tp_account_selector_dispose;

  g_object_class_install_property(
        object_class, PROP_MODEL,
        g_param_spec_object(
          "model",
          "Model",
          "The OssoABookMcAccountModel to display",
          OSSO_ABOOK_TYPE_TP_ACCOUNT_MODEL,
          GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
        object_class, PROP_SELECTION_MODE,
        g_param_spec_int(
          "selection-mode",
          "Dialog selection mode",
          "The dialog's selection mode (eg, single, multi)",
          0, 1, 0,
          GTK_PARAM_READWRITE));
}

static void
osso_abook_tp_account_selector_init(OssoABookTpAccountSelector *dialog)
{
  PRIVATE(dialog)->model = NULL;
  gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
}


GtkWidget *
osso_abook_tp_account_selector_new(GtkWindow *parent,
                                   OssoABookTpAccountModel *model)
{
  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), NULL);
  g_return_val_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_TP_ACCOUNT_SELECTOR,
                      "model", model,
                      "transient-for", parent,
                      NULL);
}

TpAccount *
osso_abook_tp_account_selector_get_account(OssoABookTpAccountSelector *dialog)
{
  GList *accounts;
  TpAccount *account = NULL;

  g_return_val_if_fail(
        osso_abook_tp_account_selector_get_selection_mode (dialog) ==
        HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE, NULL);

  accounts = osso_abook_tp_account_selector_get_accounts(dialog);

  if (accounts)
  {
    account = g_object_ref(accounts->data);
    g_list_foreach(accounts, (GFunc)&g_object_unref, NULL);
    g_list_free(accounts);
  }

  return account;
}

GList *
osso_abook_tp_account_selector_get_accounts(OssoABookTpAccountSelector *dialog)
{
  OssoABookTpAccountSelectorPrivate *priv = PRIVATE(dialog);
  HildonTouchSelector *selector;
  GList *accounts = NULL;
  GList *rows;
  GList *l;

  selector = hildon_picker_dialog_get_selector(HILDON_PICKER_DIALOG(dialog));

  if (!selector)
    return NULL;

  rows = hildon_touch_selector_get_selected_rows(
        HILDON_TOUCH_SELECTOR(selector), 0);

  for (l = rows; l; l = l->next)
  {
    GtkTreePath *path = l->data;

    if (path)
    {
      TpAccount *account = NULL;
      GtkTreeIter iter;

      gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->model), &iter, path);
      gtk_tree_model_get(GTK_TREE_MODEL(priv->model), &iter,
                         OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ACCOUNT, &account,
                         -1);
      accounts = g_list_prepend(accounts, account);
      gtk_tree_path_free(path);
    }
  }

  g_list_free(rows);

  return accounts;
}

OssoABookTpAccountModel *
osso_abook_tp_account_selector_get_model(OssoABookTpAccountSelector *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_SELECTOR(dialog), NULL);

  return PRIVATE(dialog)->model;
}

void
osso_abook_tp_account_selector_set_model(OssoABookTpAccountSelector *dialog,
                                         OssoABookTpAccountModel *model)
{
  g_return_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_SELECTOR(dialog));
  g_return_if_fail(!model || OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model));

  g_object_set(G_OBJECT(dialog),
               "model", model,
               NULL);
}

HildonTouchSelectorSelectionMode
osso_abook_tp_account_selector_get_selection_mode(
    OssoABookTpAccountSelector *dialog)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_SELECTOR(dialog),
                       HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE);

  return PRIVATE(dialog)->selection_mode;
}

void
osso_abook_mc_account_selector_set_selection_mode(
    OssoABookTpAccountSelector *dialog, HildonTouchSelectorSelectionMode mode)
{
  g_return_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_SELECTOR(dialog));

  g_object_set(G_OBJECT(dialog),
               "selection-mode", mode,
               NULL);
}
