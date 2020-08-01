#include <gtk/gtkprivate.h>
#include <hildon/hildon.h>

#include <string.h>

#include "config.h"

#include "osso-abook-tree-view.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-debug.h"
#include "osso-abook-row-model.h"
#include "osso-abook-avatar-cache.h"
#include "osso-abook-util.h"
#include "osso-abook-roster.h"
#include "osso-abook-enums.h"
#include "osso-abook-waitable.h"

//#define MC_ACCOUNT

#ifndef MC_ACCOUNT
#pragma message("FIXME!!! - replace McAccout (with TpAccount?)")
#endif

struct _OssoABookTreeViewPrivate
{
  OssoABookListStore *base_model;
  OssoABookFilterModel *filter_model;
  gpointer waitable;
  OssoABookWaitableClosure *closure;
  GtkWidget *tree_view;
  GtkWidget *pannable_area;
  HildonUIMode ui_mode;
  gpointer aggregation_account;
  GtkWidget *event_box;
  gchar *empty_text;
  GtkTreeViewColumn *column;
  GtkCellRenderer *contact_name;
  GtkCellRenderer *contact_telephone;
  GtkCellRenderer *contact_presence;
  GtkCellRenderer *contact_avatar;
  guint sync_view_id;
  guint contact_order_notify_id;
  guint name_order_notify_id;
  gulong row_inserted_id;
  gulong row_deleted_id;
  gboolean show_contact_name;
  gboolean show_contact_avatar;
  gboolean show_contact_presence;
  gboolean show_contact_telephone;
  gboolean use_sim_avatar;
  guint sensitive_caps;
  gint avatar_radius;
  gboolean avatar_border;
  guint8 border_color[4];
  GHashTable *contact_data;
  guint sync_avatar_images_id;
  GQueue *row_queue1;
  GQueue *row_queue2;
  guint show_tree_id;
  int index;
};

typedef struct _OssoABookTreeViewPrivate OssoABookTreeViewPrivate;

enum
{
  PARAM_MODEL = 1,
  PARAM_BASE_MODEL,
  PARAM_FILTER_MODEL,
  PARAM_TREE_VIEW,
  PARAM_SHOW_CONTACT_NAME,
  PARAM_SHOW_CONTACT_AVATAR,
  PARAM_SHOW_CONTACT_PRESENCE,
  PARAM_SHOW_CONTACT_TELEPHONE,
  PARAM_USE_SIM_AVATAR,
  PARAM_SENSITIVE_CAPS,
  PARAM_EMPTY_TEXT,
  PARAM_HILDON_UI_MODE,
  PARAM_AGGREGATION_ACCOUNT
};

struct _OssoABookTreeViewContact
{
  OssoABookTreeView *view;
  OssoABookContact *contact;
  OssoABookAvatar *avatar;
  GdkPixbuf *avatar_image;
};

typedef struct _OssoABookTreeViewContact OssoABookTreeViewContact;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(
  OssoABookTreeView,
  osso_abook_tree_view,
  GTK_TYPE_VBOX
);

#define OSSO_ABOOK_TREE_VIEW_PRIVATE(view) \
                osso_abook_tree_view_get_instance_private(view)

static gchar *
_osso_abook_flags_to_string(GType flags_type, guint value)
{
  GFlagsClass *flags_class;
  GString *s;

  g_return_val_if_fail(G_TYPE_IS_FLAGS(flags_type), NULL);

  flags_class = g_type_class_ref(flags_type);

  g_return_val_if_fail(G_IS_FLAGS_CLASS(flags_class), NULL);


  s = g_string_new(0);
  g_string_append_c(s, '(');

  for (int i = 0; i < flags_class->n_values; i++)
  {
    GFlagsValue *v = &flags_class->values[i];

    if (value && v->value)
    {
      if (i > 1)
        g_string_append_c(s, '|');
    }

    g_string_append(s, v->value_nick);
  }

  g_string_append_c(s, ')');

  return g_string_free(s, FALSE);
}

static void
notify_avatar_image_cb(OssoABookContact *contact, GdkPixbuf *image,
                       OssoABookTreeViewContact *contact_data)
{
  g_hash_table_remove(contact_data->view->priv->contact_data,
                      contact_data->contact);
}

static void
contact_finalyzed_cb(gpointer data, GObject *contact)
{
  OssoABookTreeViewContact *contact_data = data;

  contact_data->contact = NULL;
  g_hash_table_remove(contact_data->view->priv->contact_data, contact);
}

static void
avatar_finalyzed_cb(gpointer data, GObject *avatar)
{
  OssoABookTreeViewContact *contact_data = data;

  contact_data->avatar = NULL;
  g_hash_table_remove(contact_data->view->priv->contact_data,
                      contact_data->contact);
}

static void
destroy_contact_data(gpointer data)
{
  OssoABookTreeViewContact *contact_data = data;

  if (!contact_data)
    return;

  if (contact_data->avatar_image)
    g_object_unref(contact_data->avatar_image);

  if (contact_data->contact)
  {
    g_signal_handlers_disconnect_matched(
          contact_data->contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
          0, 0, NULL, notify_avatar_image_cb, contact_data);

    g_object_weak_unref(
          G_OBJECT(contact_data->contact), contact_finalyzed_cb, contact_data);
  }

  if (contact_data->avatar)
  {
    g_signal_handlers_disconnect_matched(
          contact_data->avatar, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
          0, 0, NULL, notify_avatar_image_cb, contact_data);
    g_object_weak_unref(
          G_OBJECT(contact_data->avatar), avatar_finalyzed_cb, contact_data);
  }

  g_slice_free(OssoABookTreeViewContact, contact_data);
}

static void
osso_abook_tree_view_init(OssoABookTreeView *view)
{
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  view->priv = priv;
  priv->show_contact_presence = TRUE;
  priv->show_contact_name = TRUE;
  priv->aggregation_account = NULL;
  priv->avatar_radius = -1;
  priv->row_queue1 = g_queue_new();
  priv->row_queue2 = g_queue_new();
  priv->contact_data = g_hash_table_new_full(
        g_direct_hash, g_direct_equal, NULL, destroy_contact_data);
}

static void
free_row_references(OssoABookTreeView *view)
{
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  if (priv->sync_avatar_images_id)
  {
    g_source_remove(priv->sync_avatar_images_id);
    priv->sync_avatar_images_id = 0;
  }

  g_queue_foreach(priv->row_queue1, (GFunc)gtk_tree_row_reference_free, NULL);
  g_queue_clear(priv->row_queue1);
  g_queue_foreach(priv->row_queue2, (GFunc)gtk_tree_row_reference_free, NULL);
  g_queue_clear(priv->row_queue2);
}

static gboolean
get_cached_avatar_image(OssoABookTreeView *view,
                        const OssoABookContact *contact,
                        GdkPixbuf **avatar_image)
{
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  OssoABookTreeViewContact *contact_data = NULL;
  gboolean rv;

  rv = g_hash_table_lookup_extended(priv->contact_data, contact, NULL,
                                    (gpointer *)&contact_data);
  if (avatar_image && contact_data)
    *avatar_image = contact_data->avatar_image;

  return rv;
}

