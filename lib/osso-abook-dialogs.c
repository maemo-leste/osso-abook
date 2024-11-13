/*
 * osso-abook-dialogs.c
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

/**
 * SECTION:osso-abook-dialogs
 * @short_description: Collection of dialog helper routines.
 *
 * Helper routines to display dialogs and hook them up to all the necessary
 * signals. Includes dialogs to delete and block contacts, create, delete and
 * rename groups, choose email address from a contact and add a contact.
 *
 */

#include "config.h"

#include <glib.h>
#include <rtcom-accounts-ui-client/client.h>

#include "osso-abook-gconf-contact.h"
#include "osso-abook-init.h"
#include "osso-abook-log.h"
#include "osso-abook-roster.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-contact-editor.h"

#include "osso-abook-dialogs.h"

static gchar *
get_delete_confirmation_description(OssoABookContact *contact)
{
  GList *contacts = g_list_prepend(NULL, contact);
  gchar *confirmation = _osso_abook_get_delete_confirmation_string(
      contacts, TRUE, "addr_nc_notification16",
      "addr_nc_notification_im_username",
      "addr_nc_notification_im_username_several_services");

  g_list_free(contacts);

  return confirmation;
}

static void
delete_contact_changed_cb(OssoABookRoster *roster, OssoABookContact **contacts,
                          GtkDialog *dialog)
{
  EContact *contact = g_object_get_data(G_OBJECT(dialog), "contact");
  const char *uid = e_contact_get_const(contact, E_CONTACT_UID);

  while (*contacts)
  {
    if (!strcmp(uid, e_contact_get_const(E_CONTACT(*contacts), E_CONTACT_UID)))
    {
      if (osso_abook_contact_get_blocked(*contacts))
        gtk_dialog_response(dialog, GTK_RESPONSE_CANCEL);

      break;
    }

    contacts++;
  }
}

static void
delete_contact_removed_cb(OssoABookRoster *roster, const char **uids,
                          GtkDialog *dialog)
{
  EContact *contact = g_object_get_data(G_OBJECT(dialog), "contact");
  const char *uid = e_contact_get_const(contact, E_CONTACT_UID);

  while (*uids)
  {
    if (!strcmp(*uids, uid))
    {
      gtk_dialog_response(dialog, GTK_RESPONSE_CANCEL);
      break;
    }

    uids++;
  }
}

gboolean
osso_abook_delete_contact_dialog_run(GtkWindow *parent, OssoABookRoster *roster,
                                     OssoABookContact *contact)
{
  GtkWidget *note;
  gint response;
  EBook *book = NULL;

  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), FALSE);
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), FALSE);
  g_return_val_if_fail(!roster || OSSO_ABOOK_IS_ROSTER(roster), FALSE);

  if (roster)
  {
    gchar *description = get_delete_confirmation_description(contact);

    note = hildon_note_new_confirmation(parent, description);
    g_free(description);
    g_object_set_data_full(G_OBJECT(note),
                           "contact", g_object_ref(contact),
                           (GDestroyNotify)&g_object_unref);
    g_signal_connect(roster, "contacts_changed",
                     G_CALLBACK(delete_contact_changed_cb), note);
    g_signal_connect(roster, "contacts_removed",
                     G_CALLBACK(delete_contact_removed_cb), note);
    response = gtk_dialog_run(GTK_DIALOG(note));
    g_signal_handlers_disconnect_matched(
      roster, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      delete_contact_changed_cb, note);
    g_signal_handlers_disconnect_matched(
      roster, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      delete_contact_removed_cb, note);
  }
  else
  {
    gchar *description;

    g_return_val_if_fail(OSSO_ABOOK_IS_GCONF_CONTACT(contact), FALSE);

    description = get_delete_confirmation_description(contact);
    note = hildon_note_new_confirmation(parent, description);
    g_free(description);
    g_object_set_data_full(G_OBJECT(note),
                           "contact", g_object_ref(contact),
                           (GDestroyNotify)&g_object_unref);
    response = gtk_dialog_run(GTK_DIALOG(note));
  }

  gtk_widget_destroy(note);

  if (response != GTK_RESPONSE_OK)
    return FALSE;

  if (roster)
    book = osso_abook_roster_get_book(roster);

  return osso_abook_contact_delete(contact, book, NULL);
}

