#include "config.h"

#include "osso-abook-avatar.h"
#include "osso-abook-gconf-contact.h"
#include "osso-abook-settings.h"
#include "osso-abook-voicemail-contact.h"

static void
osso_abook_voicemail_contact_osso_abook_avatar_iface_init(
  OssoABookAvatarIface *iface);

G_DEFINE_TYPE_WITH_CODE(
  OssoABookVoicemailContact,
  osso_abook_voicemail_contact,
  OSSO_ABOOK_TYPE_GCONF_CONTACT,
  G_IMPLEMENT_INTERFACE(
    OSSO_ABOOK_TYPE_AVATAR,
    osso_abook_voicemail_contact_osso_abook_avatar_iface_init);
);

static const char *
osso_abook_voicemail_contact_get_fallback_icon_name(OssoABookAvatar *avatar)
{
  return OSSO_ABOOK_VOICEMAIL_CONTACT_ICON_NAME;
}

static void
osso_abook_voicemail_contact_osso_abook_avatar_iface_init(
  OssoABookAvatarIface *iface)
{
  iface->get_fallback_icon_name =
    osso_abook_voicemail_contact_get_fallback_icon_name;
}

static void
osso_abook_voicemail_contact_init(OssoABookVoicemailContact *contact)
{
  e_contact_set(E_CONTACT(contact), E_CONTACT_UID,
                OSSO_ABOOK_VOICEMAIL_CONTACT_UID);
}

static guint
osso_abook_voicemail_contact_async_commit(OssoABookContact *contact,
                                          EBook *book, EBookCallback callback,
                                          gpointer user_data)
{
  return OSSO_ABOOK_CONTACT_CLASS(osso_abook_voicemail_contact_parent_class)->
         async_commit(contact, book, callback, user_data);
}

static char *
osso_abook_voicemail_contact_get_default_name(OssoABookContact *contact)
{
  return g_strdup(g_dgettext("osso-addressbook", "addr_fi_voicemailbox"));
}

static const char *
osso_abook_voicemail_contact_get_gconf_key(OssoABookGconfContact *self)
{
  return OSSO_ABOOK_SETTINGS_KEY_VOICEMAIL_VCARD;
}

static void
osso_abook_voicemail_contact_class_init(OssoABookVoicemailContactClass *klass)
{
  OssoABookContactClass *contact_class = OSSO_ABOOK_CONTACT_CLASS(klass);
  OssoABookGconfContactClass *gconf_contact_class =
    OSSO_ABOOK_GCONF_CONTACT_CLASS(klass);

  contact_class->async_commit = osso_abook_voicemail_contact_async_commit;
  contact_class->get_default_name =
    osso_abook_voicemail_contact_get_default_name;

  gconf_contact_class->get_gconf_key =
    osso_abook_voicemail_contact_get_gconf_key;
}

OssoABookVoicemailContact *
osso_abook_voicemail_contact_new()
{
  return g_object_new(OSSO_ABOOK_TYPE_VOICEMAIL_CONTACT, NULL);
}

OssoABookVoicemailContact *
osso_abook_voicemail_contact_get_default()
{
  static OssoABookVoicemailContact *contact = NULL;

  if (contact)
    return g_object_ref(contact);

  contact = osso_abook_voicemail_contact_new();
  g_object_add_weak_pointer(G_OBJECT(contact), (gpointer *)&contact);
  osso_abook_gconf_contact_load(OSSO_ABOOK_GCONF_CONTACT(contact));

  return contact;
}

char *
osso_abook_voicemail_contact_get_preferred_number(
  OssoABookVoicemailContact *contact)
{
  char *tel;

  g_return_val_if_fail(
    OSSO_ABOOK_IS_VOICEMAIL_CONTACT(contact) || !contact, NULL);

  if (!contact)
    contact = osso_abook_voicemail_contact_get_default();
  else
    contact = g_object_ref(contact);

  if (!contact)
    return NULL;

  tel = osso_abook_contact_get_value(E_CONTACT(contact), EVC_TEL);
  g_object_unref(contact);

  return tel;
}
