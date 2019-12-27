#include <libosso.h>
#include <hildon/hildon.h>
#include <gtk/gtkprivate.h>

#include "config.h"

#include "osso-abook-list-store.h"
#include "osso-abook-contact.h"
#include "osso-abook-enum-types.h"
#include "osso-abook-roster.h"
#include "osso-abook-row-model.h"
#include "osso-abook-log.h"

struct _OssoABookListStorePrivate
{
  OssoABookRoster *roster;
  gboolean roster_is_running;
  gulong contacts_added_id;
  gulong contacts_changed_master_id;
  gulong contacts_removed_id;
  gulong sequence_complete_id;
  gulong notify_book_view_id;
  guint update_contacts_id;
  guint idle_sort_id;
  OssoABookListStoreRow **rows;
  gint stamp;
  gint count;
  gint size;
  gint extra;
  gint balloon_size;
  gint balloon_offset;
  GHashTable *names;
  GHashTable *hash2;
  OssoABookListStoreCompareFunc sort_func;
  gpointer sort_data;
  GDestroyNotify sort_destroy;
  OssoABookListStoreCompareFunc group_sort_func;
  gpointer group_sort_data;
  GDestroyNotify group_sort_destroy;
  OssoABookNameOrder name_order;
};


typedef struct _OssoABookListStorePrivate OssoABookListStorePrivate;

static gboolean
osso_abook_list_iter_nth_child(GtkTreeModel *tree_model, GtkTreeIter *iter,
                               GtkTreeIter *parent, gint n)
{
  OssoABookListStore *store = (OssoABookListStore *)tree_model;
  OssoABookListStorePrivate *priv = store->priv;

  if (parent || n < 0 || n >= (priv->extra + priv->count))
    return FALSE;

  iter->user_data = tree_model;
  iter->user_data2 = GINT_TO_POINTER(n);
  iter->stamp = priv->stamp;

  return TRUE;
}

static GtkTreeModelFlags
osso_abook_list_store_get_flags(GtkTreeModel *tree_model)
{
  return GTK_TREE_MODEL_LIST_ONLY;
}

static int
osso_abook_list_store_get_n_columns(GtkTreeModel *tree_model)
{
  return 1;
}

static GType
osso_abook_list_store_get_column_type(GtkTreeModel *tree_model, gint index_)
{
  if (!index_)
    return OSSO_ABOOK_TYPE_CONTACT;

  OSSO_ABOOK_WARN("invalid column index %d", index_);

  return 0;
}

static gboolean
osso_abook_list_model_get_iter(GtkTreeModel *tree_model, GtkTreeIter *iter,
                               GtkTreePath *path)
{
  OssoABookListStore *store = (OssoABookListStore *)tree_model;
  OssoABookListStorePrivate *priv = store->priv;
  gint index;

  if (gtk_tree_path_get_depth(path) != 1)
    return FALSE;

  index = *gtk_tree_path_get_indices(path);

  if (index < 0 || index >= priv->extra + priv->count)
    return FALSE;

  iter->user_data2 = GINT_TO_POINTER(index);
  iter->stamp = priv->stamp;
  iter->user_data = tree_model;

  return TRUE;
}

static GtkTreePath *
osso_abook_list_store_get_path(GtkTreeModel *tree_model,
                               GtkTreeIter *iter)
{
  OssoABookListStore *store = (OssoABookListStore *)tree_model;
  OssoABookListStorePrivate *priv = store->priv;

  g_return_val_if_fail(iter->stamp == priv->stamp, NULL);
  g_return_val_if_fail(iter->user_data == tree_model, NULL);

  return gtk_tree_path_new_from_indices(GPOINTER_TO_INT(iter->user_data2), -1);
}

static void
osso_abook_list_store_get_value(GtkTreeModel *tree_model, GtkTreeIter *iter,
                                gint column, GValue *value)
{
  OssoABookListStore *store = (OssoABookListStore *)tree_model;

  if (column)
    OSSO_ABOOK_WARN("invalid column index %d", column);
  else
  {
    OssoABookListStoreRow *row =
        osso_abook_list_store_iter_get_row(store, iter);
    g_value_init(value, OSSO_ABOOK_TYPE_CONTACT);
    g_value_set_object(value, row ? row->contact : NULL);
  }
}

