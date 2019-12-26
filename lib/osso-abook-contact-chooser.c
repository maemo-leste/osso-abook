#include "osso-abook-contact-chooser.h"
#include "osso-abook-contact-view.h"
#include "osso-abook-contact-model.h"
#include "osso-abook-string-list.h"
#include "osso-abook-enum-types.h"
#include "osso-abook-waitable.h"
#include "osso-abook-util.h"
#include "osso-abook-aggregator.h"
#include "osso-abook-row-model.h"
#include "osso-abook-alpha-shortcuts.h"
#include "osso-abook-init.h"

#include "config.h"

#include <libintl.h>
#include <string.h>

#define DONE_BUTTON_ID 1

enum
{
  PROP_CAPABILITIES = 1,
  PROP_CONTACTS_ORDER,
  PROP_EXCLUDED_CONTACTS,
  PROP_MINIMUM_SELECTION,
  PROP_MAXIMUM_SELECTION,
  PROP_DONE_LABEL,
  PROP_CONTACT_VIEW,
  PROP_MODEL,
  PROP_TITLE,
  PROP_HIDE_OFFLINE_CONTACTS,
  PROP_SHOW_EMPTY_NOTE,
};

struct _OssoABookContactChooserPrivate
{
  OssoABookCapsFlags caps_flags;
  OssoABookContactOrder contact_order;
  OssoABookStringList excluded_contacts;
  OssoABookContactChooserVisibleFunc visible_cb;
  gpointer visible_data;
  GDestroyNotify visible_destroy;
  OssoABookFilterModel *filter_model;
  OssoABookContactModel *contact_model;
  OssoABookContactView *contact_view;
  GtkWidget *live_search;
  GtkWidget *done_button;
  GtkWidget *alpha_shortcuts;
  GtkWidget *hbox;
  OssoABookRoster *roster;
  gpointer closure;
  gboolean title_valid : 1;
  gboolean hide_offline_contacts : 1;
  gboolean multiple_selection : 1;
  gboolean show_empty_note : 1;
};

typedef struct _OssoABookContactChooserPrivate OssoABookContactChooserPrivate;

G_DEFINE_TYPE_WITH_CODE(
    OssoABookContactChooser,
    osso_abook_contact_chooser,
    GTK_TYPE_DIALOG,
    G_ADD_PRIVATE (OssoABookContactChooser)
);

static void destroy_contact_model(OssoABookContactChooser *chooser);
static void update_widgets(OssoABookContactChooser *chooser);
static void set_model_order(OssoABookContactChooser *chooser,
                            OssoABookContactModel *model,
                            OssoABookContactOrder order);

GtkWidget *
osso_abook_contact_chooser_new(GtkWindow *parent, const gchar *title)
{
  g_return_val_if_fail(GTK_IS_WINDOW (parent) || !parent, NULL);

  return g_object_new(OSSO_ABOOK_TYPE_CONTACT_CHOOSER,
                      "transient-for", parent,
                      "title", title,
                      "capabilities", OSSO_ABOOK_CAPS_ALL,
                      "modal", TRUE,
                      "destroy-with-parent", TRUE,
                      NULL);
}

static void
_response_cb(GtkDialog *dialog, gint response_id, gpointer user_data)
{
  if (response_id == DONE_BUTTON_ID)
  {
    OssoABookContactChooser *chooser = user_data;
    OssoABookContactChooserPrivate *priv =
        osso_abook_contact_chooser_get_instance_private(chooser);
    unsigned int selected_count;
    unsigned int minimum_selection;
    unsigned int maximum_selection;

    if (priv->live_search)
      gtk_widget_hide(priv->live_search);

    selected_count = gtk_tree_selection_count_selected_rows(
          osso_abook_tree_view_get_tree_selection(
            OSSO_ABOOK_TREE_VIEW(priv->contact_view)));

    minimum_selection = osso_abook_contact_view_get_minimum_selection(
          OSSO_ABOOK_CONTACT_VIEW(priv->contact_view));
    maximum_selection = osso_abook_contact_view_get_maximum_selection(
          OSSO_ABOOK_CONTACT_VIEW(priv->contact_view));
    g_signal_stop_emission_by_name(user_data, "response");

    if (selected_count >= minimum_selection &&
        selected_count <= maximum_selection)
    {
      gtk_dialog_response(GTK_DIALOG(chooser), GTK_RESPONSE_OK);
    }
  }
}

