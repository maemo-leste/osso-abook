/*
 * osso-abook-mecard-view.c
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

#include <libmodest-dbus-client/libmodest-dbus-client.h>

#include "osso-abook-account-manager.h"
#include "osso-abook-contact-detail-store.h"
#include "osso-abook-contact-editor.h"
#include "osso-abook-dialogs.h"
#include "osso-abook-errors.h"
#include "osso-abook-icon-sizes.h"
#include "osso-abook-init.h"
#include "osso-abook-message-map.h"
#include "osso-abook-presence-icon.h"
#include "osso-abook-presence.h"
#include "osso-abook-self-contact.h"
#include "osso-abook-send-contacts.h"
#include "osso-abook-util.h"
#include "osso-abook-utils-private.h"

#include "osso-abook-mecard-view.h"

struct _OssoABookMecardViewPrivate
{
  OssoABookContactDetailStore *detail_store;
  int attach;
  OssoABookAccountManager *account_manager;
  gulong account_created_id;
  gulong account_removed_id;
  GtkBox *box;
  GtkWidget *no_details_label;
  GtkWidget *presence_button;
  GtkWidget *presence_status_label;
  GtkWidget *pannable_area;
  GtkWidget *table;
  GtkWidget *menu;
};

typedef struct _OssoABookMecardViewPrivate OssoABookMecardViewPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookMecardView,
  osso_abook_mecard_view,
  HILDON_TYPE_STACKABLE_WINDOW
);

#define PRIVATE(view) \
  ((OssoABookMecardViewPrivate *)osso_abook_mecard_view_get_instance_private( \
     (OssoABookMecardView *)(view)))

static OssoABookMessageMapping map[] =
{
  {
    "", "addr_fi_mecard"
  },
  { NULL, NULL }
};

static OssoABookContact *
detail_store_get_contact(OssoABookMecardViewPrivate *priv)
{
  return osso_abook_contact_detail_store_get_contact(priv->detail_store);
}

static void
notify_presence_status_message_cb(OssoABookMecardView *view)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);
  const char *status_message = osso_abook_presence_get_presence_status_message(
      OSSO_ABOOK_PRESENCE(detail_store_get_contact(priv)));

  gtk_label_set_text(GTK_LABEL(priv->presence_status_label), status_message);
}

static void
notify_presence_status_cb(OssoABookMecardView *view)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);
  const char *display_status = osso_abook_presence_get_display_status(
      OSSO_ABOOK_PRESENCE(detail_store_get_contact(priv)));

  hildon_button_set_title(HILDON_BUTTON(priv->presence_button), display_status);
}

static void
status_changed_cb(TpAccount *account, guint old_status, guint new_status,
                  guint reason, gchar *dbus_error_name, GHashTable *details,
                  gpointer user_data)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(user_data);

  osso_abook_contact_detail_store_set_contact(priv->detail_store,
                                              detail_store_get_contact(priv));
}

static void
account_removed_cb(OssoABookAccountManager *manager, TpAccount *account,
                   OssoABookMecardView *view)
{
  g_signal_handlers_disconnect_matched(
    account, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
    0, 0, NULL, status_changed_cb, view);
}

static void
osso_abook_mecard_view_dispose(GObject *object)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(object);
  GList *accounts;
  GList *l;

  if (priv->account_created_id)
  {
    disconnect_signal_if_connected(priv->account_manager,
                                   priv->account_created_id);
    priv->account_created_id = 0;
  }

  if (priv->account_removed_id)
  {
    disconnect_signal_if_connected(priv->account_manager,
                                   priv->account_removed_id);
    priv->account_removed_id = 0;
  }

  accounts = osso_abook_account_manager_list_enabled_accounts(
      priv->account_manager);

  for (l = accounts; l; l = l->next)
  {
    if (!l->data)
      break;

    account_removed_cb(priv->account_manager, l->data,
                       OSSO_ABOOK_MECARD_VIEW(object));
  }

  g_list_free(accounts);

  if (priv->detail_store)
  {
    OssoABookContact *contact = detail_store_get_contact(priv);

    if (contact)
    {
      g_signal_handlers_disconnect_matched(
        contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
        0, 0, NULL, notify_presence_status_cb, object);
      g_signal_handlers_disconnect_matched(
        contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
        0, 0, NULL, notify_presence_status_message_cb, object);
    }

    g_object_unref(priv->detail_store);
    priv->detail_store = NULL;
  }

  G_OBJECT_CLASS(osso_abook_mecard_view_parent_class)->dispose(object);
}

static void
osso_abook_mecard_view_class_init(OssoABookMecardViewClass *klass)
{
  G_OBJECT_CLASS(klass)->dispose = osso_abook_mecard_view_dispose;
}

static void
add_button(OssoABookMecardView *view, const gchar *title, GCallback cb)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);
  GtkWidget *button;

  button = hildon_button_new_with_text(HILDON_SIZE_FINGER_HEIGHT,
                                       HILDON_BUTTON_ARRANGEMENT_VERTICAL,
                                       title, NULL);
  g_signal_connect_after(button, "clicked", cb, view);
  hildon_app_menu_append(HILDON_APP_MENU(priv->menu), GTK_BUTTON(button));
  gtk_widget_show(button);
}

static void
editmyinfo_clicked_cb(GtkButton *button, OssoABookMecardView *view)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);
  GtkWidget *contact_editor;

  contact_editor = osso_abook_contact_editor_new_with_contact(
      GTK_WINDOW(view), detail_store_get_contact(priv),
      OSSO_ABOOK_CONTACT_EDITOR_EDIT_SELF);
  g_signal_connect(contact_editor, "contact-saved", G_CALLBACK(gtk_true), NULL);
  gtk_dialog_run(GTK_DIALOG(contact_editor));
  gtk_widget_destroy(contact_editor);
}

static void
sendmycard_clicked_cb(GtkButton *button, OssoABookMecardView *view)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);
  GtkWidget *send_contacts_dialog;

  send_contacts_dialog = osso_abook_send_contacts_dialog_new(
      GTK_WINDOW(view), detail_store_get_contact(priv), 0);
  g_signal_connect(send_contacts_dialog, "response",
                   G_CALLBACK(gtk_widget_destroy), NULL);
  gtk_widget_show(send_contacts_dialog);
}

static void
accounts_clicked_cb(GtkButton *button, OssoABookMecardView *view)
{
  osso_abook_add_im_account_dialog_run(GTK_WINDOW(view));
}

static void
mydetail_clicked_cb(GtkButton *button, OssoABookMecardView *view)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);

  osso_abook_send_contacts_detail_dialog_default(detail_store_get_contact(priv),
                                                 GTK_WINDOW(view));
}

static void
email_clicked_cb(GtkButton *button, OssoABookMecardView *view)
{
  libmodest_dbus_client_open_default_inbox(osso_abook_get_osso_context());
}

static void
show_resence(OssoABookMecardView *view)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);
  GList *roster_contacts =
    osso_abook_contact_get_roster_contacts(detail_store_get_contact(priv));

  if (roster_contacts)
  {
    gtk_widget_show(priv->presence_button);
    gtk_widget_show(priv->presence_status_label);
  }
  else
  {
    gtk_widget_hide(priv->presence_button);
    gtk_widget_hide(priv->presence_status_label);
  }

  g_list_free(roster_contacts);
}

static void
add_field(OssoABookMecardView *view, OssoABookContactField *field)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);
  const char *display_title = osso_abook_contact_field_get_display_title(field);
  const char *display_value = osso_abook_contact_field_get_display_value(field);
  GtkWidget *label;

  if (IS_EMPTY(display_title) || IS_EMPTY(display_value))
    return;

  label = gtk_label_new(display_title);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
  gtk_table_attach(GTK_TABLE(priv->table), label, 0, 1,
                   priv->attach, priv->attach + 1, GTK_FILL, GTK_FILL,
                   0, 0);
  hildon_helper_set_logical_color(label, GTK_RC_FG, GTK_STATE_NORMAL,
                                  "SecondaryTextColor");
  gtk_widget_show(label);

  label = gtk_label_new(display_value);
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
  gtk_table_attach(GTK_TABLE(priv->table), label, 1, 2,
                   priv->attach, priv->attach + 1,
                   GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
  gtk_label_set_line_wrap_mode(GTK_LABEL(label),
                               PANGO_WRAP_WORD_CHAR);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
  gtk_widget_show(label);
  priv->attach++;
}

static void
size_allocate_cb(GtkWidget *window,
                 GdkRectangle *allocation,
                 gpointer user_data)
{
  GtkWidget *table;
  /* maybe 'left' is 'right', not sure, but not important also */
  GList *left = NULL;
  GList *right = NULL;
  gint right_width = 0;
  gint left_width;
  GList *children;

  if (!gtk_widget_get_realized(window) || (allocation->width < 2))
    return;

  table = gtk_bin_get_child(GTK_BIN(window));

  for (children = gtk_container_get_children(GTK_CONTAINER(table)); children;
       children = g_list_delete_link(children, children))
  {
    gint left_attach;

    gtk_container_child_get(GTK_CONTAINER(table), children->data,
                            "left-attach", &left_attach,
                            NULL);

    if (left_attach <= 0)
    {
      GtkRequisition requisition;

      gtk_widget_size_request(children->data, &requisition);

      if (right_width < requisition.width)
        right_width = requisition.width;

      right = g_list_prepend(right, children->data);
    }
    else
      left = g_list_prepend(left, children->data);
  }

  if (right_width >= 2 * allocation->width / 5)
    right_width = 2 * allocation->width / 5;

  left_width =
    allocation->width - gtk_table_get_col_spacing(GTK_TABLE(table), 0) -
    2 * gtk_container_get_border_width(GTK_CONTAINER(table)) -
    2 * table->style->xthickness - right_width;

  if ((right_width > 0) && (left_width > 0))
  {
    GList *l;

    for (l = right; l; l = l->next)
      osso_abook_enforce_label_width(l->data, right_width);

    for (l = left; l; l = l->next)
      osso_abook_enforce_label_width(l->data, left_width);
  }

  g_list_free(right);
  g_list_free(left);
}

