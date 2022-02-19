/*
 * osso-abook-tp-account-model.c
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

#include "osso-abook-account-manager.h"
#include "osso-abook-enums.h"
#include "osso-abook-util.h"
#include "osso-abook-utils-private.h"

#include "osso-abook-tp-account-model.h"

struct _OssoABookTpAccountModelPrivate
{
  OssoABookAccountManager *account_manager;
  GHashTable *accounts;
  gulong account_created_id;
  gulong account_changed_id;
  gulong account_removed_id;
};

typedef struct _OssoABookTpAccountModelPrivate OssoABookTpAccountModelPrivate;

struct _OssoABookTpAccountData
{
  gpointer user_data;
/* nobody is using that */
#ifdef ACCOUNT_DATA_SIGNALS
  gulong account_created_id;
  gulong account_changed_id;
  gulong account_removed_id;
#endif
};

typedef struct _OssoABookTpAccountData OssoABookTpAccountData;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookTpAccountModel,
  osso_abook_tp_account_model,
  GTK_TYPE_LIST_STORE
);

#define PRIVATE(model) \
  ((OssoABookTpAccountModelPrivate *) \
   osso_abook_tp_account_model_get_instance_private(((OssoABookTpAccountModel *) \
                                                     model)))

enum
{
  PROP_ACCOUNT_PROTOCOL = 1,
  PROP_ALLOWED_ACCOUNTS,
  PROP_REQUIRED_CAPABILITIES
};