static void
osso_abook_contact_chooser_init(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  priv->excluded_contacts = NULL;
  priv->alpha_shortcuts = NULL;
  priv->live_search = NULL;
  priv->title_valid = FALSE;
  priv->hide_offline_contacts = FALSE;
  priv->multiple_selection = FALSE;
  priv->show_empty_note  = FALSE;

  priv->done_button = gtk_dialog_add_button(
        GTK_DIALOG(chooser), dgettext("hildon-libs", "wdgt_bd_done"),
        DONE_BUTTON_ID);

  GTK_WIDGET_UNSET_FLAGS(chooser, GTK_PARENT_SENSITIVE);

  g_signal_connect(G_OBJECT(chooser), "response",
                   G_CALLBACK(_response_cb), chooser);
  gtk_widget_grab_default(priv->done_button);
}

static void
_row_inserted(GtkTreeModel *tree_model, GtkTreePath *path, GtkTreeIter *iter,
              gpointer user_data)
{
  OssoABookContactChooser *chooser = OSSO_ABOOK_CONTACT_CHOOSER(user_data);
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  if (osso_abook_list_store_is_loading(OSSO_ABOOK_LIST_STORE(
                                         priv->contact_model)))
  {
    osso_abook_contact_chooser_deselect_all(chooser);
  }
}

static void
_contact_activated_cb(OssoABookContactView *view,
                      OssoABookContact *master_contact, gpointer user_data)
{
  gtk_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_OK);
}

static void
_selection_changed_cb(OssoABookContactView *view, guint count_selected,
                      gpointer user_data)
{
  update_widgets(user_data);
}

static void
add_live_search(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv;
  GtkTreeView *tree_view;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_CHOOSER (chooser));

  priv = osso_abook_contact_chooser_get_instance_private(chooser);

  if (priv->live_search)
    return;

  priv->live_search =
      osso_abook_live_search_new_with_filter(priv->filter_model);
  tree_view = osso_abook_tree_view_get_tree_view(
        OSSO_ABOOK_TREE_VIEW(priv->contact_view));
  hildon_live_search_widget_hook(HILDON_LIVE_SEARCH(priv->live_search),
                                 GTK_WIDGET(chooser),
                                 GTK_WIDGET(tree_view));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(chooser)->vbox), priv->live_search,
                     FALSE, FALSE, 0);
  gtk_widget_hide(priv->live_search);
}

static void
remove_live_search(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_CHOOSER (chooser));

  priv = osso_abook_contact_chooser_get_instance_private(chooser);

  if (!priv->live_search)
    return;

  /* the original code was like:

  gtk_container_remove(GTK_CONTAINER(GTK_DIALOG(chooser)->vbox),
                       priv->live_search);

  however, gtk docs say "If you don't want to use widget again it's usually more
  efficient to simply destroy it directly using gtk_widget_destroy() since this
  will remove it from the container and help break any circular reference count
  cycles." */

  gtk_widget_destroy(priv->live_search);
  priv->live_search = NULL;
}

static void
add_alpha_shortcuts(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_CHOOSER (chooser));

  priv = osso_abook_contact_chooser_get_instance_private(chooser);

  if (!priv->alpha_shortcuts)
    return;

  priv->alpha_shortcuts = osso_abook_alpha_shortcuts_new();
  osso_abook_alpha_shortcuts_widget_hook(
        OSSO_ABOOK_ALPHA_SHORTCUTS(priv->alpha_shortcuts),
        OSSO_ABOOK_CONTACT_VIEW(priv->contact_view));
  gtk_box_pack_start(
        GTK_BOX(priv->hbox), priv->alpha_shortcuts, FALSE, FALSE, 0);
  set_model_order(chooser, priv->contact_model, priv->contact_order);
}

