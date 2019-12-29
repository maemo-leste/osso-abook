#include <gtk/gtkprivate.h>

#include "config.h"

#include "osso-abook-contact-view.h"
#include "osso-abook-all-group.h"

struct _OssoABookContactViewPrivate
{
  guint minimum_selection;
  guint maximum_selection;
  OssoABookContact *master_contact;
};

typedef struct _OssoABookContactViewPrivate OssoABookContactViewPrivate;

enum {
  PROP_MINIMUM_SELECTION = 0x1,
  PROP_MAXIMUM_SELECTION
};

enum {
  CONTACT_ACTIVATED,
  SELECTION_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookContactView,
  osso_abook_contact_view,
  OSSO_ABOOK_TYPE_TREE_VIEW
);

static void
osso_abook_contact_view_dispose(GObject *object)
{
  OssoABookContactView *view = OSSO_ABOOK_CONTACT_VIEW(object);
  OssoABookContactViewPrivate *priv =
      osso_abook_contact_view_get_instance_private(view);

  if (priv->master_contact)
  {
    g_object_unref(priv->master_contact);
    priv->master_contact = NULL;
  }

  G_OBJECT_CLASS(osso_abook_contact_view_parent_class)->dispose(object);
}

static void
hildon_row_tapped_cb(GtkTreeView *tree_view, GtkTreePath *path,
                     gpointer user_data)
{
  GtkTreeModel *tree_model;
  GtkTreeIter iter;
  gpointer master_contact;

  OssoABookContactView *view;
  OssoABookContactViewPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_VIEW(user_data));

  view = OSSO_ABOOK_CONTACT_VIEW(user_data);
  priv = osso_abook_contact_view_get_instance_private(view);

  tree_model = osso_abook_tree_view_get_model(OSSO_ABOOK_TREE_VIEW(view));

  if (gtk_tree_model_get_iter(tree_model, &iter, path))
  {
    gtk_tree_model_get(tree_model, &iter,
                       0, &master_contact,
                       -1);

    if (priv->master_contact)
      g_object_unref(priv->master_contact);

    priv->master_contact = g_object_ref(master_contact);
    g_signal_emit(view, signals[CONTACT_ACTIVATED], 0, master_contact);

    if (master_contact)
      g_object_unref(master_contact);
  }

}

static void
selection_changed_cb(GtkTreeSelection *treeselection, gpointer user_data)
{
  g_signal_emit(user_data, signals[SELECTION_CHANGED], 0,
                gtk_tree_selection_count_selected_rows(treeselection));
}

static void
osso_abook_contact_view_constructed(GObject *object)
{
  OssoABookContactView *view = OSSO_ABOOK_CONTACT_VIEW(object);
  GtkTreeView *tree_view;
  GtkTreeSelection *selection;

  G_OBJECT_CLASS(osso_abook_contact_view_parent_class)->constructed(object);

  tree_view = osso_abook_tree_view_get_tree_view(OSSO_ABOOK_TREE_VIEW(view));
  selection = gtk_tree_view_get_selection(tree_view);
  g_signal_connect_object(tree_view, "hildon-row-tapped",
                          G_CALLBACK(hildon_row_tapped_cb), view, 0);
  gtk_tree_selection_set_mode(selection, GTK_SELECTION_BROWSE);
  g_signal_connect_object(selection, "changed",
                          G_CALLBACK(selection_changed_cb), view, 0);
  osso_abook_tree_view_set_empty_text(OSSO_ABOOK_TREE_VIEW(view),
                                      _("addr_li_sel_contact_none"));
}