static void
realize_cb(GtkWidget *widget, gpointer user_data)
{
  size_allocate_cb(widget, &widget->allocation, user_data);
}

static void
detail_store_changed_cb(OssoABookMecardView *view)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);
  GSequence *fields;
  GSequenceIter *iter;

  show_resence(view);

  if (priv->pannable_area)
  {
    gtk_widget_destroy(priv->pannable_area);
    priv->pannable_area = NULL;
  }

  if (priv->no_details_label)
  {
    gtk_widget_destroy(priv->no_details_label);
    priv->no_details_label = NULL;
  }

  fields = osso_abook_contact_detail_store_get_fields(priv->detail_store);

  if (fields)
  {
    priv->pannable_area = hildon_pannable_area_new();
    g_object_set(priv->pannable_area,
                 "hscrollbar-policy", GTK_POLICY_NEVER,
                 "mov-mode", HILDON_MOVEMENT_MODE_VERT,
                 NULL);
    gtk_box_pack_start(priv->box, priv->pannable_area, TRUE, TRUE, 0);
    gtk_widget_show(priv->pannable_area);

    priv->table = gtk_table_new(g_sequence_get_length(fields), 2, 0);
    gtk_table_set_col_spacings(GTK_TABLE(priv->table), 24);
    gtk_table_set_row_spacings(GTK_TABLE(priv->table), 8);
    hildon_pannable_area_add_with_viewport(
      HILDON_PANNABLE_AREA(priv->pannable_area), priv->table);
    gtk_widget_show(priv->table);

    g_signal_connect(gtk_widget_get_parent(priv->table), "size-allocate",
                     G_CALLBACK(size_allocate_cb), NULL);
    g_signal_connect(gtk_widget_get_parent(priv->table), "realize",
                     G_CALLBACK(realize_cb), NULL);

    priv->attach = 0;

    for (iter = g_sequence_get_begin_iter(fields);
         !g_sequence_iter_is_end(iter); iter = g_sequence_iter_next(iter))
    {
      add_field(view, g_sequence_get(iter));
    }
  }
  else
  {
    priv->no_details_label = gtk_label_new(_("addr_ia_no_details"));
    gtk_misc_set_alignment(GTK_MISC(priv->no_details_label), 0.5, 0.5);
    hildon_helper_set_logical_font(priv->no_details_label, "LargeSystemFont");
    gtk_box_pack_start(priv->box, priv->no_details_label, TRUE, TRUE, 0);
    gtk_widget_show(priv->no_details_label);
  }
}

