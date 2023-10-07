/*
 * osso-abook-touch-contact-starter.c
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

#include <libintl.h>

#include "osso-abook-aggregator.h"
#include "osso-abook-avatar-image.h"
#include "osso-abook-button.h"
#include "osso-abook-contact-detail-store.h"
#include "osso-abook-contact-editor.h"
#include "osso-abook-contact-field.h"
#include "osso-abook-errors.h"
#include "osso-abook-icon-sizes.h"
#include "osso-abook-marshal.h"
#include "osso-abook-message-map.h"
#include "osso-abook-presence-label.h"
#include "osso-abook-presence.h"
#include "osso-abook-touch-contact-starter.h"
#include "osso-abook-util.h"
#include "osso-abook-voicemail-contact.h"
#include "osso-abook-voicemail-selector.h"

struct _OssoABookTouchContactStarterPrivate
{
  OssoABookContactDetailStore *details;
  GdkScreen *screen;
  GtkWidget *details_widgets;
  GtkWidget *avatar;
  GtkWidget *vbox;
  GtkWidget *status_label;
  GtkWidget *location_label;
  GtkWidget *align;
  guint cnxn;
  EVCardAttribute *highlighted_attribute;
  OssoABookContactAction allowed_actions;
  OssoABookContactAction started_action;
  EVCardAttribute *single_attribute;
  TpProtocol *single_attribute_profile;
  gboolean full_view : 1;   /* priv->flags & 0xFE 0x01 */
  gboolean flag1 : 1;       /* priv->flags & 0xFD 0x02 */
  gboolean landscape : 1;   /* priv->flags & 0xFB 0x04 */
  gboolean editable : 1;    /* priv->flags & 0xF7 0x08 */
  gboolean started : 1;     /* priv->flags & 0xEF 0x10 */
  gboolean inited : 1;      /* priv->flags & 0xDF 0x20 */
  gboolean left_part : 1;   /* priv->flags & 0xBF 0x40 */
  gboolean interactive : 1; /* priv->flags & 0x7F 0x80 */
};

typedef struct _OssoABookTouchContactStarterPrivate
  OssoABookTouchContactStarterPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookTouchContactStarter,
  osso_abook_touch_contact_starter,
  GTK_TYPE_BIN
);

#define OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter) \
  ((OssoABookTouchContactStarterPrivate *) \
   osso_abook_touch_contact_starter_get_instance_private(starter))

enum
{
  PROP_CONTACT = 1,
  PROP_EDITABLE,
  PROP_LEFT_PART,
  PROP_STORE,
  PROP_ALLOWED_ACTIONS,
  PROP_INTERACTIVE,
  PROP_SINGLE_ATTRIBUTE,
  PROP_SINGLE_ATTRIBUTE_PROFILE,
  PROP_HIGHLIGHTED_ATTRIBUTE
};

enum
{
  PRE_ACTION_START,
  ACTION_STARTED,
  EDITOR_STARTED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};

struct contacts_added_data
{
  OssoABookTouchContactStarter *starter;
  OssoABookRoster *aggregator;
  gchar *uid;
};

struct start_merge_data
{
  OssoABookTouchContactStarter *starter;
  gchar *uid;
  OssoABookMergeWithCb cb;
  gpointer user_data;
};

static OssoABookMessageMapping message_map[] =
{
  { NULL, "addr_bd_cont_starter" },
  { "phone", "addr_bd_cont_starter_phone" },
  { "phone_home", "addr_bd_cont_starter_phone" },
  { "phone_work", "addr_bd_cont_starter_phone" },
  { "phone_other", "addr_bd_cont_starter_phone" },
  { "phone_fax", "addr_bd_cont_starter_phone" },
  { "mobile", "addr_bd_cont_starter_mobile" },
  { "mobile_home", "addr_bd_cont_starter_mobile" },
  { "mobile_work", "addr_bd_cont_starter_mobile" },
  { "sms", "addr_bd_cont_starter_sms" },
  { "email", "addr_bd_cont_starter_email" },
  { "email_home", "addr_bd_cont_starter_email" },
  { "email_work", "addr_bd_cont_starter_email" },
  { "webpage", "addr_bd_cont_starter_webpage" },
  { "birthday", "addr_bd_cont_starter_birthday" },
  { "address", "addr_bd_cont_starter_address" },
  { "address_home", "addr_bd_cont_starter_address" },
  { "address_work", "addr_bd_cont_starter_address" },
  { "title", "addr_fi_cont_starter_title" },
  { "company", "addr_fi_cont_starter_company" },
  { "note", "addr_fi_cont_starter_note" },
  { "nickname", "addr_fi_cont_starter_nickname" },
  { "gender", "addr_fi_cont_starter_gender" },
  { "address_detail", NULL },
  { "address_home_detail", "addr_bd_cont_starter_address_home" },
  { "address_work_detail", "addr_bd_cont_starter_address_work" },
  { "email_detail", NULL },
  { "email_home_detail", "addr_bd_cont_starter_email_home" },
  { "email_work_detail", "addr_bd_cont_starter_email_work" },
  { "mobile_detail", NULL },
  { "mobile_home_detail", "addr_bd_cont_starter_mobile_home" },
  { "mobile_work_detail", "addr_bd_cont_starter_mobile_work" },
  { "phone_detail", NULL },
  { "phone_home_detail", "addr_bd_cont_starter_phone_home" },
  { "phone_work_detail", "addr_bd_cont_starter_phone_work" },
  { "phone_other_detail", "addr_bd_cont_starter_phone_other" },
  { "phone_fax_detail", "addr_bd_cont_starter_phone_fax" },
  { "sms_detail", NULL },
  { "sms_home_detail", "addr_bd_cont_starter_sms_home" },
  { "sms_work_detail", "addr_bd_cont_starter_sms_work" },
  { NULL, NULL }
};

/* This is an 'encrypted' markup containing the names of
 * 'Fremantle Contacts Team', see fcp_expose_event_cb how it is 'decrypted'
 *
 * TODO  - either remove altogether or add Leste team members as well
 */

