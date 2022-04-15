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

#include "config.h"

#include <glib.h>
#include <rtcom-accounts-ui-client/client.h>

#include "osso-abook-gconf-contact.h"
#include "osso-abook-init.h"
#include "osso-abook-log.h"
#include "osso-abook-roster.h"
#include "osso-abook-utils-private.h"

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
