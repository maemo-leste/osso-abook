/*
 * osso-abook-contact-field.c
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
#include <hildon-uri.h>
#include <hildon/hildon.h>

#include <langinfo.h>
#include <libintl.h>

#include "osso-abook-account-manager.h"
#include "osso-abook-address-format.h"
#include "osso-abook-avatar-chooser-dialog.h"
#include "osso-abook-avatar-editor-dialog.h"
#include "osso-abook-avatar-image.h"
#include "osso-abook-button.h"
#include "osso-abook-contact-field-selector.h"
#include "osso-abook-contact-field.h"
#include "osso-abook-dialogs.h"
#include "osso-abook-entry.h"
#include "osso-abook-enums.h"
#include "osso-abook-log.h"
#include "osso-abook-message-map.h"
#include "osso-abook-msgids.h"
#include "osso-abook-string-list.h"
#include "osso-abook-tp-account-selector.h"
#include "osso-abook-util.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-voicemail-contact.h"

typedef struct _OssoABookContactFieldTemplate OssoABookContactFieldTemplate;

struct _OssoABookContactFieldTemplate
{
  const gchar *name;
  gchar *msgid;
  gchar *title;
  gchar *icon_name;
  int sort_weight;
  OssoABookContactFieldFlags flags;
  GList *(*actions)(OssoABookContactField *field);
  GtkWidget *(*editor_widget)(OssoABookContactField *field);
  GList *(*children)(OssoABookContactField *field);
  void (*update)(OssoABookContactField *field);
  void (*synthesize_attributes)(OssoABookContactField *field);
  gchar *(*get_attr_value)(EVCardAttribute *attr);
};

struct _OssoABookContactFieldTemplateGroup
{
  OssoABookContactFieldTemplate *template;
  int count;
};

typedef struct _OssoABookContactFieldTemplateGroup \
  OssoABookContactFieldTemplateGroup;

struct _OssoABookContactFieldAction
{
  gint ref_cnt;
  OssoABookContactAction contact_action;
  OssoABookContactField *field;
  GtkWidget *widget;
  TpProtocol *protocol;
  OssoABookContactFieldActionLayoutFlags layout_flags;
};

typedef struct _OssoABookContactFieldAction OssoABookContactFieldAction;

struct _OssoABookContactFieldPrivate
{
  OssoABookContactFieldTemplate *template;
  EVCardAttribute *attribute;
  EVCardAttribute *borrowed_attribute;
  GHashTable *message_map;
  OssoABookContact *master_contact;
  OssoABookContact *roster_contact;
  gchar *display_title;
  gchar *secondary_title;
  gchar *display_value;
  int field_24;
  GtkWidget *label;
  GtkWidget *editor_widget;
  GList *secondary_attributes;
  OssoABookContactField *parent;
  GList *children;
  gchar *path;
  gboolean modified : 1 /* 0x01 0xFE */;
  gboolean mandatory : 1 /* 0x02 0xFD */;
};

typedef struct _OssoABookContactFieldPrivate OssoABookContactFieldPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookContactField,
  osso_abook_contact_field,
  G_TYPE_OBJECT
);

enum
{
  PROP_NAME = 1,
  PROP_FLAGS,
  PROP_ATTRIBUTE,
  PROP_MESSAGE_MAP,
  PROP_MASTER_CONTACT,
  PROP_ROSTER_CONTACT,
  PROP_DISPLAY_TITLE,
  PROP_DISPLAY_VALUE,
  PROP_ICON_NAME,
  PROP_SORT_WEIGHT,
  PROP_LABEL_WIDGET,
  PROP_EDITOR_WIDGET,
  PROP_MODIFIED
};

#define OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field) \
  ((OssoABookContactFieldPrivate *) \
   osso_abook_contact_field_get_instance_private( \
     ((OssoABookContactField *)field)))

#define A(n) const char *const A_##n = n

A(EVC_N);
A(EVC_FN);
A(EVC_LABEL);
A(EVC_TYPE);
A(EVC_NOTE);
A(EVC_TEL);
A(EVC_ADR);
A(EVC_ORG);
A(EVC_PHOTO);
A(EVC_BDAY);
A(EVC_EMAIL);
A(EVC_URL);
A(EVC_NICKNAME);
A(EVC_TITLE);

static GList *
get_name_children(OssoABookContactField *field);

static GList *
get_address_children(OssoABookContactField *field);

static gchar *
get_formatted_address(EVCardAttribute *attr);

static void
osso_abook_contact_field_constructed(GObject *object);

static OssoABookAvatar *
get_avatar(OssoABookContactField *field)
{
  GtkWidget *child = gtk_bin_get_child(
      GTK_BIN(osso_abook_contact_field_get_editor_widget(field)));

  return osso_abook_avatar_image_get_avatar(OSSO_ABOOK_AVATAR_IMAGE(child));
}

static gchar *
create_field_title(OssoABookContactField *field, const char *display_title,
                   const char *secondary_title)
{
  if (!display_title)
    display_title = osso_abook_contact_field_get_display_title(field);

  if (!secondary_title)
    secondary_title = osso_abook_contact_field_get_secondary_title(field);

  if (IS_EMPTY(display_title))
    return NULL;

  if (!IS_EMPTY(secondary_title))
    return g_strdup_printf("%s (%s)", display_title, secondary_title);

  return g_strdup(display_title);
}

static GtkWidget *
osso_abook_contact_field_create_button_with_style(OssoABookContactField *field,
                                                  const char *title,
                                                  const char *icon_name,
                                                  OssoABookButtonStyle style)
{
  gchar *display_title;
  const char *display_value;
  OssoABookContact *roster_contact;
  GtkWidget *button;

  if (title)
    display_title = g_strdup(title);
  else
    display_title = create_field_title(field, 0, 0);

  if (!icon_name)
    icon_name = osso_abook_contact_field_get_icon_name(field);

  display_value = osso_abook_contact_field_get_display_value(field);

  roster_contact = osso_abook_contact_field_get_roster_contact(field);

  if (roster_contact)
  {
    button = osso_abook_button_new_with_presence(
        HILDON_SIZE_AUTO_WIDTH, icon_name, display_title, display_value,
        OSSO_ABOOK_PRESENCE(roster_contact));
  }
  else
  {
    button = osso_abook_button_new_with_presence(
        HILDON_SIZE_AUTO_WIDTH, icon_name, display_title, display_value,
        NULL);
  }

  osso_abook_button_set_style(OSSO_ABOOK_BUTTON(button), style);
  gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5);
  g_free(display_title);

  return button;
}

static GtkWidget *
osso_abook_contact_field_create_button_with_label(OssoABookContactField *field)
{
  return osso_abook_contact_field_create_button_with_style(
           field, NULL, NULL, OSSO_ABOOK_BUTTON_STYLE_LABEL);
}

static void
free_attributes_list(GList *attributes)
{
  GList *l;

  for (l = attributes; l; l = g_list_delete_link(l, l))
    e_vcard_attribute_free(l->data);
}

static void
synthesize_name_attributes(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  EVCardAttribute *attr;

  g_return_if_fail(NULL != priv->attribute);

  attr = e_vcard_attribute_new(NULL, A_EVC_FN);
  e_vcard_attribute_add_value(
    attr, osso_abook_contact_field_get_display_value(field));
  free_attributes_list(priv->secondary_attributes);
  priv->secondary_attributes = g_list_prepend(NULL, attr);
}

static void
synthesize_address_attributes(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  gchar *addr;

  g_return_if_fail(NULL != priv->attribute);

  addr = get_formatted_address(osso_abook_contact_field_get_attribute(field));

  if (!IS_EMPTY(addr))
  {
    EVCardAttribute *attr_label = e_vcard_attribute_new(NULL, A_EVC_LABEL);
    EVCardAttributeParam *attr_type;
    GList *l;

    e_vcard_attribute_add_value(attr_label, addr);
    attr_type = e_vcard_attribute_param_new(A_EVC_TYPE);

    for (l = e_vcard_attribute_get_param(priv->attribute, A_EVC_TYPE); l;
         l = l->next)
    {
      osso_abook_e_vcard_attribute_param_merge_value(attr_type, l->data, NULL);
    }

    e_vcard_attribute_add_param(attr_label, attr_type);
    free_attributes_list(priv->secondary_attributes);
    priv->secondary_attributes = g_list_prepend(0, attr_label);
  }

  g_free(addr);
}

static void
update_text_attribute(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  gchar *val;

  g_return_if_fail(NULL != priv->attribute);

  if (!priv->editor_widget)
    return;

  g_return_if_fail(GTK_IS_ENTRY(priv->editor_widget));

  val = g_strdup(gtk_entry_get_text(GTK_ENTRY(priv->editor_widget)));

  e_vcard_attribute_remove_values(priv->attribute);

  e_vcard_attribute_add_value(priv->attribute, g_strchomp(g_strchug(val)));
  g_free(val);
}

static void
update_zip_attribute(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  gchar *val;

  g_return_if_fail(NULL != priv->attribute);

  if (!priv->editor_widget)
    return;

  g_return_if_fail(GTK_IS_ENTRY(priv->editor_widget));

  val = g_utf8_strup(gtk_entry_get_text(GTK_ENTRY(priv->editor_widget)), -1);
  e_vcard_attribute_remove_values(priv->attribute);

  e_vcard_attribute_add_value(priv->attribute, g_strchomp(g_strchug(val)));
  g_free(val);
}

static void
update_note_attribute(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  GtkTextIter end;
  GtkTextIter start;
  GtkTextBuffer *buffer;
  gchar *val;

  g_return_if_fail(NULL != priv->attribute);

  if (!priv->editor_widget)
    return;

  g_return_if_fail(GTK_IS_TEXT_VIEW(priv->editor_widget));

  buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(priv->editor_widget));
  gtk_text_buffer_get_bounds(buffer, &start, &end);
  val = gtk_text_buffer_get_text(buffer, &start, &end, TRUE);
  e_vcard_attribute_remove_values(priv->attribute);
  e_vcard_attribute_add_value(priv->attribute, g_strchomp(g_strchug(val)));
  g_free(val);
}

static void
update_gender_attribute(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  const char *gender = "undefined";
  const gchar *val;

  g_return_if_fail(NULL != priv->attribute);

  if (!priv->editor_widget)
    return;

  g_return_if_fail(HILDON_IS_BUTTON(priv->editor_widget));

  val = hildon_button_get_value(HILDON_BUTTON(priv->editor_widget));

  if (val)
  {
    if (!strcmp(val, g_dgettext("osso-addressbook", "addr_va_general_male")))
      gender = "male";
    else if (!strcmp(val, g_dgettext("osso-addressbook",
                                     "addr_va_general_female")))
    {
      gender = "female";
    }
  }

  e_vcard_attribute_remove_values(priv->attribute);
  e_vcard_attribute_add_value(priv->attribute, gender);
}

static void
update_date_attribute(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  GDate *d;

  g_return_if_fail(NULL != priv->attribute);

  if (!priv->editor_widget)
    return;

  d = g_object_get_data(G_OBJECT(priv->editor_widget), "date-button-value");

  if (d)
  {
    EContactDate *date = e_contact_date_new();
    gchar *string_date;

    date->year = d->year;
    date->month = d->month;
    date->day = d->day;
    string_date = e_contact_date_to_string(date);
    e_contact_date_free(date);
    e_vcard_attribute_remove_values(priv->attribute);
    e_vcard_attribute_add_value(priv->attribute, string_date);
    g_free(string_date);
  }
}

static void
update_image_attribute(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  EVCardAttribute *photo_attr;

  e_vcard_attribute_remove_values(priv->attribute);
  e_vcard_attribute_remove_params(priv->attribute);

  photo_attr = e_vcard_get_attribute(
      E_VCARD(OSSO_ABOOK_CONTACT(get_avatar(field))), A_EVC_PHOTO);

  if (photo_attr)
  {
    GList *val = e_vcard_attribute_get_values(photo_attr);
    GList *param = e_vcard_attribute_get_params(photo_attr);

    while (val)
    {
      e_vcard_attribute_add_value(priv->attribute, val->data);
      val = val->next;
    }

    while (param)
    {
      e_vcard_attribute_add_param(priv->attribute,
                                  e_vcard_attribute_param_copy(param->data));
      param = param->next;
    }
  }
}

static void
update_country_attribute(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  gchar *val;

  g_return_if_fail(NULL != priv->attribute);

  if (!priv->editor_widget)
    return;

  g_return_if_fail(HILDON_IS_BUTTON(priv->editor_widget));

  val = g_strdup(hildon_button_get_value(HILDON_BUTTON(priv->editor_widget)));
  e_vcard_attribute_remove_values(priv->attribute);
  e_vcard_attribute_add_value(priv->attribute, g_strchomp(g_strchug(val)));
  g_free(val);
}

static void
update_attribute_list(OssoABookContactFieldPrivate *priv, guint attr_count)
{
  GList *values;
  GList *child;
  guint i;
  GList *l = NULL;

  g_return_if_fail(NULL != priv->attribute);

  values = e_vcard_attribute_get_values(priv->attribute);

  for (child = priv->children; child; child = child->next)
  {
    l = g_list_prepend(l, e_vcard_attribute_get_value(
                         osso_abook_contact_field_get_attribute(child->data)));

    if (values)
      values = values->next;
  }

  while (values)
  {
    l = g_list_prepend(l, g_strdup(values->data));
    values = values->next;
  }

  for (i = g_list_length(l); i < attr_count; i++)
    l = g_list_prepend(l, g_strdup(""));

  e_vcard_attribute_remove_values(priv->attribute);

  for (l = g_list_reverse(l); l; l = g_list_delete_link(l, l))
  {
    e_vcard_attribute_add_value(priv->attribute, l->data);

    g_free(l->data);
  }
}

static void
update_name_attribute(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  g_return_if_fail(NULL != priv->attribute);

  update_attribute_list(priv, 5);
  free_attributes_list(priv->secondary_attributes);
  priv->secondary_attributes = NULL;
}

static void
update_address_attribute(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  g_return_if_fail(NULL != priv->attribute);

  update_attribute_list(priv, 7);
  free_attributes_list(priv->secondary_attributes);
  priv->secondary_attributes = NULL;
}

static void
field_modified(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (priv->modified)
    return;

  g_free(priv->display_value);
  priv->display_value = NULL;
  priv->modified = TRUE;

  g_object_notify(G_OBJECT(field), "modified");
  g_object_notify(G_OBJECT(field), "display-value");
}

static GtkWidget *
get_text_editor_widget(OssoABookContactField *field)
{
  EVCardAttribute *attr = osso_abook_contact_field_get_attribute(field);
  const gchar *s = NULL;
  gchar *text;
  GtkWidget *widget;

  if (attr)
  {
    GList *v = e_vcard_attribute_get_values(attr);

    if (v)
      s = v->data;
  }

  if (!s)
    s = "";

  if (g_utf8_validate(s, -1, NULL))
  {
    GString *str = g_string_new(NULL);

    while (*s)
    {
      gunichar uch = g_utf8_get_char(s);

      switch (g_unichar_break_type(uch))
      {
        case G_UNICODE_BREAK_CARRIAGE_RETURN:
        case G_UNICODE_BREAK_LINE_FEED:
        {
          g_string_append_c(str, ' ');
          break;
        }
        default:
        {
          str = g_string_append_unichar(str, uch);
          break;
        }
      }

      s = g_utf8_next_char(s);
    }

    text = g_string_free(str, FALSE);
  }
  else
    text = g_strdup(s);

  widget = hildon_entry_new(HILDON_SIZE_FINGER_HEIGHT);
  gtk_entry_set_text(GTK_ENTRY(widget), text);
  g_free(text);

  if (osso_abook_contact_field_get_flags(field) &
      OSSO_ABOOK_CONTACT_FIELD_FLAGS_NO_LABEL)
  {
    hildon_gtk_entry_set_placeholder_text(
      GTK_ENTRY(widget),
      osso_abook_contact_field_get_display_title(field));
  }

  g_signal_connect_swapped(widget, "changed",
                           G_CALLBACK(field_modified), field);

  return widget;
}