static unsigned char fcp_markup[] =
{
  0x4C, 0x10, 0x00, 0x03, 0x5B, 0x4C, 0x10, 0x57, 0x30, 0x0D,
  0x15, 0x52, 0x2A, 0x0B, 0x0B, 0x04, 0x13, 0x0A, 0x10, 0x16,
  0x50, 0x26, 0x0C, 0x05, 0x08, 0x4C, 0x5D, 0x0B, 0x5A, 0x59,
  0x5F, 0x10, 0x00, 0x03, 0x5B, 0x7A, 0x78, 0x23, 0xA7, 0xD3,
  0x02, 0x15, 0x0C, 0x0A, 0x45, 0x23, 0x11, 0x01, 0x01, 0x0C,
  0x12, 0x17, 0x07, 0x03, 0x17, 0x05, 0x10, 0x0C, 0x16, 0x6F,
  0x4C, 0x01, 0x04, 0x05, 0x09, 0x1C, 0x4C, 0x2A, 0x0B, 0x0B,
  0x04, 0x13, 0x0A, 0x10, 0x16, 0x50, 0x22, 0x1B, 0x0B, 0x0F,
  0x15, 0x11, 0x1D, 0x44, 0x29, 0x15, 0x13, 0x0D, 0x6E, 0x6F,
  0x4C, 0x5D, 0x1A, 0x09, 0x04, 0x1C, 0x1E, 0x57, 0x30, 0x0C,
  0x19, 0x1C, 0x08, 0x44, 0x29, 0x15, 0x1A, 0x1D, 0x0B, 0x0B,
  0x15, 0x1C, 0x63, 0x58, 0x16, 0x1D, 0x13, 0x05, 0x08, 0x5B,
  0x33, 0x1D, 0x07, 0x10, 0x04, 0x13, 0x06, 0x1A, 0x44, 0x2C,
  0x1E, 0x06, 0x0C, 0x16, 0x04, 0x13, 0x06, 0x00, 0x0B, 0x0B,
  0x50, 0x36, 0x0C, 0x17, 0x0C, 0x17, 0x1C, 0x0C, 0x16, 0x6F,
  0x7A, 0x4E, 0x46, 0x17, 0x08, 0x11, 0x1E, 0x05, 0x5A, 0x33,
  0x19, 0x1E, 0x03, 0x05, 0x45, 0x38, 0x17, 0x05, 0x0F, 0x0C,
  0x1F, 0x78, 0x55, 0x17, 0x08, 0x11, 0x1E, 0x05, 0x5A, 0x29,
  0x11, 0x0B, 0x06, 0x11, 0x11, 0x03, 0x5D, 0x2E, 0x16, 0x04,
  0x00, 0x1A, 0x00, 0x07, 0x16, 0x50, 0x36, 0x0C, 0x17, 0x0C,
  0x17, 0x1C, 0x0C, 0x16, 0x6F, 0x7A, 0x4E, 0x46, 0x17, 0x08,
  0x11, 0x1E, 0x05, 0x5A, 0x2F, 0x1F, 0x1C, 0x08, 0x10, 0x0D,
  0x1F, 0x1C, 0x49, 0x2E, 0x0A, 0x1E, 0x15, 0x1A, 0x09, 0x04,
  0x7A, 0x4E, 0x1A, 0x09, 0x04, 0x1C, 0x1E, 0x57, 0x20, 0x00,
  0x06, 0x17, 0x05, 0x0B, 0x15, 0x15, 0x00, 0x63, 0x6E, 0x59,
  0x5F, 0x01, 0x04, 0x05, 0x09, 0x1C, 0x4C, 0x23, 0x0B, 0x0B,
  0x19, 0x52, 0x3F, 0x05, 0x09, 0x04, 0x13, 0x07, 0x01, 0x0B,
  0x7A, 0x4E, 0x1A, 0x09, 0x04, 0x1C, 0x1E, 0x57, 0x20, 0x00,
  0x06, 0x17, 0x05, 0x0B, 0x15, 0x15, 0x00, 0x63, 0x6E, 0x59,
  0x5F, 0x01, 0x04, 0x05, 0x09, 0x1C, 0x4C, 0x24, 0x05, 0x17,
  0x13, 0x1D, 0x49, 0x26, 0x04, 0x02, 0x1B, 0x1A, 0x0D, 0x0A,
  0x1E, 0x17, 0x63, 0x58, 0x16, 0x1D, 0x13, 0x05, 0x08, 0x5B,
  0x34, 0x17, 0x1F, 0x01, 0x09, 0x1F, 0x02, 0x0C, 0x16, 0x6F,
  0x7A, 0x4E, 0x46, 0x17, 0x08, 0x11, 0x1E, 0x05, 0x5A, 0x28,
  0x11, 0x06, 0x01, 0x0D, 0x04, 0x03, 0x52, 0x21, 0x05, 0x16,
  0x03, 0x17, 0x05, 0x09, 0x04, 0x1E, 0x1C, 0x63, 0x58, 0x16,
  0x1D, 0x13, 0x05, 0x08, 0x5B, 0x34, 0x17, 0x1F, 0x01, 0x09,
  0x1F, 0x02, 0x0C, 0x16, 0x6F, 0x7A, 0x4E, 0x46, 0x17, 0x08,
  0x11, 0x1E, 0x05, 0x5A, 0x28, 0x05, 0x19, 0x1C, 0x0A, 0x01,
  0x50, 0x21, 0x00, 0x12, 0x04, 0x02, 0x13, 0x04, 0x05, 0x0B,
  0x7A, 0x4E, 0x1A, 0x09, 0x04, 0x1C, 0x1E, 0x57, 0x20, 0x00,
  0x06, 0x17, 0x05, 0x0B, 0x15, 0x15, 0x00, 0x63, 0x6E, 0x59,
  0x5F, 0x01, 0x04, 0x05, 0x09, 0x1C, 0x4C, 0x3B, 0x0B, 0x07,
  0x15, 0x00, 0x1D, 0x44, 0x27, 0x02, 0x13, 0x0D, 0x02, 0x0A,
  0x02, 0x16, 0x63, 0x58, 0x16, 0x1D, 0x13, 0x05, 0x08, 0x5B,
  0x34, 0x17, 0x1F, 0x01, 0x09, 0x1F, 0x02, 0x0C, 0x16, 0x6F,
  0x7A, 0x4E, 0x46, 0x17, 0x08, 0x11, 0x1E, 0x05, 0x5A, 0x37,
  0x1F, 0x10, 0x0C, 0x16, 0x11, 0x50, 0x22, 0x0C, 0x10, 0x0A,
  0x7A, 0x4E, 0x1A, 0x09, 0x04, 0x1C, 0x1E, 0x57, 0x20, 0x00,
  0x06, 0x17, 0x05, 0x0B, 0x15, 0x15, 0x00, 0x63, 0x6E, 0x59,
  0x5F, 0x01, 0x04, 0x05, 0x09, 0x1C, 0x4C, 0x3B, 0x0B, 0x16,
  0x03, 0x52, 0x2B, 0x11, 0x17, 0x04, 0x1D, 0x07, 0x6E, 0x59,
  0x03, 0x1F, 0x08, 0x08, 0x09, 0x4E, 0x36, 0x0C, 0x12, 0x00,
  0x1C, 0x1D, 0x19, 0x01, 0x17, 0x7A, 0x78, 0x55, 0x4B, 0x16,
  0x1D, 0x13, 0x05, 0x08, 0x5B, 0x24, 0x00, 0x08, 0x12, 0x0C,
  0x03, 0x52, 0x3B, 0x01, 0x0C, 0x04, 0x06, 0x0C, 0x16, 0x6F,
  0x4C, 0x01, 0x04, 0x05, 0x09, 0x1C, 0x4C, 0x2D, 0x01, 0x13,
  0x15, 0x1E, 0x06, 0x14, 0x00, 0x02, 0x78, 0x63, 0x58, 0x4A,
  0x03, 0x1F, 0x08, 0x08, 0x09, 0x4E, 0x2A, 0x08, 0x12, 0x0C,
  0x15, 0x00, 0x49, 0x27, 0x09, 0x11, 0x17, 0x1A, 0x17, 0x00,
  0x1E, 0x01, 0x63, 0x58, 0x16, 0x1D, 0x13, 0x05, 0x08, 0x5B,
  0x34, 0x17, 0x1F, 0x01, 0x09, 0x1F, 0x02, 0x0C, 0x16, 0x6F,
  0x7A, 0x4E, 0x46, 0x17, 0x08, 0x11, 0x1E, 0x05, 0x5A, 0x24,
  0x02, 0x1B, 0x0F, 0x05, 0x45, 0x31, 0x1A, 0x08, 0x09, 0x00,
  0x14, 0x78, 0x55, 0x17, 0x08, 0x11, 0x1E, 0x05, 0x5A, 0x31,
  0x15, 0x01, 0x1D, 0x44, 0x20, 0x1E, 0x15, 0x00, 0x0A, 0x00,
  0x15, 0x00, 0x49, 0x6E, 0x6F, 0x4C, 0x5D, 0x1A, 0x09, 0x04,
  0x1C, 0x1E, 0x57, 0x2F, 0x10, 0x03, 0x07, 0x04, 0x05, 0x45,
  0x3B, 0x07, 0x04, 0x05, 0x17, 0x19, 0x78, 0x55, 0x17, 0x08,
  0x11, 0x1E, 0x05, 0x5A, 0x31, 0x15, 0x01, 0x1D, 0x44, 0x20,
  0x1E, 0x15, 0x00, 0x0A, 0x00, 0x15, 0x00, 0x63, 0x6E, 0x59,
  0x5F, 0x01, 0x04, 0x05, 0x09, 0x1C, 0x4C, 0x3A, 0x05, 0x0B,
  0x04, 0x1A, 0x08, 0x0A, 0x04, 0x1D, 0x52, 0x2D, 0x01, 0x00,
  0x00, 0x13, 0x63, 0x58, 0x16, 0x1D, 0x13, 0x05, 0x08, 0x5B,
  0x24, 0x17, 0x1A, 0x10, 0x45, 0x35, 0x1C, 0x0E, 0x0D, 0x0B,
  0x15, 0x17, 0x1B, 0x6E, 0x6F, 0x4C, 0x5D, 0x1A, 0x09, 0x04,
  0x1C, 0x1E, 0x57, 0x64, 0x65
};

static void
update_details_widgets(OssoABookTouchContactStarter *starter);

static OssoABookContact *
get_details_contact(OssoABookTouchContactStarterPrivate *priv)
{
  return osso_abook_contact_detail_store_get_contact(priv->details);
}

