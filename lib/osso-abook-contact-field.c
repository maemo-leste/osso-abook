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

#include <gtk/gtkprivate.h>
#include <hildon/hildon.h>

#include <libintl.h>
#include <langinfo.h>

#include "config.h"

#include "osso-abook-address-format.h"
#include "osso-abook-button.h"
#include "osso-abook-contact-field.h"
#include "osso-abook-enums.h"
#include "osso-abook-avatar-image.h"
#include "osso-abook-message-map.h"
#include "osso-abook-utils-private.h"
#include "osso-abook-msgids.h"
#include "osso-abook-util.h"

typedef struct _OssoABookContactFieldTemplate OssoABookContactFieldTemplate;

struct _OssoABookContactFieldTemplate
{
  const gchar *name;
  gchar *msgid;
  gchar *title;
  gchar *icon_name;
  int sort_weight;
  OssoABookContactFieldFlags flags;
  GList * (*actions)(OssoABookContactField *field);
  GtkWidget * (*editor_widget)(OssoABookContactField *field);
  GList * (*children)(OssoABookContactField *field);
  void (*update)(OssoABookContactField *field);
  void (*synthesize_attributes)(OssoABookContactField *field);
  gchar *(*get_attr_value)(EVCardAttribute *attr);
};

struct _OssoABookContactFieldTemplateGroup
{
  OssoABookContactFieldTemplate *template;
  int count;
};

typedef struct _OssoABookContactFieldTemplateGroup OssoABookContactFieldTemplateGroup;

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
  gboolean modified : 1  /* 0x01 0xFE */;
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
  ((OssoABookContactFieldPrivate *)osso_abook_contact_field_get_instance_private(((OssoABookContactField *)field)))

static GList *
get_name_children(OssoABookContactField *field);

static GList *
get_address_children(OssoABookContactField *field);

static gchar *
get_formatted_address(EVCardAttribute *attr);

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
  EVCardAttribute *attr ;

  g_return_if_fail(NULL != priv->attribute);

  attr = e_vcard_attribute_new(NULL, EVC_FN);
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
    EVCardAttribute *attr_label = e_vcard_attribute_new(NULL, EVC_LABEL);
    EVCardAttributeParam *attr_type;
    GList *l;

    e_vcard_attribute_add_value(attr_label, addr);
    attr_type = e_vcard_attribute_param_new(EVC_TYPE);

    for (l = e_vcard_attribute_get_param(priv->attribute, EVC_TYPE); l;
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
        E_VCARD(OSSO_ABOOK_CONTACT(get_avatar(field))), EVC_PHOTO);

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

  for (i = g_list_length(l); i < attr_count ; i++)
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
      s= v->data;
  }

  if (!s)
    s = "";

  if (g_utf8_validate(s, -1, NULL))
  {
    GString *str = g_string_new(NULL);

    while (*s)
    {
      gunichar uch = g_utf8_get_char(s);

      switch(g_unichar_break_type(uch))
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
                   G_CALLBACK(note_editor_notify_cursor_position_cb),widget);
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
  const char *title;

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
  action->widget = g_object_ref_sink(widget);;

  if (protocol)
    action->protocol = g_object_ref(protocol);

  if (action->field)
  {
    g_object_add_weak_pointer((GObject *)&action->field,
                              (gpointer *)&action->field);
  }

  g_signal_connect(widget, "destroy",
                   G_CALLBACK(action_widget_destroy_cb), action);

  return action;
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

  if (val)
  {
    if (!osso_abook_msgids_rfind(NULL, "osso-country", val))
    {
      const char *msgid =
          osso_abook_msgids_rfind("en_GB", "osso-countries", val);

      if (msgid)
      {
        g_free(val);
        val = g_strdup(dgettext("osso-countries", msgid));
      }
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
    EVC_N, NULL, NULL, NULL, 501,
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
    EVC_FN, "name", NULL, NULL, 500,
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
    EVC_TEL, "mobile", "phone", "general_call", 412,
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
    EVC_TEL, "mobile_home", "phone", "general_call", 411,
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
    EVC_TEL, "mobile_work", "phone", "general_call", 410,
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
    EVC_TEL, "phone", "phone", "general_call", 404,
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
    EVC_TEL, "phone_home", "phone", "general_call", 403,
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
    EVC_TEL, "phone_work", "phone", "general_call", 402,
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
    EVC_TEL, "phone_other", "phone", "general_call", 401,
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
    EVC_TEL, "phone_fax", "phone", "general_call", 400,
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
    EVC_EMAIL, "email", NULL, "general_email", 302,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED,
    get_mail_actions,
    get_noautocap_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    EVC_EMAIL, "email_home", NULL, "general_email", 301,
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
    EVC_EMAIL, "email_work", NULL, "general_email", 300,
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
    EVC_PHOTO, "image", NULL, NULL, 120,
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
    EVC_BDAY, "birthday", NULL, "general_calendar", 104,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    get_date_actions,
    get_date_editor_widget,
    NULL,
    update_date_attribute,
    NULL,
    get_date_attr_value
  },
  {
    EVC_ADR, "address", NULL, "general_map", 103,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED,
    get_address_actions,
    NULL,
    get_address_children,
    update_address_attribute,
    synthesize_address_attributes,
    get_address_attr_value
  },
  {
    EVC_ADR, "address_home", NULL, "general_map", 102,
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
    EVC_ADR, "address_work", NULL, "general_map", 101,
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
    EVC_URL, "webpage", NULL, "general_web", 100,
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
    EVC_NICKNAME, "nickname", NULL, "general_business_card", 3,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    EVC_TITLE, "title", NULL, "general_business_card", 2,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    EVC_ORG, "company", NULL, "general_business_card", 1,
    OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON,
    NULL,
    get_text_editor_widget,
    NULL,
    update_text_attribute,
    NULL,
    e_vcard_attribute_get_value
  },
  {
    EVC_NOTE, "note", NULL, "general_notes", 0,
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

  while (t < &t[count])
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
      g_object_add_weak_pointer(&field->parent_instance,
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
  {general_templates, G_N_ELEMENTS(general_templates)},
  {address_templates, G_N_ELEMENTS(address_templates)},
  {name_templates, G_N_ELEMENTS(name_templates)}
};

static void
avatar_image_changed_cb(OssoABookAvatar *contact, GdkPixbuf *image,
                        OssoABookContactField *field)
{
  osso_abook_contact_set_pixbuf(OSSO_ABOOK_CONTACT(get_avatar(field)),
                                osso_abook_avatar_get_image(contact), 0, 0);
}

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
    g_object_remove_weak_pointer(G_OBJECT(priv->parent), (gpointer *)&priv->parent);
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
osso_abook_contact_field_init(OssoABookContactField* field)
{
}

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
        gtk_widget_set_sensitive(priv->editor_widget,
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
osso_abook_contact_field_get_icon_name(OssoABookContactField* field)
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
        NULL == master_contact || OSSO_ABOOK_IS_CONTACT (master_contact), NULL);
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
