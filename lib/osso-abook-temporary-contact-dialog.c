/*
 * osso-abook-temporary-contact-dialog.c
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
#include "osso-abook-contact-editor.h"
#include "osso-abook-marshal.h"
#include "osso-abook-merge.h"
#include "osso-abook-roster-manager.h"
#include "osso-abook-temporary-contact-dialog.h"
#include "osso-abook-util.h"

struct _OssoABookTemporaryContactDialogPrivate
{
  EVCardAttribute *attribute;
  OssoABookContact *contact;
  TpAccount *account;
  gint buttons_count;
  GtkWidget *table;
  gboolean is_landscape;
  GdkScreen *screen;
};

typedef struct _OssoABookTemporaryContactDialogPrivate
  OssoABookTemporaryContactDialogPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookTemporaryContactDialog,
  osso_abook_temporary_contact_dialog,
  GTK_TYPE_DIALOG
);

#define PRIVATE(dialog) \
  ((OssoABookTemporaryContactDialogPrivate *) \
   osso_abook_temporary_contact_dialog_get_instance_private( \
     ((OssoABookTemporaryContactDialog *)dialog)))

enum
{
  PROP_ATTRIBUTE = 0x1,
  PROP_TMP_CONTACT,
  PROP_ACCOUNT
};

enum
{
  CONTACT_SAVED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};

static void
osso_abook_temporary_contact_dialog_init(OssoABookTemporaryContactDialog *dialog)
{}

static void
add_button(OssoABookTemporaryContactDialog *dialog, const gchar *label,
           const gchar *icon, gpointer clicked_cb)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(dialog);
  GtkWidget *button = hildon_gtk_button_new(HILDON_SIZE_FINGER_HEIGHT);
  guint left_attach;
  guint top_attach;
  guint right_attach;

  gtk_button_set_label(GTK_BUTTON(button), label);

  if (icon)
  {
    gtk_button_set_image(
      GTK_BUTTON(button),
      gtk_image_new_from_icon_name(icon, GTK_ICON_SIZE_BUTTON));
  }

  gtk_button_set_alignment(GTK_BUTTON(button), 0.5, 0.5);
  g_signal_connect_swapped(button, "clicked",
                           G_CALLBACK(clicked_cb), dialog);

  if (priv->is_landscape)
  {
    left_attach = priv->buttons_count & 1;
    top_attach = priv->buttons_count / 2;
    right_attach = left_attach + 1;
  }
  else
  {
    top_attach = priv->buttons_count;
    left_attach = 0;
    right_attach = 1;
  }

  gtk_table_attach(GTK_TABLE(priv->table), button, left_attach, right_attach,
                   top_attach, top_attach + 1, GTK_FILL | GTK_EXPAND, 0, 0, 0);
  gtk_widget_show(button);
  priv->buttons_count++;
}

static void
action_start(OssoABookTemporaryContactDialog *dialog,
             OssoABookContactAction action)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(dialog);

  osso_abook_contact_action_start(action, priv->contact, priv->attribute,
                                  priv->account, NULL);
  gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_NONE);
}

static void
call_clicked_cb(OssoABookTemporaryContactDialog *dialog)
{
  if (!strcmp(e_vcard_attribute_get_name(PRIVATE(dialog)->attribute), EVC_TEL))
    action_start(dialog, OSSO_ABOOK_CONTACT_ACTION_TEL);
  else
    action_start(dialog, OSSO_ABOOK_CONTACT_ACTION_VOIPTO);
}

static void
chat_clicked_cb(OssoABookTemporaryContactDialog *dialog)
{
  action_start(dialog, OSSO_ABOOK_CONTACT_ACTION_CHATTO);
}

static void
sms_clicked_cb(OssoABookTemporaryContactDialog *dialog)
{
  action_start(dialog, OSSO_ABOOK_CONTACT_ACTION_SMS);
}

static void
merged_cb(const char *uid, gpointer user_data)
{
  OssoABookTemporaryContactDialog *dialog =
    OSSO_ABOOK_TEMPORARY_CONTACT_DIALOG(user_data);
  gboolean result = FALSE;

  g_signal_emit(dialog, signals[CONTACT_SAVED], 0, uid, &result);
  gtk_dialog_response(GTK_DIALOG(dialog), GTK_RESPONSE_NONE);
}

static void
add_clicked_cb(OssoABookTemporaryContactDialog *dialog)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(dialog);

  gtk_widget_hide(GTK_WIDGET(dialog));
  osso_abook_merge_with_dialog(priv->contact, NULL,
                               gtk_window_get_transient_for(GTK_WINDOW(dialog)),
                               merged_cb, dialog);
}

static gboolean
contact_saved_cb(OssoABookContactEditor *editor, const char *uid,
                 OssoABookTemporaryContactDialog *dialog)
{
  gboolean result = FALSE;

  g_signal_emit(dialog, signals[CONTACT_SAVED], 0, uid, &result);

  return result;
}

static void
response_cb(GtkWidget *dialog, int response_id,
            OssoABookTemporaryContactDialog *temporary_contact_dialog)
{
  gtk_dialog_response(GTK_DIALOG(temporary_contact_dialog), GTK_RESPONSE_NONE);
  gtk_widget_destroy(dialog);
}

static void
new_clicked_cb(OssoABookTemporaryContactDialog *dialog)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(dialog);
  GtkWidget *editor;

  gtk_widget_hide(GTK_WIDGET(dialog));
  editor = osso_abook_contact_editor_new_with_contact(
    gtk_window_get_transient_for(GTK_WINDOW(dialog)),
    priv->contact, OSSO_ABOOK_CONTACT_EDITOR_CREATE);
  g_signal_connect(editor, "contact-saved",
                   G_CALLBACK(contact_saved_cb), dialog);
  g_signal_connect(editor, "response",
                   G_CALLBACK(response_cb), dialog);
  gtk_widget_show(editor);
}

static void
create_table(OssoABookTemporaryContactDialog *dialog)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(dialog);
  gboolean sms = FALSE;
  gboolean call = FALSE;
  gboolean chat = FALSE;

  if (priv->table)
    gtk_widget_destroy(priv->table);

  if (priv->is_landscape)
    priv->table = gtk_table_new(1, 2, TRUE);
  else
    priv->table = gtk_table_new(2, 1, TRUE);

  gtk_table_set_col_spacings(GTK_TABLE(priv->table), 8);
  gtk_table_set_row_spacings(GTK_TABLE(priv->table), 8);
  gtk_box_pack_start(
    GTK_BOX(GTK_DIALOG(dialog)->vbox), priv->table, TRUE, TRUE, 0);
  priv->buttons_count = 0;

  if (priv->attribute)
  {
    if (!strcmp(e_vcard_attribute_get_name(priv->attribute), EVC_TEL))
    {
      call = TRUE;
      sms = TRUE;
    }
    else if (priv->account && tp_account_is_enabled(priv->account))
    {
      TpProtocol *protocol =
        osso_abook_account_manager_get_protocol_object(
          NULL, tp_account_get_protocol_name(priv->account));

      if (protocol)
      {
        OssoABookCapsFlags caps = osso_abook_caps_from_tp_protocol(protocol);

        call = !!(caps & OSSO_ABOOK_CAPS_VOICE);
        chat = !!(caps & OSSO_ABOOK_CAPS_CHAT);
      }
    }

    if (call)
    {
      add_button(dialog, _("addr_bd_temporary_call"), "general_call",
                 call_clicked_cb);
    }

    if (chat)
    {
      add_button(dialog, _("addr_bd_temporary_chat"), "general_chat",
                 chat_clicked_cb);
    }

    if (sms && !chat)
    {
      add_button(dialog, _("addr_bd_temporary_sms"), "general_sms",
                 sms_clicked_cb);
    }
  }

  add_button(dialog, _("addr_bd_temporary_add"), NULL, add_clicked_cb);
  add_button(dialog, _("addr_bd_temporary_new"), NULL, new_clicked_cb);
  gtk_widget_show(priv->table);
  gtk_window_resize(GTK_WINDOW(dialog), 1, 1);
}

static void
screen_size_changed_cb(GdkScreen *screen,
                       OssoABookTemporaryContactDialog *dialog)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(dialog);
  gboolean was_landscape = priv->is_landscape;

  priv->is_landscape = osso_abook_screen_is_landscape_mode(screen);

  if ((was_landscape != priv->is_landscape) || !priv->table)
    create_table(dialog);
}

static void
screen_changed_cb(OssoABookTemporaryContactDialog *dialog,
                  GdkScreen *previous_screen, gpointer user_data)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(dialog);

  if (priv->screen)
  {
    g_signal_handlers_disconnect_matched(
      priv->screen, G_SIGNAL_MATCH_DATA|G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      screen_size_changed_cb, dialog);
  }

  priv->screen = gtk_widget_get_screen(GTK_WIDGET(dialog));

  if (priv->screen)
  {
    screen_size_changed_cb(priv->screen, dialog);
    g_signal_connect(priv->screen, "size-changed",
                     G_CALLBACK(screen_size_changed_cb), dialog);
  }
}

static void
osso_abook_temporary_contact_dialog_constructed(GObject *object)
{
  OssoABookTemporaryContactDialog *dialog =
    OSSO_ABOOK_TEMPORARY_CONTACT_DIALOG(object);
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(object);

  if (priv->attribute)
  {
    EVCardAttribute *tel_attribute =
      osso_abook_convert_to_tel_attribute(priv->attribute);

    if (tel_attribute)
    {
      e_vcard_attribute_free(priv->attribute);
      priv->attribute = tel_attribute;
    }
  }

  if (!priv->contact)
  {
    priv->contact = osso_abook_contact_new();
    e_vcard_add_attribute(E_VCARD(priv->contact),
                          e_vcard_attribute_copy(priv->attribute));

    if (g_strcmp0(e_vcard_attribute_get_name(priv->attribute), EVC_TEL) &&
        priv->account)
    {
      OssoABookRoster *roster;

      roster = osso_abook_roster_manager_get_roster(
        NULL, tp_account_get_path_suffix(priv->account));

      if (roster)
      {
        OssoABookContact *roster_contact;
        EVCardAttribute *attr;
        char *uid = osso_abook_create_temporary_uid();

        e_contact_set(E_CONTACT(priv->contact), E_CONTACT_UID, uid);
        g_free(uid);
        roster_contact = osso_abook_contact_new();
        uid = osso_abook_create_temporary_uid();
        e_contact_set(E_CONTACT(roster_contact), E_CONTACT_UID, uid);
        g_free(uid);
        attr = e_vcard_attribute_copy(priv->attribute);
        e_vcard_add_attribute(E_VCARD(roster_contact), attr);
        osso_abook_contact_set_roster(roster_contact, roster);
        osso_abook_contact_attach(priv->contact, roster_contact);
        g_object_unref(roster_contact);
      }
    }
  }

  gtk_window_set_title(GTK_WINDOW(dialog),
                       osso_abook_contact_get_display_name(priv->contact));
  gtk_widget_hide(GTK_DIALOG(dialog)->action_area);
  screen_changed_cb(dialog, NULL, NULL);
  g_signal_connect(dialog, "screen-changed",
                   G_CALLBACK(screen_changed_cb), NULL);
}

static void
osso_abook_temporary_contact_dialog_dispose(GObject *object)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(object);

  if (priv->screen)
  {
    g_signal_handlers_disconnect_matched(
      object, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      screen_changed_cb, NULL);
    g_signal_handlers_disconnect_matched(
      priv->screen, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      screen_size_changed_cb, object);
    priv->screen = NULL;
  }

  G_OBJECT_CLASS(osso_abook_temporary_contact_dialog_parent_class)->dispose(
    object);
}

static void
osso_abook_temporary_contact_dialog_get_property(GObject *object,
                                                 guint property_id,
                                                 GValue *value,
                                                 GParamSpec *pspec)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_TMP_CONTACT:
    {
      g_value_set_object(value, priv->contact);
      break;
    }
    case PROP_ACCOUNT:
    {
      g_value_set_object(value, priv->account);
      break;
    }
    case PROP_ATTRIBUTE:
    {
      g_value_set_boxed(value, priv->attribute);
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
osso_abook_temporary_contact_dialog_set_property(GObject *object,
                                                 guint property_id,
                                                 const GValue *value,
                                                 GParamSpec *pspec)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_TMP_CONTACT:
    {
      OssoABookContact *contact = g_value_get_object(value);

      if (contact)
      {
        EVCardAttribute *attr;

        priv->contact = g_object_ref(contact);
        attr = e_vcard_get_attribute(E_VCARD(contact), EVC_UID);

        if (attr)
        {
          g_critical("Temporary contacts should not have a UID");
          e_vcard_remove_attribute(E_VCARD(contact), attr);
        }
      }

      break;
    }
    case PROP_ACCOUNT:
    {
      if (priv->account)
        g_object_unref(priv->account);

      priv->account = g_value_dup_object(value);
      break;
    }
    case PROP_ATTRIBUTE:
    {
      priv->attribute = g_value_dup_boxed(value);
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
osso_abook_temporary_contact_dialog_finalize(GObject *object)
{
  OssoABookTemporaryContactDialogPrivate *priv = PRIVATE(object);

  if (priv->attribute)
    e_vcard_attribute_free(priv->attribute);

  if (priv->account)
    g_object_unref(priv->account);

  if (priv->contact)
    g_object_unref(priv->contact);

  G_OBJECT_CLASS(osso_abook_temporary_contact_dialog_parent_class)->finalize(
    object);
}

static void
osso_abook_temporary_contact_dialog_class_init(
  OssoABookTemporaryContactDialogClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->constructed = osso_abook_temporary_contact_dialog_constructed;
  object_class->dispose = osso_abook_temporary_contact_dialog_dispose;
  object_class->get_property = osso_abook_temporary_contact_dialog_get_property;
  object_class->set_property = osso_abook_temporary_contact_dialog_set_property;
  object_class->finalize = osso_abook_temporary_contact_dialog_finalize;

  g_object_class_install_property(
    object_class, PROP_ATTRIBUTE,
    g_param_spec_boxed(
      "attribute",
      "Attribute",
      "The temporary attribute",
      E_TYPE_VCARD_ATTRIBUTE,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_TMP_CONTACT,
    g_param_spec_object(
      "tmp-contact",
      "Temporary contact",
      "The temporary contact",
      OSSO_ABOOK_TYPE_CONTACT,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_ACCOUNT,
    g_param_spec_object(
      "account",
      "The optional account",
      "The account of the temporary contact if any",
      TP_TYPE_ACCOUNT,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  signals[CONTACT_SAVED] =
    g_signal_new("contact-saved",
                 OSSO_ABOOK_TYPE_TEMPORARY_CONTACT_DIALOG, G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(OssoABookTemporaryContactDialogClass,
                                 contact_saved),
                 g_signal_accumulator_true_handled, NULL,
                 osso_abook_marshal_BOOLEAN__STRING,
                 G_TYPE_BOOLEAN,
                 1, G_TYPE_STRING);
}