static GtkIconSize
get_icon_finger_size(OssoABookTreeViewPrivate *priv, gint *hildon_size)
{
  if (hildon_size)
    *hildon_size = hildon_get_icon_pixel_size(HILDON_ICON_SIZE_FINGER);;

  return HILDON_ICON_SIZE_FINGER;
}

static GdkPixbuf *
get_cached_icon(OssoABookTreeView *view, const gchar *icon_name, gint size)
{
  GHashTable *icon_cache =
      g_object_get_data(G_OBJECT(view), "osso-abook-icon-cache");
  GdkPixbuf *icon;

  if (!icon_cache)
  {
    icon_cache = g_hash_table_new_full(
                   g_str_hash,
                   g_str_equal,
                   (GDestroyNotify)g_free,
                   (GDestroyNotify)g_object_unref);

    g_object_set_data_full(G_OBJECT(view), "osso-abook-icon-cache",
                           icon_cache, (GDestroyNotify)g_hash_table_unref);
  }

  icon = g_hash_table_lookup(icon_cache, icon_name);

  if (!icon)
  {
    GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(view));
    GtkIconTheme *theme = gtk_icon_theme_get_for_screen(screen);

    icon = gtk_icon_theme_load_icon(theme, icon_name, size, 0, NULL);

    if ( !icon )
      return icon;

    g_hash_table_insert(icon_cache, g_strdup(icon_name), (gpointer)icon);
  }

  g_object_ref(icon);

  return icon;
}

static const guint8 *
get_avatar_border_color(OssoABookTreeViewPrivate *priv)
{
  if (priv->avatar_border)
    return priv->border_color;

  return NULL;
}

static GdkPixbuf *
get_avatar_fallback_image(OssoABookTreeView *view, OssoABookAvatar *avatar)
{
  return get_cached_icon(
        view, osso_abook_avatar_get_fallback_icon_name(avatar),
        hildon_get_icon_pixel_size(HILDON_ICON_SIZE_FINGER));
}

static GdkPixbuf *
create_avatar_image(OssoABookTreeView *view, OssoABookContact *contact)
{
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  OssoABookAvatar *avatar = OSSO_ABOOK_AVATAR(contact);
  GdkPixbuf *avatar_image;
  OssoABookTreeViewContact *contact_data;

#if MC_ACCOUNT
  if (priv->aggregation_account)
  {
    if (!osso_abook_avatar_is_user_selected(avatar))
    {
      GList *contacts = osso_abook_contact_find_roster_contacts_for_account(
            contact, 0, priv->aggregation_account->name);

      if (contacts)
        avatar = OSSO_ABOOK_AVATAR(contacts->data);

      g_list_free(contacts);
    }
  }
#endif

  if (!avatar)
    return NULL;

  if (priv->use_sim_avatar)
  {
    const gchar *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);
    const gchar *icon_name;
    gint size;

    if (g_str_equal(uid, "osso-abook-vmbx"))
      icon_name = "general_voicemail_avatar";
    else
      icon_name = "addressbook_sim_contacts_group";

    get_icon_finger_size(priv, &size);

    avatar_image = get_cached_icon(view, icon_name, size);
  }
  else
  {
    int size = HILDON_ICON_SIZE_FINGER;
    const guint8 *border_color = get_avatar_border_color(priv);

    avatar_image = osso_abook_avatar_get_image_rounded(
          avatar, size, size, TRUE, priv->avatar_radius, border_color);

    if (!avatar_image)
      avatar_image = get_avatar_fallback_image(view, avatar);
  }

  contact_data = g_slice_new0(OssoABookTreeViewContact);
  contact_data->avatar_image = avatar_image;

  if (avatar_image)
    g_object_ref(avatar_image);

  contact_data->view = view;
  contact_data->contact = contact;

  g_object_weak_ref(G_OBJECT(contact), contact_finalyzed_cb, contact_data);

  if (avatar != OSSO_ABOOK_AVATAR(contact))
  {
    contact_data->avatar = avatar;
    g_object_weak_ref(G_OBJECT(avatar), avatar_finalyzed_cb, contact_data);
  }

  g_signal_connect(contact, "notify::avatar-image",
                   G_CALLBACK(notify_avatar_image_cb), contact_data);

  if (contact_data->avatar)
  {
    g_signal_connect(avatar, "notify::avatar-image",
                     G_CALLBACK(notify_avatar_image_cb), contact_data);
  }

  g_hash_table_insert(priv->contact_data, contact, contact_data);

  if (avatar_image)
    g_object_unref(avatar_image);

  return avatar_image;
}

static gboolean
sync_tree_idle(gpointer user_data)
{
  OssoABookTreeView *view = user_data;
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  GtkTreePath *path = gtk_tree_path_new_from_indices(priv->index, -1);
  GtkTreeIter iter;
  OssoABookContact *contact = NULL;

  if (gtk_tree_model_get_iter(GTK_TREE_MODEL(priv->base_model), &iter, path))
  {
    gtk_tree_model_get(GTK_TREE_MODEL(priv->base_model), &iter,
                       0, &contact,
                       -1);

    if (!get_cached_avatar_image(view, contact, NULL))
    {
      if (create_avatar_image(view, contact))
      {
        gtk_tree_model_row_changed(
              GTK_TREE_MODEL(priv->base_model), path, &iter);
      }
    }

    priv->index++;
  }
  else
    priv->show_tree_id = 0;

  if (contact)
    g_object_unref(contact);

  gtk_tree_path_free(path);

  return priv->show_tree_id != 0;
}

static void
sync_tree(OssoABookTreeView *view)
{
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  free_row_references(view);

  if (!priv->show_tree_id)
  {
    priv->show_tree_id =
        gdk_threads_add_idle_full(310, sync_tree_idle, view, NULL);
  }

  priv->index = 0;
}

static void
show_tree_view(OssoABookTreeViewPrivate *priv)
{
  OSSO_ABOOK_NOTE(OSSO_ABOOK_DEBUG_GTK, "showing tree-view");
  gtk_widget_hide(priv->event_box);
  gtk_widget_show(priv->pannable_area);
}

static void
show_empty_widget(OssoABookTreeViewPrivate *priv)
{
  GtkWidget *child = gtk_bin_get_child(GTK_BIN(priv->event_box));
  GtkWidget *empty_widget = NULL;
  const gchar *empty_text = NULL;

  if (child)
    gtk_container_remove(GTK_CONTAINER(priv->event_box), child);

  if (priv->filter_model)
  {
    const char *filter_text =
        osso_abook_filter_model_get_text(priv->filter_model);
    OssoABookGroup *group =
        osso_abook_filter_model_get_group(priv->filter_model);

    if (filter_text && *filter_text)
      empty_text = _("addr_ia_search_not_found");
    else if (group)
      empty_widget = osso_abook_group_get_empty_widget(group);
  }

  if (!empty_widget)
  {
    GtkWidget *empty_label;

    if (!empty_text)
        empty_text = priv->empty_text;

    empty_label = gtk_label_new(empty_text);
    gtk_misc_set_alignment(GTK_MISC(empty_label), 0.5, 0.5);
    hildon_helper_set_logical_font(empty_label, "LargeSystemFont");
    hildon_helper_set_logical_color(
          empty_label, GTK_RC_FG, GTK_STATE_NORMAL, "SecondaryTextColor");
    empty_widget = gtk_hbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(empty_widget), 24);
    gtk_container_add(GTK_CONTAINER(empty_widget), empty_label);
  }

  OSSO_ABOOK_NOTE(OSSO_ABOOK_DEBUG_GTK, "showing empty widget %s",
                  GTK_IS_LABEL(empty_widget) ?
                    gtk_label_get_text(GTK_LABEL(empty_widget)) :
                    g_type_name(G_TYPE_FROM_INSTANCE(empty_widget)));

  gtk_container_add(GTK_CONTAINER(priv->event_box), empty_widget);
  gtk_widget_hide(priv->pannable_area);
  gtk_widget_show_all(priv->event_box);
}