static void
set_details_contact_name_as_window_title(OssoABookTouchContactStarter *starter)
{
  GtkWidget *window =
    gtk_widget_get_ancestor(GTK_WIDGET(starter), GTK_TYPE_WINDOW);

  if (window)
  {
    OssoABookContact *contact =
      get_details_contact(OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter));

    gtk_window_set_title(GTK_WINDOW(window),
                         osso_abook_contact_get_display_name(contact));
  }
}

static void
contact_presence_changed_cb(OssoABookTouchContactStarter *starter)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);
  OssoABookContact *contact;
  OssoABookPresence *presence;
  TpConnectionPresenceType presence_type;

  if (!priv->inited)
    return;

  contact = get_details_contact(priv);
  presence = OSSO_ABOOK_PRESENCE(contact);
  presence_type = osso_abook_presence_get_presence_type(presence);

  if ((presence_type == TP_CONNECTION_PRESENCE_TYPE_UNKNOWN) ||
      (presence_type == TP_CONNECTION_PRESENCE_TYPE_UNSET) ||
      (presence_type == TP_CONNECTION_PRESENCE_TYPE_ERROR))
  {
    gtk_widget_hide(priv->vbox);
  }
  else
  {
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS
    const gchar *location = osso_abook_presence_get_location_string(presence);
    G_GNUC_END_IGNORE_DEPRECATIONS
    const gchar *status =
      osso_abook_presence_get_presence_status_message(presence);

    if (!status || !*status)
      status = osso_abook_presence_get_display_status(presence);

    if (status && *status)
      gtk_widget_show(priv->status_label);
    else
    {
      gtk_widget_hide(priv->status_label);
      status = "";
    }

    gtk_label_set_text(GTK_LABEL(priv->status_label), status);

    if (location && *location)
      gtk_widget_show_all(priv->align);
    else
    {
      gtk_widget_hide(priv->align);
      location = "";
    }

    gtk_label_set_text(GTK_LABEL(priv->location_label), location);
    gtk_widget_show(priv->vbox);
  }
}

static void
clear_details_contact(gpointer starter, OssoABookContact *contact)
{
  if (!contact)
    return;

  g_signal_handlers_disconnect_matched(
    contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
    set_details_contact_name_as_window_title, starter);
  g_signal_handlers_disconnect_matched(
    contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
    contact_presence_changed_cb, starter);
}

static void
details_contact_changed_cb(OssoABookTouchContactStarter *starter,
                           OssoABookContact *old_contact,
                           OssoABookContact *new_contact)
{
  OssoABookTouchContactStarterPrivate *priv;

  if (old_contact == new_contact)
    return;

  if (old_contact)
    clear_details_contact(starter, old_contact);

  if (new_contact)
  {
    g_signal_connect_swapped(
      new_contact, "notify::display-name",
      G_CALLBACK(set_details_contact_name_as_window_title), starter);
    g_signal_connect_swapped(
      new_contact, "notify::presence-status-message",
      G_CALLBACK(contact_presence_changed_cb), starter);
    g_signal_connect_swapped(
      new_contact, "notify::presence-status",
      G_CALLBACK(contact_presence_changed_cb), starter);
  }

  set_details_contact_name_as_window_title(starter);

  priv = OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  if (priv->avatar)
  {
    osso_abook_avatar_image_set_avatar(
      OSSO_ABOOK_AVATAR_IMAGE(gtk_bin_get_child(GTK_BIN(priv->avatar))),
      OSSO_ABOOK_AVATAR(get_details_contact(priv)));
  }

  contact_presence_changed_cb(starter);
}

static void
toggle_full_starter_view(OssoABookTouchContactStarter *starter)
{
  OssoABookTouchContactStarterPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_TOUCH_CONTACT_STARTER(starter));

  priv = OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  priv->full_view = !priv->full_view;

  update_details_widgets(starter);
}

static void
update_avatar(OssoABookTouchContactStarter *starter)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  gboolean editable = priv->editable &&
    !osso_abook_contact_is_sim_contact(get_details_contact(priv));

  if (priv->single_attribute &&
      ((priv->full_view && priv->flag1) || !priv->flag1))
  {
    priv->avatar = osso_abook_avatar_button_new(
      OSSO_ABOOK_AVATAR(get_details_contact(priv)),
      OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT);
    g_signal_connect_swapped(priv->avatar, "clicked",
                             G_CALLBACK(toggle_full_starter_view), starter);
  }
  else if (editable)
  {
    priv->avatar = osso_abook_avatar_button_new(
      OSSO_ABOOK_AVATAR(get_details_contact(priv)),
      OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT);
    g_signal_connect_swapped(
      priv->avatar, "clicked",
      G_CALLBACK(osso_abook_touch_contact_starter_start_editor), starter);
  }
  else
  {
    priv->avatar = osso_abook_avatar_image_new_with_avatar(
      OSSO_ABOOK_AVATAR(get_details_contact(priv)),
      OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT);
  }

  gtk_widget_show(priv->avatar);
}

static void
add_avater_to_widget(OssoABookTouchContactStarter *starter, GtkWidget *widget)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  gtk_box_pack_start(GTK_BOX(widget), priv->avatar, FALSE, FALSE, 0);
  gtk_box_reorder_child(GTK_BOX(widget), priv->avatar, 0);
}

struct fct_data
{
  GtkWidget *details_widgets;
  PangoLayout *layout;
  int layout_height;
  int ty;
  guint animate_id;
};

static gboolean
fct_animate(gpointer user_data)
{
  struct fct_data *data = user_data;

  if (data->ty >= data->layout_height)
    data->ty = -data->details_widgets->allocation.height;
  else
    data->ty += 5;

  gtk_widget_queue_draw(data->details_widgets);

  return TRUE;
}

static gboolean
fcp_expose_event_cb(GtkWidget *widget, GdkEvent *event,
                    struct fct_data *data)
{
  GtkStyle *style = widget->style;
  cairo_t *cr;
  cairo_pattern_t *pattern;
  double blue = (double)style->fg[GTK_STATE_NORMAL].blue / 65535.0;
  double red = (double)style->fg[GTK_STATE_NORMAL].red / 65535.0;
  double green = (double)style->fg[GTK_STATE_NORMAL].green / 65535.0;

  if (!data->layout)
  {
    char markup[G_N_ELEMENTS(fcp_markup)];
    unsigned char key[] = "pride";
    int i;

    memcpy(markup, fcp_markup, 765);

    markup[0] ^= 0x70u;

    for (i = 1; i < G_N_ELEMENTS(fcp_markup); i++)
      markup[i] ^= key[i % 5];

    data->layout =
      gtk_widget_create_pango_layout(data->details_widgets, 0);
    pango_layout_set_markup(data->layout, markup, -1);
    pango_layout_get_pixel_size(data->layout, NULL,
                                &data->layout_height);
    data->ty = data->layout_height;
  }

  cr = gdk_cairo_create(event->any.window);
  gdk_cairo_region(cr, event->expose.region);
  cairo_clip(cr);
  pattern = cairo_pattern_create_linear(0.0, 0.0, 0.0,
                                        widget->allocation.height);
  cairo_pattern_add_color_stop_rgba(pattern, 0.0, red, green, blue, 0.0);
  cairo_pattern_add_color_stop_rgba(pattern, 0.1, red, green, blue, 1.0);
  cairo_pattern_add_color_stop_rgba(pattern, 0.9, red, green, blue, 1.0);
  cairo_pattern_add_color_stop_rgba(pattern, 1.0, red, green, blue, 0.0);
  cairo_set_source(cr, pattern);
  cairo_translate(cr, 0.0, -data->ty);
  pango_cairo_show_layout(cr, data->layout);
  cairo_pattern_destroy(pattern);
  cairo_destroy(cr);

  return TRUE;
}

static void
fct_data_destroy(gpointer user_data, GClosure *closure)
{
  struct fct_data *data = user_data;

  g_source_remove(data->animate_id);

  if (data->layout)
    g_object_unref(data->layout);

  g_free(data);
}

static void
fcp_size_allocate_cb(GtkWidget *widget, GdkRectangle *allocation,
                     gpointer user_data)
{
  gtk_widget_set_size_request(widget, allocation->width, allocation->height);
}

