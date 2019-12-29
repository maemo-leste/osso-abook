#include <hildon/hildon.h>
#include <gtk/gtkprivate.h>

#include <string.h>

#include "config.h"

#include "osso-abook-filter-model.h"
#include "osso-abook-row-model.h"

enum
{
  PROP_BASE_MODEL = 1,
  PROP_TEXT,
  PROP_PREFIX,
  PROP_GROUP
};

struct _OssoABookFilterModelPrivate
{
  OssoABookListStore *base_model;
  gchar *text;
  GList *bits;
  gboolean prefix : 1;
  gboolean show_unnamed : 1;
  OssoABookGroup *group;
  gulong refilter_contact_id;
  gulong refilter_group_id;
  gboolean refilter_freezed;
  gboolean refilter_requested;
  gulong sort_column_changed_id;
  GtkTreeModelFilterVisibleFunc visible_cb;
  gpointer visible_data;
  GDestroyNotify visible_destroy;
};

typedef struct _OssoABookFilterModelPrivate OssoABookFilterModelPrivate;

static gboolean
osso_abook_filter_model_has_default_sort_func(GtkTreeSortable *sortable)
{
  GtkTreeModel *model =
      gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(sortable));

  return gtk_tree_sortable_has_default_sort_func(GTK_TREE_SORTABLE(model));
}

static gboolean
osso_abook_filter_model_get_sort_column_id(GtkTreeSortable *sortable,
                                           gint *sort_column_id,
                                           GtkSortType *order)
{
  GtkTreeModel *model =
      gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(sortable));

  return gtk_tree_sortable_get_sort_column_id(GTK_TREE_SORTABLE(model),
                                              sort_column_id, order);
}

static void
osso_abook_filter_model_set_sort_column_id(GtkTreeSortable *sortable,
                                           gint sort_column_id,
                                           GtkSortType order)
{
  GtkTreeModel *model =
      gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(sortable));

  gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(model),
                                       sort_column_id, order);
}

static void
osso_abook_filter_model_set_sort_func(GtkTreeSortable *sortable,
                                      gint sort_column_id,
                                      GtkTreeIterCompareFunc func,
                                      gpointer data, GDestroyNotify destroy)
{
  GtkTreeModel *model =
      gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(sortable));

  gtk_tree_sortable_set_sort_func(GTK_TREE_SORTABLE(model), sort_column_id,
                                  func, data, destroy);
}

static void
osso_abook_filter_model_set_default_sort_func(GtkTreeSortable *sortable,
                                              GtkTreeIterCompareFunc func,
                                              gpointer data,
                                              GDestroyNotify destroy)
{
  GtkTreeModel *model =
      gtk_tree_model_filter_get_model(GTK_TREE_MODEL_FILTER(sortable));

  gtk_tree_sortable_set_default_sort_func(GTK_TREE_SORTABLE(model), func, data,
                                          destroy);
}

static void
osso_abook_filter_model_gtk_tree_sortable_iface_init(
    GtkTreeSortableIface *g_iface, gpointer iface_data)
{
  g_iface->has_default_sort_func =
      osso_abook_filter_model_has_default_sort_func;
  g_iface->get_sort_column_id = osso_abook_filter_model_get_sort_column_id;
  g_iface->set_sort_column_id = osso_abook_filter_model_set_sort_column_id;
  g_iface->set_sort_func = osso_abook_filter_model_set_sort_func;
  g_iface->set_default_sort_func =
      osso_abook_filter_model_set_default_sort_func;
}