static gboolean
sync_view_idle(gpointer user_data)
{
  OssoABookTreeView *view = user_data;
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->tree_view));
  GtkTreeIter iter;

  OSSO_ABOOK_NOTE(OSSO_ABOOK_DEBUG_GTK, "%s: %d child(ren)",
                  g_type_name(G_TYPE_FROM_INSTANCE(model)),
                  gtk_tree_model_iter_n_children(model, NULL));

  if (!gtk_tree_model_get_iter_first(model, &iter))
    show_empty_widget(priv);
  else
    show_tree_view(priv);

  priv->sync_view_id = 0;
  return FALSE;
}

static void
sync_view(OssoABookTreeView *view)
{
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  if (!priv->sync_view_id)
  {
    priv->sync_view_id =
        gdk_threads_add_idle_full(100, sync_view_idle, view, NULL);
  }
}

static void
row_inserted_cb(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter,
                gpointer user_data)
{
  sync_view(user_data);
}

static void
row_deleted_cb(GtkTreeModel *tree_model, GtkTreePath *path, gpointer user_data)
{
  sync_view((OssoABookTreeView *)user_data);
}

static void
osso_abook_tree_view_set_model(OssoABookTreeView *view, GtkTreeModel *model)
{
  OssoABookTreeViewPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_TREE_VIEW(view));
  g_return_if_fail(GTK_IS_TREE_MODEL(model));

  priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  if (priv->tree_view)
  {
    GtkTreeModel *old_model =
        gtk_tree_view_get_model(GTK_TREE_VIEW(priv->tree_view));

    if (priv->row_inserted_id)
    {
      disconnect_signal_if_connected(old_model, priv->row_inserted_id);
      disconnect_signal_if_connected(old_model, priv->row_deleted_id);
    }

    priv->row_inserted_id =
        g_signal_connect_object(model, "row-inserted",
                                G_CALLBACK(row_inserted_cb), view, 0);
    priv->row_deleted_id =
        g_signal_connect_object(model, "row-deleted",
                                G_CALLBACK(row_deleted_cb), view, 0);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->tree_view), model);
    sync_view(view);
    sync_tree(view);
  }
}

static void
style_set(GtkWidget *widget, GtkStyle *previous_style, gpointer user_data)
{
  OssoABookTreeView *view = user_data;
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  GtkSettings *settings;
  guint8 border_color[4];
  GdkColor *color;
  gint height = 72;
  gint width;
  gint hildon_size;
  gint focus_line_width;
  gint horizontal_separator;

  gtk_widget_style_get(widget,
                       "horizontal-separator", &horizontal_separator,
                       "focus-line-width", &focus_line_width,
                       NULL);
  gtk_widget_style_get(GTK_WIDGET(view),
                       "avatar-radius", &priv->avatar_radius,
                       "avatar-border", &priv->avatar_border,
                       "avatar-border-color", &color,
                       NULL);

  settings = gtk_widget_get_settings(widget);

  if (!priv->show_contact_avatar)
  {
    gtk_icon_size_lookup_for_settings(
          settings, HILDON_ICON_SIZE_FINGER, NULL, &height);
  }

  get_icon_finger_size(priv, &hildon_size);
  gtk_icon_size_lookup_for_settings(
        settings, HILDON_ICON_SIZE_XSMALL, &width, NULL);

  hildon_size += 6;
  gtk_cell_renderer_set_fixed_size(priv->contact_name, -1, height);
  gtk_cell_renderer_set_fixed_size(priv->contact_telephone, -1, height);
  gtk_cell_renderer_set_fixed_size(priv->contact_avatar, hildon_size, height);
  gtk_cell_renderer_set_fixed_size(priv->contact_presence, width, height);
  gtk_tree_view_column_set_spacing(priv->column,
                                   2 * focus_line_width + horizontal_separator);

  if (priv->avatar_border)
  {
    if (color)
    {
      border_color[0] = color->red;
      border_color[1] = color->green;
      border_color[2] = color->blue;
    }
    else
    {
      GdkColor *fg = &widget->style->fg[0];

      border_color[0] = fg->red;
      border_color[1] = fg->green;
      border_color[2] = fg->blue;
    }

    border_color[3] = -1;

    if (memcmp(priv->border_color, border_color, sizeof(border_color)))
    {
      gint size = hildon_get_icon_pixel_size(HILDON_ICON_SIZE_FINGER);
      gchar *avatar_name = create_avatar_name(size, size, TRUE,
                                              priv->avatar_radius,
                                              get_avatar_border_color(priv));

      osso_abook_avatar_cache_drop_by_name(avatar_name);
      memcpy(priv->border_color, border_color, 4u);
      g_free(avatar_name);
    }
  }

  sync_tree(view);

  if (color)
    gdk_color_free(color);
}

static gint
get_path_index(GtkTreePath *path)
{
  gint *indices = gtk_tree_path_get_indices(path);
  gint index;

  if (indices)
    index = *indices;
  else
    index = -1;

  return index;
}

static gboolean
sync_avatar_images_idle(gpointer user_data)
{
  OssoABookTreeView *view = user_data;
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  GQueue *queue = priv->row_queue1;
  GtkTreeModel *tree_model;
  gint start_index = 0;
  gint end_index = G_MAXINT;
  GtkTreeIter iter;
  GtkTreePath *end_path;
  GtkTreePath *start_path;
  gboolean processing_q2 = FALSE;

  if (!queue->length)
  {
    processing_q2 = TRUE;
    queue = priv->row_queue2;
  }

  if (gtk_tree_view_get_visible_range(GTK_TREE_VIEW(priv->tree_view),
                                      &start_path, &end_path))
  {
    start_index = get_path_index(start_path);
    end_index = get_path_index(end_path);
    gtk_tree_path_free(start_path);
    gtk_tree_path_free(end_path);
  }

  while (queue->length)
  {
    GtkTreeRowReference *row_ref = g_queue_pop_head(queue);
    GtkTreePath *path = gtk_tree_row_reference_get_path(row_ref);

    if (path)
    {
      tree_model = gtk_tree_row_reference_get_model(row_ref);

      if (gtk_tree_model_get_iter(tree_model, &iter, path))
      {
        OssoABookListStoreRow *row = osso_abook_row_model_iter_get_row(
              OSSO_ABOOK_ROW_MODEL(tree_model), &iter);

        if (row && row->contact &&
            !get_cached_avatar_image(view, row->contact, NULL))
        {
          gint index = processing_q2 ? 0 : get_path_index(path);

          if (processing_q2 || (index >= start_index && index <= end_index))
          {
            if (create_avatar_image(view, row->contact))
              gtk_tree_model_row_changed(tree_model, path, &iter);

            gtk_tree_path_free(path);
            gtk_tree_row_reference_free(row_ref);
            break;
          }

          g_queue_push_head(priv->row_queue2, row_ref);
          row_ref = NULL;
        }
      }
    }

    gtk_tree_path_free(path);
    gtk_tree_row_reference_free(row_ref);
  }

  if (priv->row_queue1->length || priv->row_queue2->length)
    return TRUE;

  priv->sync_avatar_images_id = 0;

  return FALSE;
}