static void
fcp_parent_set_cb(GtkWidget *widget, GtkWidget *old_parent, gpointer user_data)
{
  GtkWidget *parent = gtk_widget_get_parent(widget);

  if (old_parent)
  {
    g_signal_handlers_disconnect_matched(
      old_parent, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      fcp_size_allocate_cb, widget);
  }

  if (parent)
  {
    g_signal_connect_swapped(parent, "size-allocate",
                             G_CALLBACK(fcp_size_allocate_cb), widget);
  }
}

static gboolean
create_fcp_details_widgets(OssoABookTouchContactStarter *starter)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);
  OssoABookContact *contact = get_details_contact(priv);

  if (OSSO_ABOOK_IS_CONTACT(contact) &&
      !osso_abook_contact_is_roster_contact(contact) &&
      !osso_abook_contact_has_roster_contacts(contact))
  {
    if (!g_strcmp0("Fremantle Contacts Team",
                   e_contact_get_const(E_CONTACT(contact), E_CONTACT_NICKNAME)))
    {
      struct fct_data *data = g_new0(struct fct_data, 1);

      priv->details_widgets = gtk_drawing_area_new();
      data->details_widgets = priv->details_widgets;
      data->animate_id = g_timeout_add(50, fct_animate, data);

      g_signal_connect_data(priv->details_widgets, "expose-event",
                            G_CALLBACK(fcp_expose_event_cb), data,
                            fct_data_destroy, 0);
      g_signal_connect(priv->details_widgets, "parent-set",
                       G_CALLBACK(fcp_parent_set_cb), NULL);
      gtk_widget_show(priv->details_widgets);
      return TRUE;
    }
  }

  return FALSE;
}

static gboolean
evcard_attribute_name_value_equal(EVCardAttribute *a1, EVCardAttribute *a2)
{
  const char *name1 = e_vcard_attribute_get_name(a1);
  const char *name2 = e_vcard_attribute_get_name(a2);
  gboolean rv;

  char *val1 = e_vcard_attribute_get_value(a1);
  char *val2 = e_vcard_attribute_get_value(a2);

  if (g_strcmp0(name1, name2) || g_strcmp0(val1, val2))
    rv = FALSE;
  else
    rv = TRUE;

  g_free(val1);
  g_free(val2);

  return rv;
}

struct contact_action_start_cb_data
{
  OssoABookTouchContactStarter *starter;
  OssoABookContactAction action;
};

static void
contact_action_start_cb(const GError *error, GtkWindow *parent,
                        gpointer user_data)
{
  struct contact_action_start_cb_data *data = user_data;
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(data->starter);

  if (error)
  {
    if ((error->domain != OSSO_ABOOK_ERROR) ||
        (error->code != OSSO_ABOOK_ERROR_CANCELLED))
    {
      osso_abook_handle_gerror(parent, g_error_copy(error));
    }
  }
  else
  {
    priv->started_action = data->action;
    g_signal_emit(data->starter, signals[ACTION_STARTED], 0);
    priv->started_action = OSSO_ABOOK_CONTACT_ACTION_NONE;
  }

  g_object_unref(data->starter);
  g_slice_free(struct contact_action_start_cb_data, data);
}

static void
action_widget_clicked_cb(GtkWidget *widget, gpointer user_data)
{
  OssoABookTouchContactStarter *starter =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER(user_data);
  OssoABookContactFieldAction *action =
    g_object_get_data(G_OBJECT(widget), "action");
  gboolean res = FALSE;

  g_signal_emit(starter, signals[PRE_ACTION_START], 0, action, &res);

  if (!res)
  {
    struct contact_action_start_cb_data *data =
      g_slice_new0(struct contact_action_start_cb_data);

    data->action = osso_abook_contact_field_action_get_action(action);
    data->starter = g_object_ref(starter);
    osso_abook_contact_field_action_start_with_callback(
      action, GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(starter))),
      contact_action_start_cb, data);
  }
}

static void
add_row(GList **rows_list, GList **columns_list, guint *rows, guint *columns)
{
  guint len;

  if (*columns_list)
  {
    if (*columns <= g_list_length(*columns_list))
      len = g_list_length(*columns_list);
    else
      len = *columns;

    *columns = len;
    ++*rows;
    *rows_list = g_list_append(*rows_list, *columns_list);
    *columns_list = NULL;
  }
}

static GList *
create_layout(OssoABookTouchContactStarter *starter, guint max_columns_per_row,
              guint *columns, guint *rows)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);
  GList *cols_list_2 = NULL;
  GList *rows_list_2 = NULL;
  GList *cols_list_1 = NULL;
  GList *rows_list_1 = NULL;
  gboolean video_button = osso_abook_settings_get_video_button();
  gboolean sms_button = osso_abook_settings_get_sms_button();
  GSequence *fields_sequence =
    osso_abook_contact_detail_store_get_fields(priv->details);

  ;
  int max_sort_weight = -1;
  int weight;
  int max_w;
  gboolean protocol_matches;
  GSequenceIter *iter;

  GList *actions;
  gboolean single_attribute_differs;
  gboolean weight_differs;
  GList **rows_list;
  GList **columns_list;

  *rows = 0;
  *columns = 0;

  if (!fields_sequence)
    return NULL;

  iter = g_sequence_get_begin_iter(fields_sequence);

  while (!g_sequence_iter_is_end(iter))
  {
    OssoABookContactField *fld1 = g_sequence_get(iter);
    int w;

    iter = g_sequence_iter_next(iter);
    w = osso_abook_contact_field_get_sort_weight(fld1);
    weight = w / 100;
    max_w = max_sort_weight / 100;
    weight_differs = w / 100 != max_w;

    if (!g_sequence_iter_is_end(iter))
    {
      if (weight ==
          osso_abook_contact_field_get_sort_weight(g_sequence_get(iter)) / 100)
      {
        weight_differs = FALSE;
      }
    }

    if (rows_list_1)
    {
      if (weight != max_w)
      {
        if (rows_list_2)
        {
          rows_list_1 = g_list_append(rows_list_1, NULL);
          rows_list_1 = g_list_concat(rows_list_1, rows_list_2);
          rows_list_2 = NULL;
        }

        rows_list_1 = g_list_append(rows_list_1, NULL);
      }
    }

    single_attribute_differs = FALSE;

    if (priv->full_view)
    {
      EVCardAttribute *attr = osso_abook_contact_field_get_attribute(fld1);

      if (attr)
      {
        single_attribute_differs = !evcard_attribute_name_value_equal(
          priv->single_attribute, attr);
      }
    }

    for (actions = osso_abook_contact_field_get_actions_full(fld1,
                                                             priv->interactive);
         actions; actions = g_list_delete_link(actions, actions))
    {
      OssoABookContactFieldAction *field_action = actions->data;
      OssoABookContactAction action =
        osso_abook_contact_field_action_get_action(field_action);
      OssoABookContactFieldActionLayoutFlags layout_flags =
        osso_abook_contact_field_action_get_layout_flags(field_action);

      if (layout_flags & OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_EXTRA)
      {
        columns_list = &cols_list_2;
        rows_list = &rows_list_2;
      }
      else
      {
        columns_list = &cols_list_1;
        rows_list = &rows_list_1;
      }

      if ((layout_flags & OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY) ||
          (weight_differs &&
           layout_flags & OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_EXPANDABLE))
      {
        add_row(rows_list, columns_list, rows, columns);
      }

      if ((1 << action) & priv->allowed_actions)
      {
        if (action == OSSO_ABOOK_CONTACT_ACTION_VOIPTO_VIDEO)
        {
          if (!video_button)
            goto next;
        }
        else if ((unsigned int)(action - 2) <= 1)
        {
          if (sms_button)
          {
            EVCardAttribute *attr =
              osso_abook_contact_field_get_attribute(fld1);
            const char *attr_name = e_vcard_attribute_get_name(attr);

            if (!g_strcmp0(attr_name, EVC_TEL) &&
                !(osso_abook_contact_field_get_flags(fld1) &
                  OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL))
            {
              goto next;
            }
          }
        }

        protocol_matches =
          !priv->single_attribute_profile ||
          (priv->single_attribute_profile ==
           osso_abook_contact_field_action_get_protocol(field_action));

        if (!priv->full_view || (!single_attribute_differs && protocol_matches))
        {
          *columns_list = g_list_append(*columns_list, field_action);

          if (g_list_length(*columns_list) >= max_columns_per_row)
            add_row(rows_list, columns_list, rows, columns);

          continue;
        }

        priv->flag1 = TRUE;
      }

next:
      osso_abook_contact_field_action_unref(field_action);
    }

    add_row(&rows_list_1, &cols_list_1, rows, columns);
    add_row(&rows_list_2, &cols_list_2, rows, columns);
    max_sort_weight = w;
  }

  if (rows_list_2)
  {
    rows_list_1 = g_list_append(rows_list_1, 0);
    rows_list_1 = g_list_concat(rows_list_1, rows_list_2);
  }

  return rows_list_1;
}