static gboolean
osso_abook_filter_model_row_get_iter(OssoABookRowModel *model,
                                     gconstpointer row, GtkTreeIter *iter)
{
  OssoABookFilterModelPrivate *priv = ((OssoABookFilterModel *)model)->priv;
  gboolean rv;
  GtkTreeIter child_iter;

  g_return_val_if_fail(priv != NULL, FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  rv = osso_abook_list_store_row_get_iter(priv->base_model, row, &child_iter);

  if (rv)
  {
    rv = gtk_tree_model_filter_convert_child_iter_to_iter(
          (GtkTreeModelFilter *)model, iter, &child_iter);
  }

  return rv;
}

static gpointer
osso_abook_filter_model_iter_get_row(OssoABookRowModel *model,
                                     GtkTreeIter *iter)
{
  OssoABookFilterModelPrivate *priv = ((OssoABookFilterModel *)model)->priv;
  GtkTreeIter child_iter;
  gpointer rv;

  g_return_val_if_fail(priv != NULL, NULL);
  g_return_val_if_fail(iter != NULL, NULL);

  gtk_tree_model_filter_convert_iter_to_child_iter(
        (GtkTreeModelFilter *)model, &child_iter, iter);
  rv = osso_abook_list_store_iter_get_row(priv->base_model, &child_iter);

  return rv;
}

static void
osso_abook_filter_model_osso_abook_row_model_iface_init(
    OssoABookRowModelIface *g_iface, gpointer iface_data)
{
  g_iface->row_get_iter = osso_abook_filter_model_row_get_iter;
  g_iface->iter_get_row = osso_abook_filter_model_iter_get_row;
}

G_DEFINE_TYPE_WITH_CODE(
  OssoABookFilterModel,
  osso_abook_filter_model,
  GTK_TYPE_TREE_MODEL_FILTER,
  G_IMPLEMENT_INTERFACE(
      GTK_TYPE_TREE_SORTABLE,
      osso_abook_filter_model_gtk_tree_sortable_iface_init);
  G_IMPLEMENT_INTERFACE(
    OSSO_ABOOK_TYPE_ROW_MODEL,
    osso_abook_filter_model_osso_abook_row_model_iface_init);
  G_ADD_PRIVATE(OssoABookFilterModel);
);

static void
osso_abook_filter_model_real_set_text(OssoABookFilterModel *model,
                                      const char *text, gboolean prefix);

static void
disconnect_base_model(OssoABookListStore *base_model, gulong handler)
{
  if (g_signal_handler_is_connected(base_model, handler))
    g_signal_handler_disconnect(base_model, handler);
}

static void
remove_group(OssoABookFilterModelPrivate *model)
{

  if (model->group)
  {
    g_signal_handler_disconnect(model->group, model->refilter_contact_id);
    g_object_unref(model->group);
    model->group = NULL;
  }
}

static void
osso_abook_filter_model_dispose(GObject *object)
{
  OssoABookFilterModel *model = (OssoABookFilterModel *)object;
  OssoABookFilterModelPrivate *priv = model->priv;

  remove_group(priv);

  if (priv->sort_column_changed_id)
  {
    disconnect_base_model(priv->base_model, priv->sort_column_changed_id);
    priv->sort_column_changed_id = 0;
  }

  G_OBJECT_CLASS(osso_abook_filter_model_parent_class)->dispose(object);
}

static void
clear_bits(OssoABookFilterModelPrivate *priv)
{
  for (GList *l = priv->bits; l; priv->bits = l)
  {
    g_free(l->data);
    l = g_list_delete_link(priv->bits, priv->bits);
  }
}

static void
osso_abook_filter_model_fimalize(GObject *object)
{
  OssoABookFilterModel *model = (OssoABookFilterModel *)object;
  OssoABookFilterModelPrivate *priv = model->priv;

  if (priv->visible_destroy)
    priv->visible_destroy(priv->visible_data);

  g_free(priv->text);
  clear_bits(priv);
  G_OBJECT_CLASS(osso_abook_filter_model_parent_class)->finalize(object);
}

static void
osso_abook_filter_model_get_property(GObject *object, guint property_id,
                                     GValue *value, GParamSpec *pspec)
{
  OssoABookFilterModel *model = (OssoABookFilterModel *)object;
  OssoABookFilterModelPrivate *priv = model->priv;

  switch (property_id)
  {
    case PROP_BASE_MODEL:
      g_value_set_object(value, priv->base_model);
      break;
    case PROP_TEXT:
      g_value_set_string(value, priv->text);
      break;
    case PROP_PREFIX:
      g_value_set_boolean(value, priv->prefix);
      break;
    case PROP_GROUP:
      g_value_set_object(value, priv->group);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
sort_column_changed_cb(GtkTreeSortable *sortable, gpointer user_data)
{
  g_signal_emit_by_name(user_data, "sort-column-changed");
}

static void
osso_abook_filter_model_set_base_model(OssoABookFilterModel *model,
                                       OssoABookListStore *base_model)
{
  OssoABookFilterModelPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model));
  g_return_if_fail(OSSO_ABOOK_IS_LIST_STORE (base_model));

  priv = model->priv;
  priv->base_model = base_model;
  priv->sort_column_changed_id =
      g_signal_connect_object(G_OBJECT(base_model), "sort-column-changed",
                              G_CALLBACK(sort_column_changed_cb),
                              G_OBJECT(model), 0);
}

static void
osso_abook_filter_model_set_property(GObject *object, guint property_id,
                                     const GValue *value, GParamSpec *pspec)
{
  OssoABookFilterModel *model = (OssoABookFilterModel *)object;
  OssoABookFilterModelPrivate *priv = model->priv;

  switch (property_id)
  {
    case PROP_BASE_MODEL:
      osso_abook_filter_model_set_base_model(model, g_value_get_object(value));
      break;
    case PROP_TEXT:
      osso_abook_filter_model_real_set_text(model, g_value_get_string(value),
                                            priv->prefix);
      break;
    case PROP_PREFIX:
      if (priv->prefix != g_value_get_boolean(value))
      {
        priv->prefix = !priv->prefix;
        g_object_notify(G_OBJECT(model), "prefix");
      }
      break;
    case PROP_GROUP:
      osso_abook_filter_model_set_group(model, g_value_get_object(value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_filter_model_class_init(OssoABookFilterModelClass *klass)
{
  GObjectClass *object_class;

  object_class = G_OBJECT_CLASS(klass);
  object_class->set_property = osso_abook_filter_model_set_property;
  object_class->get_property = osso_abook_filter_model_get_property;
  object_class->dispose = osso_abook_filter_model_dispose;
  object_class->finalize = osso_abook_filter_model_fimalize;

  g_object_class_install_property(
        object_class, PROP_BASE_MODEL,
        g_param_spec_object(
                 "base-model",
                 "Base model",
                 "The contained OssoABookListStore.",
                 OSSO_ABOOK_TYPE_LIST_STORE,
                 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
        object_class, PROP_TEXT,
        g_param_spec_string(
                 "text",
                 "Text",
                 "The text to be filtered on.",
                 NULL,
                 GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_PREFIX,
        g_param_spec_boolean(
                 "prefix",
                 "Prefix",
                 "Wether or not only prefixes are matched.",
                 FALSE,
                 GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_GROUP,
        g_param_spec_object(
                 "group",
                 "Group",
                 "The group to be filtered on.",
                 OSSO_ABOOK_TYPE_GROUP,
                 GTK_PARAM_READWRITE));
}

static gunichar *
_osso_abook_utf8_strcasestrip(const char *text)
{
  gunichar *buf = NULL;
  size_t bufsize;
  size_t j = 0;
  size_t len;
  const char *p = text;
  const char *text_end;

  if (!text || !*text)
    return NULL;

  bufsize = len = strlen(text);
  text_end = &text[len];

  buf = g_new(gunichar, bufsize + 1);

  while (p < text_end)
  {
    gsize i;
    gunichar d[G_UNICHAR_MAX_DECOMPOSITION_LENGTH];
    gsize l = g_unichar_fully_decompose(
          g_utf8_get_char(p), FALSE, d, G_N_ELEMENTS(d));

    for (i = 0; i < l; i++)
    {
      gunichar uch = d[i];

      if (j == bufsize)
      {
        bufsize += G_UNICHAR_MAX_DECOMPOSITION_LENGTH;
        buf = g_realloc(buf, (bufsize + 1) * sizeof(gunichar));
      }

      switch(g_unichar_type(uch))
      {
        case G_UNICODE_TITLECASE_LETTER:
        case G_UNICODE_UPPERCASE_LETTER:
          buf[j++] = g_unichar_tolower(uch);
          break;
        case G_UNICODE_COMBINING_MARK:
        case G_UNICODE_ENCLOSING_MARK:
        case G_UNICODE_NON_SPACING_MARK:
          break;
        default:
          buf[j++] = uch;
          break;
      }
    }

    if (!(p = g_utf8_next_char(p)))
      break;
  }

  buf[j] = 0;

  return buf;
}

static const gchar *
_osso_abook_utf8_strstrcasestrip(const char *str1, const gunichar *str2,
                                 gsize *bytes_read)
{
  const char *p = str1;
  const gunichar *q = str2;
  const char *text_end;
  const gchar *found = NULL;

  if (!str1 || !*str1 || !str2 || !*str2)
    return NULL;

  text_end = &str1[strlen(str1)];

  while (p < text_end)
  {
    gsize i;
    gunichar d[G_UNICHAR_MAX_DECOMPOSITION_LENGTH];
    gsize l = g_unichar_fully_decompose(
          g_utf8_get_char(p), FALSE, d, G_N_ELEMENTS(d));

    for (i = 0; i < l; i++)
    {
      gunichar uch = d[i];

      switch(g_unichar_type(uch))
      {
        case G_UNICODE_TITLECASE_LETTER:
        case G_UNICODE_UPPERCASE_LETTER:
          uch = g_unichar_tolower(uch);
          break;
        case G_UNICODE_COMBINING_MARK:
        case G_UNICODE_ENCLOSING_MARK:
        case G_UNICODE_NON_SPACING_MARK:
          continue;
        default:
          break;
      }

      if (*q == uch)
      {
        q++;

        if (!found)
          found = p;
      }
      else
      {
        q = str2;
        found = NULL;
      }

      if (!*q)
        goto out;
    }

    if (!(p = g_utf8_next_char(p)))
      break;
  }

out:

  if (bytes_read)
    *bytes_read = g_utf8_next_char(p) - str1;

  return found;
}

static gboolean
_osso_abook_utf8_strstartswithcasestrip(const char *str1, const gunichar *str2,
                                        gsize *bytes_read)
{
  const char *p = str1;
  const gunichar *q = str2;
  const char *text_end;
  gboolean rv = FALSE;

  if (!str1 || !*str1 || !str2 || !*str2)
    return FALSE;

  text_end = &str1[strlen(str1)];

  while (p < text_end)
  {
    gsize i;
    gunichar d[G_UNICHAR_MAX_DECOMPOSITION_LENGTH];
    gsize l = g_unichar_fully_decompose(
          g_utf8_get_char(p), FALSE, d, G_N_ELEMENTS(d));

    for (i = 0; i < l; i++)
    {
      gunichar uch = d[i];

      switch(g_unichar_type(uch))
      {
        case G_UNICODE_TITLECASE_LETTER:
        case G_UNICODE_UPPERCASE_LETTER:
          uch = g_unichar_tolower(uch);
          break;
        case G_UNICODE_COMBINING_MARK:
        case G_UNICODE_ENCLOSING_MARK:
        case G_UNICODE_NON_SPACING_MARK:
          continue;
        default:
          break;
      }

      if (*q++ != uch)
        goto out;

      if (!*q)
      {
        rv = TRUE;
        goto out;
      }
    }

    if (!(p = g_utf8_next_char(p)))
      break;
  }

out:

  if (rv && bytes_read)
    *bytes_read = g_utf8_next_char(p) - str1;

  return rv;
}

static void
osso_abook_filter_model_refilter(OssoABookFilterModel *model,
                                 OssoABookFilterModelPrivate *priv)
{
  if (!priv->refilter_freezed)
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(model));
  else
    priv->refilter_requested = TRUE;
}

static void
osso_abook_filter_model_real_set_text(OssoABookFilterModel *model,
                                      const char *text, gboolean prefix)
{
  OssoABookFilterModelPrivate *priv = model->priv;
  gboolean old_prefix;

  g_free(priv->text);
  priv->text = NULL;
  clear_bits(priv);

  if (text &&  *text )
  {
    gchar **bits;
    gchar **bit;
    gunichar *bit_lcase;

    priv->text = g_utf8_normalize(text, -1, G_NORMALIZE_DEFAULT);
    bits = g_strsplit(priv->text, " ", -1);

    for (bit = bits; *bit; bit++)
    {
      bit_lcase = _osso_abook_utf8_strcasestrip(*bit);

      if (bit_lcase)
        priv->bits = g_list_prepend(priv->bits, bit_lcase);

      g_free(*bit);
    }

    g_free(bits);
  }

  old_prefix = priv->prefix;
  priv->prefix = prefix ;

  if (priv->text)
  {
    gchar *unnamed_fold = g_utf8_casefold(_("addr_li_unnamed_contact"), -1);
    gchar *name_fold = g_utf8_casefold(priv->text, -1);

    priv->show_unnamed = g_str_has_prefix(unnamed_fold, name_fold);
    g_free(name_fold);
    g_free(unnamed_fold);
  }
  else
    priv->show_unnamed = FALSE;

  osso_abook_filter_model_refilter(model, priv);

  if (old_prefix != prefix)
  {
    g_object_notify(G_OBJECT(model), "prefix");
  }
  g_object_notify(G_OBJECT(model), "text");
}

static gboolean
visible_func(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  OssoABookFilterModelPrivate *priv = data;
  OssoABookListStoreRow *row;
  const char *name;

  if (priv->visible_cb && !priv->visible_cb(model, iter, priv->visible_data))
    return FALSE;

  row = osso_abook_list_store_iter_get_row((OssoABookListStore *)model, iter);

  g_return_val_if_fail(row != NULL, FALSE);

  if (priv->group)
  {
    if (!osso_abook_group_includes_contact(priv->group, row->contact))
      return FALSE;
  }

  if (!priv->text)
    return TRUE;

  name = osso_abook_contact_get_name(row->contact);

  if (!name || !*name)
    return priv->show_unnamed;

  if (!priv->bits)
    return TRUE;

  if (!priv->prefix)
  {
    /* text match */
    for (GList *l = priv->bits; l; l = l->next)
    {
      if (!_osso_abook_utf8_strstrcasestrip(name, l->data, NULL))
        return FALSE;
    }
  }
  else
  {
    /* prefix match */
    const char *next = name;

    for (GList *l = priv->bits; l; l = l->next)
    {
      if (!_osso_abook_utf8_strstartswithcasestrip(next, l->data, NULL))
        return FALSE;
    }

    while ((next = strchr(next, ' ')))
    {
      next++;

      for (GList *l = priv->bits; l; l = l->next)
      {
        if (!_osso_abook_utf8_strstartswithcasestrip(next, l->data, NULL))
          return FALSE;
      }
    }
  }

  return TRUE;
}

static void
osso_abook_filter_model_init(OssoABookFilterModel *model)
{
  OssoABookFilterModelPrivate *priv =
      osso_abook_filter_model_get_instance_private(model);

  model->priv = priv;
  priv->base_model = NULL;
  priv->text = NULL;
  priv->prefix = FALSE;
  priv->show_unnamed= FALSE;
  priv->bits = NULL;
  priv->group = NULL;
  priv->refilter_freezed = FALSE;
  priv->refilter_requested = FALSE;
  gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(model),
                                         visible_func, priv, NULL);
  priv->sort_column_changed_id = 0;
}

OssoABookFilterModel *
osso_abook_filter_model_new(OssoABookListStore *child_model)
{
  return g_object_new(OSSO_ABOOK_TYPE_FILTER_MODEL,
                      "base-model", child_model,
                      "child-model", child_model,
                      NULL);
}

void
osso_abook_filter_model_set_visible_func(OssoABookFilterModel *model,
                                         GtkTreeModelFilterVisibleFunc func,
                                         gpointer data, GDestroyNotify destroy)
{
  OssoABookFilterModelPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model));

  priv = model->priv;

  if (priv->visible_destroy)
    priv->visible_destroy(priv->visible_data);

  priv->visible_cb = func;
  priv->visible_data = data;
  priv->visible_destroy = destroy;

  osso_abook_filter_model_refilter(model, priv);
}

OssoABookGroup *
osso_abook_filter_model_get_group(OssoABookFilterModel *model)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model), NULL);

  return model->priv->group;
}

const char *
osso_abook_filter_model_get_text(OssoABookFilterModel *model)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model), NULL);

  return model->priv->text;
}

