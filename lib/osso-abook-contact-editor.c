/*
 * osso-abook-contact-editor.c
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

#include "osso-abook-contact-editor.h"

#include "osso-abook-contact-field-selector.h"
#include "osso-abook-contact-field.h"
#include "osso-abook-contact.h"
#include "osso-abook-dialogs.h"
#include "osso-abook-enums.h"
#include "osso-abook-marshal.h"
#include "osso-abook-message-map.h"
#include "osso-abook-string-list.h"
#include "osso-abook-util.h"
#include "osso-abook-utils-private.h"

struct _OssoABookContactEditorPrivate
{
  GHashTable *attributes;
  OssoABookContact *contact;
  OssoABookContactEditorMode mode;
  GSequence *rows;
  GSequence *fields;
  GtkWidget *table;
  OssoABookContactField *current_field;
  OssoABookContactField *photo_field;
  OssoABookContactField *name_field;
  GHashTable *message_map;
  GtkSizeGroup *editor_size_group;
  GtkSizeGroup *size_group;
  GtkWidget *field_remove_button;
  GtkWidget *pannable_area;
  OssoABookRoster *roster;
  guint jump_to_child_id;
  gint minimum_label_width;
  guint save_idle_id;
};

typedef struct _OssoABookContactEditorPrivate OssoABookContactEditorPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookContactEditor,
  osso_abook_contact_editor,
  GTK_TYPE_DIALOG
);

#define PRIVATE(editor) \
  ((OssoABookContactEditorPrivate *) \
   osso_abook_contact_editor_get_instance_private(((OssoABookContactEditor *) \
                                                   editor)))

enum
{
  PROP_CONTACT = 0x1,
  PROP_MODE = 0x2
};

enum
{
  CONTACT_SAVED,
  CONTACT_DELETED,
  LAST_SIGNAL
};

struct contact_editor_row
{
  OssoABookContactField *field;
  GtkWidget *label;
  GtkWidget *editor;
};

struct managed_attribute
{
  EVCardAttribute *evcattr;
  gboolean free_evcattr;
};

struct async_data
{
  OssoABookContactEditor *editor;
  OssoABookContact *contact;
};

static guint signals[LAST_SIGNAL] = {};

static OssoABookMessageMapping contact_editor_map[] =
{
  { "", "addr_fi_newedit" },
  { "name", "" },
  { "first_name", "addr_fi_newedit_first_name" },
  { "last_name", "addr_fi_newedit_last_name" },
  { "nickname", "addr_fi_newedit_nickname" },
  { "image", "" },
  { "mobile", "addr_bd_newedit_phone_mobile" },
  { "mobile_home", "addr_bd_newedit_phone_mobile" },
  { "mobile_work", "addr_bd_newedit_phone_mobile" },
  { "phone", "addr_bd_newedit_phone" },
  { "phone_home", "addr_bd_newedit_phone" },
  { "phone_work", "addr_bd_newedit_phone" },
  { "phone_other", "addr_bd_newedit_phone" },
  { "phone_fax", "addr_bd_newedit_phone" },
  { "email", "addr_bd_newedit_email" },
  { "email_home", "addr_bd_newedit_email" },
  { "email_work", "addr_bd_newedit_email" },
  { "chat_sip", "addr_fi_newedit_sip_chat" },
  { "webpage", "addr_fi_newedit_webpage" },
  { "birthday", "addr_bd_newedit_birthday" },
  { "address", "addr_bd_newedit_address" },
  { "address_home", "addr_bd_newedit_address" },
  { "address_work", "addr_bd_newedit_address" },
  { "address_extension", "addr_fi_newedit_address_extension" },
  { "address_street", "addr_fi_newedit_address_street" },
  { "address_p_o_box", "addr_fi_newedit_address_p_o_box" },
  { "address_city", "addr_fi_newedit_address_city" },
  { "address_postal", "addr_fi_newedit_address_postal" },
  { "address_region", "addr_fi_newedit_address_region" },
  { "address_country", "addr_bd_newedit_address_country" },
  { "title", "addr_fi_newedit_title" },
  { "company", "addr_fi_newedit_company" },
  { "note", "addr_fi_newedit_note" },
  { "gender", "addr_bd_newedit_gender" },
  { "address_detail", "" },
  { "address_home_detail", "addr_va_newedit_address_home" },
  { "address_work_detail", "addr_va_newedit_address_work" },
  { "email_detail", "" },
  { "email_home_detail", "addr_va_newedit_email_home" },
  { "email_work_detail", "addr_va_newedit_email_work" },
  { "mobile_detail", "" },
  { "mobile_home_detail", "addr_va_newedit_mobile_home" },
  { "mobile_work_detail", "addr_va_newedit_mobile_work" },
  { "phone_detail", "" },
  { "phone_home_detail", "addr_va_newedit_phone_home" },
  { "phone_work_detail", "addr_va_newedit_phone_work" },
  { "phone_other_detail", "addr_va_newedit_phone_other" },
  { "phone_fax_detail", "addr_va_newedit_phone_fax" },
  { NULL, NULL }
};

static void
managed_attr_free(struct managed_attribute *attr)
{
  if (!attr)
    return;

  if (attr->free_evcattr)
    e_vcard_attribute_free(attr->evcattr);

  g_free(attr);
}

static GList *
managed_attr_list_prepend(GList *list, EVCardAttribute *evcattr, gboolean flag)
{
  struct managed_attribute *attr;

  g_return_val_if_fail(evcattr, list);

  attr = g_new(struct managed_attribute, 1);
  attr->evcattr = evcattr;
  attr->free_evcattr = flag;
  list = g_list_prepend(list, attr);

  return list;
}

static gboolean
jump_to_child_cb(gpointer user_data)
{
  OssoABookContactEditor *editor = user_data;
  OssoABookContactEditorPrivate *priv;
  GtkWidget *focus_widget;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_EDITOR(editor), FALSE);

  priv = PRIVATE(editor);
  priv->jump_to_child_id = 0;
  focus_widget = gtk_window_get_focus(GTK_WINDOW(editor));

  if (focus_widget && gtk_widget_get_realized(focus_widget))
  {
    hildon_pannable_area_scroll_to_child(
      HILDON_PANNABLE_AREA(priv->pannable_area), focus_widget);
  }

  return FALSE;
}

static void
set_focus_cb(GtkWindow *editor, GtkWidget *widget, gpointer user_data)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  GtkWidget *i;
  GtkWidget *parent;
  struct contact_editor_row *row;
  gint pos;

  if (!widget || !gtk_widget_is_ancestor(widget, priv->table))
    return;

  for (i = widget;; i = gtk_widget_get_parent(i))
  {
    parent = gtk_widget_get_parent(i);

    if (parent == priv->table)
      break;
  }

  gtk_container_child_get(GTK_CONTAINER(parent), i, "top-attach", &pos, NULL);
  row = g_sequence_get(g_sequence_get_iter_at_pos(priv->rows, pos));

  if (row)
  {
    priv->current_field = row->field;

    if (priv->jump_to_child_id)
    {
      g_source_remove(priv->jump_to_child_id);
      priv->jump_to_child_id = 0;
    }

    if (GTK_IS_WIDGET(widget))
    {
      priv->jump_to_child_id =
        gdk_threads_add_idle_full(200, jump_to_child_cb, g_object_ref(editor),
                                  (GDestroyNotify)g_object_unref);
    }
  }
}

static void
cleanup(OssoABookContactEditorPrivate *priv)
{
  if (priv->attributes)
    g_hash_table_remove_all(priv->attributes);

  if (priv->editor_size_group)
  {
    g_object_unref(priv->editor_size_group);
    priv->editor_size_group = NULL;
  }

  if (priv->fields)
  {
    g_sequence_free(priv->fields);
    priv->fields = NULL;
  }

  if (priv->rows)
  {
    g_sequence_free(priv->rows);
    priv->rows = NULL;
  }

  priv->current_field = NULL;
}

static void
osso_abook_contact_editor_set_property(GObject *object, guint property_id,
                                       const GValue *value, GParamSpec *pspec)
{
  OssoABookContactEditor *editor = OSSO_ABOOK_CONTACT_EDITOR(object);

  switch (property_id)
  {
    case PROP_CONTACT:
    {
      osso_abook_contact_editor_set_contact(editor, g_value_get_object(value));
      break;
    }
    case PROP_MODE:
    {
      osso_abook_contact_editor_set_mode(editor, g_value_get_enum(value));
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
osso_abook_contact_editor_get_property(GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_CONTACT:
    {
      g_value_set_object(value, priv->contact);
      break;
    }
    case PROP_MODE:
    {
      g_value_set_enum(value, priv->mode);
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
_roster_master_contacts_changed_cb(OssoABookRoster *roster,
                                   OssoABookContact **contacts,
                                   gpointer user_data)
{
  OssoABookContactEditor *editor = user_data;
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  const char *uid;

  uid = e_contact_get_const(E_CONTACT(priv->contact), E_CONTACT_UID);

  if (!uid || priv->save_idle_id)
    return;

  while (*contacts)
  {
    if (!strcmp(uid, e_contact_get_const(E_CONTACT(*contacts), E_CONTACT_UID)))
    {
      gtk_dialog_response(GTK_DIALOG(editor), GTK_RESPONSE_CLOSE);
      break;
    }

    contacts++;
  }
}

static void
_roster_contacts_removed_cb(OssoABookRoster *roster, const char **uids,
                            gpointer user_data)
{
  OssoABookContactEditor *editor = user_data;
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  const char *uid;

  uid = e_contact_get_const(E_CONTACT(priv->contact), E_CONTACT_UID);

  while (*uids)
  {
    if (!strcmp(uid, *uids))
    {
      gtk_dialog_response(GTK_DIALOG(editor), GTK_RESPONSE_CLOSE);
      break;
    }

    uids++;
  }
}

static void
set_roster(OssoABookContactEditor *editor, OssoABookRoster *roster)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);

  if (roster)
    g_object_ref(roster);

  if (priv->roster)
  {
    g_signal_handlers_disconnect_matched(
      priv->roster, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      _roster_master_contacts_changed_cb, editor);
    g_signal_handlers_disconnect_matched(
      priv->roster, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
      _roster_contacts_removed_cb, editor);
    g_object_unref(priv->roster);
  }

  priv->roster = roster;

  if (roster)
  {
    g_signal_connect(roster, "contacts-changed::master",
                     G_CALLBACK(_roster_master_contacts_changed_cb), editor);
    g_signal_connect(priv->roster, "contacts-removed",
                     G_CALLBACK(_roster_contacts_removed_cb), editor);
  }
}

static void
set_contact(OssoABookContactEditor *editor, OssoABookContact *contact)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  OssoABookRoster *roster = NULL;

  if (priv->contact != contact)
  {
    if (priv->contact)
    {
      g_object_unref(priv->contact);
      priv->contact = NULL;
    }

    if (contact)
    {
      priv->contact = g_object_ref(contact);
      roster = osso_abook_contact_get_roster(priv->contact);
    }

    set_roster(editor, roster);
  }
}

static void
osso_abook_contact_editor_dispose(GObject *object)
{
  OssoABookContactEditor *editor = OSSO_ABOOK_CONTACT_EDITOR(object);
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);

  g_signal_handlers_disconnect_matched(
    editor, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
    set_focus_cb, NULL);
  set_contact(editor, NULL);
  set_roster(editor, NULL);
  cleanup(priv);

  if (priv->jump_to_child_id)
  {
    g_source_remove(priv->jump_to_child_id);
    priv->jump_to_child_id = 0;
  }

  if (priv->message_map)
  {
    g_hash_table_unref(priv->message_map);
    priv->message_map = NULL;
  }

  if (priv->attributes)
  {
    g_hash_table_unref(priv->attributes);
    priv->attributes = NULL;
  }

  if (priv->size_group)
  {
    g_object_unref(priv->size_group);
    priv->size_group = NULL;
  }

  G_OBJECT_CLASS(osso_abook_contact_editor_parent_class)->dispose(object);
}

static void
update_sizes(OssoABookContactEditor *editor)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  GtkWidget *action_area;
  gint width;
  GSequenceIter *rows_iter;
  GtkRequisition size;
  gint content_border_width;
  gint action_border_width;
  gint spacing;

  if (!priv->rows)
    return;

  action_area = GTK_DIALOG(editor)->action_area;
  g_object_get(action_area->parent, "spacing", &spacing, NULL);
  gtk_widget_size_request(action_area, &size);
  gtk_widget_style_get(GTK_WIDGET(editor),
                       "action-area-border", &action_border_width,
                       "content-area-border", &content_border_width,
                       NULL);

  width = ((GTK_WIDGET(editor)->requisition.width - size.width - spacing -
            2 * action_border_width - 2 * content_border_width - 40) / 3) - 8;

  if (width < priv->minimum_label_width)
    width = priv->minimum_label_width;

  rows_iter = g_sequence_get_begin_iter(priv->rows);

  while (!g_sequence_iter_is_end(rows_iter))
  {
    struct contact_editor_row *row = g_sequence_get(rows_iter);
    gint height;

    if (row->field != priv->photo_field)
    {
      if (row->label)
      {
        gtk_widget_get_size_request(row->label, NULL, &height);
        gtk_widget_set_size_request(row->label, width, height);
      }
    }

    if (row->editor)
    {
      gtk_widget_get_size_request(row->editor, NULL, &height);
      gtk_widget_set_size_request(row->editor, width, height);
    }

    rows_iter = g_sequence_iter_next(rows_iter);
  }
}

static void
osso_abook_contact_editor_style_set(GtkWidget *widget, GtkStyle *previous_style)
{
  GTK_WIDGET_CLASS(osso_abook_contact_editor_parent_class)->
  style_set(widget, previous_style);

  if (gtk_widget_get_realized(widget))
    update_sizes(OSSO_ABOOK_CONTACT_EDITOR(widget));
}

static void
osso_abook_contact_editor_size_request(GtkWidget *widget,
                                       GtkRequisition *requisition)
{
  GTK_WIDGET_CLASS(osso_abook_contact_editor_parent_class)->
  size_request(widget, requisition);

  requisition->width = gdk_screen_get_width(gtk_widget_get_screen(widget));
}

static void
attach_photo_editor(OssoABookContactEditorPrivate *priv)
{
  GSequenceIter *iter;
  gint photo_bottom = G_MININT;
  gint photo_top = G_MAXINT;
  GtkWidget *editor_widget;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(priv->photo_field));
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(priv->name_field));

  iter = g_sequence_get_begin_iter(priv->rows);

  while (!g_sequence_iter_is_end(iter))
  {
    struct contact_editor_row *row = g_sequence_get(iter);

    iter = g_sequence_iter_next(iter);

    if (osso_abook_contact_field_get_parent(row->field) == priv->name_field)
    {
      gint editor_top_attach;
      gint editor_bottom_attach;

      gtk_container_child_get(
        GTK_CONTAINER(priv->table),
        row->editor,
        "top-attach",
        &editor_top_attach,
        "bottom-attach",
        &editor_bottom_attach,
        NULL);

      if (photo_top >= editor_top_attach)
        photo_top = editor_top_attach;

      if (photo_bottom < editor_bottom_attach)
        photo_bottom = editor_bottom_attach;
    }
  }

  g_return_if_fail(photo_bottom < G_MAXINT);
  g_return_if_fail(photo_top < G_MAXINT);

  editor_widget = osso_abook_contact_field_get_editor_widget(priv->photo_field);
  gtk_table_attach(GTK_TABLE(priv->table), editor_widget, 0, 1, photo_top,
                   photo_bottom, GTK_FILL, GTK_FILL, 0, 0);
  gtk_size_group_add_widget(priv->editor_size_group, editor_widget);
  gtk_widget_show(editor_widget);
}

static gboolean
is_create_mode(OssoABookContactEditorPrivate *priv)
{
  switch (priv->mode)
  {
    case OSSO_ABOOK_CONTACT_EDITOR_CREATE:
    case OSSO_ABOOK_CONTACT_EDITOR_CREATE_SELF:
    {
      return TRUE;
    }
    case OSSO_ABOOK_CONTACT_EDITOR_EDIT:
    case OSSO_ABOOK_CONTACT_EDITOR_EDIT_SELF:
    {
      return FALSE;
    }
  }

  g_return_val_if_reached(FALSE);
}

static gboolean
is_self_mode(OssoABookContactEditorPrivate *priv)
{
  switch (priv->mode)
  {
    case OSSO_ABOOK_CONTACT_EDITOR_CREATE:
    case OSSO_ABOOK_CONTACT_EDITOR_EDIT:
    {
      return FALSE;
    }
    case OSSO_ABOOK_CONTACT_EDITOR_CREATE_SELF:
    case OSSO_ABOOK_CONTACT_EDITOR_EDIT_SELF:
    {
      return TRUE;
    }
  }

  g_return_val_if_reached(FALSE);
}

static GList *
get_attr_list(OssoABookContactEditorPrivate *priv)
{
  GList *attr_list = NULL;
  gboolean n_is_missing = TRUE;
  gboolean email_is_missing = TRUE;
  gboolean photo_is_missing = TRUE;
  gboolean tel_is_missing = TRUE;

  if (priv->contact)
  {
    GList *attrs;
    GList *l;

    email_is_missing = tel_is_missing = is_create_mode(priv);

    for (attrs = e_vcard_get_attributes(E_VCARD(priv->contact)); attrs;
         attrs = attrs->next)
    {
      attr_list = managed_attr_list_prepend(attr_list, attrs->data, FALSE);
    }

    for (l = attr_list; l; l = l->next)
    {
      EVCardAttribute *attr = ((struct managed_attribute *)(l->data))->evcattr;
      const char *name = e_vcard_attribute_get_name(attr);

      if (!g_ascii_strcasecmp(name, EVC_PHOTO))
        photo_is_missing = FALSE;
      else if (!g_ascii_strcasecmp(name, EVC_N))
        n_is_missing = FALSE;
      else if (!g_ascii_strcasecmp(name, EVC_EMAIL))
        email_is_missing = FALSE;
      else if (!g_ascii_strcasecmp(name, EVC_TEL))
      {
        tel_is_missing = FALSE;

        if (!e_vcard_attribute_get_param(attr, EVC_TYPE))
        {
          EVCardAttributeParam *param = e_vcard_attribute_param_new(EVC_TYPE);

          e_vcard_attribute_param_add_value(param, "CELL");
          e_vcard_attribute_param_add_value(param, "VOICE");
          e_vcard_attribute_add_param(attr, param);
        }
      }
    }
  }

  if (n_is_missing)
  {
    EVCardAttribute *attr;

    OSSO_ABOOK_NOTE(EDITOR, "adding missing N attribute");

    attr = e_vcard_attribute_new(0, EVC_N);
    attr_list = managed_attr_list_prepend(attr_list, attr, TRUE);

    if (priv->contact)
    {
      char *primary = NULL;
      char *secondary = NULL;

      osso_abook_contact_get_name_components(E_CONTACT(priv->contact),
                                             OSSO_ABOOK_NAME_ORDER_LAST, TRUE,
                                             &primary, &secondary);

      e_vcard_attribute_add_value(attr, primary ? primary : "");
      e_vcard_attribute_add_value(attr, secondary ? secondary : "");
      e_vcard_attribute_add_value(attr, "");
      e_vcard_attribute_add_value(attr, "");
      e_vcard_attribute_add_value(attr, "");
      g_free(secondary);
      g_free(primary);
    }
  }

  if (photo_is_missing)
  {
    OSSO_ABOOK_NOTE(EDITOR, "adding missing PHOTO attribute");
    attr_list = managed_attr_list_prepend(
      attr_list, e_vcard_attribute_new(NULL, EVC_PHOTO), TRUE);
  }

  if (email_is_missing)
  {
    OSSO_ABOOK_NOTE(EDITOR, "adding missing EMAIL attribute");
    attr_list = managed_attr_list_prepend(
      attr_list, e_vcard_attribute_new(NULL, EVC_EMAIL), TRUE);
  }

  if (tel_is_missing)
  {
    EVCardAttributeParam *param;
    EVCardAttribute *attr;

    OSSO_ABOOK_NOTE(EDITOR, "adding missing TEL attribute");

    param = e_vcard_attribute_param_new(EVC_TYPE);
    e_vcard_attribute_param_add_value(param, "CELL");
    e_vcard_attribute_param_add_value(param, "VOICE");
    attr = e_vcard_attribute_new(NULL, EVC_TEL);
    e_vcard_attribute_add_param(attr, param);
    attr_list = managed_attr_list_prepend(attr_list, attr, TRUE);
  }

  return attr_list;
}

static void
remove_editor_widget(GtkWidget *widget, GtkContainer *table)
{
  gtk_container_remove(table, widget);
}

static void
contact_editor_row_free(struct contact_editor_row *row)
{
  if (!row)
    return;

  if (row->field)
    g_object_unref(row->field);

  g_slice_free(struct contact_editor_row, row);
}

static gboolean
field_is_removable(OssoABookContactField *field)
{
  return !osso_abook_contact_field_is_mandatory(field) &&
         !osso_abook_contact_field_is_readonly(field);
}

static void
update_actions(OssoABookContactEditor *editor)
{
  OssoABookContactEditorPrivate *priv;
  GSequenceIter *i;

  g_return_if_fail(NULL != editor);

  priv = PRIVATE(editor);

  for (i = g_sequence_get_begin_iter(priv->rows);
       !g_sequence_iter_is_end(i); i = g_sequence_iter_next(i))
  {
    struct contact_editor_row *row = g_sequence_get(i);

    if (field_is_removable(row->field))
    {
      gtk_widget_show(priv->field_remove_button);
      return;
    }
  }

  gtk_widget_hide(priv->field_remove_button);
}

static void
update_field_positions(OssoABookContactEditor *editor)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  GSequenceIter *i;

  for (i = g_sequence_get_begin_iter(priv->rows); !g_sequence_iter_is_end(i);
       i = g_sequence_iter_next(i))
  {
    gint pos = g_sequence_iter_get_position(i);
    struct contact_editor_row *row = g_sequence_get(i);

    OSSO_ABOOK_NOTE(GTK, "moving %s row to %d",
                    osso_abook_contact_field_get_path(row->field), pos);

    if (row->label)
    {
      gtk_container_child_set(GTK_CONTAINER(priv->table), row->label,
                              "top-attach", pos, "bottom-attach", pos + 1,
                              NULL);
    }

    if (row->editor)
    {
      gtk_container_child_set(GTK_CONTAINER(priv->table), row->editor,
                              "top-attach", pos, "bottom-attach", pos + 1,
                              NULL);
    }
  }

  update_actions(editor);
}

static gboolean
grab_editor_widget_focus(GtkWidget *widget)
{
  if (gtk_widget_get_can_focus(widget))
  {
    gtk_widget_grab_focus(widget);
    return TRUE;
  }
  else if (GTK_IS_CONTAINER(widget))
  {
    GList *children;

    for (children = gtk_container_get_children(GTK_CONTAINER(widget));
         children; children = g_list_delete_link(children, children))
    {
      if (grab_editor_widget_focus(children->data))
      {
        g_list_free(children);
        return TRUE;
      }
    }
  }

  return FALSE;
}

static void
grab_focus(GSequenceIter *iter)
{
  while (!g_sequence_iter_is_end(iter))
  {
    struct contact_editor_row *row = g_sequence_get(iter);
    GtkWidget *editor = row->editor;

    if (editor)
    {
      if (grab_editor_widget_focus(editor))
        break;
    }

    iter = g_sequence_iter_next(iter);
  }
}

static int
row_compare(gconstpointer a, gconstpointer b, gpointer user_data)
{
  return osso_abook_contact_field_cmp(
    ((struct contact_editor_row *)a)->field,
    ((struct contact_editor_row *)b)->field);
}

static GSequenceIter *
attach_row_widgets(OssoABookContactEditorPrivate *priv,
                   OssoABookContactField *field)
{
  OssoABookContactField *parent = osso_abook_contact_field_get_parent(field);
  GList *children = osso_abook_contact_field_get_children(field);
  struct contact_editor_row *row = g_slice_new0(struct contact_editor_row);
  GSequenceIter *sorted = NULL;
  const gchar *button_title;
  GList *l;

  row->editor = osso_abook_contact_field_get_editor_widget(field);
  row->field = g_object_ref(field);

  if (HILDON_IS_BUTTON(row->editor) &&
      (button_title = hildon_button_get_title(HILDON_BUTTON(row->editor))) &&
      !IS_EMPTY(button_title))
  {
    row->label = row->editor;
  }
  else if (!priv->name_field || (parent != priv->name_field))
    row->label = osso_abook_contact_field_get_label_widget(field);

  if (!row->editor)
    row->editor = row->label;

  OSSO_ABOOK_NOTE(GTK, "attaching widgets for %s row: label=%p, editor=%p",
                  osso_abook_contact_field_get_path(row->field), row->label,
                  row->editor);

  if (!row->label && !row->editor)
    contact_editor_row_free(row);
  else
  {
    sorted = g_sequence_insert_sorted(priv->rows, row, row_compare, 0);
    gint pos = g_sequence_iter_get_position(sorted);

    if (HILDON_IS_BUTTON(row->label) && (row->label == row->editor))
    {
      hildon_button_add_title_size_group(HILDON_BUTTON(row->editor),
                                         priv->editor_size_group);
    }
    else if (row->label && (row->label != row->editor))
      gtk_size_group_add_widget(priv->editor_size_group, row->label);

    if (row->label == row->editor)
    {
      gtk_table_attach(GTK_TABLE(priv->table), row->label, 0, 2, pos, pos + 1,
                       GTK_FILL, GTK_FILL, 0, 0);
    }
    else
    {
      gtk_table_attach(GTK_TABLE(priv->table), row->editor, 1, 2, pos, pos + 1,
                       GTK_FILL | GTK_EXPAND, GTK_FILL, 0, 0);
    }

    if (row->label && (row->label != row->editor))
    {
      gtk_table_attach(GTK_TABLE(priv->table), row->label, 0, 1, pos, pos + 1,
                       GTK_FILL, GTK_FILL, 0, 0);
      gtk_widget_show(row->label);
    }

    gtk_widget_show(row->editor);
  }

  for (l = children; l; l = l->next)
  {
    GSequenceIter *iter = attach_row_widgets(priv, l->data);

    if (!sorted)
      sorted = iter;
  }

  return sorted;
}

static void
update_fields(OssoABookContactEditor *editor)
{
  gchar *title;
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  GList *attr_list;
  GList *roster_contacts = NULL;

  if (!osso_abook_screen_is_landscape_mode(
        gtk_widget_get_screen(GTK_WIDGET(editor))))
  {
    return;
  }

  switch (priv->mode)
  {
    case OSSO_ABOOK_CONTACT_EDITOR_CREATE:
    {
      title = g_strdup(_("addr_ti_newedit_new_title"));
      break;
    }
    case OSSO_ABOOK_CONTACT_EDITOR_EDIT:
    {
      title = g_strdup_printf(
        _("addr_ti_newedit_edit_title"),
        osso_abook_contact_get_display_name(priv->contact));
      break;
    }
    case OSSO_ABOOK_CONTACT_EDITOR_CREATE_SELF:
    {
      title = g_strdup(_("addr_ti_mecard_title_add_my_information"));
      break;
    }
    case OSSO_ABOOK_CONTACT_EDITOR_EDIT_SELF:
    {
      title = g_strdup(_("addr_ti_newedit_edit_my_information_title"));
      break;
    }
    default:
    {
      title = NULL;
      break;
    }
  }

  gtk_window_set_title(GTK_WINDOW(editor), title);
  g_free(title);

  gtk_container_foreach(GTK_CONTAINER(priv->table),
                        (GtkCallback)remove_editor_widget, priv->table);
  cleanup(priv);
  priv->editor_size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
  priv->rows = g_sequence_new((GDestroyNotify)contact_editor_row_free);
  priv->photo_field = NULL;
  priv->name_field = NULL;
  priv->fields = g_sequence_new((GDestroyNotify)&g_object_unref);
  ;

  for (attr_list = get_attr_list(priv); attr_list;
       attr_list = g_list_delete_link(attr_list, attr_list))
  {
    struct managed_attribute *managed_attr = attr_list->data;
    EVCardAttribute *evcattr = managed_attr->evcattr;

    if (osso_abook_contact_attribute_is_readonly(evcattr) &&
        !is_self_mode(priv))
    {
      managed_attr_free(managed_attr);
    }
    else
    {
      OssoABookContact *roster_contact = NULL;
      OssoABookContactField *field;

      if (priv->contact)
      {
        GList *l = osso_abook_contact_find_roster_contacts_for_attribute(
          priv->contact, evcattr);

        if (l)
        {
          while (g_list_find(roster_contacts, l->data))
          {
            l = g_list_delete_link(l, l);

            if (!l)
              break;
          }

          if (l)
          {
            roster_contact = l->data;
            roster_contacts = g_list_prepend(roster_contacts, roster_contact);
            g_list_free(l);
          }
        }
      }

      field = osso_abook_contact_field_new_full(priv->message_map,
                                                priv->contact, roster_contact,
                                                evcattr);
      g_hash_table_insert(priv->attributes, evcattr, managed_attr);

      if (field)
      {
        gboolean has_children = osso_abook_contact_field_has_children(field);
        gboolean has_editor_widget =
          osso_abook_contact_field_has_editor_widget(field);
        const char *title = osso_abook_contact_field_get_display_title(field);
        const char *field_name = osso_abook_contact_field_get_name(field);
        gboolean skip = FALSE;

        if (IS_EMPTY(title) && !(has_editor_widget || has_children))
        {
          OSSO_ABOOK_NOTE(EDITOR,
                          "skipping %s (children=%s, title=%s, editor=%p)",
                          osso_abook_contact_field_get_path(field),
                          has_children ? "yes" : "no",
                          title ? title : "",
                          editor);
          skip = TRUE;
        }
        else
        {
          if (!strcmp(field_name, EVC_N))
          {
            if (!priv->name_field)
              priv->name_field = field;
            else
              skip = TRUE;
          }

          if (!strcmp(field_name, EVC_PHOTO))
          {
            if (!priv->photo_field)
              priv->photo_field = field;
            else
              skip = TRUE;
          }
          else
            attach_row_widgets(priv, field);
        }

        if (!skip)
          g_sequence_append(priv->fields, field);
        else
          g_object_unref(field);
      }
    }
  }

  g_list_free(roster_contacts);

  if (priv->mode == OSSO_ABOOK_CONTACT_EDITOR_CREATE)
  {
    GList *l;

    for (l = osso_abook_contact_field_list_account_fields(priv->message_map,
                                                          priv->contact); l;
         l = g_list_delete_link(l, l))
    {
      g_sequence_append(priv->fields, l->data);
      attach_row_widgets(priv, l->data);
    }
  }

  update_field_positions(editor);
  attach_photo_editor(priv);

  grab_focus(g_sequence_get_begin_iter(priv->rows));
  gtk_widget_queue_resize(gtk_widget_get_parent(priv->table));

  update_sizes(editor);
}

static void
osso_abook_contact_editor_realize(GtkWidget *widget)
{
  GTK_WIDGET_CLASS(osso_abook_contact_editor_parent_class)->realize(widget);

  update_fields(OSSO_ABOOK_CONTACT_EDITOR(widget));
}

static void
osso_abook_contact_editor_size_allocate(GtkWidget *widget,
                                        GtkAllocation *allocation)
{
  OssoABookContactEditor *editor = OSSO_ABOOK_CONTACT_EDITOR(widget);
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);

  GTK_WIDGET_CLASS(osso_abook_contact_editor_parent_class)->
  size_allocate(widget, allocation);

  if (gtk_widget_get_realized(widget) && !priv->rows)
    update_fields(editor);
}

static void
osso_abook_contact_editor_class_init(OssoABookContactEditorClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

  object_class->set_property = osso_abook_contact_editor_set_property;
  object_class->get_property = osso_abook_contact_editor_get_property;
  object_class->dispose = osso_abook_contact_editor_dispose;

  widget_class->realize = osso_abook_contact_editor_realize;
  widget_class->style_set = osso_abook_contact_editor_style_set;
  widget_class->size_allocate = osso_abook_contact_editor_size_allocate;
  widget_class->size_request = osso_abook_contact_editor_size_request;

  g_object_class_install_property(
    object_class, PROP_CONTACT,
    g_param_spec_object(
      "contact", "Contact", "The displayed contact",
      OSSO_ABOOK_TYPE_CONTACT,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_MODE,
    g_param_spec_enum(
      "mode", "Mode", "Operation mode of the dialog",
      OSSO_ABOOK_TYPE_CONTACT_EDITOR_MODE,
      OSSO_ABOOK_CONTACT_EDITOR_EDIT,
      GTK_PARAM_READWRITE));

  signals[CONTACT_SAVED] =
    g_signal_new("contact-saved",
                 OSSO_ABOOK_TYPE_CONTACT_EDITOR, G_SIGNAL_RUN_LAST,
                 G_STRUCT_OFFSET(OssoABookContactEditorClass, contact_saved),
                 g_signal_accumulator_true_handled, NULL,
                 osso_abook_marshal_BOOLEAN__STRING,
                 G_TYPE_BOOLEAN,
                 1, G_TYPE_STRING);
  signals[CONTACT_DELETED] =
    g_signal_new("contact-deleted",
                 OSSO_ABOOK_TYPE_CONTACT_EDITOR, G_SIGNAL_RUN_LAST,
                 0,
                 NULL, NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void
set_progress_indicator(OssoABookContactEditor *editor, guint state)
{
  hildon_gtk_window_set_progress_indicator(GTK_WINDOW(editor), state);
  gtk_widget_set_sensitive(gtk_bin_get_child(GTK_BIN(editor)), !state);
}

static gboolean
vcard_is_empty(EVCard *vcard)
{
  GList *attrs;

  for (attrs = e_vcard_get_attributes(vcard); attrs; attrs = attrs->next)
  {
    const char *attr_name = e_vcard_attribute_get_name(attrs->data);

    g_return_val_if_fail(attr_name &&
                         !g_ascii_strcasecmp(attr_name, EVC_UID), FALSE);
  }

  return TRUE;
}

static OssoABookContact *
create_contact(OssoABookContactEditorPrivate *priv)
{
  GList *attrs = NULL;
  GType otype = OSSO_ABOOK_TYPE_CONTACT;
  OssoABookContact *contact;
  GSequenceIter *it;
  gboolean no_version = TRUE;

  if (priv->contact)
  {
    otype = G_OBJECT_TYPE(priv->contact);
    attrs = g_list_copy(e_vcard_get_attributes(E_VCARD(priv->contact)));
  }

  contact = g_object_new(otype, NULL);

  g_return_val_if_fail(contact != priv->contact, NULL);
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);
  g_return_val_if_fail(vcard_is_empty(E_VCARD(contact)), NULL);

  for (it = g_sequence_get_begin_iter(priv->fields);
       !g_sequence_iter_is_end(it); it = g_sequence_iter_next(it))
  {
    OssoABookContactField *field = g_sequence_get(it);
    EVCardAttribute *attr = osso_abook_contact_field_get_attribute(field);
    GList *l;

    if (_osso_abook_e_vcard_attribute_has_value(attr) &&
        !osso_abook_contact_attribute_is_readonly(attr))
    {
      e_vcard_add_attribute(E_VCARD(contact), e_vcard_attribute_copy(attr));
    }

    for (l = osso_abook_contact_field_get_secondary_attributes(field); l;
         l = l->next)
    {
      if (_osso_abook_e_vcard_attribute_has_value(l->data) &&
          !osso_abook_contact_attribute_is_readonly(l->data))
      {
        e_vcard_add_attribute(E_VCARD(contact),
                              e_vcard_attribute_copy(l->data));
      }
    }

    l = g_list_find(
      attrs, osso_abook_contact_field_get_borrowed_attribute(field));

    if (l)
      attrs = g_list_delete_link(attrs, l);
  }

  while (attrs)
  {
    if (g_hash_table_lookup(priv->attributes, attrs->data))
    {
      const char *attr_name = e_vcard_attribute_get_name(attrs->data);

      if (g_ascii_strcasecmp(attr_name, EVC_FN) &&
          g_ascii_strcasecmp(attr_name, EVC_LABEL))
      {
        if (!g_ascii_strcasecmp(attr_name, EVC_VERSION))
          no_version = FALSE;

        if (_osso_abook_e_vcard_attribute_has_value(attrs->data))
        {
          e_vcard_add_attribute(E_VCARD(contact),
                                e_vcard_attribute_copy(attrs->data));
        }
      }
    }

    attrs = g_list_delete_link(attrs, attrs);
  }

  if (no_version)
  {
    e_vcard_add_attribute_with_value(E_VCARD(contact),
                                     e_vcard_attribute_new(NULL, EVC_VERSION),
                                     "3.0");
  }

  OSSO_ABOOK_DUMP_VCARD(EDITOR, contact, "storing");

  return contact;
}

static int
get_contact_attributes_count(OssoABookContactEditor *editor,
                             OssoABookContact *contact)
{
  OssoABookContact *tmp_contact = NULL;
  int count;

  if (!contact)
  {
    tmp_contact = create_contact(PRIVATE(editor));
    contact = tmp_contact;
  }

  count = g_list_length(e_vcard_get_attributes(E_VCARD(contact)));

  if (!e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID))
    count++;

  if (!e_contact_get_const(E_CONTACT(contact), E_CONTACT_REV))
    count++;

  if (tmp_contact)
    g_object_unref(tmp_contact);

  return count;
}

static struct async_data *
create_async_data(OssoABookContactEditor *editor, OssoABookContact *contact)
{
  struct async_data *data = g_slice_new0(struct async_data);

  data->contact = g_object_ref(contact);
  data->editor = g_object_ref(editor);

  return data;
}

static void
update_subscriptions(OssoABookContactEditor *editor, const char *uid)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  GSequenceIter *iter;

  g_return_if_fail(NULL != uid);

  OSSO_ABOOK_NOTE(EDITOR, "updating subscriptions");

  iter = g_sequence_get_begin_iter(priv->fields);

  g_return_if_fail(NULL != iter);

  while (!g_sequence_iter_is_end(iter))
  {
    OssoABookContactField *field = g_sequence_get(iter);
    OssoABookContact *rc;

    iter = g_sequence_iter_next(iter);
    rc = osso_abook_contact_field_get_roster_contact(field);

    if (rc)
    {
      GList *vals;
      EVCardAttribute *attr;
      const char *name = osso_abook_contact_field_get_name(field);
      EVCardAttribute *name_attr;

      OSSO_ABOOK_NOTE(EDITOR, "%s => %p (modified=%d)", name, rc,
                      osso_abook_contact_field_is_modified(field));

      attr = osso_abook_contact_field_get_attribute(field);
      vals = e_vcard_attribute_get_values(attr);
      name_attr = e_vcard_get_attribute(E_VCARD(rc), name);

      if (name_attr)
      {
        if (!osso_abook_string_list_compare(
              e_vcard_attribute_get_values(name_attr), vals))
        {
          goto accept;
        }

        osso_abook_contact_reject_for_uid(rc, uid, GTK_WINDOW(editor));
        e_vcard_remove_attribute(E_VCARD(rc), name_attr);
      }

      if (vals && vals->data)
      {
        e_vcard_add_attribute(E_VCARD(rc), e_vcard_attribute_copy(attr));
accept:
        osso_abook_contact_accept_for_uid(rc, uid, GTK_WINDOW(editor));
      }
    }
  }
}

static void
cleanup_subscriptions(OssoABookContactEditor *editor, const char *uid)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  GList *roster_contacts = NULL;
  GList *l;

  g_return_if_fail(NULL != uid);

  if (priv->contact)
    roster_contacts = osso_abook_contact_get_roster_contacts(priv->contact);

  OSSO_ABOOK_NOTE(EDITOR, "cleaning subscriptions (%d roster contacts)",
                  g_list_length(roster_contacts));

  for (l = roster_contacts; l; l = l->next)
  {
    const char *rc_uid = e_contact_get_const(E_CONTACT(l->data), E_CONTACT_UID);

    if (rc_uid)
    {
      GSequenceIter *iter;

      for (iter = g_sequence_get_begin_iter(priv->fields);
           !g_sequence_iter_is_end(iter); iter = g_sequence_iter_next(iter))
      {
        OssoABookContact *field_rc =
          osso_abook_contact_field_get_roster_contact(g_sequence_get(iter));
        const char *field_uid = NULL;

        if (field_rc)
          field_uid = e_contact_get_const(E_CONTACT(field_rc), E_CONTACT_UID);

        if (field_uid && !strcmp(rc_uid, field_uid))
          break;
      }

      if (g_sequence_iter_is_end(iter))
      {
        OSSO_ABOOK_NOTE(EDITOR, "rejecting %s for %s\n", rc_uid, uid);
        osso_abook_contact_reject_for_uid(l->data, uid, GTK_WINDOW(editor));
      }
    }
  }

  g_list_free(roster_contacts);
}

static void
add_contact_cb(EBook *book, EBookStatus status, const gchar *uid,
               gpointer closure)
{
  struct async_data *data = closure;
  OssoABookContactEditorPrivate *priv = PRIVATE(data->editor);
  gboolean saved = FALSE;

  OSSO_ABOOK_NOTE(EDITOR, "status=%d, uid=%s", status, uid);

  if (status != E_BOOK_ERROR_OK)
  {
    osso_abook_handle_estatus(GTK_WINDOW(data->editor), status, book);
    set_progress_indicator(data->editor, 0);
  }
  else
  {
    if (uid != e_contact_get_const(E_CONTACT(data->contact), E_CONTACT_UID))
      e_contact_set(E_CONTACT(data->contact), E_CONTACT_UID, uid);

    if (!is_self_mode(priv))
    {
      update_subscriptions(data->editor, uid);
      cleanup_subscriptions(data->editor, uid);

      osso_abook_contact_set_roster(data->contact, priv->roster);
      set_contact(data->editor, data->contact);
      g_object_notify(G_OBJECT(data->editor), "contact");
    }

    g_signal_emit(data->editor, signals[CONTACT_SAVED], 0, uid, &saved);
    gtk_dialog_response(GTK_DIALOG(data->editor), GTK_RESPONSE_OK);

    if (!saved)
    {
      hildon_banner_show_information(
        NULL, NULL, dgettext("hildon-common-strings", "sfil_ib_saved"));
    }
  }

  priv->save_idle_id = 0;

  if (data->editor)
    g_object_unref(data->editor);

  if (data->contact)
    g_object_unref(data->contact);

  g_slice_free(struct async_data, data);
}

static void
commit_contact_cb(EBook *book, EBookStatus status, gpointer closure)
{
  struct async_data *data = closure;
  const char *uid = NULL;

  if (data->contact)
    uid = e_contact_get_const(E_CONTACT(data->contact), E_CONTACT_UID);

  add_contact_cb(book, status, uid, data);
}

static void
commit_save(OssoABookContactEditor *editor,
            OssoABookContact *contact)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  EBook *book = NULL;
  const char *uid;
  struct async_data *data;

  if (priv->roster)
    book = osso_abook_roster_get_book(priv->roster);

  uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

  if (is_create_mode(priv) || osso_abook_is_temporary_uid(uid))
    e_contact_set(E_CONTACT(contact), E_CONTACT_UID, NULL);
  else if (uid)
  {
    data = create_async_data(editor, contact);
    osso_abook_contact_async_commit(contact, book, commit_contact_cb, data);
    return;
  }

  data = create_async_data(editor, contact);
  osso_abook_contact_async_add(contact, book, add_contact_cb, data);
}

static gboolean
verify_contact(OssoABookContactEditor *editor, OssoABookContact *contact)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  gboolean all_empty = FALSE;
  const char *msgid = NULL;
  GSequenceIter *it;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), FALSE);

  for (it = g_sequence_get_begin_iter(priv->fields);
       !g_sequence_iter_is_end(it); it = g_sequence_iter_next(it))
  {
    if (!(all_empty = osso_abook_contact_field_is_empty(g_sequence_get(it))))
      break;
  }

  if (all_empty && !is_self_mode(priv))
  {
    if (!is_create_mode(priv))
    {
      if (!osso_abook_delete_contact_dialog_run(GTK_WINDOW(editor),
                                                priv->roster, priv->contact))
      {
        return FALSE;
      }

      g_signal_emit(editor, signals[CONTACT_DELETED], 0);
    }

    gtk_dialog_response(GTK_DIALOG(editor), GTK_RESPONSE_CLOSE);
    return FALSE;
  }

  if (!osso_abook_contact_has_valid_name(contact))
  {
    for (it = g_sequence_get_begin_iter(priv->rows);
         !g_sequence_iter_is_end(it); it = g_sequence_iter_next(it))
    {
      struct contact_editor_row *row = g_sequence_get(it);
      const char *name = osso_abook_contact_field_get_name(row->field);

      if (!name)
      {
        OssoABookContactField *parent =
          osso_abook_contact_field_get_parent(row->field);

        if (parent)
          name = osso_abook_contact_field_get_name(parent);
      }

      if (name && !strcmp(EVC_N, name))
      {
        grab_focus(it);
        msgid = "addr_ib_no_name_given";
        break;
      }
    }
  }

  if (!msgid && (get_contact_attributes_count(editor, contact) > 50))
    msgid = "addr_ib_field_amount_limited";

  if (msgid)
  {
    hildon_banner_show_information(GTK_WIDGET(editor), NULL, _(msgid));
    return FALSE;
  }

  return osso_abook_check_disc_space(GTK_WINDOW(editor));
}

static gboolean
save_idle_cb(gpointer user_data)
{
  OssoABookContactEditor *editor = user_data;
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  OssoABookContact *contact = create_contact(priv);

  if (verify_contact(editor, contact))
    commit_save(editor, contact);
  else
    set_progress_indicator(editor, 0);

  priv->save_idle_id = 0;

  if (contact)
    g_object_unref(contact);

  g_object_unref(editor);

  return FALSE;
}

static gboolean
accept_field(OssoABookContactField *field, gpointer user_data)
{
  const char *name = osso_abook_contact_field_get_name(field);

  if (!strcmp(name, EVC_FN) || !strcmp(name, EVC_LABEL) ||
      !strcmp(name, EVC_PHOTO))
  {
    return FALSE;
  }

  if (osso_abook_contact_field_get_flags(field) &
      OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON)
  {
    GSequenceIter *it;

    for (it = g_sequence_get_begin_iter(PRIVATE(user_data)->fields);
         !g_sequence_iter_is_end(it); it = g_sequence_iter_next(it))
    {
      if (name == osso_abook_contact_field_get_name(g_sequence_get(it)))
        return FALSE;
    }
  }

  return TRUE;
}

static void
remove_field(OssoABookContactEditor *editor, OssoABookContactField *field)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  gint pos = 0;
  GSequenceIter *it, *next;
  GtkRequisition r;

  g_object_ref(field);

  for (it = g_sequence_get_begin_iter(priv->rows); !g_sequence_iter_is_end(it);
       it = next)
  {
    struct contact_editor_row *row = g_sequence_get(it);

    next = g_sequence_iter_next(it);

    if ((row->field == field) ||
        (field == osso_abook_contact_field_get_parent(row->field)))
    {
      GtkContainer *table = GTK_CONTAINER(priv->table);
      GList *l;
      gint current_pos;

      OSSO_ABOOK_NOTE(EDITOR, "removing %s row",
                      osso_abook_contact_field_get_path(row->field));

      current_pos = g_sequence_iter_get_position(it);

      for (l = gtk_container_get_children(table); l;
           l = g_list_delete_link(l, l))
      {
        gint bottom;
        gint top;

        gtk_container_child_get(table, l->data, "top-attach", &top,
                                "bottom-attach", &bottom, NULL);

        if (current_pos >= top)
        {
          if (current_pos == top)
            gtk_container_remove(table, l->data);
        }
        else
        {
          gtk_container_child_set(table, l->data, "top-attach", top - 1,
                                  "bottom-attach", bottom - 1, NULL);
        }
      }

      pos = g_sequence_iter_get_position(it);
      g_sequence_remove(it);
    }
  }

  for (it = g_sequence_get_begin_iter(priv->fields);
       !g_sequence_iter_is_end(it); it = next)
  {
    OssoABookContactField *current_field = g_sequence_get(it);

    next = g_sequence_iter_next(it);

    if ((current_field == field) ||
        (field == osso_abook_contact_field_get_parent(current_field)))
    {
      OSSO_ABOOK_NOTE(EDITOR, "removing %s field",
                      osso_abook_contact_field_get_path(current_field));

      g_hash_table_remove(
        priv->attributes,
        osso_abook_contact_field_get_borrowed_attribute(field));
      g_sequence_remove(it);
    }
  }

  if (g_sequence_get_length(priv->rows) <= pos)
    pos = g_sequence_get_length(priv->rows) - 1;

  if (pos >= 0)
    grab_focus(g_sequence_get_iter_at_pos(priv->rows, pos));

  g_object_unref(field);
  update_actions(editor);
  gtk_widget_size_request(GTK_WIDGET(editor), &r);
  gtk_window_resize(GTK_WINDOW(editor), r.width, r.height);
}

static void
add_field_clicked_cb(GtkButton *button, OssoABookContactEditor *editor)
{
  OssoABookContactEditorPrivate *priv;
  GtkWidget *selector;
  GtkWidget *dialog;

  if (get_contact_attributes_count(editor, NULL) >= 50)
  {
    hildon_banner_show_information(GTK_WIDGET(editor), NULL,
                                   _("addr_ib_field_amount_limited"));
    return;
  }

  priv = PRIVATE(editor);
  selector = osso_abook_contact_field_selector_new();
  hildon_touch_selector_set_hildon_ui_mode(HILDON_TOUCH_SELECTOR(selector),
                                           HILDON_UI_MODE_NORMAL);
  osso_abook_contact_field_selector_load(
    OSSO_ABOOK_CONTACT_FIELD_SELECTOR(selector), priv->contact,
    accept_field, editor);
  dialog = hildon_picker_dialog_new(GTK_WINDOW(editor));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_title(GTK_WINDOW(dialog), _("addr_ti_add_field_title"));
  osso_abook_set_portrait_mode_supported(GTK_WINDOW(dialog), FALSE);
  hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog),
                                    HILDON_TOUCH_SELECTOR(selector));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
  {
    OssoABookContactField *field =
      osso_abook_contact_field_selector_get_selected(
        OSSO_ABOOK_CONTACT_FIELD_SELECTOR(selector));

    gtk_widget_destroy(dialog);

    if (field)
    {
      OssoABookContactField *current_field = priv->current_field;
      struct contact_editor_row *row;
      GSequenceIter *iter;

      osso_abook_contact_field_set_message_map(field, priv->message_map);
      g_sequence_append(priv->fields, g_object_ref(field));
      iter = attach_row_widgets(priv, field);
      update_field_positions(editor);

      row = g_sequence_get(iter);

      if (row && osso_abook_contact_field_has_children(row->field))
        iter = g_sequence_iter_next(iter);

      grab_focus(iter);

      if (!osso_abook_contact_field_activate(field))
      {
        remove_field(editor, field);
        update_actions(editor);

        if (current_field)
        {
          GSequenceIter *it;

          priv->current_field = current_field;

          for (it = g_sequence_get_begin_iter(priv->rows);
               !g_sequence_iter_is_end(it); it = g_sequence_iter_next(it))
          {
            if (g_sequence_get(it) == priv->current_field)
            {
              grab_focus(it);
              break;
            }
          }
        }
      }

      g_object_unref(field);
    }
  }
  else
    gtk_widget_destroy(dialog);
}

static void
append_selector_valuess(OssoABookContactEditorPrivate *priv,
                        OssoABookContactFieldSelector *selector)
{
  GSequenceIter *it;

  for (it = g_sequence_get_begin_iter(priv->fields);
       !g_sequence_iter_is_end(it); it = g_sequence_iter_next(it))
  {
    OssoABookContactField *field = g_sequence_get(it);

    if (field_is_removable(field))
      osso_abook_contact_field_selector_append(selector, field);
  }
}

static void
remove_field_clicked_cb(GtkButton *button, OssoABookContactEditor *editor)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  GtkWidget *selector = osso_abook_contact_field_selector_new();
  GList *fields = NULL;
  GtkWidget *dialog;

  osso_abook_contact_field_selector_set_show_values(
    OSSO_ABOOK_CONTACT_FIELD_SELECTOR(selector), TRUE);
  append_selector_valuess(priv, OSSO_ABOOK_CONTACT_FIELD_SELECTOR(selector));
  hildon_touch_selector_set_column_selection_mode(
    HILDON_TOUCH_SELECTOR(selector),
    HILDON_TOUCH_SELECTOR_SELECTION_MODE_MULTIPLE);
  hildon_touch_selector_unselect_all(HILDON_TOUCH_SELECTOR(selector), 0);

  dialog = hildon_picker_dialog_new(GTK_WINDOW(editor));
  gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_title(GTK_WINDOW(dialog), _("addr_ti_delete_field"));
  osso_abook_set_portrait_mode_supported(GTK_WINDOW(dialog), FALSE);
  g_signal_connect(dialog, "destroy",
                   G_CALLBACK(gtk_widget_destroyed), &dialog);
  hildon_picker_dialog_set_done_label(
    HILDON_PICKER_DIALOG(dialog),
    dgettext("hildon-libs", "wdgt_bd_delete"));
  hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog),
                                    HILDON_TOUCH_SELECTOR(selector));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
  {
    fields = osso_abook_contact_field_selector_get_selected_fields(
      OSSO_ABOOK_CONTACT_FIELD_SELECTOR(selector));
  }

  if (dialog)
    gtk_widget_destroy(dialog);

  if (fields)
  {
    GList *l;
    GList *list = NULL;
    gchar *confirmation_string;
    GtkWidget *note;

    for (l = fields; l; l = l->next)
    {
      OssoABookContact *rc =
        osso_abook_contact_field_get_roster_contact(l->data);

      if (rc)
      {
        char *val = osso_abook_contact_get_value(
          E_CONTACT(rc), osso_abook_contact_get_vcard_field(rc));

        if (val)
        {
          g_free(val);

          if (!osso_abook_contact_has_invalid_username(rc))
            list = g_list_prepend(list, rc);
        }
      }
    }

    if (fields->next)
    {
      confirmation_string = _osso_abook_get_delete_confirmation_string(
        list, FALSE, "addr_nc_delete_fields",
        "addr_nc_delete_im_username_fields",
        "addr_nc_delete_im_username_fields_several_services");
    }
    else
    {
      confirmation_string = _osso_abook_get_delete_confirmation_string(
        list, TRUE, "addr_nc_delete_field",
        "addr_nc_delete_im_username_field", "");
    }

    note = hildon_note_new_confirmation(GTK_WINDOW(editor),
                                        confirmation_string);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(note), TRUE);

    if (gtk_dialog_run(GTK_DIALOG(note)) == GTK_RESPONSE_OK)
    {
      gtk_widget_destroy(note);
      g_free(confirmation_string);
      g_list_free(list);

      while (fields)
      {
        OssoABookContactField *field = fields->data;

        if (field == priv->current_field)
          priv->current_field = NULL;

        remove_field(editor, field);
        g_object_unref(field);
        fields = g_list_delete_link(fields, fields);
      }

      update_actions(editor);
    }
    else
    {
      g_list_free_full(fields, g_object_unref);
      gtk_widget_destroy(note);
      g_free(confirmation_string);
      g_list_free(list);
    }
  }
}

static void
save_button_clicked_cb(GtkButton *button, OssoABookContactEditor *editor)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);

  if (!priv->save_idle_id)
  {
    editor = g_object_ref(editor);
    priv->save_idle_id = gdk_threads_add_idle(save_idle_cb, editor);
    set_progress_indicator(editor, 1);
  }
}

static void
osso_abook_contact_editor_init(OssoABookContactEditor *editor)
{
  OssoABookContactEditorPrivate *priv = PRIVATE(editor);
  GtkWidget *add_field_button;
  GtkWidget *remove_field_button;
  GtkWidget *save_button;

  priv->attributes = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                           NULL,
                                           (GDestroyNotify)managed_attr_free);
  priv->message_map = osso_abook_message_map_new(contact_editor_map);
  priv->pannable_area = osso_abook_pannable_area_new();
  priv->table = gtk_table_new(1, 2, FALSE);
  gtk_table_set_col_spacings(GTK_TABLE(priv->table), 8);
  hildon_pannable_area_add_with_viewport(
    HILDON_PANNABLE_AREA(priv->pannable_area), priv->table);
  priv->size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

  add_field_button = hildon_button_new_with_text(
    HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL,
    _("addr_bd_newedit_addfield"), NULL);
  gtk_button_set_alignment(GTK_BUTTON(add_field_button), 0.5f, 0.5f);

  gtk_widget_set_can_focus(add_field_button, FALSE);
  gtk_size_group_add_widget(priv->size_group, add_field_button);
  g_signal_connect(add_field_button, "clicked",
                   G_CALLBACK(add_field_clicked_cb), editor);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(editor)->action_area),
                    add_field_button);

  remove_field_button = hildon_button_new_with_text(
    HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL,
    _("addr_bd_newedit_removefield"), NULL);
  gtk_button_set_alignment(GTK_BUTTON(remove_field_button), 0.5f, 0.5f);
  gtk_widget_set_can_focus(remove_field_button, FALSE);
  gtk_size_group_add_widget(priv->size_group, remove_field_button);
  g_signal_connect(remove_field_button, "clicked",
                   G_CALLBACK(remove_field_clicked_cb), editor);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(editor)->action_area),
                    remove_field_button);
  gtk_widget_set_no_show_all(remove_field_button, TRUE);
  priv->field_remove_button = remove_field_button;

  save_button = hildon_button_new_with_text(
    HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_VERTICAL,
    dgettext("hildon-libs", "wdgt_bd_save"), NULL);
  gtk_button_set_alignment(GTK_BUTTON(save_button), 0.5f, 0.5f);
  gtk_widget_set_can_focus(save_button, FALSE);
  gtk_size_group_add_widget(priv->size_group, save_button);
  g_signal_connect(save_button, "clicked",
                   G_CALLBACK(save_button_clicked_cb), editor);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(editor)->action_area),
                    save_button);
  gtk_widget_show_all(GTK_DIALOG(editor)->action_area);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(editor)->vbox), priv->pannable_area,
                     TRUE, TRUE, 0);
  gtk_widget_show_all(priv->pannable_area);
  g_signal_connect(editor, "set-focus", G_CALLBACK(set_focus_cb), NULL);

  osso_abook_set_portrait_mode_supported(GTK_WINDOW(editor), FALSE); /* :( */
}