static gboolean
osso_abook_list_store_iter_next(GtkTreeModel *tree_model, GtkTreeIter *iter)
{
  OssoABookListStore *store = (OssoABookListStore *)tree_model;
  OssoABookListStorePrivate *priv = store->priv;
  gint row_index;

  g_return_val_if_fail(iter->stamp == priv->stamp, FALSE);
  g_return_val_if_fail(iter->user_data == tree_model, FALSE);

  row_index = GPOINTER_TO_INT(iter->user_data2);

  g_return_val_if_fail(row_index >= 0, FALSE);

  if (row_index == -1 || row_index > priv->extra + priv->count)
    return FALSE;

  iter->user_data2 = GINT_TO_POINTER(row_index + 1);

  return TRUE;
}

static gboolean
osso_abook_list_store_iter_children(GtkTreeModel *tree_model, GtkTreeIter *iter,
                                    GtkTreeIter *parent)
{
  return osso_abook_list_iter_nth_child(tree_model, iter, parent, 0);
}

static gboolean
osso_abook_list_store_iter_has_child(GtkTreeModel *tree_model,
                                     GtkTreeIter *iter)
{
  return iter == NULL;
}

static int
osso_abook_list_store_iter_n_children(GtkTreeModel *tree_model,
                                      GtkTreeIter *iter)
{
  OssoABookListStore *store = (OssoABookListStore *)tree_model;
  OssoABookListStorePrivate *priv = store->priv;

  if (iter)
    return 0;

  return priv->count + priv->extra;
}

static void
osso_abook_list_store_gtk_tree_model_iface_init(GtkTreeModelIface *g_iface,
                                                gpointer iface_data)
{
  g_iface->iter_nth_child = osso_abook_list_iter_nth_child;
  g_iface->get_flags = osso_abook_list_store_get_flags;
  g_iface->get_n_columns = osso_abook_list_store_get_n_columns;
  g_iface->get_column_type = osso_abook_list_store_get_column_type;
  g_iface->get_iter = osso_abook_list_model_get_iter;
  g_iface->get_path = osso_abook_list_store_get_path;
  g_iface->get_value = osso_abook_list_store_get_value;
  g_iface->iter_next = osso_abook_list_store_iter_next;
  g_iface->iter_children = osso_abook_list_store_iter_children;
  g_iface->iter_has_child = osso_abook_list_store_iter_has_child;
  g_iface->iter_n_children = osso_abook_list_store_iter_n_children;
}

static void
osso_abook_list_store_set_default_sort_func(GtkTreeSortable *sortable,
                                            GtkTreeIterCompareFunc func,
                                            gpointer data,
                                            GDestroyNotify destroy)
{
  OSSO_ABOOK_WARN("Not implemented");
}

static gboolean
osso_abook_list_store_get_sort_column_id(GtkTreeSortable *sortable,
                                         gint *sort_column_id,
                                         GtkSortType *order)
{
  if (sort_column_id)
    *sort_column_id = -1;

  if (order)
    *order = GTK_SORT_ASCENDING;

  return FALSE;
}

static void
osso_abook_list_store_gtk_tree_sortable_iface_init(
    GtkTreeSortableIface *g_iface, gpointer iface_data)
{
  g_iface->set_default_sort_func = osso_abook_list_store_set_default_sort_func;
  g_iface->get_sort_column_id = osso_abook_list_store_get_sort_column_id;
}

typedef gboolean (*row_get_iter_fn)(OssoABookRowModel *, gconstpointer,
                                    GtkTreeIter *);
typedef gpointer (*iter_get_row) (OssoABookRowModel *model, GtkTreeIter *iter);

static void
osso_abook_list_store_osso_abook_row_model_iface_init(
    OssoABookRowModelIface *g_iface, gpointer iface_data)
{
  g_iface->row_get_iter = (row_get_iter_fn)osso_abook_list_store_row_get_iter;
  g_iface->iter_get_row = (iter_get_row)osso_abook_list_store_iter_get_row;
}