static void
create_details_widgets(OssoABookTouchContactStarter *starter)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);
  guint max_cols;
  GtkWidget *action_widget;
  GList *rows_list;
  guint columns = 0;
  guint rows = 0;

  if (create_fcp_details_widgets(starter))
    return;

  if (priv->landscape)
    max_cols = 2;
  else
    max_cols = 1;

  if (priv->full_view)
    priv->flag1 = FALSE;

  rows_list = create_layout(starter, max_cols, &columns, &rows);

  if (!rows_list)
  {
    if (priv->full_view)
    {
      if (priv->single_attribute_profile)
      {
        g_object_unref(priv->single_attribute_profile);
        priv->single_attribute_profile = NULL;

        g_object_notify(G_OBJECT(starter), "single-attribute-profile");
        priv->flag1 = FALSE;

        rows_list = create_layout(starter, max_cols, &columns, &rows);
      }

      if (!rows_list && priv->single_attribute)
      {
        e_vcard_attribute_free(priv->single_attribute);
        priv->single_attribute = NULL;
        g_object_notify(G_OBJECT(starter), "single-attribute");
        priv->full_view = FALSE;
        priv->flag1 = FALSE;

        rows_list = create_layout(starter, max_cols, &columns, &rows);
      }
    }
  }

  if (rows_list)
  {
    GtkTable *table;
    guint left_attach;
    guint right_attach;
    guint top_attach = 0;
    guint bottom_attach;

    priv->details_widgets = gtk_table_new(rows, columns, 0);

    table = GTK_TABLE(priv->details_widgets);
    gtk_table_set_col_spacings(table, 8);
    gtk_table_set_row_spacings(table, 8);
    gtk_widget_show(priv->details_widgets);

    while (rows_list)
    {
      if (rows_list->data)
      {
        GList *l = rows_list->data;
        bottom_attach = top_attach + 1;
        left_attach = 1;

        for (l = rows_list->data; l; l = g_list_delete_link(l, l))
        {
          OssoABookContactFieldAction *action = l->data;

          if (l->next)
            right_attach = left_attach;
          else
            right_attach = columns;

          action_widget = osso_abook_contact_field_action_get_widget(action);

          if (GTK_IS_BUTTON(action_widget))
          {
            g_object_set_data_full(
              G_OBJECT(action_widget),
              "action",
              osso_abook_contact_field_action_ref(action),
              (GDestroyNotify)osso_abook_contact_field_action_unref);
            g_signal_connect_after(action_widget, "clicked",
                                   G_CALLBACK(action_widget_clicked_cb),
                                   starter);
          }

          if (priv->highlighted_attribute)
          {
            if (OSSO_ABOOK_IS_BUTTON(action_widget))
            {
              EVCardAttribute *attr = osso_abook_contact_field_get_attribute(
                osso_abook_contact_field_action_get_field(action));

              if (evcard_attribute_name_value_equal(
                    priv->highlighted_attribute, attr))
              {
                g_object_set(action_widget, "highlighted", TRUE, NULL);
              }
            }
          }

          gtk_table_attach(table, action_widget, left_attach - 1,
                           right_attach, top_attach, bottom_attach,
                           GTK_FILL | GTK_EXPAND, 0, 0, 0);
          gtk_widget_show(action_widget);
          osso_abook_contact_field_action_unref(action);
          left_attach++;
        }
      }
      else
      {
        if (rows_list->next)
        {
          if (rows > 7)
            gtk_table_set_row_spacing(table, top_attach - 1, 35u);

          rows_list = g_list_delete_link(rows_list, rows_list);

          continue;
        }

        bottom_attach = top_attach + 1;
      }

      rows_list = g_list_delete_link(rows_list, rows_list);
      top_attach = bottom_attach;
    }
  }
  else
  {
    priv->details_widgets = gtk_label_new(g_dgettext("osso-addressbook",
                                                     "addr_ia_no_details"));
    gtk_misc_set_alignment(GTK_MISC(priv->details_widgets), 0.5, 0.5);
    hildon_helper_set_logical_font(priv->details_widgets, "LargeSystemFont");
    hildon_helper_set_logical_color(priv->details_widgets, GTK_RC_FG,
                                    GTK_STATE_NORMAL, "SecondaryTextColor");
    gtk_widget_show(priv->details_widgets);
  }
}

static void
update_details_widgets(OssoABookTouchContactStarter *starter)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);
  GtkWidget *parent;
  gboolean is_label;
  GtkWidget *avatar_parent;
  GtkWidget *old_avatar;

  if (!priv->details_widgets)
    return;

  parent = gtk_widget_get_parent(priv->details_widgets);

  g_warn_if_fail(NULL != parent);

  is_label = GTK_IS_LABEL(priv->details_widgets);

  gtk_widget_destroy(priv->details_widgets);
  create_details_widgets(starter);
  gtk_container_add(GTK_CONTAINER(parent), priv->details_widgets);

  if (is_label)
  {
    GtkWidget *area = gtk_widget_get_ancestor(parent,
                                              HILDON_TYPE_PANNABLE_AREA);

    hildon_pannable_area_jump_to(HILDON_PANNABLE_AREA(area), 0, 0);
  }

  avatar_parent = gtk_widget_get_parent(priv->avatar);
  old_avatar = priv->avatar;
  update_avatar(starter);
  add_avater_to_widget(starter, avatar_parent);
  gtk_widget_destroy(old_avatar);
}

static void
real_set_store(OssoABookTouchContactStarter *starter,
               OssoABookContactDetailStore *store)
{
  OssoABookTouchContactStarterPrivate *priv;

  if (!store)
    return;

  priv = OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  g_return_if_fail(priv->details == NULL);

  priv->details = g_object_ref(store);
  osso_abook_contact_detail_store_set_message_map(priv->details, message_map);
  g_signal_connect_swapped(priv->details, "changed",
                           G_CALLBACK(update_details_widgets), starter);
  g_signal_connect_swapped(priv->details, "contact-changed",
                           G_CALLBACK(details_contact_changed_cb), starter);

  details_contact_changed_cb(starter, NULL, get_details_contact(priv));
  update_details_widgets(starter);
}

static void
real_set_contact(OssoABookTouchContactStarter *starter,
                 OssoABookContact *contact)
{
  OssoABookContactDetailStore *store;
  OssoABookTouchContactStarterPrivate *priv;

  if (!contact)
    return;

  priv = OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  g_return_if_fail(priv->details == NULL);

  store = osso_abook_contact_detail_store_new(
    contact,
    OSSO_ABOOK_CONTACT_DETAIL_OTHERS |
    OSSO_ABOOK_CONTACT_DETAIL_NICKNAME |
    OSSO_ABOOK_CONTACT_DETAIL_IM_CHAT |
    OSSO_ABOOK_CONTACT_DETAIL_IM_VIDEO |
    OSSO_ABOOK_CONTACT_DETAIL_IM_VOICE |
    OSSO_ABOOK_CONTACT_DETAIL_PHONE |
    OSSO_ABOOK_CONTACT_DETAIL_EMAIL);
  real_set_store(starter, store);
  g_object_unref(store);
}

