#include <glib.h>
#include <telepathy-glib/enums.h>

#include "osso-abook-contact.h"
#include "osso-abook-presence.h"
#include "osso-abook-enums.h"

#include "config.h"
#include "osso-abook-util.h"

/* FIXME */
#define OSSO_ABOOK_NAME_ORDER_COUNT 4

struct _OssoABookContactPrivate
{
  const char *name[4];
  gchar **field_10;
  int field_14;
  int field_18;
  int field_1C;
  OssoABookRoster *roster;
  GHashTable *field_24;
  int field_28;
  int field_2C;
  int field_30;
  int field_34;
  int field_38;
  int presence_type;
  int presence_status;
  gchar *presence_status_message;
  int location;
  int field_4C;
  int flags;
};

typedef struct _OssoABookContactPrivate OssoABookContactPrivate;

#define OSSO_ABOOK_CONTACT_PRIVATE(contact) \
                ((OssoABookContactPrivate *)osso_abook_contact_get_instance_private(contact))

enum
{
  PROP_AVATAR_IMAGE = 1,
  PROP_SERVER_IMAGE,
  PROP_CAPABILITIES,
  PROP_DISPLAY_NAME,
  PROP_PRESENCE_TYPE,
  PROP_PRESENCE_STATUS,
  PROP_PRESENCE_STATUS_MESSAGE,
  PROP_PRESENCE_LOCATION_STRING,
  PROP_AVATAR_USER_SELECTED,
  PROP_DONE_LOADING
};

enum
{
  RESET,
  CONTACT_ATTACHED,
  CONTACT_DETACHED,
  LAST_SIGNAL,
};

static guint signals[LAST_SIGNAL];

static void
osso_abook_contact_osso_abook_avatar_iface_init(OssoABookAvatarIface *iface,
                                                gpointer data);
static void
osso_abook_contact_osso_abook_caps_iface_init(OssoABookCapsIface *iface,
                                              gpointer data);

static void
osso_abook_contact_osso_abook_presence_iface_init(OssoABookPresenceIface *iface,
                                                  gpointer data);

G_DEFINE_TYPE_WITH_CODE(
    OssoABookContact,
    osso_abook_contact,
    E_TYPE_CONTACT,
    G_IMPLEMENT_INTERFACE(
        OSSO_ABOOK_TYPE_AVATAR,
        osso_abook_contact_osso_abook_avatar_iface_init);
    G_IMPLEMENT_INTERFACE(
        OSSO_ABOOK_TYPE_CAPS,
        osso_abook_contact_osso_abook_caps_iface_init);
    G_IMPLEMENT_INTERFACE(
        OSSO_ABOOK_TYPE_PRESENCE,
        osso_abook_contact_osso_abook_presence_iface_init);
    G_ADD_PRIVATE(OssoABookContact);
);

static void
osso_abook_contact_osso_abook_avatar_iface_init(OssoABookAvatarIface *iface,
                                                gpointer data)
{
}

static void
osso_abook_contact_osso_abook_caps_iface_init(OssoABookCapsIface *iface,
                                              gpointer data)
{
  /*iface->get_static_capabilities = osso_abook_contact_get_static_capabilities;
  iface->get_capabilities = osso_abook_contact_get_capabilities;*/
}

static OssoABookPresenceState
osso_abook_contact_get_value_state(EContact *contact, const char *attr_name)
{
  char *value = osso_abook_contact_get_value(contact, attr_name);
  OssoABookPresenceState state = OSSO_ABOOK_PRESENCE_STATE_NO;

  if (value)
  {
    const GEnumValue *ev = osso_abook_presence_state_from_nick(value);

    g_free(value);

    if (ev)
      state = ev->value;
  }

  return state;
}

static gboolean
osso_abook_contact_presence_is_invalid(OssoABookPresence *presence)
{
  OssoABookContact *contact = OSSO_ABOOK_CONTACT(presence);

  return osso_abook_contact_is_roster_contact(contact) &&
      osso_abook_contact_has_invalid_username(contact);
}

static OssoABookPresenceState
osso_abook_contact_presence_get_published(OssoABookPresence *presence)
{
  return osso_abook_contact_get_value_state(E_CONTACT(presence),
                                            OSSO_ABOOK_VCA_TELEPATHY_PUBLISHED);
}