static void
osso_abook_contact_view_set_property(GObject *object, guint property_id,
                                     const GValue *value, GParamSpec *pspec)
{
  OssoABookContactView *view = OSSO_ABOOK_CONTACT_VIEW(object);
  OssoABookContactViewPrivate *priv =
      osso_abook_contact_view_get_instance_private(view);

  switch (property_id)
  {
    case PROP_MINIMUM_SELECTION:
      priv->minimum_selection = g_value_get_uint(value);
      break;
    case PROP_MAXIMUM_SELECTION:
    {
      GtkTreeSelection *selection;

      priv->maximum_selection = g_value_get_uint(value);
      selection = osso_abook_tree_view_get_tree_selection(
            OSSO_ABOOK_TREE_VIEW(view));

      if (priv->maximum_selection < 2)
      {
        osso_abook_tree_view_set_ui_mode(OSSO_ABOOK_TREE_VIEW(view),
                                         HILDON_UI_MODE_NORMAL);
        gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);

        if (priv->master_contact)
        {
          g_object_unref(priv->master_contact);
          priv->master_contact = NULL;
        }
      }
      else
      {
        osso_abook_tree_view_set_ui_mode(OSSO_ABOOK_TREE_VIEW(view),
                                         HILDON_UI_MODE_EDIT);
        gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);
      }
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_contact_view_get_property(GObject *object, guint property_id,
                                     GValue *value, GParamSpec *pspec)
{
  OssoABookContactView *view =  (OssoABookContactView *)object;

  switch (property_id)
  {
    case PROP_MINIMUM_SELECTION:
      g_value_set_uint(value,
                       osso_abook_contact_view_get_minimum_selection(view));
      break;
    case PROP_MAXIMUM_SELECTION:
      g_value_set_uint(value,
                       osso_abook_contact_view_get_maximum_selection(view));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_contact_view_class_init(OssoABookContactViewClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->dispose = osso_abook_contact_view_dispose;
  object_class->constructed = osso_abook_contact_view_constructed;
  object_class->set_property = osso_abook_contact_view_set_property;
  object_class->get_property = osso_abook_contact_view_get_property;

  g_object_class_install_property(
        object_class, PROP_MINIMUM_SELECTION,
        g_param_spec_uint(
                 "minimum-selection",
                 "Minimum Selection",
                 "The minimum number of rows to be selected",
                 1,
                 G_MAXUINT,
                 1,
                 GTK_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_MAXIMUM_SELECTION,
        g_param_spec_uint(
                 "maximum-selection",
                 "Maximum Selection",
                 "The maximum number of rows to be selected",
                 1,
                 G_MAXUINT,
                 1,
                 GTK_PARAM_READWRITE));

  signals[CONTACT_ACTIVATED] =
      g_signal_new("contact-activated",
                   OSSO_ABOOK_TYPE_CONTACT_VIEW, G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(OssoABookContactViewClass,
                                   contact_activated),
                   0, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE,
                   1, OSSO_ABOOK_TYPE_CONTACT);
  signals[SELECTION_CHANGED] =
      g_signal_new("selection-changed",
                   OSSO_ABOOK_TYPE_CONTACT_VIEW,
                   G_SIGNAL_RUN_LAST,
                   G_STRUCT_OFFSET(OssoABookContactViewClass,
                                   selection_changed),
                   0, NULL, g_cclosure_marshal_VOID__UINT,
                   G_TYPE_NONE,
                   1, G_TYPE_UINT);
}

static void
osso_abook_contact_view_init(OssoABookContactView *view)
{
  OssoABookContactViewPrivate *priv =
      osso_abook_contact_view_get_instance_private(view);

  priv->maximum_selection = 1;
  priv->minimum_selection = 1;
}

unsigned int
osso_abook_contact_view_get_minimum_selection(OssoABookContactView *view)
{
  OssoABookContactViewPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_VIEW(view), 1);

  priv = osso_abook_contact_view_get_instance_private(view);

  return priv->minimum_selection;
}

void
osso_abook_contact_view_set_minimum_selection(OssoABookContactView *view,
                                              unsigned int limit)
{
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_VIEW(view));

  g_object_set(view, "minimum-selection", limit, NULL);
}

unsigned int
osso_abook_contact_view_get_maximum_selection(OssoABookContactView *view)
{
  OssoABookContactViewPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_VIEW(view), 1);

  priv = osso_abook_contact_view_get_instance_private(view);

  return priv->maximum_selection;
}

void
osso_abook_contact_view_set_maximum_selection(OssoABookContactView *view,
                                              unsigned int limit)
{
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_VIEW(view));

  g_object_set(view, "maximum-selection", limit, NULL);
}

GtkWidget *
osso_abook_contact_view_new(HildonUIMode mode, OssoABookContactModel *model,
                            OssoABookFilterModel *filter_model)
{
  gpointer _model;

  if (!filter_model)
    _model = model;
  else
    _model = filter_model;

  return g_object_new(OSSO_ABOOK_TYPE_CONTACT_VIEW,
                      "model", _model,
                      "base-model", model,
                      "filter-model", filter_model,
                      "show-contact-avatar", TRUE,
                      "hildon-ui-mode", mode,
                      NULL);
}

GtkWidget *
osso_abook_contact_view_new_basic(HildonUIMode mode,
                                  OssoABookContactModel *model)
{
  OssoABookFilterModel *filter_model =
      osso_abook_filter_model_new(OSSO_ABOOK_LIST_STORE(model));
  GtkWidget *view;

  osso_abook_filter_model_set_group(filter_model, osso_abook_all_group_get());
  view = osso_abook_contact_view_new(mode, model, filter_model);
  g_object_unref(filter_model);

  return view;
}