static void
contact_avatar_cell_data(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
                         GtkTreeModel *tree_model, GtkTreeIter *iter,
                         gpointer data)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(data);
  OssoABookRowModel *row_model = OSSO_ABOOK_ROW_MODEL(tree_model);
  OssoABookListStoreRow *row;
  GtkTreeCellDataHint hint;

  row = osso_abook_row_model_iter_get_row(row_model, iter);

  if (!row || !row->contact)
    return;

  hint = gtk_tree_view_column_get_cell_data_hint(tree_column);

  if (hint != GTK_TREE_CELL_DATA_HINT_KEY_FOCUS)
  {
    gboolean sensitive = TRUE;

    if (OSSO_ABOOK_TREE_VIEW_GET_CLASS(view)->is_row_sensitive)
    {
      sensitive = OSSO_ABOOK_TREE_VIEW_GET_CLASS(view)->is_row_sensitive(
            view, tree_model, iter);
    }

    g_object_set(cell, "sensitive", sensitive, NULL);

    if (hint != GTK_TREE_CELL_DATA_HINT_SENSITIVITY)
    {
      GtkTreePath *path = gtk_tree_model_get_path(tree_model, iter);
      GdkPixbuf *avatar_image;

      if (!get_cached_avatar_image(view, row->contact, &avatar_image))
      {
        if (get_path_index(path) > 9)
        {
          OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
          GtkTreeRowReference *row_ref =
              gtk_tree_row_reference_new(tree_model, path);

          g_queue_push_tail(priv->row_queue1, row_ref);

          if (!priv->sync_avatar_images_id)
          {
            priv->sync_avatar_images_id = gdk_threads_add_idle_full(
                  300, sync_avatar_images_idle, view, NULL);
          }

          avatar_image = get_avatar_fallback_image(
                view, OSSO_ABOOK_AVATAR(row->contact));
        }
        else
          avatar_image = create_avatar_image(view, row->contact);
      }

      g_object_set(cell, "pixbuf", avatar_image, NULL);
      gtk_tree_path_free(path);
    }
  }
}

static void
sync_presence(OssoABookTreeView *view)
{
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  GtkTreeCellDataFunc cell_data_func = NULL;
  gint pos;

  if (priv->show_contact_avatar)
  {
    g_object_set(priv->contact_name, "ypad", 8, NULL);
    pos = 2;
    cell_data_func = contact_avatar_cell_data;
  }
  else
  {
    g_object_set(priv->contact_name, "ypad", 0, NULL);
    pos = 0;
  }

  style_set(priv->tree_view, NULL, view);
  gtk_cell_layout_reorder((GTK_CELL_LAYOUT(priv->column)),
                          priv->contact_presence, pos);
  gtk_tree_view_column_set_cell_data_func(priv->column, priv->contact_avatar,
                                          cell_data_func, view, NULL);
  if (priv->tree_view)
  {
    GtkTreeModel *model =
        gtk_tree_view_get_model(GTK_TREE_VIEW(priv->tree_view));

    g_object_ref(G_OBJECT(model));
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->tree_view), NULL);
    gtk_tree_view_set_model(GTK_TREE_VIEW(priv->tree_view), model);
    g_object_unref(G_OBJECT(model));
  }
}

