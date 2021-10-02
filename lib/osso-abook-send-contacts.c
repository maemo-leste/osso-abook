/*
 * osso-abook-send-contacts.c
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
#include <dbus/dbus.h>
#include <gtk/gtkprivate.h>
#include <libmodest-dbus-client/libmodest-dbus-client.h>

#include <errno.h>

#include "config.h"

#include "osso-abook-errors.h"
#include "osso-abook-init.h"
#include "osso-abook-log.h"

#include "osso-abook-send-contacts.h"

struct _OssoABookSendContactsDialogPrivate
{
  OssoABookContact *contact;
  gboolean is_detail;
};

typedef struct _OssoABookSendContactsDialogPrivate
  OssoABookSendContactsDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookSendContactsDialog,
  osso_abook_send_contacts_dialog,
  GTK_TYPE_DIALOG
);

#define PRIVATE(dialog) \
  ((OssoABookSendContactsDialogPrivate *) \
   osso_abook_send_contacts_dialog_get_instance_private( \
     ((OssoABookSendContactsDialog *)dialog)))

enum
{
  PROP_CONTACT = 1,
  PROP_IS_DETAIL
};

static void
osso_abook_send_contacts_dialog_set_property(GObject *object,
                                             guint property_id,
                                             const GValue *value,
                                             GParamSpec *pspec)
{
  OssoABookSendContactsDialogPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_CONTACT:
    {
      if (priv->contact)
        g_object_unref(priv->contact);

      priv->contact = g_value_dup_object(value);
      break;
    }

    case PROP_IS_DETAIL:
    {
      priv->is_detail = g_value_get_boolean(value);
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
osso_abook_send_contacts_dialog_get_property(GObject *object,
                                             guint property_id,
                                             GValue *value,
                                             GParamSpec *pspec)
{
  OssoABookSendContactsDialogPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_CONTACT:
    {
      g_value_set_object(value, priv->contact);
      break;
    }

    case PROP_IS_DETAIL:
    {
      g_value_set_boolean(value, priv->is_detail);
    }

    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
send_sms_clicked_cb(GtkButton *button, gpointer user_data)
{
  g_assert(0);
  /*
     osso_abook_send_contacts_sms(PRIVATE(user_data)->contact);*/
  gtk_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_NONE);
}

static gboolean
ask_send_avatar(OssoABookSendContactsDialog *dialog)
{
  OssoABookSendContactsDialogPrivate *priv = PRIVATE(dialog);
  gboolean send_avatar = FALSE;

  if (osso_abook_avatar_get_image(OSSO_ABOOK_AVATAR(priv->contact)))
  {
    GtkWidget *note = hildon_note_new_confirmation(GTK_WINDOW(dialog),
                                                   _("addr_nc_send_avatar"));
    send_avatar = gtk_dialog_run(GTK_DIALOG(note)) == GTK_RESPONSE_OK;
    gtk_widget_destroy(note);
  }

  return send_avatar;
}

static void
send_email_clicked_cb(GtkButton *button, gpointer user_data)
{
  OssoABookSendContactsDialogPrivate *priv = PRIVATE(user_data);

  osso_abook_send_contacts_email(priv->contact, ask_send_avatar(user_data));
  gtk_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_NONE);
}

static void
send_bt_clicked_cb(GtkButton *button, gpointer user_data)
{
  OssoABookSendContactsDialogPrivate *priv = PRIVATE(user_data);

  osso_abook_send_contacts_bluetooth(priv->contact, ask_send_avatar(user_data));
  gtk_dialog_response(GTK_DIALOG(user_data), GTK_RESPONSE_NONE);
}

static void
osso_abook_send_contacts_dialog_constructed(GObject *object)
{
  OssoABookSendContactsDialogPrivate *priv = PRIVATE(object);
  const gchar *msgid;
  GtkWidget *button;
  GtkWidget *table;

  gtk_widget_hide(GTK_DIALOG(object)->action_area);

  if (priv->is_detail)
    msgid = "addr_ti_send_card_detail";
  else
    msgid = "addr_ti_send_card";

  gtk_window_set_title(GTK_WINDOW(object), _(msgid));
  table = gtk_table_new(2, 2, TRUE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(object)->vbox), table, TRUE, TRUE, 6);
  gtk_widget_show(table);
  button = hildon_button_new_with_text(HILDON_SIZE_FINGER_HEIGHT,
                                       HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
                                       _("addr_bd_send_card_sms"), NULL);
  g_signal_connect(button, "clicked", G_CALLBACK(send_sms_clicked_cb), object);
  gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 1, 0, 1);
  gtk_widget_show(button);
  button = hildon_button_new_with_text(HILDON_SIZE_FINGER_HEIGHT,
                                       HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
                                       _("addr_bd_send_card_email"), NULL);
  g_signal_connect(button, "clicked", G_CALLBACK(send_email_clicked_cb),
                   object);
  gtk_table_attach_defaults(GTK_TABLE(table), button, 1, 2, 0, 1);
  gtk_widget_show(button);
  button = hildon_button_new_with_text(HILDON_SIZE_FINGER_HEIGHT,
                                       HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
                                       _("addr_bd_send_card_bluetooth"), NULL);
  g_signal_connect(button, "clicked", G_CALLBACK(send_bt_clicked_cb), object);
  gtk_table_attach_defaults(GTK_TABLE(table), button, 0, 1, 1, 2);
  gtk_widget_show(button);
}

static void
osso_abook_send_contacts_dialog_dispose(GObject *object)
{
  OssoABookSendContactsDialogPrivate *priv = PRIVATE(object);

  if (priv->contact)
  {
    g_object_unref(priv->contact);
    priv->contact = NULL;
  }

  G_OBJECT_CLASS(osso_abook_send_contacts_dialog_parent_class)->dispose(object);
}