GtkWidget *
osso_abook_contact_editor_new_with_contact(GtkWindow *parent,
                                           OssoABookContact *contact,
                                           OssoABookContactEditorMode mode)
{
  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), NULL);
  g_return_val_if_fail(!contact || OSSO_ABOOK_IS_CONTACT(contact), NULL);

  return g_object_new(OSSO_ABOOK_TYPE_CONTACT_EDITOR,
                      "transient-for", parent,
                      "contact", contact,
                      "mode", mode,
                      "has-separator", FALSE,
                      "modal", TRUE,
                      "destroy-with-parent", TRUE,
                      NULL);
}

GtkWidget *
osso_abook_contact_editor_new()
{
  return osso_abook_contact_editor_new_with_contact(
    NULL, NULL, OSSO_ABOOK_CONTACT_EDITOR_EDIT);
}

void
osso_abook_contact_editor_set_mode(OssoABookContactEditor *editor,
                                   OssoABookContactEditorMode mode)
{
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_EDITOR(editor));

  PRIVATE(editor)->mode = mode;

  if (gtk_widget_get_realized(GTK_WIDGET(editor)))
    update_fields(editor);

  g_object_notify(G_OBJECT(editor), "mode");
}

OssoABookContactEditorMode
osso_abook_contact_editor_get_mode(OssoABookContactEditor *editor)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_EDITOR(editor),
                       OSSO_ABOOK_CONTACT_EDITOR_EDIT);

  return PRIVATE(editor)->mode;
}