static void
remove_alpha_shortcuts(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_CHOOSER (chooser));

  priv = osso_abook_contact_chooser_get_instance_private(chooser);

  if (!priv->alpha_shortcuts)
    return;

  /* see the note in remove_live_search */
  gtk_widget_destroy(priv->alpha_shortcuts);
  priv->alpha_shortcuts = NULL;
  set_model_order(chooser, priv->contact_model, priv->contact_order);
}

static void
update_navigation_widgets(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_CHOOSER (chooser));

  priv = osso_abook_contact_chooser_get_instance_private(chooser);

  if (!osso_abook_screen_is_landscape_mode(
        gtk_widget_get_screen(GTK_WIDGET(chooser))))
  {
    remove_live_search(chooser);

    if (!priv->multiple_selection)
      add_alpha_shortcuts(chooser);
    else
      remove_alpha_shortcuts(chooser);
  }
  else
  {
    remove_alpha_shortcuts(chooser);
    add_live_search(chooser);
  }

}

static void
update_widgets(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  g_signal_handlers_disconnect_matched(
        priv->contact_view, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0,
        NULL, _contact_activated_cb, chooser);
  g_signal_handlers_disconnect_matched(
        priv->contact_view, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0,
        NULL, _selection_changed_cb, chooser);

  if (priv->multiple_selection)
  {
    g_signal_connect(priv->contact_view, "selection-changed",
                     G_CALLBACK(_selection_changed_cb), chooser);
    gtk_widget_set_no_show_all(GTK_DIALOG(chooser)->action_area, FALSE);
    gtk_dialog_set_has_separator(GTK_DIALOG(chooser), TRUE);
    gtk_widget_show(GTK_DIALOG(chooser)->action_area);
  }
  else
  {
    g_signal_connect(priv->contact_view, "contact-activated",
                     G_CALLBACK(_contact_activated_cb), chooser);
    gtk_widget_set_no_show_all(GTK_DIALOG(chooser)->action_area, TRUE);
    gtk_dialog_set_has_separator(GTK_DIALOG(chooser), FALSE);
    gtk_widget_hide(GTK_DIALOG(chooser)->action_area);
  }

  update_navigation_widgets(chooser);

  if (priv->title_valid)
    g_object_set(chooser, "title", "", NULL);
}

static void
_maximum_selection_cb(GObject *gobject, GParamSpec *arg1,
                      OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);
  unsigned int max_selection =
      osso_abook_contact_view_get_maximum_selection(
        OSSO_ABOOK_CONTACT_VIEW(priv->contact_view));

  priv->multiple_selection = (max_selection > 1);
  update_widgets(chooser);
}

static void
roster_weak_notify(gpointer data, GObject *where_the_object_was)
{
  destroy_contact_model(OSSO_ABOOK_CONTACT_CHOOSER(data));
}

static void
destroy_contact_model(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  if (priv->closure)
  {
    osso_abook_waitable_cancel(
          OSSO_ABOOK_WAITABLE(priv->roster), priv->closure);
    g_object_weak_unref(G_OBJECT(priv->roster), roster_weak_notify, chooser);
    priv->roster = NULL;
    priv->closure = NULL;
  }
}

static void
destroy_models(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  if (priv->filter_model)
  {
    osso_abook_filter_model_set_visible_func(
          priv->filter_model, NULL, NULL, NULL);
    g_signal_handlers_disconnect_matched(
          priv->filter_model, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
          0, 0, 0, _row_inserted, chooser);
    g_object_unref(priv->filter_model);
    priv->filter_model = NULL;
  }

  if (priv->contact_model)
  {
    destroy_contact_model(chooser);
    g_object_unref(priv->contact_model);
    priv->contact_model = NULL;
  }
}

static void
_screen_size_changed_cb(GdkScreen *screen, gpointer user_data)
{
  update_navigation_widgets(user_data);
}