static GtkWidget *
get_alpha_num_text_editor_widget(OssoABookContactField *field)
{
  GtkWidget *widget = get_text_editor_widget(field);

  hildon_gtk_entry_set_input_mode(
    GTK_ENTRY(widget),
    HILDON_GTK_INPUT_MODE_NUMERIC | HILDON_GTK_INPUT_MODE_ALPHA);

  return widget;
}

static GtkWidget *
get_tele_text_editor_widget(OssoABookContactField *field)
{
  GtkWidget *widget = get_text_editor_widget(field);

  hildon_gtk_entry_set_input_mode(GTK_ENTRY(widget),
                                  HILDON_GTK_INPUT_MODE_TELE);

  return widget;
}

static GtkWidget *
get_noautocap_text_editor_widget(OssoABookContactField *field)
{
  GtkWidget *widget = get_text_editor_widget(field);
  HildonGtkInputMode im = hildon_gtk_entry_get_input_mode(GTK_ENTRY(widget));

  hildon_gtk_entry_set_input_mode(GTK_ENTRY(widget),
                                  im & ~HILDON_GTK_INPUT_MODE_AUTOCAP);

  return widget;
}

static void
note_editor_notify_cursor_position_cb(GtkTextBuffer *text_buffer,
                                      GParamSpec *pspec, GtkTextView *text_view)
{
  GtkWidget *pa;
  GtkWidget *table;
  GtkTextIter iter;
  GdkRectangle location;
  gint y;

  if (!gtk_widget_has_focus(GTK_WIDGET(text_view)))
    return;

  pa = gtk_widget_get_ancestor(
      GTK_WIDGET(text_view), HILDON_TYPE_PANNABLE_AREA);
  table = gtk_widget_get_ancestor(GTK_WIDGET(text_view), GTK_TYPE_TABLE);

  g_return_if_fail(NULL != pa);
  g_return_if_fail(NULL != table);

  gtk_text_buffer_get_iter_at_mark(text_buffer, &iter,
                                   gtk_text_buffer_get_insert(text_buffer));
  gtk_text_view_get_iter_location(text_view, &iter, &location);
  gtk_text_view_buffer_to_window_coords(text_view, GTK_TEXT_WINDOW_WIDGET,
                                        0, location.y, NULL, &y);

  if (gtk_widget_translate_coordinates(
        GTK_WIDGET(text_view), table, 0, y, 0, &y))
  {
    hildon_pannable_area_jump_to(HILDON_PANNABLE_AREA(pa), -1, y);
    hildon_pannable_area_jump_to(HILDON_PANNABLE_AREA(pa), -1,
                                 y + location.height);
  }
}

static void
note_editor_size_allocate_cb(GtkTextView *text_view,
                             GdkRectangle *allocation, gpointer user_data)
{
  note_editor_notify_cursor_position_cb(
    gtk_text_view_get_buffer(text_view), 0, text_view);
}

static GtkWidget *
get_note_editor_widget(OssoABookContactField *field)
{
  GtkTextBuffer *text_buffer = gtk_text_buffer_new(NULL);
  GtkWidget *widget;
  HildonGtkInputMode im;

  gtk_text_buffer_set_text(
    text_buffer, osso_abook_contact_field_get_display_value(field), -1);
  widget = hildon_text_view_new();
  gtk_text_view_set_buffer(GTK_TEXT_VIEW(widget), text_buffer);
  gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(widget), GTK_WRAP_WORD_CHAR);
  im = hildon_gtk_text_view_get_input_mode(GTK_TEXT_VIEW(widget));
  hildon_gtk_text_view_set_input_mode(GTK_TEXT_VIEW(widget),
                                      im | HILDON_GTK_INPUT_MODE_MULTILINE);
  g_signal_connect_swapped(text_buffer, "changed",
                           G_CALLBACK(field_modified), field);
  g_signal_connect(text_buffer, "notify::cursor-position",
                   G_CALLBACK(note_editor_notify_cursor_position_cb), widget);
  g_signal_connect(widget, "size-allocate",
                   G_CALLBACK(note_editor_size_allocate_cb), NULL);
  /* why twice? */
  im = hildon_gtk_text_view_get_input_mode(GTK_TEXT_VIEW(widget));
  hildon_gtk_text_view_set_input_mode(GTK_TEXT_VIEW(widget),
                                      im | HILDON_GTK_INPUT_MODE_MULTILINE);
  return widget;
}

static void
set_editor_window_title(GtkWindow *parent, OssoABookContactField *field)
{
  const char *title = NULL;

  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (priv->template && priv->template->title)
  {
    title = osso_abook_message_map_lookup(priv->message_map,
                                          priv->template->title);
  }

  if (!title)
    title = osso_abook_contact_field_get_display_title(field);

  osso_abook_set_portrait_mode_supported(GTK_WINDOW(parent), FALSE);
  gtk_window_set_title(parent, title);
}

static void
gender_editor_clicked_cb(GtkButton *button, OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  GtkWidget *dialog = hildon_picker_dialog_new(
      GTK_WINDOW(gtk_widget_get_toplevel(priv->editor_widget)));
  GtkWidget *selector;
  const char *display_value;
  GtkTreeModel *tree_model;
  int i;
  static const char *genders[] =
  {
    "addr_va_general_undefined",
    "addr_va_general_male",
    "addr_va_general_female"
  };

  set_editor_window_title(GTK_WINDOW(dialog), field);
  selector = hildon_touch_selector_new_text();
  hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog),
                                    HILDON_TOUCH_SELECTOR(selector));
  tree_model = hildon_touch_selector_get_model(
      HILDON_TOUCH_SELECTOR(selector), 0);
  display_value = osso_abook_contact_field_get_display_value(field);

  for (i = 0; i < G_N_ELEMENTS(genders); i++)
  {
    const char *gender = dgettext("osso-addressbook", genders[i]);

    hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(selector), gender);

    if (!strcmp(display_value, gender))
    {
      hildon_touch_selector_set_active(
        HILDON_TOUCH_SELECTOR(selector), 0,
        gtk_tree_model_iter_n_children(tree_model, 0) - 1);
    }
  }

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
  {
    gchar *text =
      hildon_touch_selector_get_current_text(HILDON_TOUCH_SELECTOR(selector));
    hildon_button_set_value(HILDON_BUTTON(priv->editor_widget), text);
    field_modified(field);
    g_free(text);
  }

  gtk_widget_destroy(dialog);
}

static GtkWidget *
get_gender_editor_widget(OssoABookContactField *field)
{
  GtkWidget *button = hildon_button_new(HILDON_SIZE_FINGER_HEIGHT,
                                        HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);

  hildon_button_set_title_alignment(HILDON_BUTTON(button), 0.0, 0.5);
  hildon_button_set_value_alignment(HILDON_BUTTON(button), 0.0, 0.5);
  gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5);
  hildon_button_set_style(HILDON_BUTTON(button), HILDON_BUTTON_STYLE_PICKER);
  hildon_button_set_title(HILDON_BUTTON(button),
                          osso_abook_contact_field_get_display_title(field));
  hildon_button_set_value(HILDON_BUTTON(button),
                          osso_abook_contact_field_get_display_value(field));
  g_signal_connect(button, "clicked",
                   G_CALLBACK(gender_editor_clicked_cb), field);

  return button;
}

static GDate *
gdate_from_attribute(EVCardAttribute *attr)
{
  GList *val = e_vcard_attribute_get_values(attr);
  GDate *date = NULL;
  EContactDate *d;

  if (!val)
    return NULL;

  if (IS_EMPTY(val->data))
    return NULL;

  d = e_contact_date_from_string(val->data);

  if (!d)
    return NULL;

  if (g_date_valid_dmy(d->day, d->month, d->year))
  {
    gchar *s;

    date = g_date_new_dmy(d->day, d->month, d->year);
    s = e_contact_date_to_string(d);
    e_vcard_attribute_remove_values(attr);
    e_vcard_attribute_add_value(attr, s);
    g_free(s);
  }

  e_contact_date_free(d);

  return date;
}

static void
date_editor_clicked_cb(GtkWidget *button, OssoABookContactField *field)
{
  GtkWidget *selector;
  GtkWidget *dialog;
  GDate *gdate;

  gdate = gdate_from_attribute(osso_abook_contact_field_get_attribute(field));

  if (!gdate)
  {
    gdate = g_date_new();
    g_date_set_time_t(gdate, time(NULL));
    g_date_subtract_years(gdate, 25);
  }

  selector = hildon_date_selector_new_with_year_range(1900, 2100);
  hildon_date_selector_select_current_date(
    HILDON_DATE_SELECTOR(selector), gdate->year, gdate->month, gdate->day);
  g_date_free(gdate);

  dialog = hildon_picker_dialog_new(
      GTK_WINDOW(gtk_widget_get_ancestor(button, GTK_TYPE_WINDOW)));
  set_editor_window_title(GTK_WINDOW(dialog), field);
  hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog),
                                    HILDON_TOUCH_SELECTOR(selector));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
  {
    gchar sdate[100];
    guint day;
    guint month;
    guint year;

    hildon_date_selector_get_date(
      HILDON_DATE_SELECTOR(selector), &year, &month, &day);
    gdate = g_date_new_dmy(day, month + 1, year);
    g_date_strftime(sdate, sizeof(sdate),
                    dgettext("hildon-libs", "wdgt_va_date_medium"), gdate);
    hildon_button_set_value(HILDON_BUTTON(button), sdate);
    g_object_set_data_full(G_OBJECT(button), "date-button-value", gdate,
                           (GDestroyNotify)g_date_free);
    field_modified(field);
  }

  gtk_widget_destroy(dialog);
}

static GtkWidget *
get_date_editor_widget(OssoABookContactField *field)
{
  GtkWidget *widget = hildon_button_new(HILDON_SIZE_FINGER_HEIGHT,
                                        HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);

  _osso_abook_button_set_date_style(HILDON_BUTTON(widget));
  hildon_button_set_title(HILDON_BUTTON(widget),
                          osso_abook_contact_field_get_display_title(field));
  hildon_button_set_value(HILDON_BUTTON(widget),
                          osso_abook_contact_field_get_display_value(field));
  g_signal_connect(widget, "clicked",
                   G_CALLBACK(date_editor_clicked_cb), field);
  return widget;
}

static void
avatar_image_changed_cb(OssoABookAvatar *contact, GdkPixbuf *image,
                        OssoABookContactField *field)
{
  osso_abook_contact_set_pixbuf(OSSO_ABOOK_CONTACT(get_avatar(field)),
                                osso_abook_avatar_get_image(contact), 0, 0);
}

static void
avatar_editor_response_cb(GtkWidget *dialog, int response_id,
                          OssoABookContactField *field)
{
  if (response_id == GTK_RESPONSE_OK)
  {
    GdkPixbuf *pixbuf = osso_abook_avatar_editor_dialog_get_scaled_pixbuf(
        OSSO_ABOOK_AVATAR_EDITOR_DIALOG(dialog));

    if (pixbuf)
    {
      OSSO_ABOOK_NOTE(AVATAR, "updating avatar pixbuf");

      osso_abook_contact_set_pixbuf(OSSO_ABOOK_CONTACT(get_avatar(field)),
                                    pixbuf, NULL, NULL);
      field_modified(field);
      g_object_unref(pixbuf);
    }

    gtk_widget_destroy(dialog);
  }
}

static void
avatar_chooser_response_cb(GtkWidget *dialog, int response_id,
                           OssoABookContactField *field)
{
  if (response_id == GTK_RESPONSE_OK)
  {
    GdkPixbuf *pixbuf = osso_abook_avatar_chooser_dialog_get_pixbuf(
        OSSO_ABOOK_AVATAR_CHOOSER_DIALOG(dialog));
    const char *icon_name = osso_abook_avatar_chooser_dialog_get_icon_name(
        OSSO_ABOOK_AVATAR_CHOOSER_DIALOG(dialog));
    OssoABookContact *master_contact =
      osso_abook_contact_field_get_master_contact(field);
    gboolean pixbuff_small = FALSE;

    if (master_contact)
    {
      g_signal_handlers_disconnect_matched(
        master_contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
        0, 0, NULL, avatar_image_changed_cb, field);
    }

    if (icon_name)
      OSSO_ABOOK_NOTE(AVATAR, "preselected avatar selected: %s", icon_name);
    else if (pixbuf)
    {
      int w = gdk_pixbuf_get_width(pixbuf);
      int h = gdk_pixbuf_get_height(pixbuf);

      if (MAX(w, h) >= 72)
      {
        GtkWidget *editor_dialog = osso_abook_avatar_editor_dialog_new(
            gtk_window_get_transient_for(GTK_WINDOW(dialog)), pixbuf);

        g_signal_connect(editor_dialog, "response",
                         G_CALLBACK(avatar_editor_response_cb), field);
        gtk_widget_show(editor_dialog);
      }
      else
      {
        OSSO_ABOOK_NOTE(AVATAR, "skipping small avatar: %dx%d px", w, h);
        pixbuff_small = TRUE;
      }
    }

    if (icon_name || pixbuff_small)
    {
      osso_abook_contact_set_pixbuf(OSSO_ABOOK_CONTACT(get_avatar(field)),
                                    pixbuf, NULL, NULL);
      field_modified(field);
    }
  }

  gtk_widget_destroy(dialog);
}

static void
avatar_button_clicked_cb(GtkWidget *widget, OssoABookContactField *field)
{
  GtkWidget *dialog = osso_abook_avatar_chooser_dialog_new(
      GTK_WINDOW(gtk_widget_get_ancestor(widget, GTK_TYPE_WINDOW)));

  osso_abook_set_portrait_mode_supported(GTK_WINDOW(dialog), FALSE);
  osso_abook_avatar_chooser_dialog_set_contact(
    OSSO_ABOOK_AVATAR_CHOOSER_DIALOG(dialog),
    osso_abook_contact_field_get_master_contact(field));
  g_signal_connect(dialog, "response",
                   G_CALLBACK(avatar_chooser_response_cb), field);
  gtk_widget_show(dialog);
}

static void
avatar_button_destroy_cb(OssoABookAvatarImage *avatar_image,
                         OssoABookContact *master_contact)
{
  OssoABookAvatar *avatar = osso_abook_avatar_image_get_avatar(avatar_image);

  g_signal_handlers_disconnect_matched(
    master_contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
    0, 0, NULL, osso_abook_contact_attach, avatar);
  g_signal_handlers_disconnect_matched(
    master_contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
    0, 0, NULL, osso_abook_contact_detach, avatar);
}