OssoABookContact *
osso_abook_contact_editor_get_contact(OssoABookContactEditor *editor)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_EDITOR(editor), NULL);

  return PRIVATE(editor)->contact;
}

static void
update_minimum_label_width(OssoABookContactEditorPrivate *priv)
{
  GtkWidget *button;
  GList *l;

  g_return_if_fail(0 == priv->minimum_label_width);

  button = osso_abook_button_new(HILDON_SIZE_FINGER_HEIGHT);
  gtk_widget_set_parent(button, priv->table);
  gtk_widget_ensure_style(button);

  for (l = osso_abook_contact_field_list_supported_fields(
         priv->message_map, priv->contact, NULL, NULL); l;
       l = g_list_delete_link(l, l))
  {
    GList *children;

    const char *display_title =
      osso_abook_contact_field_get_display_title(l->data);

    if (display_title)
    {
      GtkRequisition requisition;

      osso_abook_button_set_title(OSSO_ABOOK_BUTTON(button), display_title);
      gtk_widget_size_request(button, &requisition);

      if (requisition.width > priv->minimum_label_width)
        priv->minimum_label_width = requisition.width;
    }

    for (children = osso_abook_contact_field_get_children(l->data);
         children; children = children->next)
    {
      l = g_list_insert_before(l, l->next, g_object_ref(children->data));
    }

    g_object_unref(l->data);
  }

  /* this seem to unref it as well */
  gtk_widget_unparent(button);
}

void
osso_abook_contact_editor_set_contact(OssoABookContactEditor *editor,
                                      OssoABookContact *contact)
{
  OssoABookContactEditorPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_EDITOR(editor));
  g_return_if_fail(contact == NULL || OSSO_ABOOK_IS_CONTACT(contact));

  priv = PRIVATE(editor);

  if (priv->contact != contact)
    update_minimum_label_width(priv);

  if (contact)
    g_object_ref(contact);

  set_contact(editor, contact);

  if (priv->contact)
  {
    if (OSSO_ABOOK_DEBUG_FLAGS(EDITOR))
    {
      GList *rc;

      OSSO_ABOOK_DUMP_VCARD(EDITOR, priv->contact, "master contact");

      for (rc = osso_abook_contact_get_roster_contacts(priv->contact); rc;
           rc = rc->next)
      {
        OSSO_ABOOK_DUMP_VCARD(EDITOR, rc->data, "roster contact");
      }
    }
  }

  if (gtk_widget_get_realized(GTK_WIDGET(editor)))
    update_fields(editor);

  g_object_notify(G_OBJECT(editor), "contact");
}