static void
osso_abook_tp_account_model_set_property(GObject *object, guint property_id,
                                         const GValue *value, GParamSpec *pspec)
{
  OssoABookTpAccountModel *model = OSSO_ABOOK_TP_ACCOUNT_MODEL(object);

  switch (property_id)
  {
    case PROP_ALLOWED_ACCOUNTS:
    {
      osso_abook_tp_account_model_set_allowed_accounts(
        model, g_value_get_pointer(value));
      break;
    }
    case PROP_REQUIRED_CAPABILITIES:
    {
      osso_abook_tp_account_model_set_required_capabilities(
        model, g_value_get_flags(value));
      break;
    }
    case PROP_ACCOUNT_PROTOCOL:
    {
      osso_abook_tp_account_model_set_account_protocol(
        model, g_value_get_string(value));
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
osso_abook_tp_account_model_get_property(GObject *object, guint property_id,
                                         GValue *value, GParamSpec *pspec)
{
  OssoABookTpAccountModel *model = OSSO_ABOOK_TP_ACCOUNT_MODEL(object);

  switch (property_id)
  {
    case PROP_ALLOWED_ACCOUNTS:
    {
      g_value_set_pointer(
        value, osso_abook_tp_account_model_get_allowed_accounts(model));
      break;
    }
    case PROP_REQUIRED_CAPABILITIES:
    {
      g_value_set_flags(
        value,
        osso_abook_tp_account_model_get_required_capabilities(model));
      break;
    }
    case PROP_ACCOUNT_PROTOCOL:
    {
      g_value_set_string(
        value, osso_abook_tp_account_model_get_account_protocol(model));
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static gboolean
account_data_free(gpointer key, gpointer value, gpointer user_data)
{
#ifdef ACCOUNT_DATA_SIGNALS
  OssoABookTpAccountData *data = value;
  disconnect_signal_if_connected(key, data->account_created_id);
  disconnect_signal_if_connected(key, data->account_changed_id);
  disconnect_signal_if_connected(key, data->account_removed_id);
#endif
  return TRUE;
}

static void
osso_abook_tp_account_model_dispose(GObject *object)
{
  OssoABookTpAccountModelPrivate *priv = PRIVATE(object);

  g_hash_table_foreach_remove(priv->accounts, account_data_free, NULL);

  if (priv->account_created_id)
  {
    disconnect_signal_if_connected(priv->account_manager,
                                   priv->account_created_id);
  }

  if (priv->account_changed_id)
  {
    disconnect_signal_if_connected(priv->account_manager,
                                   priv->account_changed_id);
  }

  if (!priv->account_removed_id)
  {
    disconnect_signal_if_connected(priv->account_manager,
                                   priv->account_removed_id);
  }

  if (priv->account_manager)
  {
    g_object_unref(priv->account_manager);
    priv->account_manager = NULL;
  }

  G_OBJECT_CLASS(osso_abook_tp_account_model_parent_class)->dispose(object);
}

static void
osso_abook_tp_account_model_finalize(GObject *object)
{
  OssoABookTpAccountModelPrivate *priv = PRIVATE(object);

  g_hash_table_destroy(priv->accounts);
  G_OBJECT_CLASS(osso_abook_tp_account_model_parent_class)->finalize(object);
}

static void
osso_abook_tp_account_model_class_init(OssoABookTpAccountModelClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_tp_account_model_set_property;
  object_class->get_property = osso_abook_tp_account_model_get_property;
  object_class->dispose = osso_abook_tp_account_model_dispose;
  object_class->finalize = osso_abook_tp_account_model_finalize;

  g_object_class_install_property(
    object_class, PROP_ACCOUNT_PROTOCOL,
    g_param_spec_string(
      "account-protocol",
      "Account Protocol",
      "Accepted account protocol (NULL for all protocols)",
      NULL,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_ALLOWED_ACCOUNTS,
    g_param_spec_pointer(
      "allowed-accounts",
      "Allowed Accounts",
      "Accounts which may appear in the model (if all additional filters allow them)",
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_REQUIRED_CAPABILITIES,
    g_param_spec_flags(
      "required-capabilities",
      "Required Account Capabilities",
      "Account capabilities required an account to appear",
      OSSO_ABOOK_TYPE_CAPS_FLAGS,
      OSSO_ABOOK_CAPS_NONE,
      GTK_PARAM_READWRITE));
}

static void
account_data_destroy(gpointer user_data)
{
  g_slice_free(OssoABookTpAccountData, user_data);
}

static gint
osso_abook_tp_account_model_sort_func(GtkTreeModel *model, GtkTreeIter *a,
                                      GtkTreeIter *b, gpointer user_data)
{
  int rv;
  TpAccount *account_b;
  TpAccount *account_a;

  gtk_tree_model_get(model, a,
                     OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ACCOUNT, &account_a,
                     -1);
  gtk_tree_model_get(model, b,
                     OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ACCOUNT, &account_b,
                     -1);
  rv = g_strcmp0(tp_account_get_display_name(account_a),
                 tp_account_get_display_name(account_b));
  g_object_unref(account_a);
  g_object_unref(account_b);

  return rv;
}

static void
modify_account(OssoABookTpAccountModel *model, TpAccount *account,
               OssoABookTpAccountData *data, gboolean add)
{
  OssoABookTpAccountModelPrivate *priv = PRIVATE(model);
  GtkListStore *store = GTK_LIST_STORE(model);
  GtkTreeIter iter;

  if (add)
  {
    if (!data->user_data)
    {
      TpProtocol *protocol = osso_abook_account_manager_get_protocol_object(
        priv->account_manager, tp_account_get_protocol_name(account));
      char *display_string = osso_abook_tp_account_get_display_string(
        account, NULL, NULL);
      const gchar *icon_name = tp_protocol_get_icon_name(protocol);

      gtk_list_store_insert_with_values(
        store, &iter, 0,
        OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ACCOUNT, account,
        OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_PROTOCOL_AND_UID, display_string,
        OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ICON_NAME, icon_name,
        -1);
      g_free(display_string);
      data->user_data = iter.user_data;
    }
  }
  else if (data->user_data)
  {
    iter.user_data = data->user_data;
    iter.stamp = store->stamp;
    gtk_list_store_remove(store, &iter);
    data->user_data = NULL;
  }
}

static void
osso_abook_tp_account_model_add_account(OssoABookTpAccountModel *model,
                                        TpAccount *account)
{
  OssoABookTpAccountModelPrivate *priv;
  OssoABookTpAccountData *data;

  g_return_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model));
  g_return_if_fail(TP_IS_ACCOUNT(account));

  priv = PRIVATE(model);
  data = g_slice_new0(OssoABookTpAccountData);
  g_hash_table_insert(priv->accounts, g_object_ref(account), data);
  modify_account(model, account, data, tp_account_is_enabled(account));
}

static void
account_created_cb(OssoABookAccountManager *manager, TpAccount *account,
                   gpointer user_data)
{
  g_return_if_fail(user_data && OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(user_data));
  g_return_if_fail(account && TP_IS_ACCOUNT(account));

  osso_abook_tp_account_model_add_account(
    OSSO_ABOOK_TP_ACCOUNT_MODEL(user_data), account);
}

static void
account_changed_cb(OssoABookAccountManager *manager,
                   TpAccount *account, GQuark property, const GValue *value,
                   gpointer user_data)
{
  OssoABookTpAccountModel *model;
  OssoABookTpAccountData *data;
  GtkTreePath *path;
  GtkTreeIter iter;

  g_return_if_fail(user_data && OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(user_data));
  g_return_if_fail(account && TP_IS_ACCOUNT(account));

  model = OSSO_ABOOK_TP_ACCOUNT_MODEL(user_data);
  data = g_hash_table_lookup(PRIVATE(model)->accounts, account);
  iter.stamp = GTK_LIST_STORE(model)->stamp;
  iter.user_data = data->user_data;
  path = gtk_tree_model_get_path(GTK_TREE_MODEL(model), &iter);
  gtk_tree_model_row_changed(GTK_TREE_MODEL(model), path, &iter);
  gtk_tree_path_free(path);
}

static void
osso_abook_tp_account_model_remove_account(OssoABookTpAccountModel *model,
                                           TpAccount *account)
{
  OssoABookTpAccountModelPrivate *priv;
  OssoABookTpAccountData *data;

  g_return_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model));
  g_return_if_fail(TP_IS_ACCOUNT(account));

  priv = PRIVATE(model);
  data = g_hash_table_lookup(priv->accounts, account);

  g_return_if_fail(data != NULL);

#ifdef ACCOUNT_DATA_SIGNALS
  disconnect_signal_if_connected(account, data->account_created_id);
  disconnect_signal_if_connected(account, data->account_changed_id);
  disconnect_signal_if_connected(account, data->account_removed_id);
#endif

  modify_account(model, account, data, FALSE);
  g_hash_table_remove(priv->accounts, account);
}

static void
account_removed_cb(OssoABookAccountManager *manager, TpAccount *account,
                   gpointer user_data)
{
  g_return_if_fail(user_data && OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(user_data));
  g_return_if_fail(account && TP_IS_ACCOUNT(account));

  osso_abook_tp_account_model_remove_account(
    OSSO_ABOOK_TP_ACCOUNT_MODEL(user_data), account);
}

static void
osso_abook_tp_account_model_init(OssoABookTpAccountModel *model)
{
  OssoABookTpAccountModelPrivate *priv = PRIVATE(model);
  GType types[] = { TP_TYPE_ACCOUNT, G_TYPE_STRING, G_TYPE_STRING };
  GList *accounts;
  GList *l;

  priv->accounts = g_hash_table_new_full(NULL, NULL,
                                         (GDestroyNotify)g_object_unref,
                                         account_data_destroy);
  gtk_list_store_set_column_types(GTK_LIST_STORE(model),
                                  G_N_ELEMENTS(types), types);
  gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(model),
                                          osso_abook_tp_account_model_sort_func,
                                          NULL, NULL);
  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model), -1,
                                       GTK_SORT_ASCENDING);
  priv->account_manager = osso_abook_account_manager_new();

  priv->account_created_id =
    g_signal_connect_object(priv->account_manager, "account-created",
                            G_CALLBACK(account_created_cb), model, 0);
  priv->account_changed_id =
    g_signal_connect_object(priv->account_manager, "account-changed",
                            G_CALLBACK(account_changed_cb), model, 0);
  priv->account_removed_id =
    g_signal_connect_object(priv->account_manager, "account-removed",
                            G_CALLBACK(account_removed_cb), model, 0);
  accounts = osso_abook_account_manager_list_enabled_accounts(
    priv->account_manager);

  for (l = accounts; l; l = l->next)
    osso_abook_tp_account_model_add_account(model, TP_ACCOUNT(l->data));

  g_list_free(accounts);
}

OssoABookTpAccountModel *
osso_abook_tp_account_model_new(void)
{
  return g_object_new(OSSO_ABOOK_TYPE_TP_ACCOUNT_MODEL, NULL);
}

const char *
osso_abook_tp_account_model_get_account_protocol(OssoABookTpAccountModel *model)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model), NULL);

  return osso_abook_account_manager_get_account_protocol(
    PRIVATE(model)->account_manager);
}