static void
osso_abook_contact_chooser_dispose(GObject *object)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(
        OSSO_ABOOK_CONTACT_CHOOSER(object));

  if (priv->contact_view)
  {
    g_signal_handlers_disconnect_matched(
          priv->contact_view, G_SIGNAL_MATCH_DATA|G_SIGNAL_MATCH_FUNC, 0, 0,
          NULL, _maximum_selection_cb, object);
  }

  g_signal_handlers_disconnect_matched(
        gtk_widget_get_screen(GTK_WIDGET(object)),
        G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
        _screen_size_changed_cb, object);

  destroy_models(OSSO_ABOOK_CONTACT_CHOOSER(object));

  G_OBJECT_CLASS(osso_abook_contact_chooser_parent_class)->dispose(object);
  priv->contact_view = NULL;
}

static void
osso_abook_contact_chooser_finalize(GObject *object)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(
        OSSO_ABOOK_CONTACT_CHOOSER(object));

  osso_abook_string_list_free(priv->excluded_contacts);

  if (priv->visible_destroy)
    priv->visible_destroy(priv->visible_data);

  G_OBJECT_CLASS(osso_abook_contact_chooser_parent_class)->finalize(object);
}

static void
osso_abook_contact_chooser_get_property(GObject *object, guint property_id,
                                        GValue *value, GParamSpec *pspec)
{
  OssoABookContactChooser *chooser = OSSO_ABOOK_CONTACT_CHOOSER(object);
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  switch (property_id)
  {
    case PROP_CAPABILITIES:
      g_value_set_flags(value,
                        osso_abook_contact_chooser_get_capabilities(chooser));
      break;
    case PROP_CONTACTS_ORDER:
      g_value_set_flags(value,
                        osso_abook_contact_chooser_get_contact_order(chooser));
      break;
    case PROP_EXCLUDED_CONTACTS:
      g_value_set_boxed(
            value, osso_abook_contact_chooser_get_excluded_contacts(chooser));
      break;
    case PROP_MINIMUM_SELECTION:
      g_value_set_uint(
            value, osso_abook_contact_view_get_minimum_selection(
              OSSO_ABOOK_CONTACT_VIEW(priv->contact_view)));
      break;
    case PROP_MAXIMUM_SELECTION:
      g_value_set_uint(
            value, osso_abook_contact_view_get_maximum_selection(
              OSSO_ABOOK_CONTACT_VIEW(priv->contact_view)));
      break;
    case PROP_DONE_LABEL:
      g_value_set_string(value,
                         osso_abook_contact_chooser_get_done_label(chooser));
      break;
    case PROP_CONTACT_VIEW:
      g_value_set_object(value,
                         osso_abook_contact_chooser_get_contact_view(chooser));
      break;
    case PROP_MODEL:
      g_value_set_object(value,
                         osso_abook_contact_chooser_get_model(chooser));
      break;
    case PROP_TITLE:
    {
      GParamSpec *property = g_object_class_find_property(
            osso_abook_contact_chooser_parent_class,
            g_param_spec_get_name(pspec));
      G_OBJECT_CLASS(g_type_class_peek(property->owner_type))->get_property(
            object, property->param_id, value, property);
      break;
    }
    case PROP_HIDE_OFFLINE_CONTACTS:
      g_value_set_boolean(value, priv->hide_offline_contacts);
      break;
    case PROP_SHOW_EMPTY_NOTE:
      g_value_set_boolean(value, priv->show_empty_note);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
set_model_order(OssoABookContactChooser *chooser, OssoABookContactModel *model,
                OssoABookContactOrder order)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  if (!osso_abook_screen_is_landscape_mode(gtk_widget_get_screen(
                                             GTK_WIDGET(chooser))) &&
      !priv->multiple_selection)
  {
    order = OSSO_ABOOK_CONTACT_ORDER_NAME;
  }

  osso_abook_list_store_set_contact_order(OSSO_ABOOK_LIST_STORE(model), order);
}

static OssoABookContactModel *
create_custom_model(OssoABookContactChooser *chooser,
                    OssoABookContactOrder contact_order)
{
  GError *error = NULL;
  OssoABookRoster *roster = osso_abook_aggregator_get_default(&error);
  OssoABookContactModel *model;

  if (error)
  {
    g_warning("%s: %s", __FUNCTION__, error->message);
    g_clear_error(&error);
  }

  g_return_val_if_fail(NULL != roster, FALSE);

  model = osso_abook_contact_model_new();
  set_model_order(chooser, model, contact_order);
  osso_abook_list_store_set_roster(OSSO_ABOOK_LIST_STORE(model), roster);

  return model;
}

static gboolean
contact_is_visibile(OssoABookContact *contact, OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);
  OssoABookCapsFlags abook_caps =
      osso_abook_caps_get_capabilities(OSSO_ABOOK_CAPS(contact));
  OssoABookCapsFlags caps_flags = priv->caps_flags;
  const char *uid;
  OssoABookStringList l;

  if (priv->caps_flags != OSSO_ABOOK_CAPS_ALL && !(abook_caps & caps_flags))
    return FALSE;

  if (priv->hide_offline_contacts &&
      !(caps_flags & OSSO_ABOOK_CAPS_PHONE & abook_caps))
  {
    TpConnectionPresenceType presence_type =
        osso_abook_presence_get_presence_type(OSSO_ABOOK_PRESENCE(contact));

    if (presence_type == TP_CONNECTION_PRESENCE_TYPE_HIDDEN ||
        presence_type == TP_CONNECTION_PRESENCE_TYPE_OFFLINE ||
        presence_type == TP_CONNECTION_PRESENCE_TYPE_UNKNOWN)
    {
      return FALSE;
    }
  }

  uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

  for (l = priv->excluded_contacts; l; l = l->next)
  {
    if (!g_strcmp0(uid, (const char *)l->data))
      return FALSE;
  }

  if (priv->visible_cb)
    return priv->visible_cb(chooser, contact, priv->visible_data);
  else
    return TRUE;
}