static void
osso_abook_send_contacts_dialog_class_init(
  OssoABookSendContactsDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_send_contacts_dialog_set_property;
  object_class->get_property = osso_abook_send_contacts_dialog_get_property;
  object_class->constructed = osso_abook_send_contacts_dialog_constructed;
  object_class->dispose = osso_abook_send_contacts_dialog_dispose;

  g_object_class_install_property(
    object_class, PROP_CONTACT,
    g_param_spec_object(
      "contact", "The contact", "The contact that need to be sent",
      OSSO_ABOOK_TYPE_CONTACT,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_IS_DETAIL,
    g_param_spec_boolean(
      "is-detail", "Is detail", "Are we sending a contact detail",
      FALSE,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
osso_abook_send_contacts_dialog_init(OssoABookSendContactsDialog *dialog)
{}

GtkWidget *
osso_abook_send_contacts_dialog_new(GtkWindow *parent,
                                    OssoABookContact *contact,
                                    gboolean is_detail)
{
  g_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), NULL);
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_SEND_CONTACTS_DIALOG,
                      "contact", contact,
                      "is-detail", is_detail,
                      "transient-for", parent,
                      "modal", TRUE,
                      "destroy-with-parent", TRUE,
                      NULL);
}

static GFile *
create_temp_dir()
{
  const char *tmp_dir = g_get_tmp_dir();
  GFile *file;

  if (g_mkdir_with_parents(tmp_dir, 01777) == -1)
  {
    OSSO_ABOOK_WARN("Cannot create directory %s: %s", tmp_dir,
                    g_strerror(errno));
  }
  else
  {
    gchar *filename = g_build_filename(tmp_dir, "osso-abook-XXXXXX", NULL);

    if (mkdtemp(filename))
      file = g_file_new_for_path(filename);
    else
    {
      OSSO_ABOOK_WARN("Cannot create directory %s: %s", filename,
                      g_strerror(errno));
    }

    g_free(filename);
  }

  return file;
}

static DBusHandlerResult
send_file_done(DBusConnection *connection, DBusMessage *message,
               void *user_data)
{
  gchar *filename = user_data;
  gchar *tmp_dir;

  if (!dbus_message_is_signal(message, "com.nokia.bt_ui", "send_file"))
    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;

  dbus_connection_remove_filter(connection, send_file_done, user_data);
  tmp_dir = g_path_get_dirname(filename);
  unlink(filename + 7);
  rmdir(tmp_dir);
  g_free(tmp_dir);

  return DBUS_HANDLER_RESULT_HANDLED;
}

static void
add_bt_ui_dbus_filter(gchar *file)
{
  DBusConnection *bus = dbus_bus_get(DBUS_BUS_SYSTEM, NULL);

  dbus_connection_add_filter(bus, send_file_done, file,
                             (DBusFreeFunction)g_free);
  dbus_bus_add_match(bus, "type='signal',interface='com.nokia.bt_ui'", NULL);
  dbus_connection_unref(bus);
}

static void
send_vcard_bt(OssoABookContact *contact, GFile *file, const GError *error,
              gpointer user_data)
{
  GError *err = NULL;

  if (error)
    osso_abook_handle_gerror(NULL, g_error_copy(error));
  else
  {
    DBusGConnection *bus = dbus_g_bus_get(DBUS_BUS_SYSTEM, &err);

    if (bus)
    {
      gchar *files[2] = {};
      DBusGProxy *proxy = dbus_g_proxy_new_for_name(bus,
                                                    "com.nokia.bt_ui",
                                                    "/com/nokia/bt_ui",
                                                    "com.nokia.bt_ui");
      files[0] = g_file_get_uri(file);

      if (!dbus_g_proxy_call(proxy, "show_send_file_dlg", &err,
                             G_TYPE_STRV, files,
                             G_TYPE_INVALID, G_TYPE_INVALID))
      {
        osso_abook_handle_gerror(NULL, err);
      }

      add_bt_ui_dbus_filter(files[0]);
      g_object_unref(proxy);
      dbus_g_connection_unref(bus);
    }
    else
      osso_abook_handle_gerror(NULL, err);
  }
}

void
osso_abook_send_contacts_bluetooth(OssoABookContact *contact,
                                   gboolean send_avatar)
{
  GFile *dir;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(contact));

  dir = create_temp_dir();

  if (!dir)
    return;

  osso_abook_contact_write_to_file(contact, EVC_FORMAT_VCARD_21, send_avatar,
                                   TRUE, dir, send_vcard_bt, NULL);
  g_object_unref(dir);
}

static void
send_vcard_email(OssoABookContact *contact, GFile *file, const GError *error,
                 gpointer user_data)
{
  if (error)
    osso_abook_handle_gerror(NULL, g_error_copy(error));
  else
  {
    GSList *attachments;
    char *uri = g_file_get_uri(file);

    attachments = g_slist_prepend(NULL, uri);
    libmodest_dbus_client_compose_mail(osso_abook_get_osso_context(),
                                       NULL, NULL, NULL, NULL, NULL,
                                       attachments);
    g_slist_free(attachments);
    g_free(uri);
  }
}

void
osso_abook_send_contacts_email(OssoABookContact *contact, gboolean send_avatar)
{
  GFile *dir;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(contact));

  dir = create_temp_dir();

  if (dir)
  {
    osso_abook_contact_write_to_file(contact, EVC_FORMAT_VCARD_30, send_avatar,
                                     TRUE, dir, send_vcard_email, NULL);
    g_object_unref(dir);
  }
}