GtkWidget *
get_image_editor_widget(OssoABookContactField *field)
{
  OssoABookContact *master_contact =
    osso_abook_contact_field_get_master_contact(field);
  OssoABookContact *contact = osso_abook_contact_new();
  gchar *uid = osso_abook_create_temporary_uid();
  GtkWidget *button;

  e_contact_set(E_CONTACT(contact), E_CONTACT_UID, uid);
  g_free(uid);

  if (master_contact)
  {
    GdkPixbuf *avatar_image =
      osso_abook_avatar_get_image(OSSO_ABOOK_AVATAR(master_contact));
    EVCardAttribute *attr_photo =
      e_vcard_get_attribute(E_VCARD(master_contact), A_EVC_PHOTO);
    GList *l;

    for (l = osso_abook_contact_get_roster_contacts(master_contact); l;
         l = g_list_delete_link(l, l))
    {
      osso_abook_contact_attach(contact, l->data);
    }

    g_signal_connect_swapped(master_contact, "contact-attached",
                             G_CALLBACK(osso_abook_contact_attach), contact);
    g_signal_connect_swapped(master_contact, "contact-detached",
                             G_CALLBACK(osso_abook_contact_detach), contact);

    if (avatar_image)
    {
      osso_abook_contact_set_pixbuf(contact, avatar_image, NULL, NULL);

      if (!attr_photo)
      {
        g_signal_connect(master_contact, "notify::avatar-image",
                         G_CALLBACK(avatar_image_changed_cb), field);
      }
    }

    button = osso_abook_avatar_button_new(OSSO_ABOOK_AVATAR(contact), 128);
    gtk_widget_set_can_focus(GTK_WIDGET(button), FALSE);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(avatar_button_clicked_cb), field);
    g_signal_connect_data(gtk_bin_get_child(GTK_BIN(button)), "destroy",
                          G_CALLBACK(avatar_button_destroy_cb),
                          g_object_ref(master_contact),
                          (GClosureNotify)&g_object_unref, 0);
  }
  else
  {
    button = osso_abook_avatar_button_new(OSSO_ABOOK_AVATAR(contact), 128);
    gtk_widget_set_can_focus(GTK_WIDGET(button), FALSE);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(avatar_button_clicked_cb), field);
  }

  return button;
}

static GtkWidget *
get_country_editor_widget(OssoABookContactField *field)
{
  static GtkTreeModel *model = NULL;
  const char *title = osso_abook_contact_field_get_display_title(field);
  const char *value = osso_abook_contact_field_get_display_value(field);
  GtkWidget *sel = hildon_touch_selector_entry_new_text();
  HildonEntry *entry;
  GtkWidget *button;

  if (!model)
  {
    GStrv countries = osso_abook_msgids_get_countries();
    GStrv country = countries;

    model = hildon_touch_selector_get_model(HILDON_TOUCH_SELECTOR(sel), 0);
    g_object_ref(model);

    while (*country)
    {
      hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(sel), *country);
      country++;
    }

    g_strfreev(countries);
  }
  else
    hildon_touch_selector_set_model(HILDON_TOUCH_SELECTOR(sel), 0, model);

  entry = hildon_touch_selector_entry_get_entry(
      HILDON_TOUCH_SELECTOR_ENTRY(sel));
  gtk_entry_set_text(GTK_ENTRY(entry), value);
  button = hildon_picker_button_new(HILDON_SIZE_FINGER_HEIGHT,
                                    HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
  hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(button),
                                    HILDON_TOUCH_SELECTOR(sel));
  hildon_button_set_title(HILDON_BUTTON(button), title);
  hildon_button_set_title_alignment(HILDON_BUTTON(button), 0.0, 0.5);
  hildon_button_set_value(HILDON_BUTTON(button), value);
  hildon_button_set_value_alignment(HILDON_BUTTON(button), 0.0, 0.5);
  gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5);
  g_signal_connect_swapped(button, "value-changed",
                           G_CALLBACK(field_modified), field);

  return button;
}

static void
action_widget_destroy_cb(GtkWidget *widget, OssoABookContactFieldAction *action)
{
  g_object_unref(action->widget);
  action->widget = NULL;
}

static OssoABookContactFieldAction *
action_new(OssoABookContactField *field, OssoABookContactAction contact_action,
           TpProtocol *protocol, GtkWidget *widget,
           OssoABookContactFieldActionLayoutFlags layout_flags)
{
  OssoABookContactFieldAction *action;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  if (!widget)
    widget = osso_abook_contact_field_create_button(field, NULL, NULL);

  action = g_slice_new0(OssoABookContactFieldAction);
  action->field = field;
  action->layout_flags = layout_flags;
  action->contact_action = contact_action;
  action->ref_cnt = 1;
  action->widget = g_object_ref_sink(widget);

  if (protocol)
    action->protocol = g_object_ref(protocol);

  if (action->field)
  {
    g_object_add_weak_pointer((GObject *)action->field,
                              (gpointer *)&action->field);
  }

  g_signal_connect(widget, "destroy",
                   G_CALLBACK(action_widget_destroy_cb), action);

  return action;
}

static gboolean
secondary_vcard_field_is_tel(TpAccount *account)
{
  const gchar *const *schemes = tp_account_get_uri_schemes(account);

  if (!schemes)
    return FALSE;

  while (*schemes)
  {
    if (!strcmp(*schemes, "tel"))
      return TRUE;

    schemes++;
  }

  return FALSE;
}

static GList *
get_mail_actions(OssoABookContactField *field)
{
  OssoABookContactFieldAction *action;

  action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_MAILTO, NULL, NULL,
                      OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);

  return g_list_prepend(NULL, action);
}

static GList *
get_date_actions(OssoABookContactField *field)
{
  OssoABookContactFieldAction *action;

  action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_DATE, NULL, NULL,
                      OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);

  return g_list_prepend(NULL, action);
}

static GList *
get_address_actions(OssoABookContactField *field)
{
  OssoABookContactFieldAction *action;

  action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_MAP, NULL, NULL,
                      OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);

  return g_list_prepend(NULL, action);
}

static GList *
get_note_actions(OssoABookContactField *field)
{
  GtkWidget *button = osso_abook_contact_field_create_button_with_style(
      field, 0, 0, OSSO_ABOOK_BUTTON_STYLE_NOTE);
  OssoABookContactFieldAction *action;

  action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_NONE, NULL, button,
                      OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);

  return g_list_prepend(NULL, action);
}

static GList *
get_url_actions(OssoABookContactField *field)
{
  OssoABookContactFieldAction *action;

  action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_URL, NULL, NULL,
                      OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);

  return g_list_prepend(NULL, action);
}

static GList *
get_tel_protocols()
{
  GList *all = osso_abook_account_manager_list_accounts(NULL, NULL, NULL);
  GList *tel_accounts = NULL;
  GList *l;

  for (l = all; l; l = l->next)
  {
    if (tp_account_is_enabled(l->data))
    {
      if (secondary_vcard_field_is_tel(l->data))
      {
        TpProtocol *protocol =
            osso_abook_account_manager_get_account_protocol_object(NULL,
                                                                   l->data);
        if (protocol && !g_list_find(tel_accounts, protocol))
          tel_accounts = g_list_prepend(tel_accounts, protocol);
      }
    }
  }

  g_list_free(all);

  return tel_accounts;
}

static GList *
get_phone_actions(OssoABookContactField *field)
{
  OssoABookContact *master_contact;
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  TpProtocol *protocol_tel =
      osso_abook_account_manager_get_protocol_object(NULL, "tel");
  GList *actions = NULL;

  if ((osso_abook_contact_field_get_flags(field) &
       OSSO_ABOOK_CONTACT_FIELD_FLAGS_DEVICE_MASK) ==
      OSSO_ABOOK_CONTACT_FIELD_FLAGS_FAX)
  {
    GtkWidget *button =
        osso_abook_contact_field_create_button_with_label(field);

    actions = g_list_prepend(
          actions,
          action_new(field, OSSO_ABOOK_CONTACT_ACTION_NONE,
                     protocol_tel, button,
                     OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY));
  }
  else
  {
    if (!OSSO_ABOOK_IS_VOICEMAIL_CONTACT(priv->master_contact))
    {
      GList *protocols;

      for (protocols = get_tel_protocols(); protocols;
           protocols = g_list_delete_link(protocols, protocols))
      {
        TpProtocol *protocol = protocols->data;
        TpCapabilities *caps = tp_protocol_get_capabilities(protocol);

        if (tp_capabilities_supports_text_chats(caps))
        {
          const char *msgid = _("addr_bd_cont_starter_chat");
          GtkWidget *button;
          gchar *title = g_strdup_printf(
                msgid, tp_protocol_get_english_name(protocol));

          button = osso_abook_contact_field_create_button(
                field, title, tp_protocol_get_icon_name(protocol));
          actions = g_list_prepend(
                actions,
                action_new(field, OSSO_ABOOK_CONTACT_ACTION_CHATTO,
                           protocol, button,
                           OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_EXTRA |
                           OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY));
          g_free(title);
        }

        if (tp_capabilities_supports_audio_call(caps, TP_HANDLE_TYPE_CONTACT))
        {
          const char *msgid = _("addr_bd_cont_starter_call");
          GtkWidget *button;
          gchar *title = g_strdup_printf(
                msgid, tp_protocol_get_english_name(protocol));

          button = osso_abook_contact_field_create_button(
                field, title, tp_protocol_get_icon_name(protocol));
          actions = g_list_prepend(
                actions,
                action_new(field, OSSO_ABOOK_CONTACT_ACTION_VOIPTO_AUDIO,
                           protocol, button,
                           OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_EXTRA |
                           OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY));
          g_free(title);

        }
      }
    }

    master_contact = osso_abook_contact_field_get_master_contact(field);

    if (!OSSO_ABOOK_IS_VOICEMAIL_CONTACT(master_contact))
    {
      const char *other_id = NULL;
      gchar *sms_title = NULL;
      GtkWidget *button;

      if (priv->template)
      {
        if (!g_str_has_prefix(priv->template->msgid, "sms_"))
        {
          if (priv->template->msgid)
          {
            other_id = strrchr(priv->template->msgid, '_');

            if (other_id && !strcmp(other_id, "_other"))
              other_id = NULL;
          }
        }
        else
          sms_title = create_field_title(field, NULL, NULL);
      }

      if (!sms_title)
      {
        gchar *detail_id;
        const char *title;
        const char *stitle;

        if (!other_id)
          other_id = "";

        title = "sms";
        detail_id = g_strconcat("sms", other_id, "_detail", NULL);
        stitle = detail_id;

        if (priv->message_map)
        {
          title = osso_abook_message_map_lookup(priv->message_map, "sms");

          if (priv->message_map)
          {
            stitle = osso_abook_message_map_lookup(priv->message_map,
                                                   detail_id);
          }
        }

        sms_title = create_field_title(field, title, stitle);
        g_free(detail_id);
      }

      button = osso_abook_contact_field_create_button(field, sms_title,
                                                      "general_sms");
      actions = g_list_prepend(
            actions,
            action_new(field, OSSO_ABOOK_CONTACT_ACTION_SMS,
                       protocol_tel, button,
                       OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_EXPANDABLE));
      g_free(sms_title);
    }

    actions = g_list_prepend(
          actions, action_new(field, OSSO_ABOOK_CONTACT_ACTION_TEL,
                              protocol_tel, NULL,
                              OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY));
  }

  g_object_unref(protocol_tel);

  return actions;
}

static gchar *
get_url_attr_value(EVCardAttribute *attr)
{
  char *val = e_vcard_attribute_get_value(attr);
  gchar *s;

  if (!val || !g_str_has_prefix(val, "http://"))
    return val;

  s = g_strdup(val + 7);
  g_free(val);

  return s;
}

static gchar *
get_gender_attr_value(EVCardAttribute *attr)
{
  char *val = e_vcard_attribute_get_value(attr);

  if (val)
  {
    if (!strcmp(val, "male"))
    {
      g_free(val);
      return g_strdup(g_dgettext("osso-addressbook", "addr_va_general_male"));
    }
    else if (!strcmp(val, "female"))
    {
      g_free(val);
      return g_strdup(g_dgettext("osso-addressbook", "addr_va_general_female"));
    }
  }

  g_free(val);

  return g_strdup(g_dgettext("osso-addressbook", "addr_va_general_undefined"));
}

static gchar *
get_name_attr_value(EVCardAttribute *attr)
{
  GList *val;
  const char *val1 = NULL;
  const char *val2 = NULL;
  const char *space = "";

  val = e_vcard_attribute_get_values(attr);

  if (val)
  {
    val1 = val->data;
    val = val->next;

    if (val)
      val2 = val->data;
  }

  if (IS_EMPTY(val1))
    val1 = "";

  if (IS_EMPTY(val2))
    val2 = "";

  if (!IS_EMPTY(val1) && !IS_EMPTY(val2))
    space = " ";

  return g_strconcat(val2, space, val1, NULL);
}

static gchar *
get_country_attr_value(EVCardAttribute *attr)
{
  gchar *val = e_vcard_attribute_get_value(attr);

  /* If I get it correctly, we have to return dgettext's msgid of the localised
     country, which, on Leste, happens to be its English name, On Fremantle
     there is "osso-countries" package, but we use iso-codes here */

  /* FIXME - check if we do that correctly, once we have UI working */
  if (val)
  {
    const char *msgid = osso_abook_msgids_rfind(NULL, "iso_3166", val);

    if (msgid)
    {
      g_free(val);
      val = g_strdup(msgid);
    }
  }
  else
  {
    val = g_strdup(nl_langinfo(_NL_ADDRESS_COUNTRY_NAME));
    e_vcard_attribute_add_value(attr, val);
  }

  return val;
}

static gchar *
get_formatted_address(EVCardAttribute *attr)
{
  static GRegex *match_trim_hspace = NULL;
  static GRegex *match_multiple_hspace = NULL;
  GList *val;
  char *fmt_addr;
  gchar *s;
  gchar *trimmed;
  OssoABookAddress address = {};

  val = e_vcard_attribute_get_values(attr);

  if (val)
  {
    address.p_o_box = val->data;
    val = val->next;
  }

  if (val)
  {
    address.extension = val->data;
    val = val->next;
  }

  if (val)
  {
    address.street = val->data;
    val = val->next;
  }

  if (val)
  {
    address.city = val->data;
    val = val->next;
  }

  if (val)
  {
    address.region = val->data;
    val = val->next;
  }

  if (val)
  {
    address.postal = val->data;
    val = val->next;
  }

  if (val)
    address.country = val->data;

  fmt_addr = osso_abook_address_format(&address);

  if (!fmt_addr)
    return NULL;

  if (!match_trim_hspace)
  {
    /* matches hspaces at start or at the end */
    match_trim_hspace = g_regex_new("(^\\h+|\\h+$)",
                                    G_REGEX_OPTIMIZE | G_REGEX_MULTILINE, 0,
                                    NULL);
  }

  if (!match_multiple_hspace)
  {
    /* matches 2 or more hspaces */
    match_multiple_hspace = g_regex_new("\\h{2,}",
                                        G_REGEX_OPTIMIZE | G_REGEX_MULTILINE, 0,
                                        NULL);
  }

  trimmed = g_regex_replace_literal(match_trim_hspace, fmt_addr, -1, 0, "", 0,
                                    NULL);
  g_free(fmt_addr);
  s = g_strchomp(g_regex_replace_literal(match_multiple_hspace, trimmed, -1, 0,
                                         "#", 0, NULL));
  g_free(trimmed);

  return s;
}