static void
account_created_cb(OssoABookAccountManager *manager, TpAccount *account,
                   OssoABookMecardView *view)
{
  g_signal_connect(account, "status-changed",
                   G_CALLBACK(status_changed_cb), view);
}

static void
presence_button_clicked_cb(GtkButton *button, OssoABookMecardView *view)
{
  GError *error = NULL;
  DBusGConnection *session_bus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);

  if (session_bus)
  {
    DBusGProxy *proxy = dbus_g_proxy_new_for_name(session_bus,
                                                  "com.nokia.PresenceUI",
                                                  "/com/nokia/PresenceUI",
                                                  "com.nokia.PresenceUI");
    dbus_g_proxy_call_no_reply(proxy, "StartUp", G_TYPE_INVALID);
    g_object_unref(proxy);
    dbus_g_connection_unref(session_bus);
  }
  else
    osso_abook_handle_gerror(GTK_WINDOW(view), error);
}

static void
osso_abook_mecard_view_init(OssoABookMecardView *view)
{
  OssoABookMecardViewPrivate *priv = PRIVATE(view);
  OssoABookContact *contact =
    OSSO_ABOOK_CONTACT(osso_abook_self_contact_get_default());
  GtkWidget *align;
  GtkWidget *vbox;
  GtkWidget *hbox;
  GtkWidget *avatar_button;
  GtkWidget *presence_icon;
  GList *accounts;
  GtkBox *box;

  priv->detail_store = osso_abook_contact_detail_store_new(
      contact, OSSO_ABOOK_CONTACT_DETAIL_ALL);

  osso_abook_contact_detail_store_set_message_map(priv->detail_store, map);

  gtk_window_set_title(GTK_WINDOW(view), _("addr_ti_mecard_title"));
  align = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(align), 16, 0, 16, 16);
  gtk_container_add(GTK_CONTAINER(view), align);
  gtk_widget_show(align);

  hbox = gtk_hbox_new(FALSE, 32);
  gtk_container_add(GTK_CONTAINER(align), hbox);
  gtk_widget_show(hbox);
  priv->box = GTK_BOX(hbox);

  vbox = gtk_vbox_new(FALSE, 16);
  gtk_box_pack_start(priv->box, vbox, FALSE, FALSE, 0);
  gtk_widget_show(vbox);
  box = GTK_BOX(vbox);

  avatar_button = osso_abook_avatar_button_new(
      OSSO_ABOOK_AVATAR(detail_store_get_contact(priv)),
      OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT);
  g_signal_connect(avatar_button, "clicked",
                   G_CALLBACK(editmyinfo_clicked_cb), view);

  gtk_box_pack_start(box, avatar_button, FALSE, FALSE, 0);
  gtk_widget_show(avatar_button);

  priv->presence_button = hildon_button_new(
      HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
  gtk_box_pack_start(box, priv->presence_button, FALSE, FALSE, 0);

  presence_icon = osso_abook_presence_icon_new(
      OSSO_ABOOK_PRESENCE(detail_store_get_contact(priv)));
  hildon_button_set_image(HILDON_BUTTON(priv->presence_button), presence_icon);
  gtk_widget_show(presence_icon);

  priv->presence_status_label = gtk_label_new("");
  gtk_widget_set_size_request(priv->presence_status_label,
                              OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT, -1);
  gtk_misc_set_alignment(GTK_MISC(priv->presence_status_label), 0.0, 0.0);
  gtk_label_set_line_wrap(GTK_LABEL(priv->presence_status_label), 1);
  gtk_label_set_line_wrap_mode(GTK_LABEL(priv->presence_status_label),
                               PANGO_WRAP_WORD_CHAR);
  hildon_helper_set_logical_font(priv->presence_status_label,
                                 "SmallSystemFont");
  gtk_box_pack_start(box, priv->presence_status_label, FALSE, FALSE, 0);
  gtk_widget_show(priv->presence_status_label);

  priv->menu = hildon_app_menu_new();
  hildon_window_set_app_menu(HILDON_WINDOW(view),
                             HILDON_APP_MENU(priv->menu));

  add_button(view, _("addr_me_editmyinfo"), G_CALLBACK(editmyinfo_clicked_cb));
  add_button(view, _("addr_me_sendmycard"), G_CALLBACK(sendmycard_clicked_cb));
  add_button(view, _("addr_me_accounts"), G_CALLBACK(accounts_clicked_cb));
  add_button(view, _("addr_me_sendmydetail"), G_CALLBACK(mydetail_clicked_cb));
  add_button(view, _("addr_me_email"), G_CALLBACK(email_clicked_cb));

  detail_store_changed_cb(view);
  g_signal_connect_swapped(priv->detail_store, "changed",
                           G_CALLBACK(detail_store_changed_cb), view);

  priv->account_manager = osso_abook_account_manager_get_default();
  accounts = osso_abook_account_manager_list_enabled_accounts(
      priv->account_manager);
  priv->account_created_id =
    g_signal_connect_object(priv->account_manager, "account-created",
                            G_CALLBACK(account_created_cb), view, 0);
  priv->account_removed_id =
    g_signal_connect_object(priv->account_manager, "account-removed",
                            G_CALLBACK(account_removed_cb), view, 0);

  for (; accounts; accounts = accounts->next)
    account_created_cb(priv->account_manager, accounts->data, view);

  notify_presence_status_cb(view);
  notify_presence_status_message_cb(view);
  g_signal_connect_swapped(contact, "notify::presence-status",
                           G_CALLBACK(notify_presence_status_cb), view);
  g_signal_connect_swapped(contact, "notify::presence-status-message",
                           G_CALLBACK(notify_presence_status_message_cb), view);
  g_signal_connect(priv->presence_button, "clicked",
                   G_CALLBACK(presence_button_clicked_cb), view);
}

GtkWidget *
osso_abook_mecard_view_new()
{
  return g_object_new(OSSO_ABOOK_TYPE_MECARD_VIEW, NULL);
}