static void
osso_abook_touch_contact_starter_set_property(GObject *object,
                                              guint property_id,
                                              const GValue *value,
                                              GParamSpec *pspec)
{
  OssoABookTouchContactStarter *starter =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER(object);
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  switch (property_id)
  {
    case PROP_CONTACT:
    {
      real_set_contact(starter, g_value_get_object(value));
      break;
    }
    case PROP_EDITABLE:
    {
      priv->editable = g_value_get_boolean(value);
      break;
    }
    case PROP_LEFT_PART:
    {
      priv->left_part = g_value_get_boolean(value);
      break;
    }
    case PROP_STORE:
    {
      real_set_store(starter, g_value_get_object(value));
      break;
    }
    case PROP_ALLOWED_ACTIONS:
    {
      priv->allowed_actions = g_value_get_uint(value);
      break;
    }
    case PROP_INTERACTIVE:
    {
      priv->interactive = g_value_get_boolean(value);
      break;
    }
    case PROP_SINGLE_ATTRIBUTE:
    {
      EVCardAttribute *attr = g_value_get_pointer(value);

      if (priv->single_attribute)
        e_vcard_attribute_free(priv->single_attribute);

      if (attr)
      {
        priv->flag1 = FALSE;
        priv->full_view = TRUE;
        priv->single_attribute = e_vcard_attribute_copy(attr);
      }
      else
      {
        priv->single_attribute = NULL;
        priv->flag1 = FALSE;
        priv->full_view = FALSE;
      }

      break;
    }
    case PROP_SINGLE_ATTRIBUTE_PROFILE:
    {
      if (priv->single_attribute_profile)
        g_object_unref(priv->single_attribute_profile);

      priv->single_attribute_profile = g_value_dup_object(value);
      break;
    }
    case PROP_HIGHLIGHTED_ATTRIBUTE:
    {
      EVCardAttribute *attr = g_value_get_pointer(value);
      ;

      if (priv->highlighted_attribute)
        e_vcard_attribute_free(priv->highlighted_attribute);

      if (attr)
        priv->highlighted_attribute = e_vcard_attribute_copy(attr);
      else
        priv->highlighted_attribute = NULL;

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
osso_abook_touch_contact_starter_get_property(GObject *object,
                                              guint property_id, GValue *value,
                                              GParamSpec *pspec)
{
  OssoABookTouchContactStarter *starter =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER(object);
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  switch (property_id)
  {
    case PROP_CONTACT:
    {
      g_value_set_object(value, get_details_contact(priv));
      break;
    }
    case PROP_EDITABLE:
    {
      g_value_set_boolean(value, priv->editable);
      break;
    }
    case PROP_LEFT_PART:
    {
      g_value_set_boolean(value, priv->left_part);
      break;
    }
    case PROP_STORE:
    {
      g_value_set_object(value, priv->details);
      break;
    }
    case PROP_ALLOWED_ACTIONS:
    {
      g_value_set_uint(value, priv->allowed_actions);
      break;
    }
    case PROP_INTERACTIVE:
    {
      g_value_set_boolean(value, priv->interactive);
      break;
    }
    case PROP_SINGLE_ATTRIBUTE:
    {
      g_value_set_pointer(value, priv->single_attribute);
      break;
    }
    case PROP_SINGLE_ATTRIBUTE_PROFILE:
    {
      g_value_set_object(value, priv->single_attribute_profile);
      break;
    }
    case PROP_HIGHLIGHTED_ATTRIBUTE:
    {
      g_value_set_pointer(value, priv->highlighted_attribute);
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
remove_widget_from_parent(GtkWidget *widget)
{
  g_object_ref(widget);
  gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(widget)), widget);
}

static void
screen_size_changed_cb(GdkScreen *screen, gpointer user_data)
{
  OssoABookTouchContactStarter *starter = user_data;
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);
  gboolean is_landscape = osso_abook_screen_is_landscape_mode(screen);
  gboolean was_landscape = priv->landscape;
  GtkWidget *presence_label;
  GtkWidget *toplevel;
  GtkWidget *align;
  GtkWidget *label;
  GtkWidget *child;
  GtkWidget *hbox;
  GtkWidget *pan_area;
  GtkWidget *vbox;

  priv->landscape = is_landscape;

  if (priv->inited && (is_landscape == was_landscape))
    return;

  toplevel = gtk_widget_get_toplevel(GTK_WIDGET(starter));

  if (GTK_IS_DIALOG(toplevel))
    gtk_window_resize(GTK_WINDOW(toplevel), 1, 1);

  if (!priv->inited)
  {
    create_details_widgets(starter);
    update_avatar(starter);

    vbox = gtk_vbox_new(FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);
    gtk_widget_show(vbox);
    priv->vbox = vbox;
    presence_label = osso_abook_presence_label_new(
      OSSO_ABOOK_PRESENCE(get_details_contact(priv)));
    gtk_misc_set_alignment(GTK_MISC(presence_label), 0.0, 0.0);
    hildon_helper_set_logical_font(presence_label, "SmallSystemFont");
    hildon_helper_set_logical_color(presence_label, GTK_RC_FG,
                                    GTK_STATE_NORMAL, "DefaultTextColor");
    gtk_label_set_line_wrap(GTK_LABEL(presence_label), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(presence_label),
                                 PANGO_WRAP_WORD_CHAR);
    gtk_box_pack_start(GTK_BOX(priv->vbox), presence_label, FALSE, FALSE, 0);
    gtk_widget_show(presence_label);
    priv->status_label = presence_label;
    align = gtk_alignment_new(0.0, 0.0, 0.0, 0.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 16, 0, 0, 0);
    label = gtk_label_new(NULL);
    gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
    hildon_helper_set_logical_font(label, "SmallSystemFont");
    hildon_helper_set_logical_color(label, GTK_RC_FG, GTK_STATE_NORMAL,
                                    "SecondaryTextColor");
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(align), label);
    gtk_box_pack_start(GTK_BOX(priv->vbox), align, FALSE, FALSE, 0);
    gtk_widget_show_all(align);
    priv->location_label = label;
    priv->align = align;
    priv->inited = TRUE;
    set_details_contact_name_as_window_title(starter);
    contact_presence_changed_cb(starter);
    g_object_ref(priv->details_widgets);
    g_object_ref(priv->avatar);
    g_object_ref(priv->vbox);
  }
  else
  {
    int n_columns = 0;

    remove_widget_from_parent(priv->avatar);
    remove_widget_from_parent(priv->vbox);

    if (GTK_IS_TABLE(priv->details_widgets))
      g_object_get(priv->details_widgets, "n-columns", &n_columns, NULL);

    if (!n_columns || !priv->landscape || (priv->landscape && (n_columns == 2)))
      remove_widget_from_parent(priv->details_widgets);
    else
    {
      gtk_widget_destroy(priv->details_widgets);
      create_details_widgets(starter);
      g_object_ref(priv->details_widgets);
    }
  }

  child = gtk_bin_get_child(GTK_BIN(starter));

  if (child)
    gtk_widget_destroy(child);

  if (priv->landscape)
  {
    align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 11, 0, 0, 0);
    gtk_container_add(GTK_CONTAINER(starter), align);
    gtk_widget_show(align);
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_container_add(GTK_CONTAINER(align), hbox);
    gtk_widget_show(hbox);
    vbox = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), priv->vbox, FALSE, TRUE, 0);
    add_avater_to_widget(starter, vbox);
    gtk_box_pack_start(GTK_BOX(hbox), vbox, FALSE, FALSE, 0);

    if (priv->left_part)
      gtk_widget_show(vbox);

    align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 5, 0, 0, 0);
    gtk_box_pack_start(GTK_BOX(hbox), align, TRUE, TRUE, 0);
    gtk_widget_show(align);
    pan_area = osso_abook_pannable_area_new();
    gtk_container_add(GTK_CONTAINER(align), pan_area);
    gtk_widget_show(pan_area);
    align = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
    gtk_alignment_set_padding(GTK_ALIGNMENT(align), 0, 0, 16, 0);
    hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(pan_area),
                                           align);
    gtk_widget_show(align);
    gtk_container_add(GTK_CONTAINER(align), priv->details_widgets);
    gtk_widget_set_size_request(priv->status_label, 128, -1);
    gtk_widget_set_size_request(priv->location_label, 128, -1);
    gtk_widget_size_request(priv->status_label, NULL);
    gtk_widget_size_request(priv->location_label, NULL);
  }
  else
  {
    gint width;

    pan_area = osso_abook_pannable_area_new();
    gtk_container_add(GTK_CONTAINER(starter), pan_area);
    gtk_widget_show(pan_area);
    vbox = gtk_vbox_new(FALSE, 16);
    hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(pan_area),
                                           vbox);
    gtk_widget_show(vbox);
    hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), priv->vbox, FALSE, TRUE, 0);
    add_avater_to_widget(starter, hbox);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);

    if (priv->left_part)
      gtk_widget_show(hbox);

    gtk_box_pack_start(GTK_BOX(vbox), priv->details_widgets, TRUE, TRUE, 0);
    width = gdk_screen_get_width(priv->screen) - 232;
    gtk_widget_set_size_request(priv->status_label, width, -1);
    gtk_widget_set_size_request(priv->location_label, width, -1);
  }

  g_object_unref(priv->details_widgets);
  g_object_unref(priv->avatar);
  g_object_unref(priv->vbox);
}