static gchar *
get_address_attr_value(EVCardAttribute *attr)
{
  static GRegex *match_hspace_digit = NULL;
  static GRegex *match_vspace = NULL;
  gchar *s1;
  gchar *val;
  gchar *s2;

  if (!match_hspace_digit)
    match_hspace_digit = g_regex_new("\\h+(\\d)", G_REGEX_OPTIMIZE, 0, NULL);

  if (!match_vspace)
    match_vspace = g_regex_new("\\v+", G_REGEX_OPTIMIZE, 0, NULL);

  s1 = get_formatted_address(attr);

  if (!s1)
    return NULL;

  /* \xC2\xA0 is UTF-8 for \u00A0 (non-breaking space), \1 is 'first group' */
  s2 = g_regex_replace(match_hspace_digit, s1, -1, 0, "\xC2\xA0\\1", 0, NULL);
  g_free(s1);
  val = g_regex_replace_literal(match_vspace, s2, -1, 0, " ", 0, NULL);
  g_free(s2);

  return val;
}

static gchar *
get_date_attr_value(EVCardAttribute *attr)
{
  GDate *date = gdate_from_attribute(attr);
  gchar sdate[100];

  sdate[0] = 0;

  if (date)
  {
    g_date_strftime(sdate, sizeof(sdate),
                    dgettext("hildon-libs", "wdgt_va_date_medium"), date);
    g_date_free(date);
  }

  return g_strdup(g_strchomp(g_strchug(sdate)));
}

static OssoABookContactFieldTemplate general_templates[] =
{
  {
    A_EVC_N, NULL, NULL, NULL, 501,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_MANDATORY |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    NULL,
    get_name_children,
    update_name_attribute,
    synthesize_name_attributes,
    get_name_attr_value
  },

  {
    A_EVC_FN, "name", NULL, NULL, 500,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_MANDATORY |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    NULL,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_TEL, "mobile", "phone", "general_call", 412,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL,
    get_phone_actions,
    get_tele_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_TEL, "mobile_home", "phone", "general_call", 411,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_HOME,
    get_phone_actions,
    get_tele_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_TEL, "mobile_work", "phone", "general_call", 410,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_WORK,
    get_phone_actions,
    get_tele_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_TEL, "phone", "phone", "general_call", 404,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE,
    get_phone_actions,
    get_tele_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_TEL, "phone_home", "phone", "general_call", 403,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_HOME,
    get_phone_actions,
    get_tele_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_TEL, "phone_work", "phone", "general_call", 402,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_WORK,
    get_phone_actions,
    get_tele_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_TEL, "phone_other", "phone", "general_call", 401,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER,
    get_phone_actions,
    get_tele_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_TEL, "phone_fax", "phone", "general_call", 400,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_FAX,
    get_phone_actions,
    get_tele_text_editor_widget,
    NULL,
    &update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_EMAIL, "email", NULL, "general_email", 302,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED,
    get_mail_actions,
    get_noautocap_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_EMAIL, "email_home", NULL, "general_email", 301,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_HOME,
    get_mail_actions,
    get_noautocap_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_EMAIL, "email_work", NULL, "general_email", 300,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_WORK,
    get_mail_actions,
    get_noautocap_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_PHOTO, "image", NULL, NULL, 120,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NO_LABEL |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_MANDATORY |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    get_image_editor_widget,
    NULL,
    update_image_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_BDAY, "birthday", NULL, "general_calendar", 104,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    get_date_actions,
    get_date_editor_widget,
    NULL,
    update_date_attribute,
    NULL,
    get_date_attr_value
  },
  {
    A_EVC_ADR, "address", NULL, "general_map", 103,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED,
    get_address_actions,
    NULL,
    get_address_children,
    update_address_attribute,
    synthesize_address_attributes,
    get_address_attr_value
  },
  {
    A_EVC_ADR, "address_home", NULL, "general_map", 102,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_HOME,
    get_address_actions,
    NULL,
    get_address_children,
    update_address_attribute,
    synthesize_address_attributes,
    get_address_attr_value
  },
  {
    A_EVC_ADR, "address_work", NULL, "general_map", 101,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED |
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_WORK,
    get_address_actions,
    NULL,
    get_address_children,
    update_address_attribute,
    synthesize_address_attributes,
    get_address_attr_value
  },
  {
    A_EVC_URL, "webpage", NULL, "general_web", 100,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE,
    get_url_actions,
    get_noautocap_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    get_url_attr_value
  },
  {
    OSSO_ABOOK_VCA_GENDER, "gender", NULL, "general_business_card", 4,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    get_gender_editor_widget,
    NULL,
    update_gender_attribute,
    NULL,
    get_gender_attr_value
  },
  {
    A_EVC_NICKNAME, "nickname", NULL, "general_business_card", 3,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_TITLE, "title", NULL, "general_business_card", 2,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_ORG, "company", NULL, "general_business_card", 1,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    A_EVC_NOTE, "note", NULL, "general_notes", 0,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    get_note_actions,
    get_note_editor_widget,
    NULL,
    update_note_attribute,
    NULL,
    e_vcard_attribute_get_value
  }
};

static OssoABookContactFieldTemplate address_templates[] =
{
  {
    NULL, "address_p_o_box", NULL, NULL, 4,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    NULL, "address_extension", NULL, NULL, 6,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    NULL, "address_street", NULL, NULL, 5,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    NULL, "address_city", NULL, NULL, 2,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    NULL, "address_region", NULL, NULL, 1,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    NULL, "address_postal", NULL, NULL, 3,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE,
    NULL,
    get_alpha_num_text_editor_widget,
    NULL,
    update_zip_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    NULL, "address_country", NULL, NULL, 0,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE,
    NULL,
    get_country_editor_widget,
    NULL,
    update_country_attribute,
    NULL,
    get_country_attr_value
  }
};

static OssoABookContactFieldTemplate name_templates[] =
{
  {
    NULL, "last_name", NULL, NULL, 2,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NO_LABEL,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    NULL, "first_name", NULL, NULL, 4,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_NO_LABEL,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  }
};

static GList *
get_children(OssoABookContactField *field, OssoABookContactFieldTemplate *t,
             int count)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  GList *values = e_vcard_attribute_get_values(priv->attribute);
  GList *list = NULL;
  OssoABookContactFieldTemplate *last = &t[count];

  while (t < last)
  {
    const char *val;

    if (values)
    {
      val = values->data;
      values = values->next;
    }
    else
      val = NULL;

    if (t->msgid)
    {
      EVCardAttribute *attr = e_vcard_attribute_new(NULL, "");
      OssoABookContactField *child_field;
      OssoABookContactFieldPrivate *child_priv;

      if (!IS_EMPTY(val))
        e_vcard_attribute_add_value(attr, val);

      child_field = g_object_new(OSSO_ABOOK_TYPE_CONTACT_FIELD,
                                 "message-map", priv->message_map,
                                 "master-contact", priv->master_contact,
                                 "attribute", attr,
                                 NULL);
      e_vcard_attribute_free(attr);
      child_priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(child_field);
      child_priv->parent = field;
      child_priv->template = t;
      g_object_add_weak_pointer((gpointer)field,
                                (gpointer *)&child_priv->parent);
      list = g_list_prepend(list, child_field);
    }

    t++;
  }

  return g_list_reverse(list);
}

static GList *
get_name_children(OssoABookContactField *field)
{
  return get_children(field, &name_templates[0], G_N_ELEMENTS(name_templates));
}

static GList *
get_address_children(OssoABookContactField *field)
{
  return get_children(field, &address_templates[0],
                      G_N_ELEMENTS(address_templates));
}

static OssoABookContactFieldTemplateGroup template_groups[] =
{
  { general_templates, G_N_ELEMENTS(general_templates) },
  { address_templates, G_N_ELEMENTS(address_templates) },
  { name_templates, G_N_ELEMENTS(name_templates) }
};

static void
osso_abook_contact_field_set_property(GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(object);

  switch (property_id)
  {
    case PROP_ATTRIBUTE:
    {
      g_assert(NULL == priv->attribute);

      priv->borrowed_attribute = g_value_get_pointer(value);
      priv->attribute = e_vcard_attribute_copy(priv->borrowed_attribute);

      if (priv->modified)
      {
        priv->modified = FALSE;
        g_object_notify(object, "modified");
      }

      break;
    }
    case PROP_MESSAGE_MAP:
    {
      osso_abook_contact_field_set_message_map(
        OSSO_ABOOK_CONTACT_FIELD(object), g_value_get_boxed(value));
      break;
    }
    case PROP_MASTER_CONTACT:
    {
      g_assert(NULL == priv->master_contact);
      priv->master_contact = g_value_dup_object(value);
      break;
    }
    case PROP_ROSTER_CONTACT:
    {
      g_assert(NULL == priv->roster_contact);
      priv->roster_contact = g_value_dup_object(value);
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
osso_abook_contact_field_get_property(GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec)
{
  OssoABookContactField *field = OSSO_ABOOK_CONTACT_FIELD(object);

  switch (property_id)
  {
    case PROP_NAME:
    {
      g_value_set_string(value, osso_abook_contact_field_get_name(field));
      break;
    }
    case PROP_FLAGS:
    {
      g_value_set_flags(value, osso_abook_contact_field_get_flags(field));
      break;
    }
    case PROP_ATTRIBUTE:
    {
      g_value_set_pointer(value,
                          OSSO_ABOOK_CONTACT_FIELD_PRIVATE(object)->attribute);
      break;
    }
    case PROP_MESSAGE_MAP:
    {
      g_value_set_boxed(value, osso_abook_contact_field_get_message_map(field));
      break;
    }
    case PROP_MASTER_CONTACT:
    {
      g_value_set_object(value,
                         osso_abook_contact_field_get_master_contact(field));
      break;
    }
    case PROP_ROSTER_CONTACT:
    {
      g_value_set_object(value,
                         osso_abook_contact_field_get_roster_contact(field));
      break;
    }
    case PROP_DISPLAY_TITLE:
    {
      g_value_set_string(value,
                         osso_abook_contact_field_get_display_title(field));
      break;
    }
    case PROP_DISPLAY_VALUE:
    {
      g_value_set_string(value,
                         osso_abook_contact_field_get_display_value(field));
      break;
    }
    case PROP_ICON_NAME:
    {
      g_value_set_string(value, osso_abook_contact_field_get_icon_name(field));
      break;
    }
    case PROP_SORT_WEIGHT:
    {
      g_value_set_int(value, osso_abook_contact_field_get_sort_weight(field));
      break;
    }
    case PROP_LABEL_WIDGET:
    {
      g_value_set_object(value,
                         osso_abook_contact_field_get_label_widget(field));
      break;
    }
    case PROP_EDITOR_WIDGET:
    {
      g_value_set_object(value,
                         osso_abook_contact_field_get_editor_widget(field));
      break;
    }
    case PROP_MODIFIED:
    {
      g_value_set_boolean(value, osso_abook_contact_field_is_modified(field));
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
osso_abook_contact_field_update_dispose(GObject *object)
{
  OssoABookContactField *field = OSSO_ABOOK_CONTACT_FIELD(object);
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (priv->parent)
  {
    g_object_remove_weak_pointer(G_OBJECT(priv->parent),
                                 (gpointer *)&priv->parent);
    priv->parent = NULL;
  }

  while (priv->children)
  {
    g_object_unref(priv->children->data);
    priv->children = g_list_delete_link(priv->children, priv->children);
  }

  if (priv->editor_widget)
  {
    g_object_unref(priv->editor_widget);
    priv->editor_widget = NULL;
  }

  if (priv->label)
  {
    g_object_unref(priv->label);
    priv->label = NULL;
  }

  if (priv->roster_contact)
  {
    g_object_unref(priv->roster_contact);
    priv->roster_contact = NULL;
  }

  if (priv->master_contact)
  {
    g_signal_handlers_disconnect_matched(
      priv->master_contact, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
      0, 0, NULL, avatar_image_changed_cb, field);
    g_object_unref(priv->master_contact);
    priv->master_contact = NULL;
  }

  G_OBJECT_CLASS(osso_abook_contact_field_parent_class)->dispose(object);
}

/* FIXME - find a better algo */
static gboolean
is_template_builtin(OssoABookContactFieldTemplate *t)
{
  OssoABookContactFieldTemplateGroup *group = template_groups;

  while (group->template > t || &group->template[group->count] <= t)
  {
    if (++group == &template_groups[G_N_ELEMENTS(template_groups)])
      return FALSE;
  }

  return TRUE;
}

static void
osso_abook_contact_field_finalize(GObject *object)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(object);

  if (priv->template)
  {
    if (!is_template_builtin(priv->template))
    {
      g_free(priv->template->msgid);
      g_free(priv->template->icon_name);
      g_slice_free(OssoABookContactFieldTemplate, priv->template);
    }
  }

  free_attributes_list(priv->secondary_attributes);

  if (priv->attribute)
    e_vcard_attribute_free(priv->attribute);

  if (priv->message_map)
    g_hash_table_unref(priv->message_map);

  g_free(priv->secondary_title);
  g_free(priv->display_title);
  g_free(priv->display_value);
  g_free(priv->path);

  G_OBJECT_CLASS(osso_abook_contact_field_parent_class)->finalize(object);
}

static void
osso_abook_contact_field_update_label(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (priv->label)
  {
    g_object_set(priv->label,
                 "title", osso_abook_contact_field_get_display_title(field),
                 "value", osso_abook_contact_field_get_secondary_title(field),
                 NULL);
  }
}

static void
osso_abook_contact_field_class_init(OssoABookContactFieldClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  int i;

  for (i = 0; i < G_N_ELEMENTS(template_groups); i++)
  {
    OssoABookContactFieldTemplateGroup *group = &template_groups[i];
    int j;

    for (j = 0; j < group->count; j++)
    {
      OssoABookContactFieldTemplate *t = &group->template[j];

      if (t->name)
        t->name = g_intern_static_string(t->name);
    }
  }

  object_class->constructed = osso_abook_contact_field_constructed;
  object_class->set_property = osso_abook_contact_field_set_property;
  object_class->get_property = osso_abook_contact_field_get_property;
  object_class->dispose = osso_abook_contact_field_update_dispose;
  object_class->finalize = osso_abook_contact_field_finalize;

  klass->update_label = osso_abook_contact_field_update_label;

  g_object_class_install_property(
    object_class, PROP_NAME,
    g_param_spec_string(
      "name",
      "Name",
      "The name of this field",
      0,
      GTK_PARAM_READABLE));
  g_object_class_install_property(
    object_class, PROP_FLAGS,
    g_param_spec_flags(
      "flags",
      "Flags",
      "Flags describing this field",
      OSSO_ABOOK_TYPE_CONTACT_FIELD_FLAGS,
      0,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_ATTRIBUTE,
    g_param_spec_pointer(
      "attribute",
      "Attribute",
      "The VCard attribute backing this field",
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_MESSAGE_MAP,
    g_param_spec_boxed(
      "message-map",
      "Message Map",
      "Mapping for generic message ids to context ids",
      G_TYPE_HASH_TABLE,
      GTK_PARAM_READWRITE));
  g_object_class_install_property(
    object_class, PROP_MASTER_CONTACT,
    g_param_spec_object(
      "master-contact",
      "Master Contact",
      "The master contact if one is associated with this field",
      OSSO_ABOOK_TYPE_CONTACT,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_ROSTER_CONTACT,
    g_param_spec_object(
      "roster-contact",
      "Roster Contact",
      "The roster contact if one is associated with this field",
      OSSO_ABOOK_TYPE_CONTACT,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_DISPLAY_TITLE,
    g_param_spec_string(
      "display-title",
      "Display Title",
      "The message ID field labels",
      NULL,
      GTK_PARAM_READABLE));
  g_object_class_install_property(
    object_class, PROP_DISPLAY_VALUE,
    g_param_spec_string(
      "display-value",
      "Display Value",
      "A human friendly string represeting the attribute value",
      NULL,
      GTK_PARAM_READABLE));
  g_object_class_install_property(
    object_class, PROP_ICON_NAME,
    g_param_spec_string(
      "icon-name",
      "Icon Name",
      "The name of the icon to use with this field",
      NULL,
      GTK_PARAM_READABLE));
  g_object_class_install_property(
    object_class, PROP_SORT_WEIGHT,
    g_param_spec_int(
      "sort-weight",
      "Sort Weight",
      "The weight of this field when sorting them",
      G_MININT32, G_MAXINT32, G_MININT32,
      GTK_PARAM_READABLE));
  g_object_class_install_property(
    object_class, PROP_LABEL_WIDGET,
    g_param_spec_object(
      "label-widget",
      "Label Widget",
      "The widget describing this field",
      GTK_TYPE_WIDGET,
      GTK_PARAM_READABLE));
  g_object_class_install_property(
    object_class, PROP_EDITOR_WIDGET,
    g_param_spec_object(
      "editor-widget",
      "Editor Widget",
      "The widget for editing this field",
      GTK_TYPE_WIDGET,
      GTK_PARAM_READABLE));
  g_object_class_install_property(
    object_class, PROP_MODIFIED,
    g_param_spec_boolean(
      "modified",
      "Modified",
      "Check if the field has been modified",
      FALSE,
      GTK_PARAM_READABLE));
}

static void
osso_abook_contact_field_init(OssoABookContactField *field)
{}

GtkWidget *
osso_abook_contact_field_get_editor_widget(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (!priv->editor_widget)
  {
    g_return_val_if_fail(NULL != priv->template, NULL);

    if (priv->template->editor_widget)
    {
      priv->editor_widget = priv->template->editor_widget(field);

      if (priv->editor_widget)
      {
        gtk_widget_set_sensitive(
          priv->editor_widget,
          !osso_abook_contact_field_is_readonly(field) &&
          !osso_abook_contact_field_is_bound_im(field));
        g_object_ref_sink(priv->editor_widget);
      }
    }
  }

  return priv->editor_widget;
}

gboolean
osso_abook_contact_field_has_editor_widget(OssoABookContactField *field)
{
  OssoABookContactFieldTemplate *t;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), FALSE);

  t = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->template;

  if (t)
    return t->editor_widget != NULL;

  return FALSE;
}

gboolean
osso_abook_contact_field_is_readonly(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), TRUE);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (priv->parent && osso_abook_contact_field_is_readonly(priv->parent))
    return TRUE;

  return osso_abook_contact_attribute_is_readonly(priv->attribute);
}

gboolean
osso_abook_contact_field_is_bound_im(OssoABookContactField *field)
{
  OssoABookContact *roster_contact;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), TRUE);

  roster_contact = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->roster_contact;

  if (roster_contact)
    return !osso_abook_contact_has_invalid_username(roster_contact);

  return FALSE;
}

gboolean
osso_abook_contact_field_is_modified(OssoABookContactField *field)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), FALSE);

  return OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->modified;
}