static OssoABookPresenceState
osso_abook_contact_presence_get_blocked(OssoABookPresence *presence)
{
  return osso_abook_contact_get_value_state(E_CONTACT(presence),
                                            OSSO_ABOOK_VCA_TELEPATHY_BLOCKED);
}

static OssoABookPresenceState
osso_abook_contact_presence_get_subscribed(OssoABookPresence *presence)
{
  return osso_abook_contact_get_value_state(
        E_CONTACT(presence), OSSO_ABOOK_VCA_TELEPATHY_SUBSCRIBED);
}

static unsigned int
osso_abook_contact_presence_get_handle(OssoABookPresence *presence)
{
  OssoABookContact *contact = OSSO_ABOOK_CONTACT(presence);
  unsigned int handle = 0;

  if (osso_abook_contact_is_roster_contact(contact))
  {
    unsigned int tmp;
    GList *l = osso_abook_contact_get_values(E_CONTACT(contact),
                                             OSSO_ABOOK_VCA_TELEPATHY_HANDLE);

    if (l && l->data && sscanf(l->data, "%u", &tmp))
      handle = tmp;
  }

  return handle;
}

static void
osso_abook_contact_osso_abook_presence_iface_init(OssoABookPresenceIface *iface,
                                                  gpointer data)
{
  /*iface->get_location_string =
      osso_abook_contact_presence_get_location_string;*/
  iface->is_invalid = osso_abook_contact_presence_is_invalid;
  iface->get_handle = osso_abook_contact_presence_get_handle;
  iface->get_blocked = osso_abook_contact_presence_get_blocked;
  iface->get_published = osso_abook_contact_presence_get_published;
  iface->get_subscribed = osso_abook_contact_presence_get_subscribed;
  /*iface->get_presence_type = osso_abook_contact_presence_get_presence_type;
  iface->get_presence_status = osso_abook_contact_presence_get_presence_status;
  iface->get_presence_status_message =
      osso_abook_contact_presence_get_presence_status_message;*/
}

static void
osso_abook_contact_init(OssoABookContact *contact)
{
  OSSO_ABOOK_CONTACT_PRIVATE(contact)->field_30 = 0;
}

static void
osso_abook_contact_update_attributes(OssoABookContact *contact,
                                     const gchar *attribute_name)
{

#pragma message("FIXME!!!")
  g_assert(0);
}

static void
osso_abook_contact_add_attribute(EVCard *evc, EVCardAttribute *attr)
{
  E_VCARD_CLASS(osso_abook_contact_parent_class)->add_attribute(evc, attr);

  osso_abook_contact_update_attributes(OSSO_ABOOK_CONTACT(evc),
                                       e_vcard_attribute_get_name(attr));
}

static void
osso_abook_contact_remove_attribute(EVCard *evc, EVCardAttribute *attr)
{
  E_VCARD_CLASS(osso_abook_contact_parent_class)->remove_attribute(evc, attr);

  osso_abook_contact_update_attributes(OSSO_ABOOK_CONTACT(evc),
                                       e_vcard_attribute_get_name(attr));
}