G_DEFINE_ABSTRACT_TYPE_WITH_CODE(
  OssoABookListStore,
  osso_abook_list_store,
  G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                        osso_abook_list_store_gtk_tree_model_iface_init);
  G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_SORTABLE,
                        osso_abook_list_store_gtk_tree_sortable_iface_init);
  G_IMPLEMENT_INTERFACE(OSSO_ABOOK_TYPE_ROW_MODEL,
                        osso_abook_list_store_osso_abook_row_model_iface_init);
  G_ADD_PRIVATE (OssoABookListStore);
);

static void
destroy_array(gpointer data)
{
  g_array_free(data, TRUE);
}

static int
osso_abook_list_store_sort_name(const OssoABookListStoreRow *row_a,
                                const OssoABookListStoreRow *row_b,
                                gpointer user_data)
{
  int rv = 0;

  if (row_a)
  {
    if (row_b)
    {
      rv = osso_abook_contact_collate(row_a->contact, row_b->contact,
                                      GPOINTER_TO_INT(user_data));

      if (!rv)
        rv = osso_abook_contact_uid_compare(row_a->contact, row_b->contact);
    }
    else
      rv = 1;

  }
  else if (row_b)
    rv = -1;

  return rv;
}

static int
osso_abook_list_store_sort_presence(const OssoABookListStoreRow *row_a,
                                    const OssoABookListStoreRow *row_b,
                                    gpointer user_data)
{
  int rv = osso_abook_presence_compare_for_display(
        OSSO_ABOOK_PRESENCE(row_a->contact),
        OSSO_ABOOK_PRESENCE(row_b->contact));

  if (!rv)
    rv = osso_abook_list_store_sort_name(row_a, row_b, user_data);

  return rv;
}

static void
osso_abook_list_store_set_sort_func_by_order(
    OssoABookListStore *list_store, OssoABookContactOrder contact_order,
    OssoABookNameOrder name_order)
{
  OssoABookListStoreCompareFunc func = NULL;

  if (contact_order == OSSO_ABOOK_CONTACT_ORDER_NAME)
    func = osso_abook_list_store_sort_name;
  else if (contact_order == OSSO_ABOOK_CONTACT_ORDER_PRESENCE)
    func = osso_abook_list_store_sort_presence;
  else if (contact_order != OSSO_ABOOK_CONTACT_ORDER_NONE)
    g_warning("%s: invalid contact order %d", __FUNCTION__, contact_order);

  osso_abook_list_store_set_sort_func(
        list_store, func, GINT_TO_POINTER(name_order), NULL);
}

static void
osso_abook_list_store_init(OssoABookListStore *list_store)
{
  OssoABookListStorePrivate *priv =
      osso_abook_list_store_get_instance_private(list_store);

  priv->balloon_offset = G_MAXINT;
  priv->roster = NULL;
  priv->name_order = osso_abook_settings_get_name_order();
  priv->names =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, destroy_array);
  priv->hash2 =
      g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);

  list_store->priv = priv;

  osso_abook_list_store_set_sort_func_by_order(
        list_store, osso_abook_settings_get_contact_order(), priv->name_order);
}

enum
{
  PROP_LOADING = 1,
  PROP_ROSTER,
  PROP_BOOK_VIEW,
  PROP_CONTACT_ORDER,
  PROP_NAME_ORDER,
  PROP_ROW_COUNT,
  PROP_PRE_ALLOCATED_ROWS
};

static void
osso_abook_list_store_dispose(GObject *object)
{
  OssoABookListStore *store = OSSO_ABOOK_LIST_STORE(object);

  osso_abook_list_store_set_roster(store, NULL);
  OSSO_ABOOK_LIST_STORE_GET_CLASS(store)->clear(store);
  osso_abook_list_store_set_sort_func(store, NULL, NULL, NULL);
  G_OBJECT_CLASS(osso_abook_list_store_parent_class)->dispose(object);
}

