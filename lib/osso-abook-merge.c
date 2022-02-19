/*
 * osso-abook-merge.c
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

#include <hildon/hildon.h>

#include <libintl.h>

#include "osso-abook-contact-chooser.h"
#include "osso-abook-contact-field.h"
#include "osso-abook-contact-model.h"
#include "osso-abook-contact.h"
#include "osso-abook-debug.h"
#include "osso-abook-errors.h"
#include "osso-abook-log.h"
#include "osso-abook-message-map.h"
#include "osso-abook-row-model.h"
#include "osso-abook-tree-view.h"
#include "osso-abook-utils-private.h"

#include "osso-abook-merge.h"

typedef gchar * (*ResolverGetDataFn)(OssoABookContact *contact,
                                     EContactField e_field,
                                     const char *attr_name,
                                     GHashTable *message_map);

typedef gchar *(*ResolverGetFn)(OssoABookContact *contact,
                                EContactField e_field,
                                const gchar *attr_name);

typedef void (*ResolverSetFn)(OssoABookContact *contact, EContactField e_field,
                              const char *attr_name, gpointer value);

typedef GtkWidget *(*ResolverWidgetCreateFn)(GList *contacts,
                                             EContactField e_field,
                                             const char *e_id,
                                             gboolean has_conflict,
                                             GtkSizeGroup *size_group,
                                             const gchar *title,
                                             ResolverGetDataFn get_data_fn,
                                             GtkWindow *parent);

typedef gchar *(*ResolverWidgetGetFn)(GtkWidget *widget);

struct _MergeConflictResolver
{
  EContactField e_field;
  const char *e_id;
  const char *title;
  gint conflicts_count;
  gchar *e_field_value;
  ResolverWidgetCreateFn widget_create;
  ResolverWidgetGetFn widget_get;
  ResolverGetFn get;
  ResolverGetDataFn get_data;
  ResolverSetFn set;
};

typedef struct _MergeConflictResolver MergeConflictResolver;

static OssoABookMessageMapping merge_map[6] =
{
  { "nickname", "addr_fi_solve_conflict_nick" },
  { "company", "addr_fi_solve_conflict_company" },
  { "title", "addr_fi_solve_conflict_title" },
  { "birthday", "addr_fi_solve_conflict_birthday" },
  { "gender", "addr_fi_solve_conflict_gender" },
  { NULL, NULL }
};

static OssoABookContact *
get_source_contact(OssoABookContact *contact)
{
  GList *roster_contacts;
  OssoABookContact *source_contact = NULL;

  g_return_val_if_fail(contact, NULL);

  if (!osso_abook_is_temporary_uid(e_contact_get_const(E_CONTACT(contact),
                                                       E_CONTACT_UID)))
  {
    return contact;
  }

  roster_contacts = osso_abook_contact_get_roster_contacts(contact);

  if (roster_contacts && roster_contacts->data)
    source_contact = roster_contacts->data;

  g_list_free(roster_contacts);

  return source_contact;
}

static GtkWidget *
merge_widget_text_create(GList *contacts, EContactField e_field,
                         const char *e_id, gboolean has_conflict,
                         GtkSizeGroup *size_group, const gchar *title,
                         ResolverGetDataFn get_data_fn, GtkWindow *parent)
{
  GList *l;
  GtkWidget *button;
  GtkWidget *selector;
  GtkWidget *entry = NULL;
  GHashTable *message_map;
  gchar *text = NULL;

  g_assert(contacts != NULL);

  message_map = osso_abook_message_map_new(merge_map);

  if (has_conflict)
  {
    button = hildon_picker_button_new(HILDON_SIZE_FINGER_HEIGHT,
                                      HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
    _osso_abook_button_set_date_style(HILDON_BUTTON(button));
  }
  else
  {
    entry = hildon_entry_new(HILDON_SIZE_FINGER_HEIGHT);
    button = hildon_caption_new(size_group, title, entry, NULL,
                                HILDON_CAPTION_OPTIONAL);
  }

  selector = hildon_touch_selector_entry_new_text();

  for (l = contacts; l; l = l->next)
  {
    OssoABookContact *contact = get_source_contact(l->data);

    if (get_data_fn)
      text = get_data_fn(contact, e_field, e_id, message_map);

    if (!IS_EMPTY(text))
    {
      if (has_conflict)
      {
        hildon_touch_selector_append_text(HILDON_TOUCH_SELECTOR(selector),
                                          text);
      }
      else
        gtk_entry_set_text(GTK_ENTRY(entry), text);
    }

    g_free(text);
  }

  if (has_conflict)
  {
    hildon_touch_selector_set_active(HILDON_TOUCH_SELECTOR(selector), 0, 0);
    hildon_picker_button_set_selector(HILDON_PICKER_BUTTON(button),
                                      HILDON_TOUCH_SELECTOR(selector));
    hildon_button_set_title(HILDON_BUTTON(button), title);
  }

  if (message_map)
    g_hash_table_unref(message_map);

  return button;
}

static gchar *
merge_widget_text_get(GtkWidget *widget)
{
  gchar *text = NULL;

  if (HILDON_IS_CAPTION(widget))
  {
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));

    text = g_strdup(gtk_entry_get_text(GTK_ENTRY(child)));
  }
  else if (HILDON_IS_PICKER_BUTTON(widget))
  {
    HildonTouchSelector *selector =
      hildon_picker_button_get_selector(HILDON_PICKER_BUTTON(widget));

    text = hildon_touch_selector_get_current_text(selector);
  }
  else
    OSSO_ABOOK_WARN("Unhandled widget %s", gtk_widget_get_name(widget));

  if (text)
    text = g_strchomp(g_strchug(text));

  if (!IS_EMPTY(text))
    return text;

  g_free(text);

  return NULL;
}

static gchar *
contact_get_first_name_display_value(OssoABookContact *contact,
                                     EContactField e_field,
                                     const char *attr_name,
                                     GHashTable *message_map)
{
  gchar *given_name;
  gchar *family_name;

  g_return_val_if_fail(e_field == E_CONTACT_GIVEN_NAME, NULL);

  given_name = e_contact_get(E_CONTACT(contact), E_CONTACT_GIVEN_NAME);
  family_name = e_contact_get(E_CONTACT(contact), E_CONTACT_FAMILY_NAME);

  if (IS_EMPTY(given_name) && IS_EMPTY(family_name))
  {
    g_free(given_name);
    given_name = e_contact_get(E_CONTACT(contact), E_CONTACT_FULL_NAME);
  }

  g_free(family_name);

  return given_name;
}

static gchar *
_contact_get_first_name_display_value(OssoABookContact *contact,
                                      EContactField field,
                                      const char *attr_name)
{
  return contact_get_first_name_display_value(contact, field, attr_name, NULL);
}

static gchar *
contact_get(OssoABookContact *contact, EContactField e_field,
            const gchar *attr_name)
{
  if (e_field > E_CONTACT_FIELD_LAST)
    return NULL;

  return e_contact_get(E_CONTACT(contact), e_field);
}

static void
contact_set(OssoABookContact *contact, EContactField e_field, const char *name,
            gpointer value)
{
  e_contact_set(E_CONTACT(contact), e_field, value);
}

static gchar *
contact_get_data(OssoABookContact *contact, EContactField e_field,
                 const char *attr_name, GHashTable *message_map)
{
  gchar *data = NULL;

  if (attr_name)
  {
    EVCardAttribute *attr = e_vcard_get_attribute(E_VCARD(contact), attr_name);

    if (attr)
    {
      OssoABookContactField *field =
        osso_abook_contact_field_new(message_map, NULL, attr);

      data = g_strdup(osso_abook_contact_field_get_display_value(field));
      g_object_unref(field);
    }
  }
  else
    data = e_contact_get(E_CONTACT(contact), e_field);

  return data;
}

static gchar *
contact_date_get(OssoABookContact *contact, EContactField e_field,
                 const gchar *attr_name)
{
  EVCardAttribute *attr = e_vcard_get_attribute(E_VCARD(contact), attr_name);
  gchar *value = NULL;

  if (attr)
    value = e_vcard_attribute_get_value(attr);

  return value;
}

static void
contact_date_set(OssoABookContact *contact, EContactField field,
                 const char *name, gpointer value)
{
  EContactDate *date = e_contact_date_from_string(value);

  e_contact_set(E_CONTACT(contact), field, date);
  e_contact_date_free(date);
}

static void
e_vcard_set(OssoABookContact *contact, EContactField field,
            const char *name, gpointer value)
{
  e_vcard_add_attribute_with_value(
    E_VCARD(contact), e_vcard_attribute_new(NULL, name), value);
}

static gchar *
merge_widget_date_get(GtkWidget *widget)
{
  EContactDate *e_date = g_object_steal_data(G_OBJECT(widget), "date");
  gchar *date = e_contact_date_to_string(e_date);

  e_contact_date_free(e_date);

  if (date)
  {
    gboolean do_not_save = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(widget),
                                                             "do-not-save"));

    if (do_not_save)
    {
      g_free(date);
      date = NULL;
    }
    else
    {
      date = g_strchomp(g_strchug(date));

      if (IS_EMPTY(date))
      {
        g_free(date);
        date = NULL;
      }
    }
  }

  return date;
}

static gchar *
format_hildon_date(EContactDate *date)
{
  GtkWidget *button;
  gchar *hildon_date;

  button = hildon_date_button_new_with_year_range(
      HILDON_SIZE_FINGER_HEIGHT,
      HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
      1900, 2100);
  hildon_date_button_set_date(
    HILDON_DATE_BUTTON(button), date->year, date->month - 1, date->day);
  hildon_date = g_strdup(hildon_button_get_value(HILDON_BUTTON(button)));
  gtk_widget_destroy(button);

  return hildon_date;
}

struct conflict_birthday_data
{
  GList *contacts;
  GtkWindow *parent;
};

static void
birthday_date_button_value_changed_cb(GtkButton *button, GtkDialog *dialog)
{
  HildonTouchSelector *selector;
  EContactDate *date;
  guint day;
  guint month;
  guint year;

  selector = hildon_picker_button_get_selector(HILDON_PICKER_BUTTON(button));

  hildon_date_selector_get_date(HILDON_DATE_SELECTOR(selector),
                                &year, &month, &day);
  date = e_contact_date_new();
  date->year = year;
  date->month = month + 1;
  date->day = day;
  g_object_set_data_full(G_OBJECT(dialog), "date", date,
                         (GDestroyNotify)&e_contact_date_free);
  g_object_set_data(G_OBJECT(button), "do-not-save", FALSE);
  gtk_dialog_response(dialog, GTK_RESPONSE_OK);
}

static void
resolve_conflict_birthday_clicked_cb(GtkButton *button, GtkDialog *dialog)
{
  g_object_set_data_full(G_OBJECT(dialog),
                         "date", g_object_steal_data(G_OBJECT(button), "date"),
                         (GDestroyNotify)&e_contact_date_free);
  gtk_dialog_response(dialog, GTK_RESPONSE_OK);
}

static void
solve_conflict_birthday_clicked_cb(GtkButton *button,
                                   struct conflict_birthday_data *data)
{
  GList *contact = data->contacts;
  EContactDate *date;
  gchar *date_text;
  GtkWidget *dialog;
  GtkWidget *date_button;

  dialog = gtk_dialog_new_with_buttons(
      _("addr_ti_solve_conflict"), data->parent,
      GTK_DIALOG_DESTROY_WITH_PARENT | GTK_DIALOG_MODAL, NULL);
  g_object_set(GTK_DIALOG(dialog)->vbox,
               "border-width", 8,
               "spacing", 8,
               NULL);

  date_button = hildon_date_button_new(HILDON_SIZE_FINGER_HEIGHT,
                                       HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);
  hildon_button_set_title(HILDON_BUTTON(date_button),
                          _("addr_fi_solve_conflict_birthday"));
  _osso_abook_button_set_date_style(HILDON_BUTTON(date_button));
  g_signal_connect(date_button, "value-changed",
                   G_CALLBACK(birthday_date_button_value_changed_cb), dialog);
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox),
                     date_button, TRUE, TRUE, 0);

  while (contact && contact->data)
  {
    date = e_contact_get(E_CONTACT(contact->data),
                         E_CONTACT_BIRTH_DATE);

    if (date)
    {
      date_text = format_hildon_date(date);
      date_button = hildon_button_new_with_text(
          HILDON_SIZE_FINGER_HEIGHT,
          HILDON_BUTTON_ARRANGEMENT_HORIZONTAL,
          _("addr_fi_solve_conflict_birthday"),
          date_text);
      _osso_abook_button_set_date_style(HILDON_BUTTON(date_button));
      g_object_set_data_full(G_OBJECT(date_button), "date", date,
                             (GDestroyNotify)&e_contact_date_free);
      g_signal_connect(date_button, "clicked",
                       G_CALLBACK(resolve_conflict_birthday_clicked_cb),
                       dialog);
      gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog)->vbox), date_button, TRUE,
                         TRUE, 0);
      g_free(date_text);
    }

    contact = contact->next;
  }

  gtk_widget_show_all(GTK_WIDGET(dialog));

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
  {
    date = g_object_steal_data(G_OBJECT(dialog), "date");

    if (date)
      date_text = format_hildon_date(date);
    else
    {
      OSSO_ABOOK_WARN("failed to get final mergee birthdate");
      date_text = NULL;
    }

    hildon_button_set_value(HILDON_BUTTON(button), date_text);

    g_object_set_data_full(G_OBJECT(button), "date", date,
                           (GDestroyNotify)&e_contact_date_free);
    g_free(date_text);
  }

  gtk_widget_destroy(GTK_WIDGET(dialog));
}

static GtkWidget *
merge_widget_date_create(GList *contacts, EContactField e_field,
                         const char *e_id, gboolean has_conflict,
                         GtkSizeGroup *size_group, const gchar *title,
                         ResolverGetDataFn get_data_fn, GtkWindow *parent)
{
  GList *l = contacts;
  gchar *value = NULL;
  struct conflict_birthday_data *data;
  GtkWidget *widget;

  widget = hildon_button_new(HILDON_SIZE_FINGER_HEIGHT,
                             HILDON_BUTTON_ARRANGEMENT_HORIZONTAL);

  while (l && l->data)
  {
    EContactDate *bday = e_contact_get(E_CONTACT(l->data),
                                       E_CONTACT_BIRTH_DATE);

    if (bday)
    {
      value = format_hildon_date(bday);
      g_object_set_data_full(G_OBJECT(widget), "date", bday,
                             (GDestroyNotify)&e_contact_date_free);
      break;
    }

    l = l->next;
  }

  hildon_button_set_text(HILDON_BUTTON(widget),
                         _("addr_fi_solve_conflict_birthday"), value);
  _osso_abook_button_set_date_style(HILDON_BUTTON(widget));

  data = g_new0(struct conflict_birthday_data, 1);
  data->contacts = contacts;
  data->parent = parent;
  g_signal_connect_data(widget, "clicked",
                        G_CALLBACK(solve_conflict_birthday_clicked_cb), data,
                        (GClosureNotify)&g_free, 0);
  gtk_widget_show(widget);
  g_free(value);

  return widget;
}

static GtkWidget *
merge_widget_contact_field_create(GList *contacts, EContactField e_field,
                                  const char *e_id, gboolean has_conflict,
                                  GtkSizeGroup *size_group, const gchar *title,
                                  ResolverGetDataFn get_data_fn,
                                  GtkWindow *parent)
{
  GHashTable *map;
  EVCardAttribute *attr;
  OssoABookContactField *field;
  GtkWidget *widget;

  g_assert(contacts != NULL);

  map = osso_abook_message_map_new(merge_map);

  while (contacts && contacts->data)
  {
    if ((attr = e_vcard_get_attribute(E_VCARD(contacts->data), e_id)))
      break;

    contacts = contacts->next;
  }

  if (!attr)
    attr = e_vcard_attribute_new(NULL, OSSO_ABOOK_VCA_GENDER);

  field = osso_abook_contact_field_new(map, 0, attr);
  widget = osso_abook_contact_field_get_editor_widget(field);

  g_object_set_data(G_OBJECT(widget), "contact-field", field);

  if (map)
    g_hash_table_unref(map);

  return widget;
}

static gchar *
merge_widget_contact_get_data(GtkWidget *widget)
{
  OssoABookContactField *field = g_object_get_data(G_OBJECT(widget),
                                                   "contact-field");
  EVCardAttribute *attr = osso_abook_contact_field_get_attribute(field);
  gchar *result;

  if (!_osso_abook_e_vcard_attribute_has_value(attr))
    return NULL;

  if (!(result = e_vcard_attribute_get_value(attr)))
    return result;

  result = g_strstrip(result);

  if (IS_EMPTY(result))
  {
    g_free(result);
    return NULL;
  }

  return result;
}

static MergeConflictResolver resolvers[] =
{
  {
    E_CONTACT_GIVEN_NAME,
    NULL,
    "addr_fi_solve_conflict_first",
    0,
    NULL,
    merge_widget_text_create,
    merge_widget_text_get,
    _contact_get_first_name_display_value,
    contact_get_first_name_display_value,
    contact_set
  },
  {
    E_CONTACT_FAMILY_NAME,
    NULL,
    "addr_fi_solve_conflict_last",
    0,
    NULL,
    merge_widget_text_create,
    merge_widget_text_get,
    contact_get,
    contact_get_data,
    contact_set
  },
  {
    E_CONTACT_NICKNAME,
    EVC_NICKNAME,
    "addr_fi_solve_conflict_nick",
    0,
    NULL,
    merge_widget_text_create,
    merge_widget_text_get,
    contact_get,
    contact_get_data,
    contact_set
  },
  {
    E_CONTACT_ORG,
    EVC_ORG,
    "addr_fi_solve_conflict_company",
    0,
    NULL,
    merge_widget_text_create,
    merge_widget_text_get,
    contact_get,
    contact_get_data,
    contact_set
  },
  {
    E_CONTACT_TITLE,
    EVC_TITLE,
    "addr_fi_solve_conflict_title",
    0,
    NULL,
    merge_widget_text_create,
    merge_widget_text_get,
    contact_get,
    contact_get_data,
    contact_set
  },
  {
    E_CONTACT_BIRTH_DATE,
    EVC_BDAY,
    "addr_fi_solve_conflict_birthday",
    0,
    NULL,
    merge_widget_date_create,
    merge_widget_date_get,
    contact_date_get,
    NULL,
    contact_date_set
  },
  {
    0,
    OSSO_ABOOK_VCA_GENDER,
    "addr_fi_solve_conflict_gender",
    0,
    NULL,
    merge_widget_contact_field_create,
    merge_widget_contact_get_data,
    contact_get,
    contact_get_data,
    e_vcard_set
  }
};

static OssoABookContact *
merge_contact(MergeConflictResolver *resolvers, int count)
{
  OssoABookContact *merged = osso_abook_contact_new();
  MergeConflictResolver *resolver = resolvers;

  while (resolver < &resolvers[count])
  {
    gchar *v = resolver->e_field_value;

    if (v)
      resolver->set(merged, resolver->e_field, resolver->e_id, v);

    resolver++;
  }

  return merged;
}

static void
save_button_clicked_cb(GtkButton *button, GtkDialog *dialog)
{
  MergeConflictResolver *resolver;
  OssoABookContact *merged;
  GObject *object = G_OBJECT(dialog);

  for (resolver = resolvers; resolver < &resolvers[G_N_ELEMENTS(resolvers)];
       resolver++)
  {
    GtkWidget *widget = GTK_WIDGET(g_object_get_data(object,
                                                     resolver->title));

    if (widget)
    {
      g_free(resolver->e_field_value);
      resolver->e_field_value = resolver->widget_get(widget);
    }
  }

  merged = merge_contact(resolvers, G_N_ELEMENTS(resolvers));

  if (osso_abook_contact_has_valid_name(merged))
  {
    g_object_set_data(G_OBJECT(dialog), "contact", merged);
    gtk_dialog_response(dialog, GTK_RESPONSE_OK);
  }
  else
  {
    hildon_banner_show_information(GTK_WIDGET(dialog), NULL,
                                   _("addr_ib_no_name_given"));

    if (merged)
      g_object_unref(merged);
  }
}

static OssoABookContact *
resolve_conflicts_dialog_run(GList *contacts, GtkWindow *parent)
{
  OssoABookContact *contact = NULL;
  GtkWidget *dialog;
  GtkWidget *button;
  int i;
  GtkSizeGroup *size_group;

  g_assert(contacts != NULL);

  dialog = gtk_dialog_new();

  gtk_window_set_title(GTK_WINDOW(dialog), _("addr_ti_solve_conflict"));
  gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
  g_object_set(GTK_DIALOG(dialog)->vbox,
               "border-width", 8,
               "spacing", 8,
               NULL);
  size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);

  for (i = 0; i < G_N_ELEMENTS(resolvers); i++)
  {
    MergeConflictResolver *r = &resolvers[i];

    if (((unsigned int)(r->e_field - 5) <= 1) || (r->conflicts_count > 1))
    {
      GtkWidget *widget = r->widget_create(contacts, r->e_field, r->e_id,
                                           r->conflicts_count > 1, size_group,
                                           _(r->title), r->get_data,
                                           GTK_WINDOW(dialog));
      gtk_widget_show_all(widget);
      gtk_box_pack_start(
        GTK_BOX(GTK_DIALOG(dialog)->vbox), widget, TRUE, TRUE, 0);
      g_object_set_data(G_OBJECT(dialog), r->title, widget);
    }
  }

  button = hildon_button_new_with_text(HILDON_SIZE_FINGER_HEIGHT,
                                       HILDON_BUTTON_ARRANGEMENT_VERTICAL,
                                       dgettext("hildon-libs",
                                                "wdgt_bd_save"),
                                       NULL);
  g_object_set_data(G_OBJECT(dialog), "to_merge", contacts);
  g_signal_connect(button, "clicked",
                   G_CALLBACK(save_button_clicked_cb), dialog);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), button);
  gtk_widget_show_all(GTK_DIALOG(dialog)->action_area);
  g_object_unref(size_group);

  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    contact = g_object_get_data(G_OBJECT(dialog), "contact");

  gtk_widget_destroy(dialog);

  return contact;
}

static int
get_attribute_weight(EVCardAttribute *attr)
{
  GList *param;
  int weight = 0;

  if (g_ascii_strcasecmp(e_vcard_attribute_get_name(attr), EVC_TEL))
    return 0;

  param = e_vcard_attribute_get_params(attr);

  while (param)
  {
    if (!g_ascii_strcasecmp(e_vcard_attribute_param_get_name(param->data),
                            EVC_TYPE))
    {
      GList *v;

      for (v = e_vcard_attribute_param_get_values(param->data); v; v = v->next)
      {
        int i = 0;

        struct
        {
          const char *name;
          int weight;
        }
        subtype[] =
        {
          { "HOME", 2 },
          { "WORK", 2 },
          { "CELL", 1 },
          { "VOICE", 1 }
        };

        while (g_ascii_strcasecmp(v->data, subtype[i].name))
        {
          if (++i == G_N_ELEMENTS(subtype))
            return -1;
        }

        if (!subtype[i].weight)
          return -1;

        weight += subtype[i].weight;
      }
    }

    param = param->next;
  }

  return weight;
}

static EVCardAttribute *
copy_safer_attribute(EVCardAttribute *attr_a, EVCardAttribute *attr_b)
{
  const char *attr_name_a = e_vcard_attribute_get_name(attr_a);
  const char *attr_name_b = e_vcard_attribute_get_name(attr_b);

  g_return_val_if_fail(!g_strcmp0(attr_name_a, attr_name_b), NULL);

  if (get_attribute_weight(attr_b) < get_attribute_weight(attr_b))
    return e_vcard_attribute_copy(attr_b);
  else
    return e_vcard_attribute_copy(attr_a);
}

static guint
e_vcard_attribute_hash(EVCardAttribute *attr)
{
  return g_str_hash(e_vcard_attribute_get_name(attr));
}

static gboolean
attribute_values_cmp(GList *values_a, GList *values_b)
{
  if (!values_a)
    return values_b == NULL;

  while (values_b && !strcmp(values_a->data, values_b->data))
  {
    values_a = values_a->next;
    values_b = values_b->next;

    if (!values_a)
      return values_b == NULL;
  }

  return FALSE;
}

static gboolean
e_vcard_attribute_compare(EVCardAttribute *attr_a, EVCardAttribute *attr_b)
{
  if (g_ascii_strcasecmp(e_vcard_attribute_get_name(attr_a),
                         e_vcard_attribute_get_name(attr_b)))
  {
    return FALSE;
  }

  return attribute_values_cmp(e_vcard_attribute_get_values(attr_a),
                              e_vcard_attribute_get_values(attr_b));
}

static void
merge_attributes(EVCardAttribute *key, gpointer value,
                 OssoABookContact *contact)
{
  EVCardAttribute *attr = e_vcard_attribute_copy(key);

  if (!g_ascii_strcasecmp(e_vcard_attribute_get_name(attr), EVC_TEL))
  {
    GList *param = e_vcard_attribute_get_params(attr);

    while (param)
    {
      if (!g_ascii_strcasecmp(e_vcard_attribute_param_get_name(param->data),
                              EVC_TYPE))
      {
        break;
      }

      param = param->next;
    }

    if (!param)
    {
      e_vcard_attribute_add_param_with_values(
        attr, e_vcard_attribute_param_new(EVC_TYPE), "CELL", "VOICE", NULL);
    }
  }

  e_vcard_add_attribute(E_VCARD(contact), attr);
}

static gint
compare_contacts(OssoABookContact *contact_a, OssoABookContact *contact_b)
{
  MergeConflictResolver *resolver;
  guint num_data_a = 0;
  guint num_data_b = 0;

  if (!osso_abook_contact_is_roster_contact(contact_a) &&
      osso_abook_contact_is_roster_contact(contact_b))
  {
    return -1;
  }

  if (osso_abook_contact_is_roster_contact(contact_a) &&
      !osso_abook_contact_is_roster_contact(contact_b))
  {
    return 1;
  }

  resolver = &resolvers[1];
  resolver = &resolvers[1];

  for (resolver = resolvers; resolver < &resolvers[G_N_ELEMENTS(resolvers)];
       resolver++)
  {
    gchar *data = resolver->get(contact_a, resolver->e_field, resolver->e_id);

    if (data)
      num_data_a++;

    g_free(data);
  }

  for (resolver = resolvers; resolver < &resolvers[G_N_ELEMENTS(resolvers)];
       resolver++)
  {
    gchar *data = resolver->get(contact_b, resolver->e_field, resolver->e_id);

    if (data)
      num_data_b++;

    g_free(data);
  }

  if (num_data_a > num_data_b)
    return -1;

  return num_data_a < num_data_b;
}

static gboolean
param_values_compare(EVCardAttributeParam *param_a,
                     EVCardAttributeParam *param_b)
{
  if (param_a)
  {
    if (!param_b)
      return FALSE;

    if (g_ascii_strcasecmp(e_vcard_attribute_param_get_name(param_a),
                           e_vcard_attribute_param_get_name(param_b)))
    {
      return FALSE;
    }

    if (!attribute_values_cmp(
          e_vcard_attribute_param_get_values(param_a),
          e_vcard_attribute_param_get_values(param_b)))
    {
      return FALSE;
    }
  }
  else if (param_b)
    return FALSE;

  return TRUE;
}

static gboolean
is_known_attribute(const gchar *name)
{
  static const char *attr_names[] =
  {
    EVC_VERSION,
    EVC_REV,
    EVC_UID,
    "X-EVOLUTION-FILE-AS",
    EVC_N,
    EVC_FN,
    EVC_NICKNAME,
    EVC_ORG,
    EVC_TITLE,
    EVC_BDAY,
    OSSO_ABOOK_VCA_GENDER,
    EVC_CATEGORIES,
    EVC_PHOTO
  };
  int i;

  for (i = 0; i < G_N_ELEMENTS(attr_names); i++)
  {
    if (!g_ascii_strcasecmp(name, attr_names[i]))
      return TRUE;
  }

  return FALSE;
}

OssoABookContact *
osso_abook_merge_contacts(GList *contacts, GtkWindow *parent)
{
  GList *rc;
  EVCardAttribute *replace_attr;
  OssoABookContact *source_contact;
  GHashTable *rc_table;
  EContactPhoto *photo;
  GHashTable *attr_table;
  GHashTableIter iter;
  EVCardAttribute *attr;
  GList *attrs;

  GList *l;
  gboolean has_conflict = FALSE;
  int i;
  OssoABookContact *merged = NULL;

  g_return_val_if_fail(contacts != NULL, NULL);

  if (!contacts->next)
    return NULL;

  contacts = g_list_copy(contacts);

  for (l = contacts; l; l = l->next)
  {
    source_contact = get_source_contact(l->data);

    for (i = 0; i < G_N_ELEMENTS(resolvers); i++)
    {
      MergeConflictResolver *resolver = &resolvers[i];
      gchar *e_field_value = resolver->get(source_contact, resolver->e_field,
                                           resolver->e_id);

      if (IS_EMPTY(e_field_value))
        g_free(e_field_value);
      else
      {
        if (resolver->e_field_value)
        {
          if (!g_str_equal(resolver->e_field_value, e_field_value))
          {
            resolver->conflicts_count++;
            has_conflict = TRUE;
          }

          g_free(e_field_value);
        }
        else
        {
          resolver->e_field_value = e_field_value;
          resolver->conflicts_count++;
        }
      }
    }
  }

  if (has_conflict)
  {
    contacts = g_list_sort(contacts, (GCompareFunc)compare_contacts);

    merged = resolve_conflicts_dialog_run(contacts, parent);

    if (!merged)
      goto out;
  }
  else
    merged = merge_contact(resolvers, G_N_ELEMENTS(resolvers));

  photo = osso_abook_contact_get_contact_photo(E_CONTACT(contacts->data));
  attr_table = g_hash_table_new_full((GHashFunc)e_vcard_attribute_hash,
                                     (GEqualFunc)e_vcard_attribute_compare,
                                     (GDestroyNotify)&e_vcard_attribute_free,
                                     NULL);
  rc_table = g_hash_table_new((GHashFunc)&g_direct_hash,
                              (GEqualFunc)&g_direct_equal);

  for (l = contacts; l; l = l->next)
  {
    if (!photo)
      photo = osso_abook_contact_get_contact_photo(E_CONTACT(l->data));

    for (attrs = e_vcard_get_attributes(E_VCARD(l->data)); attrs;
         attrs = attrs->next)
    {
      if (is_known_attribute(e_vcard_attribute_get_name(attrs->data)))
        continue;

      rc = osso_abook_contact_find_roster_contacts_for_attribute(
          l->data, attrs->data);

      if (rc)
      {
        while (rc)
        {
          if (!g_hash_table_lookup(rc_table, rc->data))
            g_hash_table_insert(rc_table, rc->data, attrs->data);

          rc = g_list_delete_link(rc, rc);
        }
      }
      else
      {
        EVCardAttribute *merged_attr = g_hash_table_lookup(attr_table,
                                                           attrs->data);

        if (merged_attr)
        {
          GList *merged_params = e_vcard_attribute_get_params(merged_attr);
          GList *new_params = e_vcard_attribute_get_params(attrs->data);

          if (new_params && merged_params)
          {
            while (new_params)
            {
              if (!param_values_compare(merged_params->data,
                                        new_params->data))
              {
                replace_attr = copy_safer_attribute(attrs->data,
                                                    merged_attr);
                break;
              }

              merged_params = merged_params->next;
              new_params = new_params->next;

              if (!merged_params && !new_params)
                continue;
            }
          }
          else
            continue;
        }
        else
          replace_attr = e_vcard_attribute_copy(attrs->data);
      }

      g_hash_table_replace(attr_table, replace_attr, replace_attr);
    }
  }

  g_hash_table_iter_init(&iter, rc_table);

  while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&attr))
  {
    attr = e_vcard_attribute_copy(attr);
    e_vcard_add_attribute(E_VCARD(merged), attr);
    g_hash_table_remove(attr_table, attr);
  }

  g_hash_table_foreach(attr_table, (GHFunc)merge_attributes, merged);

  if (e_vcard_get_attribute(E_VCARD(merged), EVC_N) ||
      e_vcard_get_attribute(E_VCARD(merged), EVC_FN))
  {
    e_contact_set(E_CONTACT(merged), E_CONTACT_FULL_NAME,
                  osso_abook_contact_get_display_name(merged));
  }

  if (photo)
  {
    if (photo->type)
    {
      if (photo->type == E_CONTACT_PHOTO_TYPE_URI)
      {
        const char *uri;

        if (!memcmp(photo->data.uri, FILE_SCHEME, strlen(FILE_SCHEME)))
          uri = photo->data.uri + 7;
        else
          uri = photo->data.uri;

        osso_abook_contact_set_photo(merged, uri, NULL, parent);
      }
    }
    else
    {
      osso_abook_contact_set_photo_data(merged,
                                        photo->data.inlined.data,
                                        photo->data.inlined.length,
                                        0,
                                        parent);
    }

    g_hash_table_destroy(attr_table);
    g_hash_table_destroy(rc_table);
    e_contact_photo_free(photo);
  }
  else
  {
    g_hash_table_destroy(attr_table);
    g_hash_table_destroy(rc_table);
  }

out:
  g_list_free(contacts);

  for (i = 0; i < G_N_ELEMENTS(resolvers); i++)
  {
    MergeConflictResolver *resolver = &resolvers[i];

    resolver->conflicts_count = 0;
    g_free(resolver->e_field_value);
    resolver->e_field_value = NULL;
  }

  return merged;
}

struct MergeContactsData
{
  GtkWindow *parent;
  GList *contacts;
  OssoABookContact *merged_contact;
  OssoABookContact *contact;
  GSList *home_applets;
  OssoABookMergeWithCb cb;
  gpointer user_data;
};

static void
merge_data_free(struct MergeContactsData *data)
{
  g_list_free_full(data->contacts, g_object_unref);

  if (data->merged_contact)
    g_object_unref(data->merged_contact);

  if (data->contact)
    g_object_unref(data->contact);

  g_slist_free_full(data->home_applets, g_free);
  g_slice_free(struct MergeContactsData, data);
}

static void
commit_contact_cb(EBook *book, EBookStatus status, gpointer closure)
{
  struct MergeContactsData *data = closure;

  const char *merged_uid = NULL;
  GList *contact;

  if (!status)
  {
    merged_uid = e_contact_get_const(E_CONTACT(data->merged_contact),
                                     E_CONTACT_UID);

    for (contact = data->contacts; contact; contact = contact->next)
    {
      const char *uid = e_contact_get_const(E_CONTACT(contact->data),
                                            E_CONTACT_UID);

      if (g_strcmp0(merged_uid, uid))
      {
        GList *rc;

        for (rc = osso_abook_contact_get_roster_contacts(contact->data); rc;
             rc = g_list_delete_link(rc, rc))
        {
          osso_abook_contact_accept_for_uid(rc->data, merged_uid, NULL);

          if (!osso_abook_is_temporary_uid(uid))
            osso_abook_contact_reject_for_uid(rc->data, uid, NULL);
        }

        if (!osso_abook_contact_is_roster_contact(contact->data) &&
            !osso_abook_contact_is_sim_contact(contact->data))
        {
          if (!IS_EMPTY(uid))
          {
            if (!osso_abook_is_temporary_uid(uid))
              osso_abook_contact_delete(contact->data, NULL, NULL);
          }
        }
      }
    }

    osso_abook_settings_set_home_applets(data->home_applets);
  }
  else
    osso_abook_handle_estatus(NULL, status, book);

  if (data->cb)
    data->cb(merged_uid, data->user_data);

  merge_data_free(data);
}

static void
add_contact_cb(EBook *book, EBookStatus status, const gchar *uid,
               gpointer closure)
{
  struct MergeContactsData *data = closure;

  e_contact_set(E_CONTACT(data->merged_contact), E_CONTACT_UID, uid);
  commit_contact_cb(book, status, data);
}

static signed int
remove_contact_home_applet(OssoABookContact *contact,
                           GSList **home_applets)
{
  const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);
  GSList *applet;

  for (applet = *home_applets; applet; applet = applet->next)
  {
    const char *applet_uid = applet->data;

    if (applet_uid)
    {
      if (g_str_has_prefix(applet_uid, OSSO_ABOOK_HOME_APPLET_PREFIX))
        applet_uid += strlen(OSSO_ABOOK_HOME_APPLET_PREFIX);
    }

    if (!g_strcmp0(applet_uid, uid))
    {
      g_free(applet->data);
      *home_applets = g_slist_delete_link(*home_applets, applet);
      return TRUE;
    }
  }

  return FALSE;
}

static void
osso_abook_merge_contacts_and_save_real(struct MergeContactsData *merge_data)
{
  OssoABookContact *merged;
  GSList *applets;
  GList *contact;
  const char *applet_uid = NULL;
  const char *merged_uid = NULL;

  merged = osso_abook_merge_contacts(merge_data->contacts, merge_data->parent);
  merge_data->merged_contact = merged;

  if (!merged)
  {
    if (merge_data->cb)
      merge_data->cb(NULL, merge_data->user_data);

    merge_data_free(merge_data);
    return;
  }

  OSSO_ABOOK_DUMP_VCARD(VCARD, merged,
                        "successfully merged contacts into new contact");

  applets = osso_abook_settings_get_home_applets();
  contact = merge_data->contacts;

  if (!contact)
  {
    merge_data->home_applets = applets;

    osso_abook_contact_async_add(merge_data->merged_contact, NULL,
                                 add_contact_cb, merge_data);
    return;
  }

  for (; contact; contact = contact->next)
  {
    GList *rc;
    const char *contact_uid;

    for (rc = osso_abook_contact_get_roster_contacts(contact->data); rc;
         rc = g_list_delete_link(rc, rc))
    {
      if (remove_contact_home_applet(rc->data, &applets))
      {
        if (!applet_uid)
          applet_uid = e_contact_get_const(E_CONTACT(rc->data), E_CONTACT_UID);
      }
    }

    contact_uid = e_contact_get_const(E_CONTACT(contact->data), E_CONTACT_UID);

    if (!osso_abook_is_temporary_uid(contact_uid))
    {
      gboolean removed = remove_contact_home_applet(contact->data, &applets);

      if (removed)
      {
        if (!applet_uid)
          applet_uid = contact_uid;
      }

      if (!osso_abook_contact_is_roster_contact(contact->data) &&
          !osso_abook_contact_is_sim_contact(contact->data))
      {
        if (!IS_EMPTY(contact_uid))
        {
          if (removed)
          {
            merged_uid = contact_uid;
            applet_uid = contact_uid;
          }
          else if (!merged_uid)
            merged_uid = contact_uid;
        }
      }
    }
  }

  if (applet_uid)
  {
    applets = g_slist_prepend(
        applets, g_strconcat("osso-abook-applet-", applet_uid, NULL));
  }

  merge_data->home_applets = applets;

  if (merged_uid)
  {
    e_contact_set(E_CONTACT(merge_data->merged_contact), E_CONTACT_UID,
                  merged_uid);
    osso_abook_contact_async_commit(merge_data->merged_contact, NULL,
                                    commit_contact_cb, merge_data);
  }
}

void
osso_abook_merge_contacts_and_save(GList *contacts, GtkWindow *parent,
                                   OssoABookMergeWithCb cb, gpointer user_data)
{
  struct MergeContactsData *data;

  g_return_if_fail(contacts);
  g_return_if_fail(!parent || GTK_IS_WINDOW(parent));

  data = g_slice_new0(struct MergeContactsData);
  data->parent = parent;
  data->cb = cb;
  data->user_data = user_data;
  data->contacts = g_list_copy(contacts);
  g_list_foreach(data->contacts, (GFunc)g_object_ref, NULL);

  osso_abook_merge_contacts_and_save_real(data);
}

static void
contact_chooser_response_cb(GtkWidget *chooser, gint response_id,
                            gpointer user_data)
{
  struct MergeContactsData *data = user_data;

  if (response_id == GTK_RESPONSE_OK)
  {
    GList *selection;

    gtk_widget_hide(chooser);

    selection = osso_abook_contact_chooser_get_selection(
        OSSO_ABOOK_CONTACT_CHOOSER(chooser));
    data->contacts = selection;

    if (selection && selection->data)
    {
      data->contacts = g_list_prepend(selection, data->contact);
      g_list_foreach(data->contacts, (GFunc)&g_object_ref, NULL);
      osso_abook_merge_contacts_and_save_real(data);
      gtk_widget_destroy(chooser);
      return;
    }
  }

  gtk_widget_destroy(chooser);

  if (data->cb)
    data->cb(NULL, data->user_data);

  merge_data_free(data);
}

static void
contact_view_map_cb(GtkWidget *widget, gpointer data)
{
  g_signal_handlers_disconnect_matched(
    widget, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
    contact_view_map_cb, data);
  osso_abook_tree_view_pan_to_contact(OSSO_ABOOK_TREE_VIEW(widget),
                                      OSSO_ABOOK_CONTACT(data),
                                      OSSO_ABOOK_TREE_VIEW_PAN_MODE_TOP);
  g_object_unref(data);
}

static gchar **
split_name(gchar *name)
{
  GPtrArray *array = g_ptr_array_new();

  while (*name)
  {
    g_ptr_array_add(array, name);

    while (*name && *name != ' ' && *name != ',')
      name = g_utf8_next_char(name);

    if (*name)
      *name++ = 0;
  }

  if (!array->len)
    return (gchar **)g_ptr_array_free(array, TRUE);

  g_ptr_array_add(array, NULL);

  return (gchar **)g_ptr_array_free(array, FALSE);
}

static void
merge_with_dialog(OssoABookContact *contact,
                  OssoABookContactModel *contact_model, GtkWindow *parent,
                  OssoABookMergeWithCb cb, gpointer user_data, gboolean many)
{
  OssoABookContactModel *choser_model;
  GList *excluded;
  const char *uid;
  GtkWidget *contact_view;
  struct MergeContactsData *data;
  GtkWidget *chooser;
  gchar **names;
  gchar *display_name;
  OssoABookRowModel *row_model;
  GtkTreeModel *tree_model;
  GtkTreeIter iter;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(contact));
  g_return_if_fail(!contact_model ||
                   OSSO_ABOOK_IS_CONTACT_MODEL(contact_model));
  g_return_if_fail(!parent || GTK_IS_WINDOW(parent));

  if (!osso_abook_check_disc_space(parent))
    return;

  chooser = osso_abook_contact_chooser_new(parent, _("addr_ti_merge_contacts"));

  if (contact_model)
  {
    osso_abook_contact_chooser_set_model(OSSO_ABOOK_CONTACT_CHOOSER(chooser),
                                         contact_model);
  }

  osso_abook_contact_chooser_set_maximum_selection(
    OSSO_ABOOK_CONTACT_CHOOSER(chooser), many ? G_MAXUINT : 1);
  osso_abook_contact_chooser_set_minimum_selection(
    OSSO_ABOOK_CONTACT_CHOOSER(chooser), 1);
  osso_abook_contact_chooser_set_contact_order(
    OSSO_ABOOK_CONTACT_CHOOSER(chooser), OSSO_ABOOK_CONTACT_ORDER_NAME);
  choser_model = osso_abook_contact_chooser_get_model(
      OSSO_ABOOK_CONTACT_CHOOSER(chooser));

  excluded = g_list_prepend(NULL, "osso-abook-vmbx");
  uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

  if (uid)
    excluded = g_list_prepend(excluded, (gchar *)uid);

  osso_abook_contact_chooser_set_excluded_contacts(
    OSSO_ABOOK_CONTACT_CHOOSER(chooser), excluded);
  g_list_free(excluded);

  tree_model = GTK_TREE_MODEL(choser_model);
  row_model = OSSO_ABOOK_ROW_MODEL(tree_model);
  display_name = g_utf8_casefold(
      osso_abook_contact_get_display_name(contact), -1);
  names = split_name(display_name);

  if (names && gtk_tree_model_get_iter_first(tree_model, &iter))
  {
    gchar *row_display_name;
    OssoABookContact *candidate = NULL;
    int max_matched = 0;
    int current_matched = 0;

    do
    {
      OssoABookListStoreRow *row = osso_abook_row_model_iter_get_row(
          row_model, &iter);
      gchar **row_names;
      gchar **row_name;
      gchar **name;

      if (contact == row->contact)
        continue;

      row_display_name = g_utf8_casefold(
          osso_abook_contact_get_display_name(row->contact), -1);
      row_names = split_name(row_display_name);

      if (!row_names || !*names)
      {
        g_free(row_names);
        g_free(row_display_name);
        continue;
      }

      name = names;
      row_name = row_names;

      while (*name)
      {
        gchar *p = *name;

        while (*row_name)
        {
          gchar *q = *row_name;
          int matched = 0;

          while (*p && *q)
          {
            if (g_utf8_get_char(p) != g_utf8_get_char(q))
              break;

            matched++;
            p = g_utf8_next_char(p);
            q = g_utf8_next_char(q);
          }

          current_matched += matched;
          row_name++;
        }

        name++;
      }

      g_free(row_names);
      g_free(row_display_name);

      if (current_matched > max_matched)
      {
        candidate = row->contact;
        max_matched = current_matched;
      }
    }
    while (gtk_tree_model_iter_next(tree_model, &iter));

    if (candidate)
    {
      contact_view = osso_abook_contact_chooser_get_contact_view(
          OSSO_ABOOK_CONTACT_CHOOSER(chooser));
      g_object_ref(candidate);

      if (gtk_widget_get_mapped(contact_view))
        contact_view_map_cb(GTK_WIDGET(contact_view), candidate);
      else
      {
        g_signal_connect(contact_view, "map",
                         G_CALLBACK(contact_view_map_cb), candidate);
      }
    }
  }

  g_free(names);
  g_free(display_name);

  data = g_slice_new0(struct MergeContactsData);
  data->parent = parent;
  data->contact = g_object_ref(contact);
  data->cb = cb;
  data->user_data = user_data;
  g_signal_connect(chooser, "response",
                   G_CALLBACK(contact_chooser_response_cb), data);
  gtk_widget_show(chooser);
}

void
osso_abook_merge_with_dialog(OssoABookContact *contact,
                             OssoABookContactModel *model, GtkWindow *parent,
                             OssoABookMergeWithCb cb, gpointer user_data)
{
  merge_with_dialog(contact, model, parent, cb, user_data, FALSE);
}

void
osso_abook_merge_with_many_dialog(OssoABookContact *contact,
                                  OssoABookContactModel *model,
                                  GtkWindow *parent, OssoABookMergeWithCb cb,
                                  gpointer user_data)
{
  merge_with_dialog(contact, model, parent, cb, user_data, TRUE);
}