static void
osso_abook_contact_class_init(OssoABookContactClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  EVCardClass *e_vcard_class = E_VCARD_CLASS(klass);
  GEnumClass *name_order_class = g_type_class_ref(OSSO_ABOOK_TYPE_NAME_ORDER);

  g_assert_cmpint(OSSO_ABOOK_NAME_ORDER_COUNT, ==, name_order_class->n_values);
  g_type_class_unref(name_order_class);
/*
  object_class->dispose = osso_abook_contact_dispose;
  object_class->finalize = osso_abook_contact_finalize;
  object_class->get_property = osso_abook_contact_get_property;
*/
  e_vcard_class->remove_attribute = osso_abook_contact_remove_attribute;
  e_vcard_class->add_attribute = osso_abook_contact_add_attribute;

  g_object_class_override_property(object_class, PROP_AVATAR_IMAGE,
                                   "avatar-image");
  g_object_class_override_property(object_class, PROP_SERVER_IMAGE,
                                   "server-image");
  g_object_class_override_property(object_class, PROP_AVATAR_USER_SELECTED,
                                   "avatar-user-selected");
  g_object_class_override_property(object_class, PROP_DONE_LOADING,
                                   "done-loading");
  g_object_class_override_property(object_class, PROP_CAPABILITIES,
                                   "capabilities");
  g_object_class_override_property(object_class, PROP_PRESENCE_TYPE,
                                   "presence-type");
  g_object_class_override_property(object_class, PROP_PRESENCE_STATUS,
                                   "presence-status");
  g_object_class_override_property(object_class, PROP_PRESENCE_STATUS_MESSAGE,
                                   "presence-status-message");
  g_object_class_override_property(object_class, PROP_PRESENCE_LOCATION_STRING,
                                   "presence-location-string");

  g_object_class_install_property(
        object_class, PROP_DISPLAY_NAME,
        g_param_spec_string(
                 "display-name",
                 "Display Name",
                 "The text used to display the name of the contact",
                 NULL,
                 G_PARAM_STATIC_BLURB | G_PARAM_PRIVATE | G_PARAM_READABLE));

  signals[RESET] =
      g_signal_new("reset",
                G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
                G_STRUCT_OFFSET(OssoABookContactClass, reset),
                0, NULL,
                g_cclosure_marshal_VOID__VOID,
                G_TYPE_NONE,
                0);
  signals[CONTACT_ATTACHED] =
      g_signal_new("contact-attached",
                   G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
                   G_STRUCT_OFFSET(OssoABookContactClass, contact_attached),
                   0, NULL,
                   g_cclosure_marshal_VOID__OBJECT,
                   G_TYPE_NONE,
                   1, OSSO_ABOOK_TYPE_CONTACT);
  signals[CONTACT_DETACHED] =
      g_signal_new("contact-detached",
                   G_TYPE_FROM_CLASS(klass), G_SIGNAL_RUN_FIRST,
                   G_STRUCT_OFFSET(OssoABookContactClass, contact_detached),
                   0, NULL,
                   g_cclosure_marshal_VOID__OBJECT,
                   G_TYPE_NONE,
                   1, OSSO_ABOOK_TYPE_CONTACT);
}

OssoABookContact *
osso_abook_contact_new(void)
{
  return g_object_new(OSSO_ABOOK_TYPE_CONTACT, NULL);
}

OssoABookContact *
osso_abook_contact_new_from_vcard(const char *uid, const char *vcard)
{
  OssoABookContact *contact;
  EVCard *evc;

  g_return_val_if_fail(NULL != vcard, NULL);

  contact = osso_abook_contact_new();
  evc = E_VCARD(contact);

  /*
   * Code copied from eds, as upstream e_vcard_construct_with_uid() returns void
   * as opposed to maemo one, which returns gboolean
   */

  g_return_val_if_fail(E_IS_VCARD(evc), (g_object_unref(contact), NULL));
  /* uid is optional, but if it is given, it must be valid utf-8 */
  g_return_val_if_fail(uid == NULL || g_utf8_validate(uid, -1, NULL),
                       (g_object_unref(contact), NULL));
  g_return_val_if_fail(!e_vcard_is_parsed(evc),
                       (g_object_unref(contact), NULL));

  e_vcard_construct_with_uid(evc, vcard, uid);

  return contact;
}

void
osso_abook_contact_set_roster(OssoABookContact *contact,
                              OssoABookRoster *roster)
{
  OssoABookContactPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(contact));
  g_return_if_fail(OSSO_ABOOK_IS_ROSTER(roster) || !roster);

  priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);

  if (priv->roster)
    g_object_remove_weak_pointer(G_OBJECT(priv->roster),
                                 (gpointer *)&priv->roster);

  priv->roster = roster;

  if (roster)
    g_object_add_weak_pointer(G_OBJECT(roster), (gpointer *)&priv->roster);
}

char *
osso_abook_contact_get_value(EContact *contact, const char *attr_name)
{
  EVCardAttribute *attr;

  g_return_val_if_fail(E_IS_CONTACT(contact), NULL);
  g_return_val_if_fail(NULL != attr_name, NULL);

  attr = e_vcard_get_attribute(E_VCARD(contact), attr_name);

  if (attr)
    return e_vcard_attribute_get_value(attr);

  return NULL;
}