static void
osso_abook_tree_view_get_property(GObject *object, guint property_id,
                                  GValue *value, GParamSpec *pspec)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(object);
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  switch (property_id)
  {
    case PARAM_MODEL:
      g_value_set_object(value, osso_abook_tree_view_get_model(view));
      break;
    case PARAM_BASE_MODEL:
      g_value_set_object(value, priv->base_model);
      break;
    case PARAM_FILTER_MODEL:
      g_value_set_object(value, priv->filter_model);
      break;
    case PARAM_TREE_VIEW:
      g_value_set_object(value, priv->tree_view);
      break;
    case PARAM_SHOW_CONTACT_NAME:
      g_value_set_boolean(value, priv->show_contact_name);
      break;
    case PARAM_SHOW_CONTACT_AVATAR:
      g_value_set_boolean(value, priv->show_contact_avatar);
      break;
    case PARAM_SHOW_CONTACT_PRESENCE:
      g_value_set_boolean(value, priv->show_contact_presence);
      break;
    case PARAM_SHOW_CONTACT_TELEPHONE:
      g_value_set_boolean(value, priv->show_contact_telephone);
      break;
    case PARAM_USE_SIM_AVATAR:
      g_value_set_boolean(value, priv->use_sim_avatar);
      break;
    case PARAM_EMPTY_TEXT:
      g_value_set_string(value, priv->empty_text);
      break;
    case PARAM_HILDON_UI_MODE:
      g_value_set_enum(value, priv->ui_mode);
      break;
    case PARAM_AGGREGATION_ACCOUNT:
      g_value_set_object(value, priv->aggregation_account);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_tree_view_set_property(GObject *object, guint property_id,
                                  const GValue *value, GParamSpec *pspec)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(object);
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  switch ( property_id )
  {
    case PARAM_MODEL:
      osso_abook_tree_view_set_model(view, g_value_get_object(value));
      break;
    case PARAM_BASE_MODEL:
      osso_abook_tree_view_set_base_model(view, g_value_get_object(value));
      break;
    case PARAM_FILTER_MODEL:
      osso_abook_tree_view_set_filter_model(view, g_value_get_object(value));
      break;
    case PARAM_SHOW_CONTACT_NAME:
      if (priv->show_contact_name != g_value_get_boolean(value))
      {
        priv->show_contact_name = !priv->show_contact_name;
        g_object_set(priv->contact_name,
                     "visible", priv->show_contact_name,
                     NULL);
      }
      break;
    case PARAM_SHOW_CONTACT_AVATAR:
      if (priv->show_contact_avatar != g_value_get_boolean(value))
      {
        priv->show_contact_avatar = !priv->show_contact_avatar;
        g_object_set(priv->contact_avatar,
                     "visible", priv->show_contact_avatar,
                     NULL);
        sync_presence(view);
      }
      break;
    case PARAM_SHOW_CONTACT_PRESENCE:
      if (priv->show_contact_presence != g_value_get_boolean(value))
      {
        priv->show_contact_presence = !priv->show_contact_presence;
        g_object_set(priv->contact_presence,
                     "visible", priv->show_contact_presence,
                     NULL);
        sync_presence(view);
      }
      break;
    case PARAM_SHOW_CONTACT_TELEPHONE:
      if (priv->show_contact_telephone != g_value_get_boolean(value))
      {

        priv->show_contact_telephone = !priv->show_contact_telephone;
        g_object_set(priv->contact_telephone,
                     "visible", priv->show_contact_telephone,
                     NULL);
      }
      break;
    case PARAM_USE_SIM_AVATAR:
      priv->use_sim_avatar = g_value_get_boolean(value);
      style_set(priv->tree_view, NULL, view);
      sync_tree(view);
      break;
    case PARAM_SENSITIVE_CAPS:
      priv->sensitive_caps = g_value_get_uint(value);

      if (gtk_widget_get_mapped(GTK_WIDGET(view)) && priv->filter_model)
      {
        gtk_tree_model_filter_refilter(
              GTK_TREE_MODEL_FILTER(priv->filter_model));
      }

      break;
    case PARAM_EMPTY_TEXT:
    {
      GtkWidget *label = gtk_bin_get_child(GTK_BIN(priv->event_box));

      g_free(priv->empty_text);
      priv->empty_text = g_value_dup_string(value);

      if (GTK_IS_LABEL(label))
        gtk_label_set_text(GTK_LABEL(label), priv->empty_text);

      break;
    }
    case PARAM_HILDON_UI_MODE:
      priv->ui_mode = g_value_get_enum(value);

      if (priv->tree_view)
      {
        hildon_gtk_tree_view_set_ui_mode(GTK_TREE_VIEW(priv->tree_view),
                                         priv->ui_mode);
      }
      break;
#if MC_ACCOUNT
    case PARAM_AGGREGATION_ACCOUNT:
      osso_abook_tree_view_set_aggregation_account(view,
                                                   g_value_get_object(value));
#endif
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static gboolean
tap_and_hold_query_cb(GtkWidget *widget, GdkEvent *returns, gpointer user_data)
{
  g_signal_stop_emission_by_name(widget, "tap-and-hold-query");
  return FALSE;
}

static PangoAttrList *
get_pango_attributes(PangoAttrList *attr_list, GtkWidget *widget,
                     guint start_index, guint end_index)
{
  PangoAttribute *attr;
  GtkSettings *settings;
  GtkStyle *style;
  PangoAttribute *desc;
  GdkColor color;

  if (gtk_style_lookup_color(widget->style, "SecondaryTextColor", &color))
  {
    if (!attr_list)
      attr_list = pango_attr_list_new();

    attr = pango_attr_foreground_new(color.red, color.green, color.blue);
    attr->start_index = start_index;
    attr->end_index = end_index;
    pango_attr_list_insert(attr_list, attr);

    settings = gtk_widget_get_settings(GTK_WIDGET(widget));
    style = gtk_rc_get_style_by_paths(
          settings, "SmallSystemFont", NULL, G_TYPE_NONE);
    desc = pango_attr_font_desc_new(style->font_desc);
    desc->start_index = start_index;
    desc->end_index = end_index;
    pango_attr_list_insert(attr_list, desc);
  }

  return attr_list;
}

static void
contact_telefone_cell_data(GtkTreeViewColumn *tree_column,
                           GtkCellRenderer *cell, GtkTreeModel *tree_model,
                           GtkTreeIter *iter, gpointer data)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(data);
  OssoABookListStoreRow *row;
  gchar *text = NULL;
  gboolean sensitive;
  PangoAttrList *attr_list;
  row = osso_abook_row_model_iter_get_row(OSSO_ABOOK_ROW_MODEL(tree_model),
                                          iter);

  if (row)
  {
    text = osso_abook_contact_get_value(E_CONTACT(row->contact), "TEL");

    if (!text || !*text)
    {
      g_free(text);
      text = osso_abook_contact_get_value(E_CONTACT(row->contact), "EMAIL");
    }

    if (!text || !*text)
    {
      g_free(text);
      text = osso_abook_contact_get_value(E_CONTACT(row->contact), "NICKNAME");
    }
  }

  if (!text || !*text)
  {
    g_free(text);
    text = g_strdup(_("addr_ia_no_details"));
  }

  if (OSSO_ABOOK_TREE_VIEW_GET_CLASS(view)->is_row_sensitive)
  {
    sensitive = OSSO_ABOOK_TREE_VIEW_GET_CLASS(view)->
        is_row_sensitive(view, tree_model, iter);
  }
  else
    sensitive = TRUE;

  attr_list = get_pango_attributes(0, GTK_WIDGET(view), 0, strlen(text));
  g_object_set(cell,
               "text", text,
               "attributes", attr_list,
               "sensitive", sensitive,
               NULL);

  if (attr_list)
    pango_attr_list_unref(attr_list);

  g_free(text);
}

static void
contact_presence_cell_data(GtkTreeViewColumn *tree_column,
                           GtkCellRenderer *cell, GtkTreeModel *tree_model,
                           GtkTreeIter *iter, gpointer data)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(data);
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  gboolean sensitive = FALSE;
  GdkPixbuf *icon = NULL;
  const gchar *icon_name = NULL;
  OssoABookListStoreRow *row =
      osso_abook_row_model_iter_get_row(OSSO_ABOOK_ROW_MODEL(tree_model), iter);

  if (row)
  {
    if (priv->aggregation_account)
    {
#if MC_ACCOUNT
      McProfile *profile;
      GList *contacts = osso_abook_contact_find_roster_contacts_for_account(
            row->contact, 0, priv->aggregation_account->name);
      OssoABookPresence *max_presence = NULL;
      GList *l = contacts;

      if (l)
        max_presence = OSSO_ABOOK_PRESENCE(l->data);

      while (l && l->next)
      {
        OssoABookPresence *next_presence = OSSO_ABOOK_PRESENCE(l->next->data);

        if (osso_abook_presence_compare(max_presence, next_presence) > 0)
          max_presence = next_presence;

        l = l->next;
      }

      g_list_free(contacts);

      profile = mc_profile_lookup(
            mc_account_compat_get_profile(priv->aggregation_account));
      icon_name =
          osso_abook_presence_get_branded_icon_name(max_presence, profile);
#else
      g_assert(0);
#endif
    }
    else
    {
      icon_name =
          osso_abook_presence_get_icon_name(OSSO_ABOOK_PRESENCE(row->contact));
    }
  }

  if (OSSO_ABOOK_TREE_VIEW_GET_CLASS(view)->is_row_sensitive &&
      OSSO_ABOOK_TREE_VIEW_GET_CLASS(view)->is_row_sensitive(view, tree_model,
                                                             iter))
  {
    sensitive = TRUE;
  }

  if (icon_name)
  {
    icon = get_cached_icon(view, icon_name,
                           hildon_get_icon_pixel_size(HILDON_ICON_SIZE_XSMALL));
  }

  g_object_set(cell,
               "pixbuf", icon,
               "yalign", (gfloat)0.0f,
               "sensitive", sensitive,
               NULL);

  if (icon)
    g_object_unref(icon);
}
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
static void
contact_name_cell_data(GtkTreeViewColumn *tree_column, GtkCellRenderer *cell,
                       GtkTreeModel *tree_model, GtkTreeIter *iter,
                       gpointer data)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(data);
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  OssoABookListStoreRow *row;
  const char *contact_name;
  gboolean sensitive;

  const gchar *status_message;
  const char *location;
  GString *s_contact_name;
  gchar *text;
  PangoAttrList *attr_list = NULL;
  OssoABookRoster *roster;
  const char *uid;
  guint start_index;

  row = osso_abook_row_model_iter_get_row(
        OSSO_ABOOK_ROW_MODEL(tree_model), iter);

  if (row)
  {
    contact_name = osso_abook_contact_get_name(row->contact);

    if (priv->filter_model)
    {
      attr_list = osso_abook_filter_model_get_markup(priv->filter_model,
                                                     contact_name);
    }
  }

  if (!contact_name || !*contact_name)
    contact_name = _("addr_li_unnamed_contact");

  if (OSSO_ABOOK_TREE_VIEW_GET_CLASS(view)->is_row_sensitive)
  {
    sensitive = OSSO_ABOOK_TREE_VIEW_GET_CLASS(view)->
        is_row_sensitive(view, tree_model, iter);
  }
  else
    sensitive = TRUE;

  if (priv->show_contact_avatar)
  {
    if (row)
    {
      status_message = osso_abook_presence_get_presence_status_message(
            OSSO_ABOOK_PRESENCE(row->contact));
      location = osso_abook_presence_get_location_string(
            OSSO_ABOOK_PRESENCE(row->contact));
    }
    else
    {
      location = NULL;
      status_message = _("addr_li_pending_contact");
    }

    if (status_message || location || OSSO_ABOOK_DEBUG_FLAGS(TREE_VIEW))
    {
      s_contact_name = g_string_new(contact_name);
      start_index = s_contact_name->len;

      if (OSSO_ABOOK_DEBUG_FLAGS(TREE_VIEW))
      {
        OssoABookCapsFlags caps = OSSO_ABOOK_CAPS_NONE;
        gchar *flags_s;

        if (row)
        {
          caps = osso_abook_caps_get_capabilities(
                OSSO_ABOOK_CAPS(row->contact));
          GList *contacts;

          uid = e_contact_get_const(E_CONTACT(row->contact), E_CONTACT_UID);

          if (osso_abook_is_temporary_uid(uid) &&
              (contacts = osso_abook_contact_get_roster_contacts(row->contact)))
          {
            roster = osso_abook_contact_get_roster(contacts->data);
          }
          else
          {
            contacts = 0;
            roster = osso_abook_contact_get_roster(row->contact);
          }

          g_list_free(contacts);
        }
        else
        {
          roster = NULL;
          uid = NULL;
        }

        g_string_append_c(s_contact_name, ' ');
        flags_s = _osso_abook_flags_to_string(OSSO_ABOOK_TYPE_CAPS_FLAGS, caps);
        g_string_append(s_contact_name, flags_s);
        g_free(flags_s);

        g_string_append_c(s_contact_name, '\n');

        if (uid)
          g_string_append(s_contact_name, uid);
        else
          g_string_append(s_contact_name, "<none>");

        g_string_append_printf(s_contact_name, "-%p",
                               row ? row->contact : NULL);

        if (roster)
        {
          g_string_append(s_contact_name, "-");
          g_string_append(s_contact_name,
                          osso_abook_roster_get_book_uri(roster));
        }
      }

      if (status_message && *status_message)
      {
        gchar **lines;
        gchar **line;

        g_string_append_c(s_contact_name, '\n');

        line = lines = g_strsplit(status_message, "\n", -1);

        while (*line)
        {
          g_string_append(s_contact_name, g_strchomp(g_strchug(*line)));

          line++;

          if (*line)
            g_string_append_c(s_contact_name, ' ');
        }

        g_strfreev(lines);
      }

      if (location && *location)
      {
        if (status_message && *status_message)
          g_string_append(s_contact_name, " - ");

        g_string_append(s_contact_name, location);
      }

      if (sensitive)
      {
        attr_list = get_pango_attributes(attr_list, GTK_WIDGET(view),
                                         start_index, s_contact_name->len);
      }

      text = g_string_free(s_contact_name, FALSE);
    }
    else
      text = g_strdup(contact_name);

    g_object_set(cell, "text", text, "attributes", attr_list, "sensitive", sensitive, NULL);
    g_free(text);
  }
  else
  {
    g_object_set(cell, "text", contact_name, "attributes", attr_list, "sensitive", sensitive, NULL);
  }

  if (attr_list)
    pango_attr_list_unref(attr_list);
}
#pragma GCC diagnostic pop