static void
screen_changed_cb(OssoABookTouchContactStarter *starter, GdkScreen *prev,
                  gpointer user_data)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  if (priv->screen)
  {
    g_signal_handlers_disconnect_matched(
      priv->screen, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      screen_size_changed_cb, starter);
  }

  priv->screen = gtk_widget_get_screen(GTK_WIDGET(starter));
  ;

  if (priv->screen)
  {
    screen_size_changed_cb(priv->screen, starter);
    g_signal_connect(priv->screen, "size-changed",
                     G_CALLBACK(screen_size_changed_cb), starter);
  }
}

static void
osso_abook_touch_contact_starter_dispose(GObject *object)
{
  OssoABookTouchContactStarter *starter =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER(object);
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  if (priv->cnxn)
  {
    gconf_client_notify_remove(osso_abook_get_gconf_client(), priv->cnxn);
    priv->cnxn = 0;
  }

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

  if (priv->details)
  {
    clear_details_contact(object, get_details_contact(priv));
    g_object_unref(priv->details);
    priv->details = NULL;
  }

  if (priv->single_attribute)
  {
    e_vcard_attribute_free(priv->single_attribute);
    priv->single_attribute = NULL;
  }

  if (priv->single_attribute_profile)
  {
    g_object_unref(priv->single_attribute_profile);
    priv->single_attribute_profile = NULL;
  }

  if (priv->highlighted_attribute)
  {
    e_vcard_attribute_free(priv->highlighted_attribute);
    priv->highlighted_attribute = NULL;
  }

  G_OBJECT_CLASS(osso_abook_touch_contact_starter_parent_class)->
  dispose(object);
}

static void
osso_abook_touch_contact_starter_size_request(GtkWidget *widget,
                                              GtkRequisition *requisition)
{
  OssoABookTouchContactStarter *starter =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER(widget);
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);
  GtkBin *bin = GTK_BIN(widget);
  GtkContainer *container = GTK_CONTAINER(widget);

  requisition->width = 2 * container->border_width;
  requisition->height = 2 * container->border_width;

  if (bin->child && gtk_widget_get_visible(bin->child))
  {
    gint h;
    gint height;
    GtkRequisition child_requisition;

    gtk_widget_size_request(bin->child, &child_requisition);

    if (priv->landscape)
      h = 0;
    else
      h = 156;

    if (h < child_requisition.height)
      height = requisition->height + child_requisition.height;
    else
      height = requisition->height + h;

    requisition->width += child_requisition.width;
    requisition->height = height;
  }
}

static void
osso_abook_touch_contact_starter_size_allocate(GtkWidget *widget,
                                               GtkAllocation *allocation)
{
  GtkBin *bin = GTK_BIN(widget);
  GtkContainer *container = GTK_CONTAINER(widget);

  widget->allocation.x = allocation->x;
  widget->allocation.y = allocation->y;
  widget->allocation.width = allocation->width;
  widget->allocation.height = allocation->height;

  if (bin->child && gtk_widget_get_visible(bin->child))
  {
    GtkAllocation new_allocation;
    int border_width = container->border_width;
    int w = allocation->width - 2 * border_width;
    int h = allocation->height - 2 * border_width;

    if (w < 0)
      w = 0;

    if (h < 0)
      h = 0;

    new_allocation.x = allocation->x + border_width;
    new_allocation.y = allocation->y + border_width;
    new_allocation.width = w;
    new_allocation.height = h;

    gtk_widget_size_allocate(bin->child, &new_allocation);
  }
}