void
osso_abook_tp_account_model_set_account_protocol(OssoABookTpAccountModel *model,
                                                 const char *protocol)
{
  g_return_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model));

  osso_abook_account_manager_set_account_protocol(
    PRIVATE(model)->account_manager, protocol);
}

OssoABookCapsFlags
osso_abook_tp_account_model_get_required_capabilities(
  OssoABookTpAccountModel *model)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model),
                       OSSO_ABOOK_CAPS_NONE);

  return osso_abook_account_manager_get_required_capabilities(
    PRIVATE(model)->account_manager);
}

void
osso_abook_tp_account_model_set_required_capabilities(
  OssoABookTpAccountModel *model,
  OssoABookCapsFlags caps)
{
  g_return_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model));

  osso_abook_account_manager_set_required_capabilities(
    PRIVATE(model)->account_manager, caps);
}

GList *
osso_abook_tp_account_model_get_allowed_accounts(OssoABookTpAccountModel *model)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model), NULL);

  return osso_abook_account_manager_get_allowed_accounts(
    PRIVATE(model)->account_manager);
}

void
osso_abook_tp_account_model_set_allowed_accounts(OssoABookTpAccountModel *model,
                                                 GList *accounts)
{
  g_return_if_fail(OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(model));

  osso_abook_account_manager_set_allowed_accounts(
    PRIVATE(model)->account_manager, accounts);
}