static void
name_order_notify_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry,
                     gpointer user_data)
{

  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(user_data);
  GConfValue *val = gconf_entry_get_value(entry);

  if (val)
  {
    osso_abook_list_store_set_name_order(view->priv->base_model,
                                         gconf_value_get_int(val));
  }
}

static void
contact_order_notify_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry,
                        gpointer user_data)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(user_data);
  GConfValue *val = gconf_entry_get_value(entry);

  if (val)
  {
    osso_abook_list_store_set_contact_order(view->priv->base_model,
                                            gconf_value_get_int(val));
  }
}

static void
osso_abook_tree_view_constructed(GObject *object)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(object);
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  GtkTreeModel *tree_model;
  GtkTreeView *tree_view;

  G_OBJECT_CLASS(osso_abook_tree_view_parent_class)->constructed(object);

  if (priv->filter_model)
    tree_model = GTK_TREE_MODEL(priv->filter_model);
  else
    tree_model = GTK_TREE_MODEL(priv->base_model);

  priv->tree_view = hildon_gtk_tree_view_new(priv->ui_mode);
  osso_abook_tree_view_set_model(view, tree_model);

  tree_view = GTK_TREE_VIEW(priv->tree_view);
  gtk_tree_view_set_fixed_height_mode(tree_view, TRUE);
  gtk_tree_view_set_rules_hint(tree_view, TRUE);
  g_signal_connect(tree_view, "tap-and-hold-query",
                   G_CALLBACK(tap_and_hold_query_cb), NULL);
  gtk_tree_view_set_headers_visible(tree_view, FALSE);
  priv->column = gtk_tree_view_column_new();
  gtk_tree_view_column_set_sizing(priv->column, GTK_TREE_VIEW_COLUMN_FIXED);
  gtk_tree_view_column_set_expand(priv->column, TRUE);
  gtk_tree_view_insert_column(tree_view, priv->column, 0);
  priv->contact_presence = gtk_cell_renderer_pixbuf_new();
  priv->contact_avatar = gtk_cell_renderer_pixbuf_new();
  priv->contact_name = gtk_cell_renderer_text_new();

  priv->contact_telephone = gtk_cell_renderer_text_new();
  g_object_set(priv->contact_name, "ellipsize", 3, "xpad", 8, NULL);
  g_object_set(priv->contact_telephone, "visible", 0, "xalign", (gfloat)0.0f,
               "ellipsize", 3, NULL);

  g_object_set(priv->contact_presence, "stock-size", HILDON_ICON_SIZE_XSMALL,
               "xalign", (gfloat)0.0f, NULL);
  g_object_set(priv->contact_avatar, "visible", FALSE, NULL);
  gtk_tree_view_column_pack_start(priv->column, priv->contact_name, TRUE);
  gtk_tree_view_column_set_cell_data_func(priv->column, priv->contact_name,
                                          contact_name_cell_data, view, NULL);
  gtk_tree_view_column_pack_start(priv->column, priv->contact_telephone, TRUE);
  gtk_tree_view_column_set_cell_data_func(priv->column, priv->contact_telephone,
                                          contact_telefone_cell_data, view,
                                          NULL);
  gtk_tree_view_column_pack_start(priv->column, priv->contact_presence, FALSE);
  gtk_tree_view_column_set_cell_data_func(priv->column, priv->contact_presence,
                                          contact_presence_cell_data, view,
                                          NULL);
  gtk_tree_view_column_pack_end(priv->column, priv->contact_avatar, FALSE);
  g_signal_connect(priv->tree_view, "style-set",
                   G_CALLBACK(style_set), view);
  priv->pannable_area = osso_abook_pannable_area_new();

  gtk_container_add(GTK_CONTAINER(priv->pannable_area), priv->tree_view);
  gtk_box_pack_start(GTK_BOX(view), priv->pannable_area, TRUE, TRUE, 0);
  gtk_widget_show(priv->tree_view);
  gtk_widget_show(priv->pannable_area);
  priv->event_box = gtk_event_box_new();
  gtk_box_pack_start(GTK_BOX(view), priv->event_box, TRUE, TRUE, 0);

  priv->name_order_notify_id =
      gconf_client_notify_add(osso_abook_get_gconf_client(),
                              OSSO_ABOOK_SETTINGS_KEY_NAME_ORDER,
                              name_order_notify_cb, view, NULL, NULL);
  priv->contact_order_notify_id =
      gconf_client_notify_add(osso_abook_get_gconf_client(),
                              OSSO_ABOOK_SETTINGS_KEY_CONTACT_ORDER,
                              contact_order_notify_cb, view, NULL, NULL);
  sync_view(view);
}