static gboolean
_contact_visible_cb(GtkTreeModel *model, GtkTreeIter *iter, gpointer data)
{
  OssoABookListStoreRow *row =
      osso_abook_row_model_iter_get_row(OSSO_ABOOK_ROW_MODEL(model), iter);

  g_return_val_if_fail(NULL != row, FALSE);

  return contact_is_visibile(row->contact, data);
}

static void
show_empty_note(GtkWidget *widget, gpointer user_data)
{
  OssoABookContactChooser *chooser = OSSO_ABOOK_CONTACT_CHOOSER(user_data);
  osso_context_t *context = osso_abook_get_osso_context();
  GtkWidget *note =
      hildon_note_new_confirmation(GTK_WINDOW(chooser),
                                   _("addr_nc_notification14"));
  int response;

  gtk_window_set_destroy_with_parent(GTK_WINDOW(note), TRUE);
  response = gtk_dialog_run(GTK_DIALOG(note));

  gtk_widget_destroy(note);

  if (response == GTK_RESPONSE_OK)
  {
    g_return_if_fail(context != NULL);
    osso_rpc_run_with_defaults(context, "osso_addressbook", "top_application",
                               NULL, DBUS_TYPE_INVALID );
  }
}

static void
_roster_is_ready_cb(OssoABookWaitable *waitable, const GError *error,
                    gpointer data)
{
  OssoABookContactChooser *chooser;
  OssoABookContactChooserPrivate *priv;
  OssoABookRoster *roster;
  GtkTreeModel *tree_model;
  OssoABookRowModel *row_model;
  GtkTreeIter iter;
  gboolean is_empty = TRUE;

  if (error)
  {
    g_warning("%s: Returning early due to error: %s",
              __FUNCTION__, error->message);
    return;
  }

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_CHOOSER (data));

  chooser = OSSO_ABOOK_CONTACT_CHOOSER(data);
  priv = osso_abook_contact_chooser_get_instance_private(chooser);
  roster = osso_abook_list_store_get_roster(
        OSSO_ABOOK_LIST_STORE(priv->contact_model));

  g_return_if_fail(roster != priv->roster);

  tree_model = GTK_TREE_MODEL(priv->contact_model);
  row_model = OSSO_ABOOK_ROW_MODEL(tree_model);

  if (gtk_tree_model_get_iter_first(tree_model, &iter))
  {
    do
    {
      OssoABookListStoreRow *row =
          osso_abook_row_model_iter_get_row(row_model, &iter);

      if (contact_is_visibile(row->contact, chooser))
      {
        is_empty = FALSE;
        g_signal_handlers_disconnect_matched(
              G_OBJECT(chooser), G_SIGNAL_MATCH_DATA|G_SIGNAL_MATCH_FUNC, 0, 0,
              NULL, show_empty_note, chooser);
        break;
      }
    }
    while (gtk_tree_model_iter_next(tree_model, &iter));
  }

  if (is_empty)
  {
    if (gtk_widget_get_sensitive(GTK_WIDGET(chooser)))
      show_empty_note(GTK_WIDGET(chooser), chooser);
    else
    {
      g_signal_connect(G_OBJECT(chooser), "show",
                       G_CALLBACK(show_empty_note), chooser);
    }
  }

  g_object_weak_unref(G_OBJECT(priv->roster), roster_weak_notify, chooser);
  priv->roster = NULL;
  priv->closure = NULL;
}