void
osso_abook_filter_model_set_text(OssoABookFilterModel *model, const char *text)
{
  g_return_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model));

  osso_abook_filter_model_real_set_text(model, text, FALSE);
}

gboolean
osso_abook_filter_model_get_prefix(OssoABookFilterModel *model)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model), FALSE);

  return model->priv->prefix;
}

void
osso_abook_filter_model_set_prefix(OssoABookFilterModel *model,
                                   const char *prefix)
{
  g_return_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model));

  osso_abook_filter_model_real_set_text(model, prefix, TRUE);
}

void
osso_abook_filter_model_freeze_refilter(OssoABookFilterModel *model)
{
  g_return_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model));

  model->priv->refilter_freezed = TRUE;
}

void
osso_abook_filter_model_thaw_refilter(OssoABookFilterModel *model)
{
  OssoABookFilterModelPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model));

  priv = model->priv;

  if (priv->refilter_requested)
  {
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(model));
    priv->refilter_requested = FALSE;
  }

  priv->refilter_freezed = FALSE;
}

gboolean
osso_abook_filter_model_is_row_visible(OssoABookFilterModel *model,
                                       GtkTreeIter *iter)
{
  OssoABookFilterModelPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_FILTER_MODEL (model), FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);

  priv = model->priv;
  return visible_func(GTK_TREE_MODEL(priv->base_model), iter, priv);
}