static void
osso_abook_tree_view_dispose(GObject *object)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(object);
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  if (priv->name_order_notify_id)
  {
    gconf_client_notify_remove(osso_abook_get_gconf_client(),
                               priv->name_order_notify_id);
    priv->name_order_notify_id = 0;
  }
  if (priv->contact_order_notify_id)
  {
    gconf_client_notify_remove(osso_abook_get_gconf_client(),
                               priv->contact_order_notify_id);
    priv->contact_order_notify_id = 0;
  }

  if (priv->sync_view_id)
  {
    g_source_remove(priv->sync_view_id);
    priv->sync_view_id = 0;
  }

  if (priv->row_inserted_id)
  {
    GtkTreeModel *model = gtk_tree_view_get_model(GTK_TREE_VIEW(priv->tree_view));

    disconnect_signal_if_connected(model, priv->row_inserted_id);
    priv->row_inserted_id = 0;
    disconnect_signal_if_connected(model, priv->row_deleted_id);
    priv->row_deleted_id = 0;
  }

  if (priv->waitable)
  {
    if (priv->closure)
    {
      osso_abook_waitable_cancel(OSSO_ABOOK_WAITABLE(priv->waitable),
                                 priv->closure);
      priv->closure = NULL;
    }

    g_object_remove_weak_pointer(priv->waitable, &priv->waitable);
    priv->waitable = NULL;
  }

  if (priv->filter_model)
  {
    g_object_unref(priv->filter_model);
    priv->filter_model = NULL;
  }

  if (priv->base_model)
  {
    g_object_unref(priv->base_model);
    priv->base_model = NULL;
  }

  g_hash_table_remove_all(priv->contact_data);
  free_row_references(view);

  if (priv->show_tree_id)
    g_source_remove(priv->show_tree_id);

  G_OBJECT_CLASS(osso_abook_tree_view_parent_class)->dispose(object);
}

static void
osso_abook_tree_view_finalize(GObject *object)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(object);
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  g_free(priv->empty_text);
  g_hash_table_unref(priv->contact_data);
  g_queue_free(priv->row_queue1);
  g_queue_free(priv->row_queue2);

  G_OBJECT_CLASS(osso_abook_tree_view_parent_class)->finalize(object);
}

static void
osso_abook_tree_view_show_all(GtkWidget *widget)
{
  gtk_widget_show(widget);
}

static void
osso_abook_tree_view_grab_focus(GtkWidget *widget)
{
  OssoABookTreeView *view = OSSO_ABOOK_TREE_VIEW(widget);

  gtk_widget_grab_focus(view->priv->tree_view);
}

static gboolean
osso_abook_tree_view_is_row_sensitive(OssoABookTreeView *view,
                                      GtkTreeModel *tree_model,
                                      GtkTreeIter *iter)
{
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);
  OssoABookListStoreRow *row;

  if (!priv->sensitive_caps)
    return TRUE;

  row =
      osso_abook_row_model_iter_get_row(OSSO_ABOOK_ROW_MODEL(tree_model), iter);

  if (!row)
    return TRUE;

  return osso_abook_caps_get_capabilities(OSSO_ABOOK_CAPS(row)) &
      priv->sensitive_caps;
}