gboolean
osso_abook_contact_field_is_empty(OssoABookContactField *field)
{
  gchar *display_value;
  gchar *s;
  gboolean empty;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), TRUE);

  display_value = g_strdup(osso_abook_contact_field_get_display_value(field));
  s = g_strchomp(display_value);

  empty = !s || !*s;

  g_free(display_value);

  return empty;
}

gboolean
osso_abook_contact_field_is_mandatory(OssoABookContactField *field)
{
  return osso_abook_contact_field_get_flags(field) &
         OSSO_ABOOK_CONTACT_FIELD_FLAGS_MANDATORY;
}

void
osso_abook_contact_field_set_mandatory(OssoABookContactField *field,
                                       gboolean mandatory)
{
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field));

  if (osso_abook_contact_field_is_mandatory(field) != mandatory)
  {
    OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->mandatory = mandatory;
    g_object_notify(G_OBJECT(field), "flags");
  }
}

gboolean
osso_abook_contact_field_activate(OssoABookContactField *field)
{
  GtkWidget *editor_widget;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), FALSE);

  editor_widget = osso_abook_contact_field_get_editor_widget(field);

  if (GTK_IS_BUTTON(editor_widget))
  {
    gtk_button_clicked(GTK_BUTTON(editor_widget));
    return osso_abook_contact_field_is_modified(field);
  }

  return TRUE;
}

TpAccount *
osso_abook_contact_field_get_account(OssoABookContactField *field)
{
  OssoABookContact *contact;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  contact = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->roster_contact;

  if (contact)
    return osso_abook_contact_get_account(contact);

  return NULL;
}

const char *
osso_abook_contact_field_get_icon_name(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  g_return_val_if_fail(NULL != priv->template, NULL);

  return priv->template->icon_name;
}

int
osso_abook_contact_field_get_sort_weight(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), G_MININT32);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  g_return_val_if_fail(NULL != priv->template, G_MININT32);

  return priv->template->sort_weight;
}

OssoABookContact *
osso_abook_contact_field_get_roster_contact(OssoABookContactField *field)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  return OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->roster_contact;
}

OssoABookContact *
osso_abook_contact_field_get_master_contact(OssoABookContactField *field)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  return OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->master_contact;
}

const char *
osso_abook_contact_field_get_display_title(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (!priv->display_title)
  {
    OssoABookContactFieldTemplate *t = priv->template;

    g_return_val_if_fail(NULL != priv->template, NULL);

    if (t->msgid)
    {
      const char *msgid = t->msgid;

      if (priv->message_map)
        msgid = osso_abook_message_map_lookup(priv->message_map, msgid);

      priv->display_title = g_strdup(msgid);
    }
  }

  return priv->display_title;
}

const char *
osso_abook_contact_field_get_secondary_title(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (!priv->secondary_title)
  {
    OssoABookContactFieldTemplate *t = priv->template;

    if (t)
    {
      if (t->flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED)
      {
        gchar *msgid = g_strconcat(t->msgid, "_detail", NULL);
        const char *s = msgid;

        if (priv->message_map)
          s = osso_abook_message_map_lookup(priv->message_map, msgid);

        priv->secondary_title = g_strdup(s);
        g_free(msgid);
      }
    }
  }

  return priv->secondary_title;
}

static void
update_if_modified(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (!priv->modified)
    return;

  if (priv->template && priv->template->update)
    priv->template->update(field);

  priv->modified = FALSE;
  g_object_notify(G_OBJECT(field), "modified");
}

const char *
osso_abook_contact_field_get_display_value(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  update_if_modified(field);

  if (!priv->display_value)
  {
    g_return_val_if_fail(NULL != priv->template, NULL);

    if (priv->template->get_attr_value)
      priv->display_value = priv->template->get_attr_value(priv->attribute);

    if (!priv->display_value)
      priv->display_value = g_strdup("");
  }

  return priv->display_value;
}

GList *
osso_abook_contact_field_get_secondary_attributes(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (priv->template)
  {
    OssoABookContactFieldTemplate *t = priv->template;

    if (t->update && osso_abook_contact_field_is_modified(field))
      t->update(field);

    if (t->synthesize_attributes && !priv->secondary_attributes)
      t->synthesize_attributes(field);
  }

  return priv->secondary_attributes;
}

EVCardAttribute *
osso_abook_contact_field_get_borrowed_attribute(OssoABookContactField *field)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  return OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->borrowed_attribute;
}

EVCardAttribute *
osso_abook_contact_field_get_attribute(OssoABookContactField *field)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  update_if_modified(field);

  return OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->attribute;
}

gboolean
osso_abook_contact_field_has_children(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), FALSE);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  g_return_val_if_fail(NULL != priv->template, FALSE);

  return priv->template->children != NULL;
}

static void
child_modified_cb(OssoABookContactField *child_field, GParamSpec *pspec,
                  OssoABookContactField *field)
{
  if (osso_abook_contact_field_is_modified(child_field))
    field_modified(field);
}

GList *
osso_abook_contact_field_get_children(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (!priv->children)
  {
    if (priv->template && priv->template->children)
    {
      GList *l;

      priv->children = priv->template->children(field);

      for (l = priv->children; l; l = l->next)
      {
        g_signal_connect(l->data, "notify::modified",
                         G_CALLBACK(child_modified_cb), field);
      }
    }
  }

  return priv->children;
}

OssoABookContactField *
osso_abook_contact_field_get_parent(OssoABookContactField *field)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  return OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->parent;
}

OssoABookContactFieldFlags
osso_abook_contact_field_get_flags(OssoABookContactField *field)
{
  OssoABookContactFieldFlags flags;

  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field),
                       OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (priv->template)
    flags = priv->template->flags;
  else
    flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE;

  if (priv->mandatory)
    flags |= OSSO_ABOOK_CONTACT_FIELD_FLAGS_MANDATORY;

  if (priv->parent && osso_abook_contact_field_is_mandatory(priv->parent))
    flags |= OSSO_ABOOK_CONTACT_FIELD_FLAGS_MANDATORY;

  return flags;
}

const char *
osso_abook_contact_field_get_path(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (!priv->path)
  {
    const char *name = osso_abook_contact_field_get_name(field);

    if (priv->parent)
    {
      const char *parent_path = osso_abook_contact_field_get_path(priv->parent);

      if (name && *name)
        priv->path = g_strconcat(parent_path, ".", name, NULL);
      else
      {
        gint idx = g_list_index(osso_abook_contact_field_get_children(
                                  priv->parent), field);

        priv->path = g_strdup_printf("%s[%d]", parent_path, idx);
      }
    }
    else
      priv->path = g_strdup(name);
  }

  return priv->path;
}

const char *
osso_abook_contact_field_get_name(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  g_return_val_if_fail(NULL != priv->template, NULL);

  return priv->template->name;
}

void
osso_abook_contact_field_set_message_map(OssoABookContactField *field,
                                         GHashTable *mapping)
{
  OssoABookContactFieldPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field));

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (priv->message_map != mapping)
  {
    GList *l;
    OssoABookContactFieldTemplate *t = priv->template;

    if (priv->message_map)
      g_hash_table_unref(priv->message_map);

    if (mapping)
      priv->message_map = g_hash_table_ref(mapping);
    else
      priv->message_map = NULL;

    if (t)
    {
      if (t->msgid)
      {
        g_free(priv->display_title);
        priv->display_title = NULL;
      }
      else if (!(t->flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_DYNAMIC))
      {
        g_free(priv->secondary_title);
        priv->secondary_title = NULL;
      }
    }

    for (l = priv->children; l; l = l->next)
      osso_abook_contact_field_set_message_map(l->data, mapping);

    OSSO_ABOOK_CONTACT_FIELD_GET_CLASS(field)->update_label(field);
  }

  g_object_notify(G_OBJECT(field), "message-map");
}

GHashTable *
osso_abook_contact_field_get_message_map(OssoABookContactField *field)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  return OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->message_map;
}

OssoABookContactField *
osso_abook_contact_field_new_full(GHashTable *message_map,
                                  OssoABookContact *master_contact,
                                  OssoABookContact *roster_contact,
                                  EVCardAttribute *attribute)
{
  OssoABookContactField *field = g_object_new(OSSO_ABOOK_TYPE_CONTACT_FIELD,
                                              "message-map",
                                              message_map,
                                              "master-contact",
                                              master_contact,
                                              "roster-contact",
                                              roster_contact,
                                              "attribute",
                                              attribute,
                                              NULL);

  if (!OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field)->template)
  {
    g_object_unref(field);
    field = NULL;
  }

  return field;
}

OssoABookContactField *
osso_abook_contact_field_new(GHashTable *message_map,
                             OssoABookContact *master_contact,
                             EVCardAttribute *attribute)
{
  g_return_val_if_fail(
    NULL == master_contact || OSSO_ABOOK_IS_CONTACT(master_contact), NULL);
  g_return_val_if_fail(NULL != attribute, NULL);

  return osso_abook_contact_field_new_full(message_map, master_contact,
                                           NULL, attribute);
}

GtkWidget *
osso_abook_contact_field_create_button(OssoABookContactField *field,
                                       const char *title, const char *icon_name)
{
  return osso_abook_contact_field_create_button_with_style(
           field, title, icon_name, OSSO_ABOOK_BUTTON_STYLE_NORMAL);
}

GList *
osso_abook_contact_field_get_actions(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  g_return_val_if_fail(NULL != priv->template, NULL);

  if (priv->template->actions)
    return priv->template->actions(field);
  else
  {
    GtkWidget *button =
      osso_abook_contact_field_create_button_with_label(field);
    OssoABookContactFieldAction *action;

    action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_NONE, 0, button,
                        OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);

    return g_list_prepend(NULL, action);
  }
}

static void
dynamic_editor_widget_changed_cb(GtkEditable *editable, gpointer user_data)
{
  _osso_abook_entry_set_normal_style(GTK_ENTRY(editable));
}

static GtkWidget *
get_dynamic_editor_widget(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  GtkWidget *widget = get_noautocap_text_editor_widget(field);
  const char *uid;

  if (!priv->roster_contact)
    return widget;

  if (!osso_abook_contact_has_invalid_username(priv->roster_contact))
    return widget;

  uid = e_contact_get_const(E_CONTACT(priv->roster_contact), E_CONTACT_UID);

  if (!uid || !*uid)
    return widget;

  _osso_abook_entry_set_error_style(GTK_ENTRY(widget));
  g_signal_connect(widget, "changed",
                   G_CALLBACK(dynamic_editor_widget_changed_cb), NULL);

  return widget;
}

static GList *
create_dynamic_actions(OssoABookContactField *field, OssoABookCapsFlags caps,
                       TpProtocol *protocol)
{
  const char *display_title = osso_abook_contact_field_get_display_title(field);
  GList *actions = NULL;
  gchar *title;
  GtkWidget *button;
  OssoABookContactFieldAction *action;

  if ((caps & OSSO_ABOOK_CAPS_VIDEO))
  {
    title = g_strdup_printf(
        g_dgettext("osso-addressbook", "addr_bd_cont_starter_video_call"),
        display_title);
    button = osso_abook_contact_field_create_button(field, title, NULL);
    action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_VOIPTO_VIDEO, protocol,
                        button, OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);
    actions = g_list_prepend(actions, action);
    g_free(title);
  }

  if ((caps & OSSO_ABOOK_CAPS_VOICE))
  {
    title = g_strdup_printf(
        g_dgettext("osso-addressbook", "addr_bd_cont_starter_call"),
        display_title);
    button = osso_abook_contact_field_create_button(field, title, NULL);
    action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_VOIPTO, protocol,
                        button, OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);
    actions = g_list_prepend(actions, action);
    g_free(title);
  }

  if ((caps & OSSO_ABOOK_CAPS_CHAT))
  {
    title = g_strdup_printf(
        g_dgettext("osso-addressbook", "addr_bd_cont_starter_chat"),
        display_title);
    button = osso_abook_contact_field_create_button(field, title, NULL);
    action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_CHATTO, protocol,
                        button, OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);
    actions = g_list_prepend(actions, action);
    g_free(title);
  }

  return actions;
}