static void
osso_abook_list_store_finalize(GObject *object)
{
  OssoABookListStore *store = OSSO_ABOOK_LIST_STORE(object);
  OssoABookListStorePrivate *priv = store->priv;

  g_warn_if_fail(priv->count == 0);
  g_warn_if_fail(priv->extra == 0);

  if (priv->update_contacts_id)
    g_source_remove(priv->update_contacts_id);

  if (priv->idle_sort_id)
    g_source_remove(priv->idle_sort_id);

  if (priv->sort_destroy)
    priv->sort_destroy(priv->sort_data);

  if (priv->group_sort_destroy)
    priv->group_sort_destroy(priv->group_sort_data);

  g_hash_table_unref(priv->hash2);
  g_hash_table_unref(priv->names);
  g_free(priv->rows);

  G_OBJECT_CLASS(osso_abook_list_store_parent_class)->finalize(object);
}

static void
osso_abook_list_store_get_property(GObject *object, guint property_id,
                                   GValue *value, GParamSpec *pspec)
{
  OssoABookListStore *store = OSSO_ABOOK_LIST_STORE(object);

  switch (property_id)
  {
    case PROP_LOADING:
      g_value_set_boolean(value, store->priv->roster_is_running);
      break;
    case PROP_ROSTER:
      g_value_set_object(value, store->priv->roster);
      break;
    case PROP_BOOK_VIEW:
      g_value_set_object(value, osso_abook_list_store_get_book_view(store));
      break;
    case PROP_CONTACT_ORDER:
      g_value_set_enum(value, osso_abook_list_store_get_contact_order(store));
      break;
    case PROP_NAME_ORDER:
      g_value_set_enum(value, osso_abook_list_store_get_name_order(store));
      break;
    case PROP_ROW_COUNT:
      g_value_set_uint(value, store->priv->extra + store->priv->count);
      break;
    case PROP_PRE_ALLOCATED_ROWS:
      g_value_set_uint(value, store->priv->extra);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_list_store_set_property(GObject *object, guint property_id,
                                   const GValue *value, GParamSpec *pspec)
{
  OssoABookListStore *store = OSSO_ABOOK_LIST_STORE(object);

  switch (property_id)
  {
    case PROP_ROSTER:
      osso_abook_list_store_set_roster(store, g_value_get_object(value));
      break;
    case PROP_BOOK_VIEW:
      osso_abook_list_store_set_book_view(store, g_value_get_object(value));
      break;
    case PROP_CONTACT_ORDER:
      osso_abook_list_store_set_contact_order(store, g_value_get_enum(value));
      break;
    case PROP_NAME_ORDER:
      osso_abook_list_store_set_name_order(store, g_value_get_enum(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_list_store_class_init(OssoABookListStoreClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
/*
  klass->row_added = osso_abook_list_store_row_added;
  klass->row_removed = osso_abook_list_store_row_removed;
  klass->contact_changed = osso_abook_list_store_real_contact_changed;
  klass->clear = osso_abook_list_store_clear;
  klass->row_type = NULL;
*/
  object_class->set_property = osso_abook_list_store_set_property;
  object_class->get_property = osso_abook_list_store_get_property;
  object_class->finalize = osso_abook_list_store_finalize;
  object_class->dispose = osso_abook_list_store_dispose;

  g_object_class_install_property(
        object_class, PROP_LOADING,
        g_param_spec_boolean(
                 "loading",
                 "Loading",
                 "Indicator if the list-store still is loading contacts.",
                 FALSE,
                 GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_ROSTER,
        g_param_spec_object(
                 "roster",
                 "Roster",
                 "The contact roster backing this list store.",
                 OSSO_ABOOK_TYPE_ROSTER,
                 GTK_PARAM_READWRITE));

  g_object_class_install_property(
        object_class, PROP_BOOK_VIEW,
        g_param_spec_object(
                 "book-view",
                 "Book view",
                 "The EBookView backing this list store.",
                 E_TYPE_BOOK_VIEW,
                 GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_CONTACT_ORDER,
        g_param_spec_enum(
                  "contact-order",
                  "Contact Order",
                  "Contact sort order used by the model",
                  OSSO_ABOOK_TYPE_CONTACT_ORDER,
                  0,
                  GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_NAME_ORDER,
        g_param_spec_enum(
                  "name-order",
                  "Name Order",
                  "Name sort order used by the model",
                  OSSO_ABOOK_TYPE_NAME_ORDER,
                  0,
                  GTK_PARAM_READWRITE));

  g_object_class_install_property(
        object_class, PROP_ROW_COUNT,
        g_param_spec_uint(
                  "row-count",
                  "Row Count",
                  "Current number of rows",
                  0,
                  G_MAXUINT,
                  0,
                  GTK_PARAM_READABLE));

  g_object_class_install_property(
        object_class, PROP_PRE_ALLOCATED_ROWS,
        g_param_spec_uint(
                  "pre-allocated-rows",
                  "Pre-Allocated Rows",
                  "Current number of pre-allocated rows",
                          0,
                  G_MAXUINT,
                  0,
                  GTK_PARAM_READABLE));
}

static void
osso_abook_list_store_move_rows(OssoABookListStore *store, guint src_idx,
                                guint dst_idx, gint n)
{
  OssoABookListStorePrivate *priv = store->priv;
  OssoABookListStoreRow **rows = priv->rows;

  if (src_idx >= dst_idx)
  {
    if (src_idx > dst_idx)
    {
      OssoABookListStoreRow **first = &rows[src_idx];
      OssoABookListStoreRow **dst = &rows[n + src_idx - 1];

      if (dst >= first)
      {
        OssoABookListStoreRow **src = &rows[n + dst_idx - 1];
        do
        {
          OssoABookListStoreRow *row = *src;

          g_warn_if_fail(NULL == *dst);
          g_warn_if_fail(NULL != *src);

          *dst = row;
          *src = NULL;
          row = *dst;
          dst--;
          src--;
          row->offset += src_idx - dst_idx;
        }
        while (first <= dst);
      }
    }
  }
  else
  {
    OssoABookListStoreRow **dst = &rows[src_idx];
    OssoABookListStoreRow **last = &dst[n];

    if (dst < last)
    {
      OssoABookListStoreRow **src = &rows[dst_idx];

      do
      {
        OssoABookListStoreRow *row = *src;

        g_warn_if_fail(NULL == *dst);
        g_warn_if_fail(NULL != *src);

        *dst = row;
        *src = NULL;
        row = *dst;
        dst++;
        src++;
        row->offset -= dst_idx - src_idx;
      }
      while (last > dst);
    }
  }
}

static void
osso_abook_list_store_move_baloon(OssoABookListStore *store)
{
  OssoABookListStorePrivate *priv = store->priv;

  if (priv->balloon_size)
  {
    osso_abook_list_store_move_rows(store,
                                    priv->balloon_offset,
                                    priv->balloon_offset + priv->balloon_size,
                                    priv->count - priv->balloon_offset);
    priv->balloon_offset = G_MAXINT;
    priv->balloon_size = 0;
  }
}

static int
osso_abook_list_store_sort(gconstpointer a, gconstpointer b, gpointer user_data)
{
  const OssoABookListStoreRow *row_a = a;
  const OssoABookListStoreRow *row_b = b;
  OssoABookListStore *store = user_data;
  OssoABookListStorePrivate *priv = store->priv;
  int rv;

  if (!priv->group_sort_func ||
      (rv = priv->group_sort_func(row_a, row_b, priv->group_sort_data)) == 0)
  {
    if (priv->sort_func)
      rv = priv->sort_func(row_a, row_b, priv->sort_data);
    else
      rv = 0;
  }

  return rv;
}

static gboolean
idle_sort_cb(gpointer user_data)
{
  OssoABookListStore *store = user_data;
  OssoABookListStorePrivate *priv = store->priv;
  gint new_order[priv->count];
  OssoABookListStoreRow **rows = priv->rows;
  GtkTreePath *path;
  int i;

  priv->idle_sort_id = 0;
  osso_abook_list_store_move_baloon(store);
  g_qsort_with_data(rows, priv->count, sizeof(rows[0]),
                    osso_abook_list_store_sort, store);

  for (i = 0; i < priv->count; i++)
  {
    new_order[i] = rows[i]->offset;
    rows[i]->offset = i;
  }

  path = gtk_tree_path_new();
  gtk_tree_model_rows_reordered((GtkTreeModel *)store, path, 0, new_order);
  gtk_tree_path_free(path);

  return FALSE;
}

static void
osso_abook_list_store_idle_sort(OssoABookListStore *store)
{
  OssoABookListStorePrivate *priv = store->priv;

  if (!priv->idle_sort_id)
    priv->idle_sort_id = gdk_threads_add_idle(idle_sort_cb, store);
}

void
osso_abook_list_store_set_sort_func(OssoABookListStore *store,
                                    OssoABookListStoreCompareFunc callback,
                                    gpointer user_data,
                                    GDestroyNotify destroy_data)
{
  OssoABookListStorePrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_LIST_STORE (store));

  priv = store->priv;

  if (priv->sort_destroy)
    priv->sort_destroy(priv->sort_data);

  priv->sort_destroy = destroy_data;
  priv->sort_func = callback;
  priv->sort_data = user_data;
  osso_abook_list_store_idle_sort(store);
  g_object_notify(G_OBJECT(store), "contact-order");
}

void
osso_abook_list_store_set_group_sort_func(
    OssoABookListStore *store, OssoABookListStoreCompareFunc callback,
    gpointer user_data, GDestroyNotify destroy_data)
{
  OssoABookListStorePrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_LIST_STORE (store));

  priv = store->priv;

  if (priv->group_sort_destroy)
    priv->group_sort_destroy(priv->group_sort_data);

  priv->group_sort_destroy = destroy_data;
  priv->group_sort_func = callback;
  priv->group_sort_data = user_data;
  osso_abook_list_store_idle_sort(store);
}

OssoABookNameOrder
osso_abook_list_store_get_name_order(OssoABookListStore *store)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_LIST_STORE (store),
                       OSSO_ABOOK_NAME_ORDER_FIRST);

  return store->priv->name_order;
}

void
osso_abook_list_store_set_name_order(OssoABookListStore *store,
                                     OssoABookNameOrder order)
{
  OssoABookListStorePrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_LIST_STORE (store));

  priv = store->priv;

  if (osso_abook_list_store_get_name_order(store) != order)
  {
    priv = store->priv;
    priv->name_order = order;
    osso_abook_list_store_set_sort_func_by_order(
          store, osso_abook_list_store_get_contact_order(store),
          priv->name_order);

    if (priv->roster && !osso_abook_roster_is_running(priv->roster))
      osso_abook_roster_set_name_order(priv->roster, order);

    g_object_notify(G_OBJECT(store), "name-order");
  }
}

gboolean
osso_abook_list_store_is_loading(OssoABookListStore *store)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_LIST_STORE (store), FALSE);

  return store->priv->roster_is_running;
}

OssoABookContactOrder
osso_abook_list_store_get_contact_order(OssoABookListStore *store)
{
  OssoABookListStoreCompareFunc sort_func;

  g_return_val_if_fail(OSSO_ABOOK_IS_LIST_STORE (store),
                       OSSO_ABOOK_CONTACT_ORDER_NONE);

  sort_func = store->priv->sort_func;

  if (sort_func == osso_abook_list_store_sort_name)
    return OSSO_ABOOK_CONTACT_ORDER_NAME;
  else if (sort_func == osso_abook_list_store_sort_presence)
    return OSSO_ABOOK_CONTACT_ORDER_PRESENCE;
  else if (sort_func)
     return OSSO_ABOOK_CONTACT_ORDER_CUSTOM;
  else
    return OSSO_ABOOK_CONTACT_ORDER_NONE;
}

void
osso_abook_list_store_set_contact_order(OssoABookListStore *store,
                                        OssoABookContactOrder order)
{
  g_return_if_fail(OSSO_ABOOK_IS_LIST_STORE (store));

  if (osso_abook_list_store_get_contact_order(store) != order)
  {
    osso_abook_list_store_set_sort_func_by_order(
          store, order, osso_abook_list_store_get_name_order(store));
  }
}
