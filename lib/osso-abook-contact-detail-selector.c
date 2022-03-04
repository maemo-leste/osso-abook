/*
 * osso-abook-contact-detail-selector.c
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

#include "osso-abook-contact.h"
#include "osso-abook-message-map.h"
#include "osso-abook-util.h"

#include "osso-abook-contact-detail-selector.h"

enum
{
  PROP_DETAILS = 1
};

struct _OssoABookContactDetailSelectorPrivate
{
  OssoABookContactDetailStore *store;
  GtkWidget *pannable_area;
  GtkWidget *table;
  OssoABookContactField *selected_field;
  guint gconf_handler;
};

typedef struct _OssoABookContactDetailSelectorPrivate \
  OssoABookContactDetailSelectorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookContactDetailSelector,
  osso_abook_contact_detail_selector,
  GTK_TYPE_DIALOG
);

#define PRIVATE(selector) \
  ((OssoABookContactDetailSelectorPrivate *) \
   osso_abook_contact_detail_selector_get_instance_private( \
     (OssoABookContactDetailSelector *)(selector)))

static const OssoABookMessageMapping store_map[] =
{
  { "", "addr_bd_cont_starter" },
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
  { "address_detail", "" },
  { "address_home_detail", "addr_bd_cont_starter_address_home" },
  { "address_work_detail", "addr_bd_cont_starter_address_work" },
  { "email_detail", "" },
  { "email_home_detail", "addr_bd_cont_starter_email_home" },
  { "email_work_detail", "addr_bd_cont_starter_email_work" },
  { "mobile_detail", "" },
  { "mobile_home_detail", "addr_bd_cont_starter_mobile_" },
  { "mobile_work_detail", "addr_bd_cont_starter_mobile_work" },
  { "phone_detail", "" },
  { "phone_home_detail", "addr_bd_cont_starter_phone_home" },
  { "phone_work_detail", "addr_bd_cont_starter_phone_work" },
  { "phone_other_detail", "addr_bd_cont_starter_phone_other" },
  { "phone_fax_detail", "addr_bd_cont_starter_phone_fax" },
  { "sms_detail", "" },
  { "sms_home_detail", "addr_bd_cont_starter_sms_home" },
  { "sms_work_detail", "addr_bd_cont_starter_sms_work" },
  { NULL, NULL }
};

static void
set_title(OssoABookContactDetailSelector *selector)
{
  OssoABookContactDetailSelectorPrivate *priv = PRIVATE(selector);
  OssoABookContact *contact;

  contact = osso_abook_contact_detail_store_get_contact(priv->store);

  if (contact)
  {
    gtk_window_set_title(GTK_WINDOW(selector),
                         osso_abook_contact_get_display_name(contact));
  }
  else
    gtk_window_set_title(GTK_WINDOW(selector), "");
}

static void
field_button_clicked_cb(GtkButton *button,
                        OssoABookContactDetailSelector *selector)
{
  OssoABookContactDetailSelectorPrivate *priv = PRIVATE(selector);

  if (priv->selected_field)
    g_object_unref(priv->selected_field);

  priv->selected_field = g_object_ref(
      g_object_get_data(G_OBJECT(button), "detail-selector-field"));

  gtk_dialog_response(GTK_DIALOG(selector), GTK_RESPONSE_OK);
}

static void
contact_store_changed(OssoABookContactDetailSelector *selector)
{
  OssoABookContactDetailSelectorPrivate *priv = PRIVATE(selector);
  GtkWidget *table;
  GSequence *fields;
  GSequenceIter *iter;
  int last_weight = -1;
  guint top_attach = 0;
  gint rows;

  set_title(selector);

  if (priv->table)
  {
    gtk_widget_destroy(priv->table);
    priv->table = NULL;
  }

  if (!priv->store)
    return;

  fields = osso_abook_contact_detail_store_get_fields(priv->store);

  if (!fields)
    return;

  rows = g_sequence_get_length(fields);
  table = gtk_table_new(rows, 1, FALSE);
  priv->table = table;
  gtk_table_set_col_spacings(GTK_TABLE(priv->table), 8);
  gtk_table_set_row_spacings(GTK_TABLE(priv->table), 8);
  hildon_pannable_area_add_with_viewport(
    HILDON_PANNABLE_AREA(priv->pannable_area), priv->table);
  gtk_widget_show(priv->table);
  iter = g_sequence_get_begin_iter(fields);

  while (!g_sequence_iter_is_end(iter))
  {
    OssoABookContactField *field = g_sequence_get(iter);
    int weight = osso_abook_contact_field_get_sort_weight(field);
    GtkWidget *button = osso_abook_contact_field_create_button(field, NULL,
                                                               NULL);

    g_object_set_data_full(G_OBJECT(button), "detail-selector-field",
                           g_object_ref(field),
                           (GDestroyNotify)&g_object_unref);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(field_button_clicked_cb), selector);
    gtk_widget_show(button);

    if (top_attach && (rows > 7) && ((last_weight / 100) != (weight / 100)))
      gtk_table_set_row_spacing(GTK_TABLE(priv->table), top_attach - 1, 35);

    gtk_table_attach(GTK_TABLE(priv->table), button, 0, 1, top_attach,
                     top_attach + 1, GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
    iter = g_sequence_iter_next(iter);
    last_weight = weight;
    top_attach++;
  }
}

static void
disconnect_store_signals(OssoABookContactDetailSelector *selector)
{
  OssoABookContactDetailSelectorPrivate *priv = PRIVATE(selector);

  if (priv->store)
  {
    g_signal_handlers_disconnect_matched(
      priv->store, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, contact_store_changed, selector);
    g_object_unref(priv->store);
    priv->store = NULL;
  }
}

static void
osso_abook_contact_detail_selector_set_property(GObject *object,
                                                guint property_id,
                                                const GValue *value,
                                                GParamSpec *pspec)
{
  OssoABookContactDetailSelector *selector =
    OSSO_ABOOK_CONTACT_DETAIL_SELECTOR(object);
  OssoABookContactDetailSelectorPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_DETAILS:
    {
      OssoABookContactDetailStore *store = g_value_get_object(value);

      disconnect_store_signals(selector);

      if (store)
      {
        store = g_object_ref(store);
        priv->store = store;
        osso_abook_contact_detail_store_set_message_map(store, store_map);
        g_signal_connect_swapped(priv->store, "changed",
                                 G_CALLBACK(contact_store_changed), selector);
      }

      contact_store_changed(selector);
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_contact_detail_selector_get_property(GObject *object,
                                                guint property_id,
                                                GValue *value,
                                                GParamSpec *pspec)
{
  OssoABookContactDetailSelectorPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_DETAILS:
    {
      g_value_set_object(value, priv->store);
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
osso_abook_contact_detail_selector_dispose(GObject *object)
{
  OssoABookContactDetailSelectorPrivate *priv = PRIVATE(object);

  if (priv->gconf_handler)
  {
    gconf_client_notify_remove(osso_abook_get_gconf_client(),
                               priv->gconf_handler);
    priv->gconf_handler = 0;
  }

  disconnect_store_signals(OSSO_ABOOK_CONTACT_DETAIL_SELECTOR(object));

  if (priv->selected_field)
  {
    g_object_unref(priv->selected_field);
    priv->selected_field = NULL;
  }

  G_OBJECT_CLASS(
    osso_abook_contact_detail_selector_parent_class)->dispose(object);
}

static void
osso_abook_contact_detail_selector_class_init(
  OssoABookContactDetailSelectorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_contact_detail_selector_set_property;
  object_class->get_property = osso_abook_contact_detail_selector_get_property;
  object_class->dispose = osso_abook_contact_detail_selector_dispose;

  g_object_class_install_property(
    object_class, PROP_DETAILS,
    g_param_spec_object(
      "details",
      "The contact detail store",
      "The contact detail store",
      OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE,
      GTK_PARAM_READWRITE));
}

static void
name_order_changed_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry,
                      gpointer user_data)
{
  set_title(OSSO_ABOOK_CONTACT_DETAIL_SELECTOR(user_data));
}

static void
osso_abook_contact_detail_selector_init(
  OssoABookContactDetailSelector *selector)
{
  OssoABookContactDetailSelectorPrivate *priv = PRIVATE(selector);

  priv->pannable_area = osso_abook_pannable_area_new();
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(selector)->vbox), priv->pannable_area,
                     TRUE, TRUE, 0);
  gtk_widget_show(priv->pannable_area);
  gtk_widget_hide(GTK_DIALOG(selector)->action_area);
  priv->gconf_handler =
    gconf_client_notify_add(osso_abook_get_gconf_client(),
                            OSSO_ABOOK_SETTINGS_KEY_NAME_ORDER,
                            name_order_changed_cb, selector, NULL, NULL);
}

GtkWidget *
osso_abook_contact_detail_selector_new(GtkWindow *parent,
                                       OssoABookContactDetailStore *details)
{
  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), NULL);
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(details), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_CONTACT_DETAIL_SELECTOR,
                      "details", details,
                      "transient-for", parent,
                      "modal", TRUE,
                      "destroy-with-parent", TRUE,
                      NULL);
}

GtkWidget *
osso_abook_contact_detail_selector_new_for_contact(
  GtkWindow *parent, OssoABookContact *contact,
  OssoABookContactDetailFilters filters)
{
  OssoABookContactDetailStore *details;
  GtkWidget *selector;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);

  details = osso_abook_contact_detail_store_new(contact, filters);
  selector = osso_abook_contact_detail_selector_new(parent, details);
  g_object_unref(details);

  return selector;
}

EVCardAttribute *
osso_abook_contact_detail_selector_get_detail(
  OssoABookContactDetailSelector *self)
{
  OssoABookContactField *selected_field;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_SELECTOR(self), NULL);

  selected_field = PRIVATE(self)->selected_field;

  if (selected_field)
    return osso_abook_contact_field_get_attribute(selected_field);

  return NULL;
}

OssoABookContactField *
osso_abook_contact_detail_selector_get_selected_field(
  OssoABookContactDetailSelector *self)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_DETAIL_SELECTOR(self), NULL);

  return PRIVATE(self)->selected_field;
}