gboolean
osso_abook_launch_applet(GtkWindow *parent, const char *applet)
{
  osso_context_t *ctx;
  osso_return_t rc;

  g_return_val_if_fail(!IS_EMPTY(applet), FALSE);
  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), FALSE);

  ctx = osso_abook_get_osso_context();

  if (ctx)
  {
    rc = osso_cp_plugin_execute(ctx, applet, parent, TRUE);

    if (!rc)
      return TRUE;

    OSSO_ABOOK_WARN("Launching `%s' failed, rc=%d", applet, rc);
  }
  else
    OSSO_ABOOK_WARN("OSSO context not set");

  return FALSE;
}

void
osso_abook_add_im_account_dialog_run(GtkWindow *parent)
{
  AuicClient *auic = auic_client_new(parent);

  auic_client_open_accounts_list(auic);
  g_object_unref(auic);
}

void
osso_abook_confirm_delete_contacts_dialog_run(GtkWindow *parent,
                                              OssoABookRoster *roster,
                                              GList *contacts)
{
  GList *l = NULL;
  gchar *confirm;
  GtkWidget *note;
  gint response_id;
  EBook *book = NULL;

  g_return_if_fail(!parent || GTK_IS_WINDOW(parent));
  g_return_if_fail(!roster || OSSO_ABOOK_IS_ROSTER(roster));

  if (!contacts)
    return;

  for (; contacts; contacts = contacts->next)
  {
    l = g_list_prepend(l, contacts->data);

    osso_abook_weak_ref((GObject **)&(l->data));
  }

  if (l->next)
  {
    confirm = _osso_abook_get_delete_confirmation_string(
        l, FALSE, "addr_nc_notification7",
        "addr_nc_notification_im_username_multiple",
        "addr_nc_notification_im_username_multiple_several_services");
  }
  else
    confirm = get_delete_confirmation_description(l->data);

  note = hildon_note_new_confirmation(parent, confirm);
  response_id = gtk_dialog_run(GTK_DIALOG(note));
  gtk_widget_destroy(note);
  g_free(confirm);

  if (response_id == GTK_RESPONSE_OK)
  {
    l = g_list_remove_all(l, NULL);

    if (roster)
      book = osso_abook_roster_get_book(roster);

    if (l)
      osso_abook_contact_delete_many(l, book, parent);
  }

  for (; l; l = g_list_delete_link(l, l))
    osso_abook_weak_unref((GObject **)&(l->data));
}

enum
{
  COLUMN_VALUE,
  COLUMN_ATRIBUTE,
  N_COLUMNS
};