static GList *
get_dynamic_actions(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  const char *attr_name = e_vcard_attribute_get_name(priv->attribute);
  GList *actions = NULL;
  gchar *title;
  GtkWidget *button;
  OssoABookContactFieldAction *action;

  if (priv->roster_contact)
  {
    TpProtocol *protocol =
      osso_abook_contact_get_protocol(priv->roster_contact);

    if (osso_abook_contact_has_invalid_username(priv->roster_contact))
    {
      title = g_strdup_printf(
          g_dgettext("osso-addressbook",
                     "addr_bd_cont_starter_chat"),
          osso_abook_contact_field_get_display_title(field));

      button = osso_abook_contact_field_create_button(field, title, NULL);
      action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_CHATTO_ERROR,
                          protocol, button,
                          OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);
      actions = g_list_prepend(actions, action);
      g_free(title);

      title = g_strdup_printf(
          g_dgettext("osso-addressbook",
                     "addr_bd_cont_starter_call"),
          osso_abook_contact_field_get_display_title(field));
      button = osso_abook_contact_field_create_button(field, title, NULL);
      action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_VOIPTO_ERROR,
                          protocol, button,
                          OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);
      actions = g_list_prepend(actions, action);
      g_free(title);
    }
    else
    {
      OssoABookCapsFlags flags = osso_abook_caps_get_capabilities(
          OSSO_ABOOK_CAPS(priv->roster_contact));
      actions = create_dynamic_actions(field, flags, protocol);

      if (!actions)
      {
        title = g_strdup_printf(
            g_dgettext("osso-addressbook",
                       "addr_bd_cont_starter_chat"),
            osso_abook_contact_field_get_display_title(field));
        button = osso_abook_contact_field_create_button(field, title, NULL);
        action = action_new(field,
                            OSSO_ABOOK_CONTACT_ACTION_CHATTO,
                            protocol,
                            button,
                            OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);
        actions = g_list_prepend(actions, action);
        g_free(title);
      }
    }
  }
  else
  {
    GList *protocols =
      osso_abook_account_manager_list_protocols(NULL, attr_name, TRUE);

    if (protocols)
    {
      while (protocols)
      {
        TpProtocol *protocol = protocols->data;
        OssoABookCapsFlags flags = osso_abook_caps_from_tp_protocol(protocol);
        actions = g_list_concat(create_dynamic_actions(field, flags, protocol),
                                actions);
        g_object_unref(protocol);
        protocols = g_list_delete_link(protocols, protocols);
      }
    }
    else
    {
      GList *accounts =
        osso_abook_account_manager_list_by_vcard_field(NULL, attr_name);

      if (accounts)
      {
        while (!tp_account_is_enabled(accounts->data))
          accounts = g_list_delete_link(accounts, accounts);
      }

      if (accounts)
      {
        button = osso_abook_contact_field_create_button(field, NULL, NULL);
        action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_BIND, NULL, button,
                            OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);
        actions = g_list_prepend(actions, action);
        g_list_free(accounts);
      }
      else
      {
        button = osso_abook_contact_field_create_button(field, NULL, NULL);
        action = action_new(field, OSSO_ABOOK_CONTACT_ACTION_CREATE_ACCOUNT,
                            NULL, button,
                            OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY);
        actions = g_list_prepend(actions, action);
      }
    }
  }

  return actions;
}

static OssoABookContactFieldFlags
flags_from_value(const char *v)
{
  OssoABookContactFieldFlags flags;

  if (!g_ascii_strcasecmp(v, "CELL") || !g_ascii_strcasecmp(v, "CAR"))
    flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL;
  else if (!g_ascii_strcasecmp(v, "VOICE"))
    flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE;
  else if (!g_ascii_strcasecmp(v, "FAX"))
    flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_FAX;
  else if (!g_ascii_strcasecmp(v, "HOME"))
    flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_HOME;
  else if (!g_ascii_strcasecmp(v, "WORK"))
    flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_WORK;
  else if (!g_ascii_strcasecmp(v, "PREF"))
    flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE;
  else
    flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER;

  return flags;
}

static OssoABookContactFieldFlags
flags_from_params(GList *param)
{
  OssoABookContactFieldFlags flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE;

  for (; param; param = param->next)
  {
    EVCardAttributeParam *p = param->data;

    if (!strcmp(e_vcard_attribute_param_get_name(p), "TYPE"))
    {
      GList *vals;

      for (vals = e_vcard_attribute_param_get_values(p); vals;
           vals = vals->next)
      {
        if (vals->data)
          flags |= flags_from_value(vals->data);
      }
    }
  }

  return flags;
}

static int
flags_type(OssoABookContactFieldFlags flags)
{
  return flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_TYPE_MASK;
}

static OssoABookContactFieldTemplate *
find_template_by_name_and_flags(OssoABookContactFieldTemplate *t, size_t t_size,
                                const char *name,
                                OssoABookContactFieldFlags type)
{
  size_t i;

  for (i = 0; i < t_size; i++)
  {
    OssoABookContactFieldTemplate *_t = &t[i];

    if (_t->name == name)
    {
      if (!(_t->flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER) &&
          (flags_type(_t->flags) == type))
      {
        return _t;
      }
    }
  }

  return NULL;
}

static void
osso_abook_contact_field_constructed(GObject *object)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(object);
  const char *attr_name = e_vcard_attribute_get_name(priv->attribute);
  OssoABookContactFieldFlags flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE;
  static const OssoABookContactFieldFlags flags_tel[3] =
  {
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_FAX
  };
  GList *param;
  TpProtocol *protocol = NULL;
  GList *contacts = NULL;
  const gchar *name;
  gchar *icon_name = NULL;
  gboolean titles_set = FALSE;
  int i;

  if (IS_EMPTY(attr_name))
    return;

  name = g_intern_string(attr_name);

  param = e_vcard_attribute_get_params(priv->attribute);

  if (param)
  {
    flags = flags_from_params(param);

    if ((flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_DEVICE_MASK))
      flags &= ~OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER;
  }

  if (!param || !(flags & (OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER |
                           OSSO_ABOOK_CONTACT_FIELD_FLAGS_FAX)))
  {
    if (!strcmp(e_vcard_attribute_get_name(priv->attribute), A_EVC_TEL))
      flags |= OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE;
  }

  if (priv->template)
    return;

  priv->template = find_template_by_name_and_flags(
      general_templates, G_N_ELEMENTS(general_templates), name, flags);

  if (priv->template)
    return;

  priv->template = find_template_by_name_and_flags(
      general_templates, G_N_ELEMENTS(general_templates), name,
      flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_DEVICE_MASK);

  if (priv->template)
    return;

  for (i = 0; i < G_N_ELEMENTS(flags_tel); i++)
  {
    if (flags_tel[i] & flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_DEVICE_MASK)
    {
      priv->template = find_template_by_name_and_flags(
          general_templates, G_N_ELEMENTS(general_templates), name,
          flags_tel[i]);

      if (priv->template)
        return;
    }
  }

  for (i = 0; i < G_N_ELEMENTS(general_templates); i++)
  {
    OssoABookContactFieldTemplate *tmplt = &general_templates[i];

    if ((name == tmplt->name) &&
        tmplt->flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER)
    {
      priv->template = tmplt;
      return;
    }
  }

  if (!priv->roster_contact && priv->master_contact)
  {
    contacts = osso_abook_contact_find_roster_contacts_for_attribute(
        priv->master_contact, priv->attribute);

    if (contacts)
      priv->roster_contact = contacts->data;

    if (priv->roster_contact)
      g_object_ref(priv->roster_contact);
  }

  if (priv->roster_contact)
  {
    TpAccount *account = osso_abook_contact_get_account(priv->roster_contact);

    if (account)
    {
      const gchar *display_title = NULL;
      const gchar *tmp_icon_name;
      protocol = osso_abook_contact_get_protocol(priv->roster_contact);

      if (contacts && !contacts->next)
        display_title = tp_account_get_display_name(account);

      if (IS_EMPTY(priv->display_title))
        display_title = tp_protocol_get_english_name(protocol);

      priv->display_title = g_strdup(display_title);
      priv->secondary_title =
        g_strdup(osso_abook_tp_account_get_bound_name(account));
      titles_set = TRUE;

      tmp_icon_name = tp_account_get_icon_name(account);

      if (!IS_EMPTY(tmp_icon_name))
        icon_name = g_strdup(tmp_icon_name);
    }
  }

  if (!titles_set)
    protocol = osso_abook_contact_attribute_get_protocol(priv->attribute);

  g_list_free(contacts);

  if (protocol)
  {
    OssoABookContactFieldTemplate t;

    t.name = g_intern_string(e_vcard_attribute_get_name(priv->attribute));
    t.msgid = NULL;
    t.title = NULL;
    t.icon_name = NULL;
    t.sort_weight = 200;
    t.flags = OSSO_ABOOK_CONTACT_FIELD_FLAGS_DYNAMIC;
    t.actions = NULL;
    t.editor_widget = get_dynamic_editor_widget;
    t.children = NULL;
    t.update = update_text_attribute;
    t.synthesize_attributes = NULL;
    t.get_attr_value = e_vcard_attribute_get_value;

    priv->template = g_slice_new(OssoABookContactFieldTemplate);
    *priv->template = t;
    priv->template->actions = get_dynamic_actions;

    if (!icon_name)
      icon_name = g_strdup(tp_protocol_get_icon_name(protocol));

    priv->template->icon_name = icon_name;

    if (!priv->display_title)
      priv->display_title = g_strdup(tp_protocol_get_english_name(protocol));

    g_object_unref(protocol);
  }
}

GList *
osso_abook_contact_field_get_actions_full(OssoABookContactField *field,
                                          gboolean button)
{
  if (button)
    return osso_abook_contact_field_get_actions(field);

  return g_list_prepend(
           NULL,
           action_new(field, OSSO_ABOOK_CONTACT_ACTION_NONE, NULL,
                      osso_abook_contact_field_create_button_with_label(field),
                      OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY));
}

OssoABookContactFieldActionLayoutFlags
osso_abook_contact_field_action_get_layout_flags(
  OssoABookContactFieldAction *action)
{
  g_return_val_if_fail(NULL != action,
                       OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_NORMAL);

  return action->layout_flags;
}

OssoABookContactAction
osso_abook_contact_field_action_get_action(OssoABookContactFieldAction *action)
{
  g_return_val_if_fail(NULL != action, OSSO_ABOOK_CONTACT_ACTION_NONE);

  return action->contact_action;
}

TpProtocol *
osso_abook_contact_field_action_get_protocol(
  OssoABookContactFieldAction *action)
{
  g_return_val_if_fail(NULL != action, NULL);

  return action->protocol;
}

GtkWidget *
osso_abook_contact_field_action_get_widget(OssoABookContactFieldAction *action)
{
  g_return_val_if_fail(NULL != action, NULL);

  return action->widget;
}

OssoABookContactField *
osso_abook_contact_field_action_get_field(OssoABookContactFieldAction *action)
{
  g_return_val_if_fail(NULL != action, NULL);

  return action->field;
}

OssoABookContactFieldAction *
osso_abook_contact_field_action_ref(OssoABookContactFieldAction *action)
{
  g_return_val_if_fail(NULL != action, NULL);

  g_atomic_int_add(&action->ref_cnt, 1);

  return action;
}

void
osso_abook_contact_field_action_unref(OssoABookContactFieldAction *action)
{
  g_return_if_fail(NULL != action);

  if (g_atomic_int_add(&action->ref_cnt, -1) == 0)
  {
    if (action->field)
      g_object_remove_weak_pointer((GObject *)action->field,
                                   (gpointer *)&action->field);

    if (action->protocol)
      g_object_unref(action->protocol);

    if (action->widget)
    {
      g_signal_handlers_disconnect_matched(
        action->widget, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0,
        NULL, action_widget_destroy_cb, action);
      g_object_unref(action->widget);
    }

    g_slice_free(OssoABookContactFieldAction, action);
  }
}

int
osso_abook_contact_field_cmp(OssoABookContactField *a,
                             OssoABookContactField *b)
{
  OssoABookContactField *pa = osso_abook_contact_field_get_parent(a);
  OssoABookContactField *pb = osso_abook_contact_field_get_parent(b);
  gint rv = 0;

  if (pa != pb)
  {
    if (pa == b)
      return 1;

    if (a == pb)
      return -1;

    if (!pa)
      pa = a;

    if (!pb)
      pb = b;

    rv = osso_abook_contact_field_cmp(pa, pb);
  }

  if (rv)
    return rv;

  rv = osso_abook_contact_field_get_sort_weight(b) -
    osso_abook_contact_field_get_sort_weight(a);

  if (rv)
    return rv;

  rv = g_strcmp0(osso_abook_contact_field_get_display_title(a),
                 osso_abook_contact_field_get_display_title(b));

  if (rv)
    return rv;

  rv = g_strcmp0(osso_abook_contact_field_get_display_value(a),
                 osso_abook_contact_field_get_display_value(b));

  if (rv)
    return rv;

  if (a != b)
  {
    if (a >= b)
      return 1;

    return -1;
  }

  return rv;
}

/* *INDENT-OFF* */
gboolean
osso_abook_contact_field_action_start_with_callback(
  OssoABookContactFieldAction *action, GtkWindow *parent,
  OssoABookContactActionStartCb callback, gpointer callback_data)
/* *INDENT-ON* */
{
  TpAccount *account;
  gboolean aborted = TRUE;
  gboolean rv = FALSE;

  g_return_val_if_fail(action != NULL, FALSE);
  g_return_val_if_fail(action->field != NULL, FALSE);
  g_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), FALSE);

  account = osso_abook_contact_field_action_request_account(
      action, parent, &aborted);

  if (!aborted)
  {
    OssoABookContactAction contact_action = action->contact_action;

    if (!account && (contact_action == OSSO_ABOOK_CONTACT_ACTION_BIND))
      contact_action = OSSO_ABOOK_CONTACT_ACTION_CREATE_ACCOUNT;

    rv = osso_abook_contact_action_start_with_callback(
        contact_action,
        osso_abook_contact_field_get_master_contact(action->field),
        osso_abook_contact_field_get_attribute(action->field),
        account, parent, callback, callback_data);
  }

  if (account)
    g_object_unref(account);

  return rv;
}