static void
setup_roster_waitable(OssoABookContactChooser *chooser)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  destroy_contact_model(chooser);

  if (priv->show_empty_note)
  {
    priv->roster = osso_abook_list_store_get_roster(
          OSSO_ABOOK_LIST_STORE(priv->contact_model));
    g_object_weak_ref(G_OBJECT(priv->roster), roster_weak_notify, chooser);
    priv->closure = osso_abook_waitable_call_when_ready(
          OSSO_ABOOK_WAITABLE(priv->roster),
          _roster_is_ready_cb, chooser, NULL);
  }
}

static void
osso_abook_contact_chooser_real_set_model(OssoABookContactChooser *chooser,
                                          OssoABookContactModel *model)
{
  OssoABookContactChooserPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_MODEL (model));

  priv = osso_abook_contact_chooser_get_instance_private(chooser);

  g_object_freeze_notify(G_OBJECT(chooser));
  destroy_models(chooser);
  priv->contact_model = g_object_ref(model);
  setup_roster_waitable(chooser);

  if (priv->contact_order != -1)
  {
    OssoABookContactOrder order = osso_abook_list_store_get_contact_order(
          OSSO_ABOOK_LIST_STORE(priv->contact_model));

    if (order != priv->contact_order )
    {
      priv->contact_order = order;
      g_object_notify(G_OBJECT(chooser), "contact-order");
    }
  }

  priv->filter_model = osso_abook_filter_model_new(
        OSSO_ABOOK_LIST_STORE(priv->contact_model));
  priv->filter_model = priv->filter_model;
  osso_abook_filter_model_set_visible_func(
        priv->filter_model, _contact_visible_cb, chooser, NULL);
  g_signal_connect_after(priv->filter_model, "row-inserted",
                         G_CALLBACK(_row_inserted), chooser);

  if (priv->live_search)
  {
    hildon_live_search_set_filter(HILDON_LIVE_SEARCH(priv->live_search),
                                  GTK_TREE_MODEL_FILTER(priv->filter_model));
  }

  if (priv->contact_view)
  {
    osso_abook_tree_view_set_base_model(
          OSSO_ABOOK_TREE_VIEW(priv->contact_view),
          OSSO_ABOOK_LIST_STORE(priv->contact_model));
    osso_abook_tree_view_set_filter_model(
          OSSO_ABOOK_TREE_VIEW(priv->contact_view),
          OSSO_ABOOK_FILTER_MODEL(priv->filter_model));
  }

  g_object_thaw_notify(G_OBJECT(chooser));
}