static GList *
osso_abook_choose_attribute_run(GtkWindow *parent, EContact *contact,
                                const char *title,
                                HildonTouchSelectorSelectionMode mode,
                                gboolean (*filter_func)(EVCardAttribute *,
                                                        gpointer),
                                gpointer user_data)
{
  GList *attributes = NULL;
  GList *attrs;
  GtkListStore *store;
  EVCardAttribute *attr;
  char *attr_value;
  GtkWidget *selector;
  GtkWidget *dialog;
  GtkTreeIter iter;

  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), NULL);
  g_return_val_if_fail(E_IS_CONTACT(contact), NULL);

  store = gtk_list_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_POINTER);

  for (attrs = e_vcard_get_attributes(E_VCARD(contact)); attrs;
       attrs = attrs->next)
  {
    if (filter_func(attrs->data, user_data))
    {
      attr_value = e_vcard_attribute_get_value(attr);
      gtk_list_store_append(store, &iter);
      gtk_list_store_set(store, &iter,
                         COLUMN_VALUE, attr_value,
                         COLUMN_ATRIBUTE, attr,
                         -1);
      g_free(attr_value);
    }
  }

  selector = hildon_touch_selector_new();
  hildon_touch_selector_append_text_column(HILDON_TOUCH_SELECTOR(selector),
                                           GTK_TREE_MODEL(store), 1);

  if (hildon_touch_selector_get_column_selection_mode(
        HILDON_TOUCH_SELECTOR(selector)) != mode)
  {
    hildon_touch_selector_set_column_selection_mode(
      HILDON_TOUCH_SELECTOR(selector), mode);
  }

  dialog = hildon_picker_dialog_new(parent);
  gtk_window_set_title(GTK_WINDOW(dialog), title);
  hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog),
                                    HILDON_TOUCH_SELECTOR(selector));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
  {
    GList *paths = hildon_touch_selector_get_selected_rows(
        HILDON_TOUCH_SELECTOR(selector), 0);

    while (paths)
    {
      GtkTreePath *path = paths->data;

      if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path))
      {
        gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
                           COLUMN_ATRIBUTE, &attr,
                           -1);
        attributes = g_list_prepend(attributes, attr);
      }

      paths = g_list_delete_link(paths, paths);
      gtk_tree_path_free(path);
    }
  }

  gtk_widget_destroy(dialog);

  return attributes;
}

static gboolean
filter_attribute_name(EVCardAttribute *attr, const gpointer user_data)
{
  return !g_ascii_strcasecmp(e_vcard_attribute_get_name(attr), user_data);
}

GList *
osso_abook_choose_email_dialog_run(GtkWindow *parent, EContact *contact)
{
  GList *attrs;
  GList *l;

  attrs = osso_abook_choose_attribute_run(
      parent, contact, g_dgettext("osso-addressbook", "addr_ti_select_email"),
      HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE, filter_attribute_name,
      EVC_EMAIL);

  for (l = attrs; l; l = l->next)
    l->data = e_vcard_attribute_get_value(l->data);

  return attrs;
}

static gboolean
filter_caps(EVCardAttribute *attr, gpointer user_data)
{
  if (GPOINTER_TO_UINT(user_data) & OSSO_ABOOK_CAPS_EMAIL)
    return filter_attribute_name(attr, EVC_EMAIL);

  return FALSE;
}

EVCardAttribute *
osso_abook_choose_im_dialog_run(GtkWindow *parent, OssoABookContact *contact,
                                OssoABookCapsFlags type)
{
  GList *attributes;
  EVCardAttribute *attr = NULL;

  attributes = osso_abook_choose_attribute_run(
      parent, E_CONTACT(contact),
      g_dgettext("osso-addressbook", "addr_ti_select_im"),
      HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE, filter_caps,
      GUINT_TO_POINTER(type));

  if (attributes)
    attr = attributes->data;

  g_list_free(attributes);

  return attr;
}

char *
osso_abook_choose_url_dialog_run(GtkWindow *parent, EContact *contact)
{
  GList *attributes;
  char *url = NULL;

  attributes = osso_abook_choose_attribute_run(
      parent, contact, g_dgettext("osso-addressbook", "addr_ti_select_url"),
      HILDON_TOUCH_SELECTOR_SELECTION_MODE_SINGLE, filter_attribute_name,
      EVC_URL);

  if (attributes)
    url = e_vcard_attribute_get_value(attributes->data);

  g_list_free(attributes);

  return url;
}

void
osso_abook_add_contact_dialog_run(GtkWindow *parent)
{
  gint res;
  GtkWidget *dialog;

  g_return_if_fail(GTK_IS_WINDOW(parent));

  dialog = hildon_note_new_confirmation(
      parent, g_dgettext("osso-addressbook", "addr_nc_notification14"));
  res = gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(GTK_WIDGET(dialog));

  if (res == GTK_RESPONSE_OK)
  {
    dialog = osso_abook_contact_editor_new_with_contact(
        parent, NULL, OSSO_ABOOK_CONTACT_EDITOR_CREATE);
    gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
  }
}