static void
flags_to_vcard_attribute_values(EVCardAttribute *attr,
                                OssoABookContactFieldFlags flags)
{
  GList *l = NULL;

  if (flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL)
  {
    flags &= ~OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE;
    l = g_list_prepend(l, "CELL");
  }

  if (flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE)
    l = g_list_prepend(l, "VOICE");

  if (flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_FAX)
    l = g_list_prepend(l, "FAX");

  if (flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_HOME)
    l = g_list_prepend(l, "HOME");

  if (flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_WORK)
    l = g_list_prepend(l, "WORK");

  if (l)
  {
    EVCardAttributeParam *param = e_vcard_attribute_param_new(A_EVC_TYPE);

    while (l)
    {
      osso_abook_e_vcard_attribute_param_merge_value(param, l->data, NULL);
      l = g_list_delete_link(l, l);
    }

    e_vcard_attribute_remove_param(attr, A_EVC_TYPE);

    if (param)
      e_vcard_attribute_add_param(attr, param);
  }
  else
    e_vcard_attribute_remove_param(attr, A_EVC_TYPE);
}

/* *INDENT-OFF* */
GList *
osso_abook_contact_field_list_supported_fields(
  GHashTable *message_map, OssoABookContact *master_contact,
  OssoABookContactFieldPredicate accept_field, gpointer user_data)
/* *INDENT-ON* */
{
  OssoABookContactFieldTemplate *t;
  GList *fields = NULL;
  GList *protocols;
  GList *rosters;
  GList *l;

  g_return_val_if_fail(NULL == master_contact ||
                       OSSO_ABOOK_IS_CONTACT(master_contact), NULL);

  for (t = general_templates;
       t < &general_templates[G_N_ELEMENTS(general_templates)]; t++)
  {
    if (!(t->flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER))
    {
      EVCardAttribute *attr = e_vcard_attribute_new(NULL, t->name);
      OssoABookContactField *field;

      flags_to_vcard_attribute_values(attr, t->flags);
      field = osso_abook_contact_field_new_full(message_map, master_contact,
                                                NULL, attr);
      e_vcard_attribute_free(attr);

      if (field)
      {
        if (!accept_field || accept_field(field, user_data))
          fields = g_list_prepend(fields, field);
        else
          g_object_unref(field);
      }
    }
  }

  for (rosters = osso_abook_roster_manager_list_rosters(NULL); rosters;
       rosters = g_list_delete_link(rosters, rosters))
  {
    OssoABookRoster *roster = rosters->data;
    const char *vcard_field = osso_abook_roster_get_vcard_field(roster);

    if (vcard_field && strcmp(vcard_field, "tel"))
    {
      TpProtocol *protocol;
      OssoABookContactField *field;
      OssoABookContact *roster_contact = osso_abook_contact_new();
      EVCardAttribute *attr = e_vcard_attribute_new(NULL, vcard_field);

      osso_abook_contact_set_roster(roster_contact, roster);
      protocol = osso_abook_roster_get_protocol(roster);

      if (protocol)
      {
        e_vcard_attribute_add_param_with_value(
          attr, e_vcard_attribute_param_new(A_EVC_TYPE),
          tp_protocol_get_name(protocol));
        g_object_unref(protocol);
      }

      field = osso_abook_contact_field_new_full(message_map, master_contact,
                                                roster_contact, attr);
      g_object_unref(roster_contact);
      e_vcard_attribute_free(attr);

      if (field)
      {
        if (!accept_field || accept_field(field, user_data))
          fields = g_list_prepend(fields, field);
        else
          g_object_unref(field);
      }
    }
  }

  protocols = osso_abook_account_manager_get_protocols(NULL);

  for (l = protocols; l; l = l->next)
  {
    TpProtocol *protocol = l->data;

    if (!_osso_abook_tp_protocol_has_rosters(protocol))
    {
      const char *vcard_field = tp_protocol_get_vcard_field(protocol);

      if (vcard_field && strcmp(vcard_field, "tel"))
      {
        EVCardAttribute *attr = e_vcard_attribute_new(NULL, vcard_field);
        OssoABookContactField *field;

        e_vcard_attribute_add_param_with_value(
          attr, e_vcard_attribute_param_new(A_EVC_TYPE),
          tp_protocol_get_name(protocol));
        field = osso_abook_contact_field_new_full(
            message_map, master_contact, NULL, attr);
        e_vcard_attribute_free(attr);

        if (field)
        {
          if (!accept_field || accept_field(field, user_data))
            fields = g_list_prepend(fields, field);
          else
            g_object_unref(field);
        }
      }
    }
  }

  g_list_free(protocols);

  return fields;
}

GList *
osso_abook_contact_field_list_account_fields(GHashTable *message_map,
                                             OssoABookContact *master_contact)
{
  GList *fields = NULL;
  GList *protocols = NULL;
  GList *accounts;

  g_return_val_if_fail(NULL == master_contact ||
                       OSSO_ABOOK_IS_CONTACT(master_contact), NULL);

  for (accounts = osso_abook_account_manager_list_enabled_accounts(NULL);
       accounts; accounts = g_list_delete_link(accounts, accounts))
  {
    TpAccount *account = accounts->data;
    gchar *vcard_field = _osso_abook_tp_account_get_vcard_field(accounts->data);
    const char *protocol = tp_account_get_protocol_name(account);

    if (vcard_field && protocol && strcmp(vcard_field, "tel"))
    {
      OssoABookRoster *roster = osso_abook_roster_manager_get_roster(
          NULL, tp_account_get_path_suffix(account));

      if (roster || !osso_abook_string_list_find(protocols, protocol))
      {
        OssoABookContact *roster_contact = NULL;
        OssoABookContactField *field;
        EVCardAttribute *attr;

        if (roster)
        {
          roster_contact = osso_abook_contact_new();
          osso_abook_contact_set_roster(roster_contact, roster);
        }
        else
          protocols = g_list_prepend(protocols, (gpointer)protocol);

        attr = e_vcard_attribute_new(NULL, vcard_field);
        e_vcard_attribute_add_param_with_value(
          attr, e_vcard_attribute_param_new(A_EVC_TYPE), protocol);
        field = osso_abook_contact_field_new_full(message_map, master_contact,
                                                  roster_contact, attr);

        if (field)
          fields = g_list_prepend(fields, field);

        if (roster_contact)
          g_object_unref(roster_contact);

        e_vcard_attribute_free(attr);
      }
    }

    g_free(vcard_field);
  }

  g_list_free(protocols);

  return fields;
}

static void
update_field_type(OssoABookContactField *field, OssoABookContactField *update)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  OssoABookContactFieldTemplate *new_t;
  OssoABookContactFieldTemplate *t;
  OssoABookContactFieldFlags new_flags;

  g_return_if_fail(priv && priv->template);

  t = priv->template;
  new_t = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(update)->template;

  new_flags = flags_type(new_t->flags);

  if (new_flags != flags_type(t->flags))
  {
    flags_to_vcard_attribute_values(
      osso_abook_contact_field_get_attribute(field), new_flags);

    if (t->msgid)
    {
      g_free(priv->display_title);
      priv->display_title = NULL;
    }

    if (!(t->flags & OSSO_ABOOK_CONTACT_FIELD_FLAGS_DYNAMIC))
    {
      g_free(priv->secondary_title);
      priv->secondary_title = NULL;
    }

    priv->modified = TRUE;
    priv->template = new_t;
    OSSO_ABOOK_CONTACT_FIELD_GET_CLASS(field)->update_label(field);
    g_object_notify(G_OBJECT(field), "modified");
  }
}

static gboolean
compare_field_names(OssoABookContactField *field, gpointer user_data)
{
  return
    osso_abook_contact_field_get_name(field) ==
    osso_abook_contact_field_get_name(user_data);
}

static gboolean
compare_field_flags(OssoABookContactField *field, gpointer user_data)
{
  return
    flags_type(osso_abook_contact_field_get_flags(field)) ==
    flags_type(osso_abook_contact_field_get_flags(user_data));
}

static void
update_field_type_dialog_set_title(GtkWindow *window,
                                   OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);
  OssoABookContactFieldTemplate *t = priv->template;
  const char *title = NULL;

  if (t && t->title)
  {
    title = t->title;

    if (priv->message_map)
      title = osso_abook_message_map_lookup(priv->message_map, title);
  }

  if (!title)
    title = osso_abook_contact_field_get_display_title(field);

  osso_abook_set_portrait_mode_supported(window, FALSE);
  gtk_window_set_title(window, title);
}

static void
label_button_clicked_cb(GtkWidget *button, OssoABookContactField *field)
{
  GtkWidget *selector;
  OssoABookContact *contact;
  GtkWidget *dialog;
  OssoABookContactField *selected;

  selector = osso_abook_contact_field_selector_new();
  contact = osso_abook_contact_field_get_master_contact(field);
  osso_abook_contact_field_selector_load(
    OSSO_ABOOK_CONTACT_FIELD_SELECTOR(selector), contact,
    compare_field_names, field);
  hildon_touch_selector_set_active(
    HILDON_TOUCH_SELECTOR(selector), 0,
    osso_abook_contact_field_selector_find_custom(
      OSSO_ABOOK_CONTACT_FIELD_SELECTOR(selector), compare_field_flags,
      field));
  dialog = hildon_picker_dialog_new(
      GTK_WINDOW(gtk_widget_get_ancestor(button, GTK_TYPE_WINDOW)));
  update_field_type_dialog_set_title(GTK_WINDOW(dialog), field);
  hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog),
                                    HILDON_TOUCH_SELECTOR(selector));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
  {
    selected = osso_abook_contact_field_selector_get_selected(
        OSSO_ABOOK_CONTACT_FIELD_SELECTOR(selector));

    if (selected)
    {
      update_field_type(field, selected);

      g_object_unref(selected);
    }
  }

  gtk_widget_destroy(dialog);
}

GtkWidget *
osso_abook_contact_field_get_label_widget(OssoABookContactField *field)
{
  OssoABookContactFieldPrivate *priv;
  GList *l = NULL;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FIELD(field), NULL);

  priv = OSSO_ABOOK_CONTACT_FIELD_PRIVATE(field);

  if (priv->label)
    return priv->label;

  g_return_val_if_fail(NULL != priv->template, NULL);

  if (IS_EMPTY(osso_abook_contact_field_get_display_title(field)))
    return NULL;

  if (!((osso_abook_contact_field_get_flags(field) &
         OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER)) &&
      is_template_builtin(priv->template))
  {
    static const OssoABookContactFieldTemplate *last =
        &general_templates[G_N_ELEMENTS(general_templates)];
    OssoABookContactFieldTemplate *t = priv->template;
    const char *name = t->name;

    if (t >= general_templates && t < last)
    {
      while (t >= general_templates && name == t->name)
        t--;

      for (t = t + 1; t < last && t->name == name; t++)
        l = g_list_prepend(l, t);
    }

    l = g_list_reverse(l);
  }

  if (l && l->next && !osso_abook_contact_field_is_readonly(field))
  {
    const char *title = osso_abook_contact_field_get_display_title(field);
    const char *value = osso_abook_contact_field_get_secondary_title(field);
    GtkWidget *button;

    if (osso_abook_contact_field_has_editor_widget(field))
    {
      button = osso_abook_button_new_with_text(HILDON_SIZE_FINGER_HEIGHT,
                                               title, value);
      osso_abook_button_set_style(OSSO_ABOOK_BUTTON(button),
                                  OSSO_ABOOK_BUTTON_STYLE_PICKER);
    }
    else
    {
      button = hildon_button_new_with_text(
          HILDON_SIZE_FINGER_HEIGHT, HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
          title, value);
      gtk_button_set_alignment(GTK_BUTTON(button), 0.0, 0.5);
      hildon_button_set_style(HILDON_BUTTON(button),
                              HILDON_BUTTON_STYLE_PICKER);
    }

    gtk_widget_set_can_focus(button, FALSE);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(label_button_clicked_cb), field);
    priv->label = button;
  }
  else
  {
    GtkWidget *button = osso_abook_button_new_with_text(
        HILDON_SIZE_FINGER_HEIGHT,
        osso_abook_contact_field_get_display_title(field),
        osso_abook_contact_field_get_secondary_title(field));
    osso_abook_button_set_style(OSSO_ABOOK_BUTTON(button),
                                OSSO_ABOOK_BUTTON_STYLE_LABEL);
    gtk_widget_set_can_focus(button, FALSE);
    priv->label = button;
  }

  g_object_ref_sink(priv->label);
  g_list_free(l);

  return priv->label;
}

TpAccount *
osso_abook_contact_field_action_request_account(
  OssoABookContactFieldAction *action,
  GtkWindow *parent,
  gboolean *aborted)
{
  GList *accounts = NULL;
  EVCardAttribute *field_attr;
  GtkWidget *account_selector;
  const gchar *msgid;
  TpAccount *account = NULL;

  if (aborted)
    *aborted = FALSE;

  g_return_val_if_fail(action != NULL, NULL);
  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), NULL);
  g_return_val_if_fail(NULL != action->field, NULL);

  field_attr = osso_abook_contact_field_get_attribute(action->field);

  if (osso_abook_contact_field_get_roster_contact(action->field))
  {
    OssoABookContact *master_contact;
    GList *roster_contacts;
    GList *l;
    gchar *attr_val;

    master_contact = osso_abook_contact_field_get_master_contact(action->field);
    attr_val = e_vcard_attribute_get_value(field_attr);
    roster_contacts = osso_abook_contact_find_roster_contacts_for_attribute(
        master_contact, field_attr);

    for (l = roster_contacts; l; l = l->next)
    {
      accounts = g_list_prepend(accounts,
                                osso_abook_contact_get_account(l->data));
    }

    g_list_free(roster_contacts);
    g_free(attr_val);
  }
  else if (action->protocol)
  {
    gboolean is_tel = !g_strcmp0(e_vcard_attribute_get_name(field_attr), "tel");

    accounts = osso_abook_account_manager_list_by_protocol(
        NULL, tp_protocol_get_name(action->protocol));

    while (accounts)
    {
      GList *next = accounts->next;

      if (!tp_account_is_enabled(accounts->data) ||
          (is_tel && !secondary_vcard_field_is_tel(accounts->data)))
      {
        accounts = g_list_delete_link(accounts, accounts);
      }

      accounts = next;
    }
  }
  else
  {
    if (action->contact_action == OSSO_ABOOK_CONTACT_ACTION_BIND)
    {
      GList *l;

      accounts = osso_abook_account_manager_list_by_vcard_field(
          NULL, e_vcard_attribute_get_name(field_attr));

      l = accounts;

      while (l)
      {
        GList *next = l->next;

        if (!tp_account_is_enabled(l->data))
          accounts = g_list_delete_link(accounts, l);

        l = next;
      }
    }
  }

  if (accounts)
  {
    if (!accounts->next &&
        (action->contact_action != OSSO_ABOOK_CONTACT_ACTION_BIND))
    {
      account = g_object_ref(accounts->data);
      g_list_free(accounts);
    }
    else
    {
      OssoABookTpAccountModel *model = osso_abook_tp_account_model_new();

      osso_abook_tp_account_model_set_allowed_accounts(model, accounts);
      account_selector = osso_abook_tp_account_selector_new(parent, model);
      g_object_unref(model);
      g_list_free(accounts);

      if (action->contact_action == OSSO_ABOOK_CONTACT_ACTION_CHATTO)
        msgid = "addr_ti_choose_own_account_chat";
      else if (action->contact_action == OSSO_ABOOK_CONTACT_ACTION_BIND)
        msgid = "addr_ti_choose_own_account_bind";
      else
        msgid = "addr_ti_choose_own_account_voip";

      gtk_window_set_title(GTK_WINDOW(account_selector), _(msgid));

      if (gtk_dialog_run(GTK_DIALOG(account_selector)) == GTK_RESPONSE_OK)
      {
        account = osso_abook_tp_account_selector_get_account(
            OSSO_ABOOK_TP_ACCOUNT_SELECTOR(account_selector));
      }
      else
      {
        if (aborted)
          *aborted = 1;
      }

      gtk_widget_destroy(account_selector);
    }
  }

  return account;
}