static void
osso_abook_tree_view_class_init(OssoABookTreeViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->set_property = osso_abook_tree_view_set_property;
  object_class->get_property = osso_abook_tree_view_get_property;
  object_class->constructed = osso_abook_tree_view_constructed;
  object_class->dispose = osso_abook_tree_view_dispose;
  object_class->finalize = osso_abook_tree_view_finalize;

  widget_class->show_all = osso_abook_tree_view_show_all;
  widget_class->grab_focus = osso_abook_tree_view_grab_focus;

  klass->is_row_sensitive = osso_abook_tree_view_is_row_sensitive;

  g_object_class_install_property(
        object_class, PARAM_MODEL,
        g_param_spec_object(
                 "model",
                 "Model",
                 "The top contained GtkTreeModel.",
                 GTK_TYPE_TREE_MODEL,
                 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
        object_class, PARAM_BASE_MODEL,
        g_param_spec_object(
                 "base-model",
                 "Base model",
                 "The contained OssoABookListStore.",
                 OSSO_ABOOK_TYPE_LIST_STORE,
                 GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
        object_class, PARAM_FILTER_MODEL,
        g_param_spec_object(
                  "filter-model",
                  "Filter model",
                  "The contained OssoABookFilterModel, if any.",
                  OSSO_ABOOK_TYPE_FILTER_MODEL,
                  GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
        object_class, PARAM_TREE_VIEW,
        g_param_spec_object(
                  "tree-view",
                  "GtkTreeView",
                  "The contained GtkTreeView widget.",
                  GTK_TYPE_TREE_VIEW,
                  GTK_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PARAM_SHOW_CONTACT_NAME,
        g_param_spec_boolean(
                  "show-contact-name",
                  "Show contact name",
                  "TRUE if contact names should be shown",
                  TRUE,
                  GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PARAM_SHOW_CONTACT_AVATAR,
        g_param_spec_boolean(
                  "show-contact-avatar",
                  "Show contact avatar",
                  "TRUE if avatars should be shown",
                  FALSE,
                  GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PARAM_SENSITIVE_CAPS,
        g_param_spec_uint(
                  "sensitive-caps",
                  "Sensitive Caps",
                  "OssoABookCapsFlags mask that determines which entries are sensitive. A 0 value disables this feature.",
                  OSSO_ABOOK_CAPS_NONE,
                  G_MAXUINT,
                  OSSO_ABOOK_CAPS_NONE,
                  GTK_PARAM_WRITABLE));
  g_object_class_install_property(
        object_class, PARAM_SHOW_CONTACT_PRESENCE,
        g_param_spec_boolean(
                  "show-contact-presence",
                  "Show contact presence",
                  "TRUE if presence should be shown",
                  TRUE,
                  GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PARAM_SHOW_CONTACT_TELEPHONE,
        g_param_spec_boolean(
                  "show-contact-telephone",
                  "Show contact telephone",
                  "TRUE if telephone number should be shown",
                  FALSE,
                  GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PARAM_USE_SIM_AVATAR,
        g_param_spec_boolean(
                  "use-sim-avatar",
                  "Use sim avatar",
                  "TRUE if not setted avatar use sim-card 'avatar' instead of default",
                  FALSE,
                  GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PARAM_EMPTY_TEXT,
        g_param_spec_string(
                  "empty-text",
                  "Empty Text",
                  "The text to show if there are not contacts",
                  _("addr_ia_no_contacts"),
                  GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PARAM_HILDON_UI_MODE,
        g_param_spec_enum(
                  "hildon-ui-mode",
                  "Hildon UI Mode",
                  "The Hildon UI mode according to which the tree view should behave",
                  HILDON_TYPE_UI_MODE,
                  HILDON_UI_MODE_NORMAL,
                  GTK_PARAM_READWRITE));
#if MC_ACCOUNT
  g_object_class_install_property(
        object_class, PARAM_AGGREGATION_ACCOUNT,
        g_param_spec_object(
                  "aggregation-account",
                  "Aggregation Account",
                  "A single MC Account to aggregate presence and avatar from (as opposed to all accounts).",
                  MC_TYPE_ACCOUNT,
                  GTK_PARAM_READWRITE));
#endif
  gtk_widget_class_install_style_property(
        widget_class,
        g_param_spec_int(
                  "avatar-radius",
                  "Avatar Radius",
                  "Radius of rounded avatar borders",
                  -1,
                  G_MAXINT,
                  -1,
                  GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property(
        widget_class,
        g_param_spec_boolean(
                  "avatar-border",
                  "Avatar Border",
                  "Wheither to show an avatar borders",
                  FALSE,
                  GTK_PARAM_READWRITE));
  gtk_widget_class_install_style_property(
        widget_class,
        g_param_spec_boxed(
                  "avatar-border-color",
                  "Avatar Border Color",
                  "Color of the avatar borders",
                  GDK_TYPE_COLOR,
                  GTK_PARAM_READWRITE));
}

GtkTreeSelection *
osso_abook_tree_view_get_tree_selection(OssoABookTreeView *view)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_TREE_VIEW (view), NULL);

  return gtk_tree_view_get_selection(osso_abook_tree_view_get_tree_view(view));
}

GtkTreeView *
osso_abook_tree_view_get_tree_view(OssoABookTreeView *view)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_TREE_VIEW (view), NULL);

  return GTK_TREE_VIEW(view->priv->tree_view);
}

void
osso_abook_tree_view_set_ui_mode(OssoABookTreeView *view, HildonUIMode mode)
{
  g_return_if_fail(OSSO_ABOOK_IS_TREE_VIEW (view));

  g_object_set(G_OBJECT(view), "hildon-ui-mode", mode, NULL);
}

GtkTreeModel *
osso_abook_tree_view_get_model(OssoABookTreeView *view)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_TREE_VIEW (view), NULL);

  return gtk_tree_view_get_model(GTK_TREE_VIEW(view->priv->tree_view));
}

void
osso_abook_tree_view_set_empty_text(OssoABookTreeView *view, const char *text)
{
  g_return_if_fail(OSSO_ABOOK_IS_TREE_VIEW (view));
  g_return_if_fail(text != NULL);

  g_object_set(G_OBJECT(view), "empty-text", text, NULL);
}

static void
notify_group_cb(GObject *gobject, GParamSpec *arg1, OssoABookTreeView *view)
{
  sync_view(view);
}

static void
notify_text_cb(GObject *gobject, GParamSpec *arg1, OssoABookTreeView *view)
{
  OssoABookTreeViewPrivate *priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  sync_view(view);
  gtk_widget_queue_draw(priv->tree_view);
}

void
osso_abook_tree_view_set_filter_model(OssoABookTreeView *view,
                                      OssoABookFilterModel *model)
{
  OssoABookTreeViewPrivate *priv;
  gpointer tree_model;

  g_return_if_fail(OSSO_ABOOK_IS_TREE_VIEW(view));
  g_return_if_fail(OSSO_ABOOK_IS_FILTER_MODEL(model) || model == NULL);

  priv = OSSO_ABOOK_TREE_VIEW_PRIVATE(view);

  if (model)
    g_object_ref(model);

  if (priv->filter_model)
    g_object_unref(priv->filter_model);

  priv->filter_model = model;

  if (model)
  {
    g_signal_connect_object(model, "notify::group",
                            G_CALLBACK(notify_group_cb), view, 0);
    g_signal_connect_object(model, "notify::text",
                            G_CALLBACK(notify_text_cb), view, 0);
    tree_model = model;
  }
  else
    tree_model = priv->base_model;

  osso_abook_tree_view_set_model(view, tree_model);
}

static void
on_roster_ready(OssoABookWaitable *waitable, const GError *error, gpointer data)
{
  OssoABookTreeView *view = data;
  OssoABookTreeViewPrivate *priv = view->priv;

  priv->closure = NULL;

  g_return_if_fail(
        waitable == (OssoABookWaitable *)osso_abook_list_store_get_roster(
          priv->base_model));

  sync_view(view);
}

void
osso_abook_tree_view_set_base_model(OssoABookTreeView *view,
                                    OssoABookListStore *model)
{
  OssoABookTreeViewPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_TREE_VIEW(view));
  g_return_if_fail(OSSO_ABOOK_IS_LIST_STORE(model));

  priv = view->priv;

  if (priv->base_model == model)
    return;

  if (priv->waitable && priv->closure)
  {
    osso_abook_waitable_cancel(OSSO_ABOOK_WAITABLE(priv->waitable),
                               priv->closure);
  }

  priv->closure = NULL;
  g_object_ref(model);

  if (priv->base_model)
    g_object_unref(priv->base_model);

  priv->base_model = model;
  osso_abook_tree_view_set_model(view, (GtkTreeModel *)model);

  if (!priv->base_model)
    return;

  if (priv->waitable)
    g_object_remove_weak_pointer(G_OBJECT(priv->waitable), &priv->waitable);

  priv->waitable = osso_abook_list_store_get_roster(priv->base_model);

  if (priv->waitable)
  {
    g_object_add_weak_pointer(priv->waitable, &priv->waitable);
    priv->closure = osso_abook_waitable_call_when_ready(
          OSSO_ABOOK_WAITABLE(priv->waitable), on_roster_ready, view, NULL);
  }
}