static void
osso_abook_contact_chooser_real_set_contact_order(
    OssoABookContactChooser *chooser, OssoABookContactOrder order)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  if (order != priv->contact_order)
  {
    priv->contact_order = order;

    if (order != -1 && priv->contact_model)
    {
      if (osso_abook_contact_model_is_default(priv->contact_model))
      {
        osso_abook_contact_chooser_real_set_model(
              chooser, create_custom_model(chooser, priv->contact_order));
      }
      else
        set_model_order(chooser, priv->contact_model, priv->contact_order);
    }
  }
}

static void
osso_abook_contact_chooser_real_set_done_label(OssoABookContactChooser *chooser,
                                               const char *label)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  if (!label)
    label = dgettext("hildon-libs", "wdgt_bd_done");

  gtk_button_set_label(GTK_BUTTON(priv->done_button), label);
}

static void
osso_abook_contact_chooser_real_set_hide_offline_contacts(
    OssoABookContactChooser *chooser, gboolean state)
{
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  priv->hide_offline_contacts = state;
  osso_abook_contact_chooser_refilter(chooser);

  if (priv->show_empty_note)
  {
    g_warning(
      "%s: Setting property \"hide-offline-contacts\" after \"show-empty-note\" is slow. Please set \"show-empty-note"
      "\" first, then \"hide-offline-contacts\".", __FUNCTION__);
    setup_roster_waitable(chooser);
  }
}

static void
osso_abook_contact_chooser_set_property(GObject *object, guint property_id,
                                        const GValue *value, GParamSpec *pspec)
{
  OssoABookContactChooser *chooser = OSSO_ABOOK_CONTACT_CHOOSER(object);
  OssoABookContactChooserPrivate *priv =
      osso_abook_contact_chooser_get_instance_private(chooser);

  switch (property_id)
  {
    case PROP_CAPABILITIES:
      priv->caps_flags = g_value_get_flags(value);
      osso_abook_contact_chooser_refilter(chooser);
      return;
    case PROP_CONTACTS_ORDER:
      osso_abook_contact_chooser_real_set_contact_order(
            chooser, g_value_get_enum(value));
      return;
    case PROP_EXCLUDED_CONTACTS:
      osso_abook_string_list_free(priv->excluded_contacts);
      priv->excluded_contacts = g_value_dup_boxed(value);
      osso_abook_contact_chooser_refilter(chooser);
      return;
    case PROP_MINIMUM_SELECTION:
      osso_abook_contact_view_set_minimum_selection(
            OSSO_ABOOK_CONTACT_VIEW(priv->contact_view),
            g_value_get_uint(value));
      return;
    case PROP_MAXIMUM_SELECTION:
      osso_abook_contact_view_set_maximum_selection(
            OSSO_ABOOK_CONTACT_VIEW(priv->contact_view),
            g_value_get_uint(value));
      return;
    case PROP_DONE_LABEL:
    {
      osso_abook_contact_chooser_real_set_done_label(chooser,
                                                     g_value_get_string(value));
      return;
    }
    case PROP_MODEL:
      osso_abook_contact_chooser_real_set_model(
            chooser, OSSO_ABOOK_CONTACT_MODEL(g_value_get_object(value)));
      return;
    case PROP_TITLE:
    {
      GParamSpec *property;
      const char *title = g_value_get_string(value);
      GValue val = G_VALUE_INIT;
      const GValue *v = value;

      priv->title_valid = TRUE;

      if (title && *title)
      {
        if (strcmp(title, _("addr_ti_sel_contact")) &&
            strcmp(title, _("addr_ti_dia_select_contacts")))
        {
            priv->title_valid = FALSE;
        }
      }

      if (priv->title_valid)
      {
        g_value_init(&val, G_TYPE_STRING);

        if (priv->multiple_selection)
          title = _("addr_ti_dia_select_contacts");
        else
          title = _("addr_ti_sel_contact");

        g_value_set_string(&val, title);
        v = &val;
      }

      property = g_object_class_find_property(
            osso_abook_contact_chooser_parent_class,
            g_param_spec_get_name(pspec));
      G_OBJECT_CLASS(g_type_class_peek(property->owner_type))->set_property(
            object, property->param_id, v, property);

      if (v != value)
        g_value_reset(&val);

      return;
    }
    case PROP_HIDE_OFFLINE_CONTACTS:
      osso_abook_contact_chooser_real_set_hide_offline_contacts(
            chooser, g_value_get_boolean(value));
      return;
    case PROP_SHOW_EMPTY_NOTE:
      priv->show_empty_note = g_value_get_boolean(value);
      setup_roster_waitable(chooser);
      return;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      return;
  }
}