struct action_start_data
{
  OssoABookContactActionStartCb cb;
  gpointer cb_data;
  guint timeout_id;
  GtkWindow *parent;
  gchar *uid;
  OssoABookContact *contact;
};

static void
destroy_action_start_data(struct action_start_data *data)
{
  if (data)
  {
    if (data->contact)
      g_object_unref(data->contact);

    g_free(data->uid);

    if (data->parent)
    {
      hildon_gtk_window_set_progress_indicator(data->parent, 0);
      g_object_remove_weak_pointer((GObject *)data->parent,
                                   (gpointer *)&data->parent);
    }

    if (data->timeout_id)
      g_source_remove(data->timeout_id);

    g_slice_free(struct action_start_data, data);
  }
}

static gboolean
contact_action_date(GtkWindow *parent, OssoABookContact *contact)
{
  GError *error = NULL;
  DBusGConnection *dbus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);

  if (dbus)
  {
    DBusGProxy *proxy = dbus_g_proxy_new_for_name(dbus,
                                                  "com.nokia.calendar",
                                                  "/",
                                                  "com.nokia.calendar");
    const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

    dbus_g_proxy_call_no_reply(proxy,
                               "open_bday_event",
                               G_TYPE_UINT, 4, /* hmm, what is this? */
                               G_TYPE_STRING, uid,
                               G_TYPE_INVALID,
                               G_TYPE_INVALID);
    g_object_unref(proxy);
    dbus_g_connection_unref(dbus);
  }
  else
    osso_abook_handle_gerror(parent, error);

  return TRUE;
}

static void
open_url(GtkWindow *parent, const gchar *url)
{
  GError *error = NULL;

  if (!hildon_uri_open(url, NULL, &error))
    osso_abook_handle_gerror(parent, error);
}

static gboolean
contact_action_mailto(GtkWindow *parent, OssoABookContact *contact,
                      const char *address)
{
  const char *display_name;
  GString *s;
  gchar *url;

  if (!address)
    return TRUE;

  display_name = osso_abook_contact_get_display_name(contact);

  s = g_string_new("mailto:");

  if (!IS_EMPTY(display_name) && strcmp(display_name, address))
  {
    gchar *tmp;

    g_string_append_c(s, '"');

    tmp = g_uri_escape_string(display_name, NULL, FALSE);
    g_string_append(s, tmp);
    g_free(tmp);

    g_string_append(s, "\" <");

    tmp = g_uri_escape_string(address, NULL, FALSE);
    g_string_append(s, tmp);
    g_free(tmp);

    g_string_append_c(s, '>');
  }
  else
  {
    gchar *tmp = g_uri_escape_string(address, NULL, FALSE);

    g_string_append(s, tmp);
    g_free(tmp);
  }

  url = g_string_free(s, FALSE);
  open_url(parent, url);
  g_free(url);

  return TRUE;
}

static gboolean
contact_action_url(GtkWindow *parent, const char *url)
{
  gchar *_url;

  if (g_regex_match_simple("^[A-Za-z][A-Za-z0-9-+.]*:", url, 0, 0))
    _url = g_strdup(url);
  else
    _url = g_strconcat("http://", url, NULL);

  open_url(parent, _url);
  g_free(_url);

  return TRUE;
}

static void
handle_mmi_code_cb(DBusGProxy *proxy, DBusGProxyCall *call_id,
                   gpointer user_data)
{
  struct action_start_data *data = user_data;
  GError *error = NULL;

  dbus_g_proxy_end_call(proxy, call_id, &error, G_TYPE_INVALID);

  if (data && data->cb)
    data->cb(error, data->parent, data->cb_data);

  if (error)
    g_error_free(error);

  g_object_unref(proxy);
}

static gboolean
contact_action_mmicode(struct action_start_data *data, const char *code)
{
  DBusGConnection *dbus;
  DBusGProxy *proxy;
  GError *error = NULL;

  if (!g_str_has_suffix(code, "#") ||
      code[strcspn(code, OSSO_ABOOK_DTMF_CHARS)])
  {
    return FALSE;
  }

  dbus = dbus_g_bus_get(DBUS_BUS_SESSION, &error);

  if (error)
  {
    OSSO_ABOOK_WARN("%s", error->message);
    g_clear_error(&error);
    return FALSE;
  }

  proxy = dbus_g_proxy_new_for_name(dbus,
                                    "com.nokia.CallUI",
                                    "/com/nokia/CallUI",
                                    "com.nokia.CallUI");
  dbus_g_proxy_begin_call(proxy,
                          "HandleMMICode", handle_mmi_code_cb, data,
                          (GDestroyNotify)destroy_action_start_data,
                          G_TYPE_STRING, code,
                          G_TYPE_INVALID);

  dbus_g_connection_unref(dbus);

  return TRUE;
}

static gboolean
contact_action_error(GtkWindow *parent, const char *msgid, const char *msg)
{
  gchar *s = g_strdup_printf(_(msgid), msg);

  hildon_banner_show_information(GTK_WIDGET(parent), NULL, s);
  g_free(s);

  return FALSE;
}

static gboolean
is_online_connected(TpAccount *account, GError **error)
{
  const gchar *msgid;

  if (!account)
  {
    g_set_error_literal(
          error, OSSO_ABOOK_ERROR, OSSO_ABOOK_ERROR_CANCELLED, "");
    return FALSE;
  }

  if (tp_account_get_connection_status(account, NULL) !=
      TP_CONNECTION_STATUS_CONNECTED)
  {
    TpConnectionPresenceType type =
        tp_account_get_requested_presence(account, NULL, NULL);

    if (type == TP_CONNECTION_PRESENCE_TYPE_OFFLINE ||
        type == TP_CONNECTION_PRESENCE_TYPE_UNSET)
    {
      msgid = "pres_ib_must_be_online";
    }
    else
      msgid = "pres_ib_must_be_network_connected";

    g_set_error_literal(error, OSSO_ABOOK_ERROR, OSSO_ABOOK_ERROR_OFFLINE,
                        g_dgettext("osso-statusbar-presence", msgid));
    return FALSE;
  }

  return TRUE;
}

static void
ensure_channel_async_cb(GObject *source_object, GAsyncResult *res,
                               gpointer user_data)
{
  TpAccountChannelRequest *request = TP_ACCOUNT_CHANNEL_REQUEST(source_object);
  GError *error = NULL;

  if (!tp_account_channel_request_ensure_channel_finish(
        request, res, &error))
  {
    osso_abook_handle_gerror(NULL, error);
  }
}

static gboolean
request_channel(TpAccount *account, OssoABookContactAction action,
                gchar *target_id, GError **error)
{
  guint32 user_action_time;

  g_return_val_if_fail(TP_IS_ACCOUNT(account), FALSE);

  if (!is_online_connected(account, error))
    return FALSE;

  TpAccountChannelRequest *request;

  user_action_time = gtk_get_current_event_time();

  switch (action)
  {
    case OSSO_ABOOK_CONTACT_ACTION_TEL:
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO:
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO_AUDIO:
    {
      request = tp_account_channel_request_new_audio_call(account,
                                                          user_action_time);
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_SMS:
    {
      request = tp_account_channel_request_new_text(account, user_action_time);
      tp_account_channel_request_set_sms_channel(request, TRUE);
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_CHATTO:
    {
      request = tp_account_channel_request_new_text(account, user_action_time);
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO_VIDEO:
    {
      request = tp_account_channel_request_new_audio_video_call(
          account, user_action_time);
      break;
    }
    default:
      g_return_val_if_reached(FALSE);
  }

  tp_account_channel_request_set_target_id(request, TP_HANDLE_TYPE_CONTACT,
                                           target_id);

  tp_account_channel_request_ensure_channel_async(
        request, NULL, NULL, ensure_channel_async_cb, NULL);

  g_object_unref(request);

  return TRUE;
}

static gboolean
set_action_progress_indicator(gpointer user_data)
{
  struct action_start_data *data = user_data;

  if (data->parent)
    hildon_gtk_window_set_progress_indicator(data->parent, 1);

  data->timeout_id = 0;

  return FALSE;
}

static struct action_start_data *
create_action_start_data(OssoABookContactActionStartCb cb, gpointer cb_data,
                         GtkWindow *parent)
{
  struct action_start_data *data = g_slice_new0(struct action_start_data);

  data->cb = cb;
  data->cb_data = cb_data;
  data->parent = parent;

  if (parent)
  {
    data->timeout_id = g_timeout_add(250, set_action_progress_indicator, data);
    g_object_add_weak_pointer((GObject *)data->parent, (gpointer *)&data->parent);
  }

  return data;
}

static void
bind_accept_contact_cb(EBook *book, EBookStatus status, const gchar *id,
                       gpointer closure)
{
  struct action_start_data *data = closure;
  GError *error;

  if (status != E_BOOK_ERROR_OK)
    error = osso_abook_error_new_from_estatus(status);
  else
    error = g_error_new(OSSO_ABOOK_ERROR, OSSO_ABOOK_ERROR_CANCELLED, "%s", "");

  if (data && data->cb )
    data->cb(error, data->parent, data->cb_data);

  if (error)
    g_error_free(error);

  destroy_action_start_data(data);
}

static void
bind_commit_contact_cb(EBook *book, EBookStatus status, gpointer closure)
{
  struct action_start_data *data = closure;

  if (status != E_BOOK_ERROR_OK)
  {
    if (data && data->cb)
    {
      GError *error = osso_abook_error_new_from_estatus(status);
      data->cb(error, data->parent, data->cb_data);
      g_error_free(error);
    }

    destroy_action_start_data(data);
  }
  else
  {
    osso_abook_contact_async_accept(data->contact, data->uid,
                                    bind_accept_contact_cb, data);
  }
}


static GList *
find_attribute(GList *attrs, EVCardAttribute *attr)
{
  GList *l;

  for (l = attrs; l; l = l->next)
  {
    if (osso_abook_e_vcard_attribute_equal(attr, l->data))
      break;
  }

  return l;
}

static void
contact_action_bind(GtkWindow *parent, TpAccount *account,
                    OssoABookContact *contact,
                    EVCardAttribute *attribute,
                    struct action_start_data *data)
{
    OssoABookRoster *roster = osso_abook_roster_manager_get_roster(
        NULL, tp_account_get_path_suffix(account));
    OssoABookContact *new_contact;
    EVCardAttribute *protocol_attribute;
    GList *found;

    data->uid = g_strdup(e_contact_get_const(E_CONTACT(contact),
                                             E_CONTACT_UID));
    data->contact = osso_abook_contact_new();
    osso_abook_contact_set_roster(data->contact, roster);
    e_vcard_add_attribute(E_VCARD(data->contact),
                          e_vcard_attribute_copy(attribute));
    new_contact = osso_abook_contact_new_from_template(E_CONTACT(contact));
    found = find_attribute(e_vcard_get_attributes(E_VCARD(new_contact)),
                           attribute);

    if (found)
      protocol_attribute = found->data;
    else
    {
      protocol_attribute = e_vcard_attribute_copy(attribute);
      e_vcard_add_attribute(E_VCARD(new_contact), protocol_attribute);
    }

    osso_abook_contact_attribute_set_protocol(
          protocol_attribute, tp_account_get_protocol_name(account));
    osso_abook_contact_async_commit(new_contact, NULL, bind_commit_contact_cb,
                                    data);
    g_object_unref(new_contact);
}

static gboolean
request_uri_open(OssoABookContactAction action, const char *id, GError **error)
{
  const char *scheme;
  gchar *uri;
  gboolean rv;

  switch (action)
  {
    case OSSO_ABOOK_CONTACT_ACTION_TEL:
    {
      scheme = "tel://";
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_SMS:
    {
      scheme = "sms://";
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_CHATTO:
    {
      scheme = "xmpp://";
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO:
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO_AUDIO:
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO_VIDEO:
    {
      scheme = "sip://";
      break;
    }
    default:
      g_return_val_if_reached(FALSE);
  }

  uri = g_strconcat(scheme, id, NULL);
  rv = hildon_uri_open_filter(uri, HILDON_URI_MODIFIER_OPEN, NULL, NULL, error);

  g_free(uri);

  return rv;
}

/* *INDENT-OFF* */
gboolean
osso_abook_contact_action_start_with_callback(
  OssoABookContactAction action, OssoABookContact *contact,
  EVCardAttribute *attribute, TpAccount *account, GtkWindow *parent,
  OssoABookContactActionStartCb callback, gpointer callback_data)
/* *INDENT-ON* */
{
  GList *values;
  gboolean rv = FALSE;
  GError *error = NULL;
  struct action_start_data *data = NULL;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), FALSE);
  g_return_val_if_fail(NULL != attribute, FALSE);
  g_return_val_if_fail(!parent || GTK_IS_WINDOW(parent), FALSE);
  g_return_val_if_fail(!account || TP_IS_ACCOUNT(account), FALSE);

  values = e_vcard_attribute_get_values(attribute);

  g_return_val_if_fail(values != NULL, FALSE);

  if (!account &&
      ((action == OSSO_ABOOK_CONTACT_ACTION_TEL) ||
       (action == OSSO_ABOOK_CONTACT_ACTION_SMS)))
  {
    account = osso_abook_account_manager_lookup_by_name(
        osso_abook_account_manager_get_default(), "ring/tel/account0");
  }

  switch (action)
  {
    case OSSO_ABOOK_CONTACT_ACTION_TEL:
    {
      data = create_action_start_data(callback, callback_data, parent);

      if (contact_action_mmicode(data, values->data))
        return TRUE;
    }
    /* fallback */
    case OSSO_ABOOK_CONTACT_ACTION_SMS:
    case OSSO_ABOOK_CONTACT_ACTION_CHATTO:
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO:
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO_AUDIO:
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO_VIDEO:
    {
      if (account)
        rv = request_channel(account, action, values->data, &error);
      else
        rv = request_uri_open(action, values->data, &error);

      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_DATE:
    {
      rv = contact_action_date(parent, contact);
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_MAILTO:
    {
      rv = contact_action_mailto(parent, contact, values->data);
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_URL:
    {
      rv = contact_action_url(parent, values->data);
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_CREATE_ACCOUNT:
    {
      osso_abook_add_im_account_dialog_run(parent);
      rv = TRUE;
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_BIND:
    {
      if (is_online_connected(account, &error))
      {
        data = create_action_start_data(callback, callback_data, parent);
        contact_action_bind(parent, account, contact, attribute, data);
        return TRUE;
      }

      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_CHATTO_ERROR:
    {
      rv = contact_action_error(parent, "addr_ib_invalid_username_chat",
                                values->data);
      break;
    }
    case OSSO_ABOOK_CONTACT_ACTION_VOIPTO_ERROR:
    {
      rv = contact_action_error(parent, "addr_ib_invalid_username_call",
                                values->data);
      break;
    }
    default:
    {
      rv = TRUE;
      break;
    }
  }

  if (callback)
    callback(error, parent, callback_data);

  if (data)
    destroy_action_start_data(data);

  if (error)
    g_error_free(error);

  return rv;
}