gboolean
osso_abook_contact_get_blocked(OssoABookContact *contact)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), FALSE);

  return osso_abook_presence_get_blocked(OSSO_ABOOK_PRESENCE(contact)) ==
      OSSO_ABOOK_PRESENCE_STATE_YES;
}

OssoABookContact *
osso_abook_contact_new_from_template(EContact *templ)
{
  OssoABookContact *contact;
  char *vcard;

  g_return_val_if_fail(E_IS_CONTACT (templ), NULL);

  vcard = e_vcard_to_string(E_VCARD(templ), EVC_FORMAT_VCARD_30);
  contact = osso_abook_contact_new_from_vcard(
        e_contact_get_const(templ, E_CONTACT_UID), vcard);
  g_free(vcard);

  return contact;
}

GList *
osso_abook_contact_get_values(EContact *contact, const char *attr_name)
{
  EVCardAttribute *attr;

  g_return_val_if_fail(E_IS_CONTACT(contact), NULL);
  g_return_val_if_fail(NULL != attr_name, NULL);

  attr = e_vcard_get_attribute(E_VCARD(contact), attr_name);

  if (attr)
    return e_vcard_attribute_get_values(attr);

  return NULL;
}

void
osso_abook_contact_set_value(EContact *contact, const char *attr_name,
                             const char *value)
{
  EVCard *evc;
  EVCardAttribute *attr;

  g_return_if_fail(E_IS_CONTACT(contact));
  g_return_if_fail(NULL != attr_name);

  evc = E_VCARD(contact);
  attr = e_vcard_get_attribute(evc, attr_name);

  if (attr)
  {
    if (value)
    {
      e_vcard_attribute_remove_values(attr);
      e_vcard_attribute_add_value(attr, value);
    }
    else
      e_vcard_remove_attribute(evc, attr);
  }
  else if (value)
  {
    e_vcard_add_attribute_with_value(evc,
                                     e_vcard_attribute_new(NULL, attr_name),
                                     value);
  }

  if (OSSO_ABOOK_IS_CONTACT(contact))
  {
    osso_abook_contact_update_attributes(OSSO_ABOOK_CONTACT(contact),
                                         attr_name);

  }
}

gboolean
osso_abook_contact_has_invalid_username(OssoABookContact *contact)
{
  const char *vcf;
  EVCardAttribute *attr;
  GList *l;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), TRUE);
  g_return_val_if_fail(osso_abook_contact_is_roster_contact(contact), TRUE);

  vcf = osso_abook_contact_get_vcard_field(contact);

  if (!vcf)
    return TRUE;

  attr = e_vcard_get_attribute(E_VCARD(contact), vcf);

  if (!attr)
    return TRUE;

  for (l = e_vcard_attribute_get_param(attr, "X-OSSO-VALID"); l; l = l->next)
  {
    if (!g_ascii_strcasecmp(l->data, "no"))
      return TRUE;
  }

  return FALSE;
}

const char *
osso_abook_contact_get_vcard_field(OssoABookContact *contact)
{
  OssoABookRoster *roster;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);

  roster = OSSO_ABOOK_CONTACT_PRIVATE(contact)->roster;

  if (roster)
    return osso_abook_roster_get_vcard_field(roster);

  return NULL;
}

const char *
osso_abook_contact_get_name(OssoABookContact *contact)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);

  return osso_abook_contact_get_name_with_order(
        contact, osso_abook_settings_get_name_order());
}

const char *
osso_abook_contact_get_name_with_order(OssoABookContact *contact,
                                       OssoABookNameOrder order)
{
  OssoABookContactPrivate *priv;
  char *secondary;
  char *primary;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);
  g_return_val_if_fail(order < OSSO_ABOOK_NAME_ORDER_COUNT, NULL);

  priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);

  if (!priv->name[order])
  {
    osso_abook_contact_get_name_components(E_CONTACT(contact), order, FALSE,
                                           &primary, &secondary);
    priv->name[order] = osso_abook_concat_names(order, primary, secondary);
    g_free(secondary);
    g_free(primary);
  }

  return priv->name[order];
}

OssoABookRoster *
osso_abook_contact_get_roster(OssoABookContact *contact)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);

  return OSSO_ABOOK_CONTACT_PRIVATE(contact)->roster;
}