static void
osso_abook_contact_chooser_class_init(OssoABookContactChooserClass *klass)
{
  GObjectClass *object_class  = G_OBJECT_CLASS(klass);

//  object_class->constructed = osso_abook_contact_chooser_constructed;
  object_class->set_property = osso_abook_contact_chooser_set_property;
  object_class->get_property = osso_abook_contact_chooser_get_property;
  object_class->dispose = osso_abook_contact_chooser_dispose;
  object_class->finalize = osso_abook_contact_chooser_finalize;

  g_object_class_install_property(
        object_class, PROP_CAPABILITIES,
        g_param_spec_flags(
          "capabilities",
          "Capabilities",
          "The set of permitted contact capabilities",
          OSSO_ABOOK_TYPE_CAPS,
          OSSO_ABOOK_CAPS_ALL,
          G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
        object_class, PROP_CONTACTS_ORDER,
        g_param_spec_enum(
                 "contact-order",
                 "Contact order",
                 "Sort order for contacts",
                 OSSO_TYPE_ABOOK_CONTACT_ORDER,
                 -1,
                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
        object_class, PROP_EXCLUDED_CONTACTS,
        g_param_spec_boxed(
                 "excluded-contacts",
                 "Excluded Contacts",
                 "UIDs of contacts explicitly excluded from the model",
                 OSSO_ABOOK_TYPE_STRING_LIST,
                 G_PARAM_READWRITE | G_PARAM_CONSTRUCT));
  g_object_class_install_property(
        object_class, PROP_MINIMUM_SELECTION,
        g_param_spec_uint(
                  "minimum-selection",
                  "Minimum Selection",
                  "The minimum number of rows to be selected",
                  PROP_CAPABILITIES,
                  G_MAXUINT,
                  PROP_CAPABILITIES,
                  G_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_MAXIMUM_SELECTION,
        g_param_spec_uint(
                  "maximum-selection",
                  "Maximum Selection",
                  "The maximum number of rows to be selected",
                  PROP_CAPABILITIES,
                  G_MAXUINT,
                  PROP_CAPABILITIES,
                  G_PARAM_READWRITE));

  g_object_class_install_property(
        object_class, PROP_DONE_LABEL,
        g_param_spec_string(
                  "done-label",
                  "Done Label",
                  "The text shown on the Done button when visible",
                  dgettext("hildon-libs", "wdgt_bd_done"),
                  G_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_CONTACT_VIEW,
        g_param_spec_object(
                  "contact-view",
                  "Contact View",
                  "The tree view listing the contacts",
                  OSSO_ABOOK_TYPE_CONTACT_VIEW,
                  G_PARAM_READABLE));
  g_object_class_install_property(
        object_class, PROP_MODEL,
        g_param_spec_object(
                  "model",
                  "Model",
                  "The tree model from which to retrieve contacts",
                  OSSO_ABOOK_TYPE_CONTACT_MODEL,
                  G_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_HIDE_OFFLINE_CONTACTS,
        g_param_spec_boolean(
                  "hide-offline-contacts",
                  "Hide offline contacts",
                  "Whether to hide offline contacts in the chooser",
                  FALSE,
                  G_PARAM_READWRITE));
  g_object_class_install_property(
        object_class, PROP_SHOW_EMPTY_NOTE,
        g_param_spec_boolean(
          "show-empty-note",
          "Show empty note",
          "Whether to show empty note when no contacts exist",
          FALSE,
          G_PARAM_READWRITE));

  g_object_class_override_property(object_class, PROP_TITLE, "title");
}