static void
osso_abook_touch_contact_starter_class_init(
  OssoABookTouchContactStarterClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->set_property = osso_abook_touch_contact_starter_set_property;
  object_class->get_property = osso_abook_touch_contact_starter_get_property;
  object_class->dispose = osso_abook_touch_contact_starter_dispose;

  widget_class->size_request = osso_abook_touch_contact_starter_size_request;
  widget_class->size_allocate = osso_abook_touch_contact_starter_size_allocate;

  g_object_class_install_property(
    object_class, PROP_CONTACT,
    g_param_spec_object(
      "contact",
      "Contact",
      "The displayed OssoABookContact.",
      OSSO_ABOOK_TYPE_CONTACT,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_EDITABLE,
    g_param_spec_boolean(
      "editable",
      "Editable",
      "Whether the contact is editable.",
      FALSE,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_LEFT_PART,
    g_param_spec_boolean(
      "left-part",
      "Left part",
      "Whether the left part should be shown.",
      TRUE,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_STORE,
    g_param_spec_object(
      "store",
      "Store",
      "The OssoABookContactDetailStore to use.",
      OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_ALLOWED_ACTIONS,
    g_param_spec_uint(
      "allowed-actions",
      "Allowed actions",
      "The allowed OssoABookContactAction flags.",
      0,
      G_MAXUINT,
      G_MAXUINT,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_INTERACTIVE,
    g_param_spec_boolean(
      "interactive",
      "Interactive",
      "Whether buttons are clickable.",
      TRUE,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_SINGLE_ATTRIBUTE,
    g_param_spec_pointer(
      "single-attribute",
      "single-attribute",
      "An optional attribute to restrict the visible buttons to.",
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_SINGLE_ATTRIBUTE_PROFILE,
    g_param_spec_object(
      "single-attribute-profile",
      "single-attribute-profile",
      "An optional profile if single-attribute refers to a TEL field, but the call was made through a VOIP service.",
      TP_TYPE_PROTOCOL,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_HIGHLIGHTED_ATTRIBUTE,
    g_param_spec_pointer(
      "highlighted-attribute",
      "Highlighted attribute",
      "An optional attribute to highlight",
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  signals[PRE_ACTION_START] =
    g_signal_new("pre-action-start", OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER,
                 G_SIGNAL_RUN_LAST, 0, g_signal_accumulator_true_handled,
                 0, osso_abook_marshal_BOOLEAN__POINTER, G_TYPE_BOOLEAN,
                 1, G_TYPE_POINTER);
  signals[ACTION_STARTED] =
    g_signal_new("action-started", OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER,
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(OssoABookTouchContactStarterClass,
                                 action_started),
                 0, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  signals[EDITOR_STARTED] =
    g_signal_new("editor-started", OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER,
                 G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(OssoABookTouchContactStarterClass,
                                 editor_started),
                 0, NULL, g_cclosure_marshal_VOID__OBJECT, G_TYPE_NONE,
                 1, OSSO_ABOOK_TYPE_CONTACT_EDITOR);
}

static void
name_order_changed_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry,
                      gpointer user_data)
{
  set_details_contact_name_as_window_title(user_data);
}

static void
osso_abook_touch_contact_starter_init(OssoABookTouchContactStarter *starter)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  g_signal_connect(starter, "screen-changed",
                   G_CALLBACK(screen_changed_cb), NULL);
  priv->cnxn = gconf_client_notify_add(
    osso_abook_get_gconf_client(), OSSO_ABOOK_SETTINGS_KEY_NAME_ORDER,
    name_order_changed_cb, starter, NULL, NULL);
  priv->started_action = OSSO_ABOOK_CONTACT_ACTION_NONE;
  priv->highlighted_attribute = OSSO_ABOOK_CONTACT_ACTION_NONE;
  priv->single_attribute = NULL;
  priv->single_attribute_profile = NULL;

  priv->full_view = FALSE;
  priv->flag1 = FALSE;
}

static void
editor_response_cb(GtkWidget *dialog, int response_id, gpointer user_data)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(user_data);

  priv->started = FALSE;
  gtk_widget_destroy(dialog);
}

static void
voicemail_response_cb(GtkWidget *dialog, gint response_id, gpointer user_data)
{
  if (response_id == GTK_RESPONSE_OK)
  {
    HildonTouchSelector *selector =
      hildon_picker_dialog_get_selector(HILDON_PICKER_DIALOG(dialog));

    osso_abook_voicemail_selector_apply(
      OSSO_ABOOK_VOICEMAIL_SELECTOR(selector));
    osso_abook_voicemail_selector_save(OSSO_ABOOK_VOICEMAIL_SELECTOR(selector));
  }

  editor_response_cb(dialog, response_id, user_data);
}

static void
contacts_added_closure_finish(struct contacts_added_data *data,
                              OssoABookContact *contact)
{
  OssoABookTouchContactStarterPrivate *priv =
    OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(data->starter);

  g_return_if_fail(contact && OSSO_ABOOK_IS_CONTACT(contact));

  g_signal_handlers_disconnect_matched(data->aggregator, G_SIGNAL_MATCH_DATA, 0,
                                       0, NULL, NULL, data);
  osso_abook_contact_detail_store_set_contact(priv->details, contact);
  g_free(data->uid);
  g_free(data);
}

static void
contacts_added_cb(OssoABookRoster *roster, OssoABookContact **contacts,
                  gpointer user_data)
{
  struct contacts_added_data *data = user_data;

  while (*contacts)
  {
    if (!g_strcmp0(e_contact_get_const(E_CONTACT(*contacts), E_CONTACT_UID),
                   data->uid))
    {
      contacts_added_closure_finish(data, *contacts);
      break;
    }

    contacts++;
  }
}

static gboolean
contact_saved_cb(OssoABookContactEditor *editor, const char *uid,
                 gpointer user_data)
{
  struct contacts_added_data *data = g_new(struct contacts_added_data, 1);
  GList *contact;

  data->starter = user_data;
  data->aggregator = osso_abook_aggregator_get_default(NULL);
  data->uid = g_strdup(uid);

  g_signal_connect(data->aggregator, "contacts-added",
                   G_CALLBACK(contacts_added_cb), data);
  contact = osso_abook_aggregator_lookup(
    OSSO_ABOOK_AGGREGATOR(data->aggregator), uid);

  if (contact && contact->data)
    contacts_added_closure_finish(data, contact->data);

  g_list_free(contact);

  return FALSE;
}

void
osso_abook_touch_contact_starter_start_editor(
  OssoABookTouchContactStarter *starter)
{
  OssoABookTouchContactStarterPrivate *priv;
  GtkWindow *parent;
  OssoABookContact *contact;
  GtkWidget *dialog;

  g_return_if_fail(OSSO_ABOOK_IS_TOUCH_CONTACT_STARTER(starter));

  priv = OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  g_return_if_fail(priv->editable);

  if (priv->started)
    return;

  priv->started = TRUE;
  parent = (GtkWindow *)gtk_widget_get_ancestor(GTK_WIDGET(starter),
                                                GTK_TYPE_WINDOW);
  contact = get_details_contact(priv);

  if (OSSO_ABOOK_IS_VOICEMAIL_CONTACT(contact))
  {
    GtkWidget *selector;

    dialog = hildon_picker_dialog_new(parent);
    gtk_window_set_title(GTK_WINDOW(dialog),
                         osso_abook_contact_get_display_name(contact));
    selector = osso_abook_voicemail_selector_new();
    hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog),
                                      HILDON_TOUCH_SELECTOR(selector));
    hildon_picker_dialog_set_done_label(
      HILDON_PICKER_DIALOG(dialog),
      dgettext("hildon-libs", "wdgt_bd_save"));
    g_signal_connect(dialog, "response",
                     G_CALLBACK(voicemail_response_cb), starter);
  }
  else
  {
    dialog = osso_abook_contact_editor_new_with_contact(
      parent, contact, OSSO_ABOOK_CONTACT_EDITOR_EDIT);

    if (osso_abook_contact_is_temporary(contact))
    {
      g_signal_connect(dialog, "contact-saved",
                       G_CALLBACK(contact_saved_cb), starter);
    }

    g_signal_connect(dialog, "response",
                     G_CALLBACK(editor_response_cb), starter);

    g_signal_emit(starter, signals[EDITOR_STARTED], 0, dialog);
  }

  gtk_widget_show(dialog);
}

static GtkWidget *
osso_abook_touch_contact_starter_new_full(GtkWindow *parent,
                                          OssoABookContact *contact,
                                          gboolean editable)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER,
                      "contact", contact,
                      "editable", editable,
                      "single-attribute", FALSE,
                      NULL);
}

GtkWidget *
osso_abook_touch_contact_starter_new_with_editor(GtkWindow *parent,
                                                 OssoABookContact *contact)
{
  return osso_abook_touch_contact_starter_new_full(parent, contact, TRUE);
}

GtkWidget *
osso_abook_touch_contact_starter_new_with_contact(GtkWindow *parent,
                                                  OssoABookContact *contact)
{
  return osso_abook_touch_contact_starter_new_full(parent, contact, FALSE);
}

OssoABookContact *
osso_abook_touch_contact_starter_get_contact(
    OssoABookTouchContactStarter *starter)
{
  OssoABookTouchContactStarterPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_TOUCH_CONTACT_STARTER(starter), NULL);

  priv = OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);

  return get_details_contact(priv);
}

OssoABookContactAction
osso_abook_touch_contact_starter_get_started_action(
    OssoABookTouchContactStarter *starter)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_TOUCH_CONTACT_STARTER(starter),
                       OSSO_ABOOK_CONTACT_ACTION_NONE);

  return OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter)->started_action;
}

static void
merge_with_cb(const char *uid, gpointer user_data)
{
  struct start_merge_data *merge_data = user_data;
  OssoABookTouchContactStarterPrivate *priv =
      OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(merge_data->starter);

  if (merge_data->cb)
    merge_data->cb(uid, merge_data->user_data);

  if (uid)
  {
    if (g_strcmp0(merge_data->uid, uid))
    {
      OssoABookRoster *aggregator = osso_abook_aggregator_get_default(NULL);
      GList *contact =
          osso_abook_aggregator_lookup(OSSO_ABOOK_AGGREGATOR(aggregator),uid);

      if (contact)
      {
        if (g_list_length(contact) == 1)
        {
          osso_abook_contact_detail_store_set_contact(priv->details,
                                                      contact->data);
        }
        else
          g_critical("Unexpected multiple contacts matching UID %s", uid);
      }
      else
      {
        struct contacts_added_data *data =
            g_new0(struct contacts_added_data, 1);

        data->starter = merge_data->starter;
        data->aggregator = aggregator;
        data->uid = g_strdup(uid);
        g_signal_connect(aggregator, "contacts-added",
                         G_CALLBACK(contacts_added_cb), data);
      }

      g_list_free(contact);
    }
  }

  g_object_unref(merge_data->starter);
  g_free(merge_data->uid);
  g_free(merge_data);
}

void
osso_abook_touch_contact_starter_start_merge(
    OssoABookTouchContactStarter *starter, OssoABookContactModel *contact_model,
    OssoABookMergeWithCb cb, gpointer user_data)
{
  OssoABookTouchContactStarterPrivate *priv;
  OssoABookContact *contact;
  struct start_merge_data *data;
  gpointer parent;

  g_return_if_fail(OSSO_ABOOK_IS_TOUCH_CONTACT_STARTER(starter));

  priv = OSSO_ABOOK_TOUCH_CONTACT_STARTER_PRIVATE(starter);
  contact = get_details_contact(priv);
  parent = gtk_widget_get_ancestor(GTK_WIDGET(starter), GTK_TYPE_WINDOW);

  data = g_new0(struct start_merge_data, 1);

  data->starter = g_object_ref(starter);
  data->cb = cb;
  data->user_data = user_data;
  data->uid = g_strdup(e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID));

  osso_abook_merge_with_many_dialog(contact, contact_model, parent,
                                    merge_with_cb, data);
}

GtkWidget *
osso_abook_touch_contact_starter_dialog_new(
    GtkWindow *parent, OssoABookTouchContactStarter *starter)
{
  GtkWidget *dialog = gtk_dialog_new();

  gtk_widget_hide(GTK_DIALOG(dialog)->action_area);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), GTK_WIDGET(starter),
                     TRUE, TRUE, 0);
  g_signal_connect_swapped(starter, "action-started",
                           G_CALLBACK(gtk_widget_destroy), dialog);
  return dialog;
}
