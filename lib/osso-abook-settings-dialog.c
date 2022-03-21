/*
 * osso-abook-settings-dialog.c
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

#include <gtk/gtkprivate.h>

#include <libintl.h>

#include "osso-abook-aggregator.h"
#include "osso-abook-errors.h"
#include "osso-abook-settings.h"
#include "osso-abook-voicemail-contact.h"
#include "osso-abook-voicemail-selector.h"

#include "osso-abook-settings-dialog.h"

struct _OssoABookSettingsDialogPrivate
{
  EBook *book;
  OssoABookNameOrder name_order;
  OssoABookVoicemailSelector *voicemail_selector;
  GtkWidget *display_video_calling_options;
  GtkWidget *display_sms_only_for_mobile;
  int delete_count;
};

typedef struct _OssoABookSettingsDialogPrivate OssoABookSettingsDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookSettingsDialog,
  osso_abook_settings_dialog,
  GTK_TYPE_DIALOG
);

#define PRIVATE(dialog) \
  ((OssoABookSettingsDialogPrivate *) \
   osso_abook_settings_dialog_get_instance_private( \
     (OssoABookSettingsDialog *)(dialog)))

enum
{
  PROP_EBOOK = 1
};

static void
osso_abook_settings_dialog_set_property(GObject *object, guint property_id,
                                        const GValue *value, GParamSpec *pspec)
{
  OssoABookSettingsDialogPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_EBOOK:
    {
      if (priv->book)
        g_object_unref(priv->book);

      priv->book = g_object_ref(g_value_get_object(value));
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
osso_abook_settings_dialog_get_property(GObject *object, guint property_id,
                                        GValue *value, GParamSpec *pspec)
{
  OssoABookSettingsDialogPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_EBOOK:
    {
      g_value_set_object(value, priv->book);
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
osso_abook_settings_dialog_finalize(GObject *object)
{
  OssoABookSettingsDialogPrivate *priv = PRIVATE(object);

  g_object_unref(priv->book);

  G_OBJECT_CLASS(osso_abook_settings_dialog_parent_class)->finalize(object);
}

static void
osso_abook_settings_dialog_class_init(OssoABookSettingsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_settings_dialog_set_property;
  object_class->get_property = osso_abook_settings_dialog_get_property;
  object_class->finalize = osso_abook_settings_dialog_finalize;

  g_object_class_install_property(
    object_class, PROP_EBOOK,
    g_param_spec_object(
      "ebook",
      "The system addressbook",
      "The system addressbook",
      E_TYPE_BOOK,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
osso_abook_settings_dialog_response(GtkDialog *dialog, gint response_id,
                                    gpointer user_data)
{
  OssoABookSettingsDialogPrivate *priv = PRIVATE(dialog);

  if (priv->delete_count)
    g_signal_stop_emission_by_name(dialog, "response");
  else if (response_id == GTK_RESPONSE_OK)
  {
    osso_abook_settings_set_name_order(priv->name_order);
    osso_abook_voicemail_selector_save(priv->voicemail_selector);
    osso_abook_settings_set_video_button(
      hildon_check_button_get_active(
        HILDON_CHECK_BUTTON(priv->display_video_calling_options)));
    osso_abook_settings_set_sms_button(
      hildon_check_button_get_active(
        HILDON_CHECK_BUTTON(priv->display_sms_only_for_mobile)));
  }
}

static const gchar *
name_order_get_label(OssoABookNameOrder name_order)
{
  const gchar *label;

  switch (name_order)
  {
    case OSSO_ABOOK_NAME_ORDER_FIRST:
    {
      label = _("addr_va_firstname");
      break;
    }
    case OSSO_ABOOK_NAME_ORDER_LAST:
    {
      label = _("addr_va_lastname_comma");
      break;
    }
    case OSSO_ABOOK_NAME_ORDER_LAST_SPACE:
    {
      label = _("addr_va_lastname");
      break;
    }
    case OSSO_ABOOK_NAME_ORDER_NICK:
    {
      label = _("addr_va_nickname");
      break;
    }
    default:
    {
      g_return_val_if_reached(NULL);
      break;
    }
  }

  return label;
}

static void
displayname_value_changed_cb(HildonPickerButton *button, gpointer user_data)
{
  OssoABookSettingsDialogPrivate *priv = PRIVATE(user_data);
  HildonTouchSelector *selector = hildon_picker_button_get_selector(button);
  GtkTreeIter iter;

  if (hildon_touch_selector_get_selected(selector, 0, &iter))
  {
    GtkTreeModel *model = hildon_touch_selector_get_model(selector, 0);
    OssoABookNameOrder name_order;

    gtk_tree_model_get(model, &iter, 1, &name_order, -1);
    priv->name_order = name_order;
  }
}

typedef struct
{
  EBook *book;
  EBookCallback callback;
  gpointer closure;
} remove_data;

static void
remove_all_contacts_get_contacts_cb (EBook *book, EBookStatus status,
                                     GList *contacts, gpointer closure)
{
  remove_data *data = closure;
  EBookCallback cb = data->callback;

  if (E_BOOK_ERROR_OK == status)
  {
    GList *uids = NULL;
    GList *l;

    for (l = contacts; l; l = l->next)
    {
      uids = g_list_prepend(uids,
                            (gpointer)e_contact_get_const(l->data,
                                                          E_CONTACT_UID));
    }

    if (e_book_async_remove_contacts(book, uids, cb, data->closure))
      status = E_BOOK_ERROR_OTHER_ERROR;

    g_list_free(uids);
  }

  g_list_free_full(contacts, g_object_unref);

  if ((E_BOOK_ERROR_OK != status) && cb)
    cb(data->book, status, data->closure);

  g_free(data);
}

static guint
async_remove_all_contacts(EBook *book, EBookCallback cb, gpointer closure)
{
  EBookQuery *query = e_book_query_any_field_contains("");
  remove_data *data = g_new(remove_data, 1);
  gint rv;

  data->book = book;
  data->callback = cb;
  data->closure = closure;

    rv = e_book_async_get_contacts(book, query,
                                 remove_all_contacts_get_contacts_cb, data);
  e_book_query_unref(query);

  return rv;
}

static void
contact_removed_cb(EBook *book, EBookStatus status, gpointer closure)
{
  OssoABookSettingsDialogPrivate *priv = PRIVATE(closure);

  priv->delete_count--;

  if (!priv->delete_count)
  {
    hildon_gtk_window_set_progress_indicator(closure, FALSE);
    gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(closure)), TRUE);
  }

  if (status != E_BOOK_ERROR_OK)
    osso_abook_handle_estatus(NULL, status, book);

  g_object_unref(closure);
}

static void
delete_all_note_response_cb(GtkWidget *dialog, gint response_id,
                            GTypeInstance *user_data)
{
  if (response_id == GTK_RESPONSE_OK)
  {
    OssoABookSettingsDialogPrivate *priv = PRIVATE(user_data);
    OssoABookRoster *aggregator;
    GList *l;

    hildon_gtk_window_set_progress_indicator(GTK_WINDOW(user_data), TRUE);

    aggregator = osso_abook_aggregator_get_default(NULL);
    gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(user_data)), FALSE);

    for (l = osso_abook_aggregator_list_roster_contacts(
             OSSO_ABOOK_AGGREGATOR(aggregator)); l;
         l = g_list_delete_link(l, l))
    {
      EBook *book;
      gchar *id = g_strdup_printf(
          "%s;*", (char *)e_contact_get_const(E_CONTACT(l->data),
                                              E_CONTACT_UID));
      priv->delete_count++;
      book = osso_abook_roster_get_book(osso_abook_contact_get_roster(l->data));
      e_book_async_remove_contact_by_id(book, id, contact_removed_cb,
                                        g_object_ref(user_data));
      g_free(id);
    }

    priv->delete_count++;
    async_remove_all_contacts(priv->book, contact_removed_cb,
                              g_object_ref(user_data));
  }

  gtk_widget_destroy(dialog);
  g_object_unref(user_data);
}

static void
delete_locally_clicked_cb(GtkWidget *button, gpointer user_data)
{
  GtkWidget *note = hildon_note_new_confirmation(
      GTK_WINDOW(user_data), _("addr_nc_delete_all_locally"));

  g_signal_connect(note, "response",
                   G_CALLBACK(delete_all_note_response_cb),
                   g_object_ref(user_data));
  gtk_widget_show(note);
}

static void
osso_abook_settings_dialog_init(OssoABookSettingsDialog *dialog)
{
  OssoABookSettingsDialogPrivate *priv = PRIVATE(dialog);
  OssoABookNameOrder name_order;
  GtkListStore *store;
  HildonTouchSelectorColumn *col;
  GtkWidget *button;
  GtkWidget *voicemail_picker;
  GtkWidget *voicemail_selector;
  char *preferred_number;
  GtkWidget *displayname_button;
  GtkWidget *content_area;
  HildonTouchSelector *selector;
  GtkTreeIter iter;

  priv->name_order = osso_abook_settings_get_name_order();
  g_signal_connect(dialog, "delete-event", G_CALLBACK(gtk_true), NULL);
  g_signal_connect(dialog, "response",
                   G_CALLBACK(osso_abook_settings_dialog_response), NULL);
  gtk_window_set_title(GTK_WINDOW(dialog), _("addr_ti_settings2"));
  content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  displayname_button = hildon_picker_button_new(
      HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  name_order = osso_abook_settings_get_name_order();
  hildon_button_set_text(HILDON_BUTTON(displayname_button),
                         _("addr_fi_displayname"), NULL);
  gtk_button_set_alignment(GTK_BUTTON(displayname_button), 0.0f, 0.5f);
  selector = HILDON_TOUCH_SELECTOR(hildon_touch_selector_new());
  store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_INT);
  col = hildon_touch_selector_append_text_column(
      selector, GTK_TREE_MODEL(store), TRUE);
  g_object_set(col, "text-column", 0, NULL);

  gtk_list_store_insert_with_values(
    store, NULL, -1,
    0, name_order_get_label(OSSO_ABOOK_NAME_ORDER_FIRST),
    1, OSSO_ABOOK_NAME_ORDER_FIRST,
    -1);
  gtk_list_store_insert_with_values(
    store, NULL, -1,
    0, name_order_get_label(OSSO_ABOOK_NAME_ORDER_LAST),
    1, OSSO_ABOOK_NAME_ORDER_LAST,
    -1);
  gtk_list_store_insert_with_values(
    store, NULL, -1,
    0, name_order_get_label(OSSO_ABOOK_NAME_ORDER_LAST_SPACE),
    1, OSSO_ABOOK_NAME_ORDER_LAST_SPACE,
    -1);
  gtk_list_store_insert_with_values(
    store, NULL, -1,
    0, name_order_get_label(OSSO_ABOOK_NAME_ORDER_NICK),
    1, OSSO_ABOOK_NAME_ORDER_NICK,
    -1);

  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(displayname_button),
                                    selector);

  if (!gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &iter))
    g_assert(0);

  while (1)
  {
    gint idx;

    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, 1, &idx, -1);

    if (name_order == idx)
      break;

    if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &iter))
      g_assert(0);
  }

  hildon_touch_selector_select_iter(selector, 0, &iter, 0);
  hildon_button_set_value(HILDON_BUTTON(displayname_button),
                          name_order_get_label(name_order));
  gtk_container_add(GTK_CONTAINER(content_area), displayname_button);
  g_signal_connect(displayname_button, "value-changed",
                   G_CALLBACK(displayname_value_changed_cb), dialog);

  priv->display_video_calling_options =
    hildon_check_button_new(HILDON_SIZE_FINGER_HEIGHT);
  gtk_button_set_label(GTK_BUTTON(priv->display_video_calling_options),
                       _("addr_fi_display_video_calling_options"));
  hildon_check_button_set_active(
    HILDON_CHECK_BUTTON(priv->display_video_calling_options),
    osso_abook_settings_get_video_button());
  gtk_container_add(GTK_CONTAINER(content_area),
                    priv->display_video_calling_options);

  priv->display_sms_only_for_mobile =
    hildon_check_button_new(HILDON_SIZE_FINGER_HEIGHT);
  gtk_button_set_label(GTK_BUTTON(priv->display_sms_only_for_mobile),
                       _("addr_fi_display_sms_only_for_mobile"));
  hildon_check_button_set_active(
    HILDON_CHECK_BUTTON(priv->display_sms_only_for_mobile),
    osso_abook_settings_get_sms_button());
  gtk_container_add(GTK_CONTAINER(content_area),
                    priv->display_sms_only_for_mobile);

  button = hildon_gtk_button_new(HILDON_SIZE_FINGER_HEIGHT);
  gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5);
  gtk_button_set_label(GTK_BUTTON(button), _("addr_bd_delete_locally"));
  gtk_container_add(GTK_CONTAINER(content_area), button);
  g_signal_connect(button, "clicked",
                   G_CALLBACK(delete_locally_clicked_cb), dialog);

  voicemail_picker = hildon_picker_button_new(
      HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL);
  voicemail_selector = osso_abook_voicemail_selector_new();
  priv->voicemail_selector = OSSO_ABOOK_VOICEMAIL_SELECTOR(voicemail_selector);
  hildon_button_set_text(HILDON_BUTTON(voicemail_picker),
                         _("addr_bd_voicemail"), NULL);
  gtk_button_set_alignment(GTK_BUTTON(voicemail_picker), 0.0, 0.5);
  gtk_container_add(GTK_CONTAINER(content_area), voicemail_picker);
  preferred_number = osso_abook_voicemail_contact_get_preferred_number(NULL);
  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(voicemail_picker),
                                    HILDON_TOUCH_SELECTOR(voicemail_selector));
  hildon_button_set_value(HILDON_BUTTON(voicemail_picker), preferred_number);
  g_free(preferred_number);
  g_signal_connect_swapped(voicemail_picker, "value-changed",
                           G_CALLBACK(osso_abook_voicemail_selector_apply),
                           voicemail_selector);

  gtk_dialog_add_button(GTK_DIALOG(dialog),
                        dgettext("hildon-libs", "wdgt_bd_save"),
                        GTK_RESPONSE_OK);
  gtk_widget_show_all(gtk_bin_get_child(GTK_BIN(dialog)));
}

GtkWidget *
osso_abook_settings_dialog_new(GtkWindow *parent,
                               EBook *book)
{
  g_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), NULL);
  g_return_val_if_fail(E_IS_BOOK(book), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_SETTINGS_DIALOG,
                      "ebook", book,
                      "transient-for", parent,
                      "modal", TRUE,
                      "destroy-with-parent", TRUE,
                      "has-separator", FALSE,
                      NULL);
}
