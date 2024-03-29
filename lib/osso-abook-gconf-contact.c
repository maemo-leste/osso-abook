#include "config.h"

#include <gconf/gconf-client.h>
#include <gtk/gtkprivate.h>

#include "osso-abook-gconf-contact.h"
#include "osso-abook-settings.h"

struct _OssoABookGconfContactPrivate
{
  gchar *key;
  guint cnxn;
  GConfClient *gconf;
  gboolean vcard_empty : 1;
  gboolean deleted : 1;
};

typedef struct _OssoABookGconfContactPrivate OssoABookGconfContactPrivate;

#define OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(contact) \
  ((OssoABookGconfContactPrivate *)osso_abook_gconf_contact_get_instance_private( \
     contact))

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookGconfContact,
  osso_abook_gconf_contact,
  OSSO_ABOOK_TYPE_CONTACT
);

enum
{
  PROP_KEY = 1
};

static void
osso_abook_gconf_contact_set_property(GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec)
{
  OssoABookGconfContactPrivate *priv =
    OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(OSSO_ABOOK_GCONF_CONTACT(object));

  switch (property_id)
  {
    case PROP_KEY:
    {
      g_free(priv->key);
      priv->key = g_value_dup_string(value);
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
osso_abook_gconf_contact_get_property(GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec)
{
  OssoABookGconfContactPrivate *priv =
    OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(OSSO_ABOOK_GCONF_CONTACT(object));

  switch (property_id)
  {
    case PROP_KEY:
    {
      g_value_set_string(value, priv->key);
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
osso_abook_gconf_contact_constructed(GObject *object)
{
  GObjectClass *object_class =
    G_OBJECT_CLASS(osso_abook_gconf_contact_parent_class);
  OssoABookGconfContactClass *contact_class =
    OSSO_ABOOK_GCONF_CONTACT_GET_CLASS(object);
  OssoABookGconfContact *contact = OSSO_ABOOK_GCONF_CONTACT(object);
  OssoABookGconfContactPrivate *priv =
    OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(contact);

  if (!priv->key)
  {
    g_return_if_fail(NULL != contact_class->get_gconf_key);

    priv->key = g_strdup(contact_class->get_gconf_key(contact));

    g_return_if_fail(NULL != priv->key);
  }

  if (object_class->constructed)
    object_class->constructed(object);
}

static void
osso_abook_gconf_contact_finalize(GObject *object)
{
  OssoABookGconfContactPrivate *priv =
    OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(OSSO_ABOOK_GCONF_CONTACT(object));

  g_free(priv->key);

  if (priv->cnxn)
    gconf_client_notify_remove(priv->gconf, priv->cnxn);

  G_OBJECT_CLASS(osso_abook_gconf_contact_parent_class)->finalize(object);
}

static EBookStatus
set_gconf_string(OssoABookGconfContact *contact, const gchar *s)
{
  OssoABookGconfContactPrivate *priv =
    OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(contact);

  if (gconf_client_set_string(priv->gconf, priv->key, s, NULL))
    return E_BOOK_ERROR_OK;

  return E_BOOK_ERROR_OTHER_ERROR;
}

static EBookStatus
set_gconf_vcard(OssoABookGconfContact *contact)
{
  EVCard *evc = e_vcard_new();
  GList *attrs = e_vcard_get_attributes(E_VCARD(contact));
  GList *attr;
  gchar *vcs;
  EBookStatus status;

  for (attr = attrs; attr; attr = attr->next)
  {
    if (!osso_abook_contact_attribute_is_readonly(attr->data))
      e_vcard_add_attribute(evc, e_vcard_attribute_copy(attr->data));
  }

  vcs = e_vcard_to_string(evc, EVC_FORMAT_VCARD_30);
  status = set_gconf_string(contact, vcs);
  g_free(vcs);
  g_object_unref(evc);

  return status;
}

static guint
osso_abook_gconf_contact_async_add(OssoABookContact *contact, EBook *book,
                                   EBookIdCallback callback, gpointer user_data)
{
  EBookStatus status = set_gconf_vcard(OSSO_ABOOK_GCONF_CONTACT(contact));

  if (callback)
  {
    const char *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

    callback(book, status, uid, user_data);
  }

  return status == E_BOOK_ERROR_OK;
}

static guint
osso_abook_gconf_contact_async_commit(OssoABookContact *contact, EBook *book,
                                      EBookCallback callback,
                                      gpointer user_data)
{
  EBookStatus status = set_gconf_vcard(OSSO_ABOOK_GCONF_CONTACT(contact));

  if (callback)
    callback(book, status, user_data);

  return status == E_BOOK_ERROR_OK;
}

static EBookStatus
set_gconf_user_deleted(OssoABookGconfContact *contact)
{
  return set_gconf_string(contact, "user-deleted");
}

static guint
osso_abook_gconf_contact_async_remove(OssoABookContact *contact, EBook *book,
                                      EBookCallback callback,
                                      gpointer user_data)
{
  EBookStatus status =
    set_gconf_user_deleted(OSSO_ABOOK_GCONF_CONTACT(contact));

  if (callback)
    callback(book, status, user_data);

  return status == E_BOOK_ERROR_OK;
}

static guint
osso_abook_gconf_contact_async_remove_many(GList *contacts, EBook *book,
                                           EBookCallback callback,
                                           gpointer user_data)
{
  EBookStatus status = E_BOOK_ERROR_OK;
  GList *l;

  for (l = contacts; l; l = l->next)
  {
    EBookStatus contact_status;

    if (!l->data)
      break;

    contact_status = set_gconf_user_deleted(OSSO_ABOOK_GCONF_CONTACT(l->data));

    if (contact_status)
      status = contact_status;
  }

  if (callback)
    callback(book, status, user_data);

  return status == E_BOOK_ERROR_OK;
}

static void
osso_abook_gconf_contact_class_init(OssoABookGconfContactClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  OssoABookContactClass *contact_class = OSSO_ABOOK_CONTACT_CLASS(klass);

  object_class->set_property = osso_abook_gconf_contact_set_property;
  object_class->get_property = osso_abook_gconf_contact_get_property;
  object_class->constructed = osso_abook_gconf_contact_constructed;
  object_class->finalize = osso_abook_gconf_contact_finalize;

  contact_class->async_add = osso_abook_gconf_contact_async_add;
  contact_class->async_commit = osso_abook_gconf_contact_async_commit;
  contact_class->async_remove = osso_abook_gconf_contact_async_remove;
  contact_class->async_remove_many = osso_abook_gconf_contact_async_remove_many;

  g_object_class_install_property(
    object_class, PROP_KEY,
    g_param_spec_string(
      "key",
      "Key",
      "The gconf key",
      NULL,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
osso_abook_gconf_contact_init(OssoABookGconfContact *contact)
{
  OssoABookGconfContactPrivate *priv =
    OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(contact);

  priv->gconf = osso_abook_get_gconf_client();
}

gboolean
osso_abook_gconf_contact_is_vcard_empty(OssoABookGconfContact *self)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_GCONF_CONTACT(self), TRUE);

  return OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(self)->vcard_empty;
}

gboolean
osso_abook_gconf_contact_is_deleted(OssoABookGconfContact *self)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_GCONF_CONTACT(self), FALSE);

  return OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(self)->deleted;
}

static void
load_from_vcs(OssoABookGconfContact *self, const char *vcs)
{
  OssoABookGconfContactPrivate *priv =
    OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(self);
  const char *uid;
  OssoABookContact *contact;

  priv->vcard_empty = !vcs || *vcs == '\0';

  if (!priv->vcard_empty)
    priv->deleted = !g_strcmp0(vcs, "user-deleted");
  else
    priv->deleted = FALSE;

  uid = e_contact_get_const(E_CONTACT(self), E_CONTACT_UID);

  if (priv->vcard_empty || priv->deleted)
    contact = osso_abook_contact_new();
  else
    contact = osso_abook_contact_new_from_vcard(uid, vcs);

  if (contact)
  {
    e_contact_set(E_CONTACT(contact), E_CONTACT_UID, uid);
    osso_abook_contact_reset(OSSO_ABOOK_CONTACT(self), contact);
    g_object_unref(contact);
  }
}

static void
gconf_notiy_cb(GConfClient *client, guint cnxn_id, GConfEntry *entry,
               gpointer user_data)
{
  OssoABookGconfContact *contact = (OssoABookGconfContact *)user_data;

  if (entry && entry->value)
    load_from_vcs(contact, gconf_value_get_string(entry->value));
  else
    load_from_vcs(contact, NULL);
}

void
osso_abook_gconf_contact_load(OssoABookGconfContact *self)
{
  OssoABookGconfContactPrivate *priv;
  gchar *vcs;

  g_return_if_fail(OSSO_ABOOK_IS_GCONF_CONTACT(self));

  priv = OSSO_ABOOK_GCONF_CONTACT_GET_PRIVATE(self);

  if (!priv->cnxn)
  {
    priv->cnxn = gconf_client_notify_add(priv->gconf, priv->key, gconf_notiy_cb,
                                         self, NULL, NULL);
  }

  vcs = gconf_client_get_string(priv->gconf, priv->key, NULL);
  load_from_vcs(self, vcs);
  g_free(vcs);
}

EVCardAttribute *
osso_abook_gconf_contact_add_ro_attribute(OssoABookGconfContact *self,
                                          const gchar *attr_name,
                                          const gchar *value)
{
  EVCardAttribute *attr = e_vcard_attribute_new(NULL, attr_name);

  e_vcard_add_attribute_with_value(E_VCARD(self), attr, value);
  osso_abook_contact_attribute_set_readonly(attr, TRUE);

  return attr;
}
