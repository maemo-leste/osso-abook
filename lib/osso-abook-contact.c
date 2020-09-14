#include <glib.h>
#include <telepathy-glib/enums.h>

#include <string.h>

#include "osso-abook-contact.h"
#include "osso-abook-presence.h"
#include "osso-abook-enums.h"
#include "osso-abook-roster.h"
#include "osso-abook-string-list.h"
#include "osso-abook-account-manager.h"
#include "osso-abook-contact-private.h"

#include "config.h"

#include "osso-abook-util.h"

/* FIXME - generate during compile time, somehow*/
#define OSSO_ABOOK_NAME_ORDER_COUNT 4

#define FILE_SCHEME "file://"
#define MAX_AVATAR_SIZE 512000

struct _OssoABookContactPrivate
{
  gchar *name[OSSO_ABOOK_NAME_ORDER_COUNT];
  char **collate_keys[OSSO_ABOOK_NAME_ORDER_COUNT];
  OssoABookRoster *roster;
  GHashTable *contacts;
  OssoABookStringList master_uids;
  GdkPixbuf *avatar_image;
  int field_30;
  OssoABookCapsFlags caps;
  int field_38;
  TpConnectionPresenceType presence_type;
  gchar *presence_status;
  gchar *presence_status_message;
  gchar *presence_location_string;
  OssoABookPresence *presence;
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

struct roster_contact
{
  gpointer uid;
  gpointer contact;
};

struct roster_uid
{
  gpointer a;
  gpointer b;
  unsigned short priority;
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
presence_type_cb(OssoABookPresence *presence, GParamSpec *pspec,
                 OssoABookContact *contact)
{
  if (pspec)
    g_object_notify(G_OBJECT(contact), g_param_spec_get_name(pspec));
}

static void
avatar_image_cb(OssoABookPresence *presence, GParamSpec *pspec,
                OssoABookContact *contact)
{
  OssoABookContactPrivate *priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);

  if (!osso_abook_contact_photo_is_user_selected(contact))
  {
    if (priv->avatar_image)
    {
      g_object_unref(priv->avatar_image);
      priv->avatar_image = 0;
    }
  }

  presence_type_cb(presence, pspec, contact);
}

static void
osso_abook_contact_notify(OssoABookContact *contact, const gchar *property_name)
{
  OssoABookContactPrivate *priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);

  if (!(priv->flags & 0x40)) //!priv->disposed
  {
    /* if (!e_vcard_is_parsing(E_VCARD(contact))) - not in upstream eds*/
      g_object_notify(G_OBJECT(contact), property_name);
  }
}

static void
connect_signals(OssoABookContact *contact, OssoABookPresence *presence)
{
  OssoABookContactPrivate *priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);

  if (priv->presence == presence)
    return;

  if ( priv->presence )
  {
    g_signal_handlers_disconnect_matched(
          priv->presence, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
          presence_type_cb, contact);
    g_signal_handlers_disconnect_matched(
          priv->presence, G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
          avatar_image_cb, contact);
  }

  priv->presence = presence;
  priv->presence_type = TP_CONNECTION_PRESENCE_TYPE_UNSET;

  if (presence)
  {
    priv->presence_type = osso_abook_presence_get_presence_type(presence);
    g_signal_connect(priv->presence, "notify::avatar-image",
                     G_CALLBACK(avatar_image_cb), contact);
    g_signal_connect(priv->presence, "notify::presence-type",
                     G_CALLBACK(presence_type_cb), contact);
    g_signal_connect(priv->presence, "notify::presence-status",
                     G_CALLBACK(presence_type_cb), contact);
    g_signal_connect(priv->presence, "notify::presence-status-message",
                     G_CALLBACK(presence_type_cb), contact);
    g_signal_connect(priv->presence, "notify::presence-location-string",
                     G_CALLBACK(presence_type_cb), contact);
    avatar_image_cb(priv->presence, NULL, contact);
  }

  if (!(priv->flags & 0x40))
  {
    g_object_freeze_notify(G_OBJECT(contact));
    osso_abook_contact_notify(contact, "avatar-image");
    osso_abook_contact_notify(contact, "server-image");
    osso_abook_contact_notify(contact, "presence-type");
    osso_abook_contact_notify(contact, "presence-status");
    osso_abook_contact_notify(contact, "presence-status-message");
    osso_abook_contact_notify(contact, "presence-location-string");
    g_object_thaw_notify(G_OBJECT(contact));
  }
}

static void
osso_abook_contact_dispose(GObject *object)
{
  OssoABookContact *contact = OSSO_ABOOK_CONTACT(object);
  OssoABookContactPrivate *priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);

  priv->flags |= 0x41u;
  connect_signals(contact, NULL);

  if (priv->avatar_image)
  {
    g_object_unref(priv->avatar_image);
    priv->avatar_image = NULL;
  }

  if (priv->roster)
  {
    g_object_remove_weak_pointer((GObject *)priv->roster,
                                 (gpointer *)&priv->roster);
    priv->roster = NULL;
  }

  if (priv->contacts)
    g_hash_table_remove_all(priv->contacts);

  G_OBJECT_CLASS(osso_abook_contact_parent_class)->dispose(object);
}

static void
free_names_and_collate_keys(OssoABookContactPrivate *priv)
{
  int i;

  for (i = 0; i < G_N_ELEMENTS(priv->collate_keys); i++)
  {
    g_strfreev(priv->collate_keys[i]);
    priv->collate_keys[i] = NULL;
  }

  for (i = 0; i < G_N_ELEMENTS(priv->name); i++)
  {
    g_free(priv->name[i]);
    priv->name[i] = NULL;
  }
}

static int
compare_roster_contacts(gconstpointer a, gconstpointer b)
{
  struct roster_contact const *_a = a;
  struct roster_contact const *_b = b;
  struct roster_uid const *uid_a = _a->uid;
  struct roster_uid const *uid_b = _b->uid;

  return uid_b->priority - uid_a->priority;
}

static GdkPixbuf *
get_server_image(OssoABookContact *contact)
{
  OssoABookContactPrivate *priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);
  GdkPixbuf *pixbuf = NULL;
  GPtrArray *arr;
  struct roster_contact *c;
  GHashTableIter iter;

  if (!priv->contacts)
    return NULL;

  arr = g_ptr_array_new();
  g_hash_table_iter_init(&iter, priv->contacts);

  while (g_hash_table_iter_next(&iter, 0, (gpointer *)&c))
    g_ptr_array_add(arr, c);

  if (arr->len)
  {
    int i;

    g_ptr_array_sort(arr, compare_roster_contacts);

    for (i = 0; i < arr->len; i++)
    {
      c = arr->pdata[i];

      if ((pixbuf = osso_abook_avatar_get_image(OSSO_ABOOK_AVATAR(c->contact))))
        break;
    }
  }

  g_ptr_array_free(arr, TRUE);

  return pixbuf;
}

static void
size_prepared_cb(GdkPixbufLoader *loader, gint width, gint height,
                 gpointer user_data)
{
  double cw = 144.0f / (double)width;
  double ch = 144.0f / (double)height;
  double c = cw >= ch ? ch : cw;

  if (c > 1.0f)
    c = 1.0f;

  gdk_pixbuf_loader_set_size(loader, (c * (double)width), (c * (double)height));
}

static gboolean
_is_regular_file(const gchar *fname)
{
  if (!fname)
    return FALSE;

  return g_file_test(fname, G_FILE_TEST_IS_REGULAR);

}

static gboolean
_is_local_file(const gchar *uri)
{
  const gchar *fname;

  if ( !uri || !*uri )
    return FALSE;

  if (g_str_has_prefix(uri, FILE_SCHEME))
    fname = uri + strlen(FILE_SCHEME);
  else if (*uri == '/')
    fname = uri;
  else
    return FALSE;

  return _is_regular_file(fname);
}

static gboolean
_osso_abook_avatar_read_file(GFile *file, char **contents, gsize *length,
                             GError **error)
{
  gchar *uri;

  g_return_val_if_fail(file, FALSE);
  g_return_val_if_fail(contents, FALSE);

  uri = g_file_get_uri(file);

  if (_is_local_file(uri))
  {
    GFileInfo *info = g_file_query_info(file, G_FILE_ATTRIBUTE_STANDARD_SIZE,
                                        G_FILE_QUERY_INFO_NONE, NULL, error);
    if (info)
    {
      if (g_file_info_get_size(info) <= MAX_AVATAR_SIZE)
      {
        if (g_file_load_contents(file, NULL, contents, length, NULL, error))
        {
          g_object_unref(info);
          g_free(uri);
          return TRUE;
        }
      }
      else
      {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED,
                    "File '%s' is too big for an avatar", uri);
      }
    }

    g_object_unref(info);
  }

  g_free(uri);

  *contents = NULL;

  if (length)
    *length = 0;

  return FALSE;
}
static GdkPixbuf *
get_avatar_image(OssoABookContact *contact)
{
  OssoABookContactPrivate *priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);
  EContactPhoto *photo;

  if (priv->avatar_image)
    return priv->avatar_image;

  photo = osso_abook_contact_get_contact_photo(E_CONTACT(contact));

  if (photo)
  {
    GdkPixbufLoader *pixbuf_loader = gdk_pixbuf_loader_new();

    g_signal_connect(pixbuf_loader, "size-prepared",
                     G_CALLBACK(size_prepared_cb), NULL);

    if (photo->type)
    {
      if (photo->type == E_CONTACT_PHOTO_TYPE_URI)
      {
        GFile *file = g_file_new_for_uri(photo->data.uri);
        char *buf;
        gsize count;

        if (_osso_abook_avatar_read_file(file, &buf, &count, NULL))
        {
          gdk_pixbuf_loader_write(pixbuf_loader, (guchar *)buf, count, NULL);
          g_free(buf);
        }
        g_object_unref((gpointer)file);
      }
    }
    else
    {
      gdk_pixbuf_loader_write(pixbuf_loader, photo->data.inlined.data,
                              photo->data.inlined.length, NULL);
    }

    if (gdk_pixbuf_loader_close(pixbuf_loader, NULL))
    {
      priv->avatar_image =
          g_object_ref(gdk_pixbuf_loader_get_pixbuf(pixbuf_loader));
    }

    e_contact_photo_free(photo);
    g_object_unref(pixbuf_loader);
  }

  return priv->avatar_image;
}

static void
osso_abook_contact_get_property(GObject *object, guint property_id,
                                GValue *value, GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_AVATAR_IMAGE:
    {
      OssoABookContact *contact = OSSO_ABOOK_CONTACT(object);
      GdkPixbuf *image = get_avatar_image(contact);

      if (!image && !osso_abook_contact_is_roster_contact(contact))
        image = get_server_image(contact);

      g_value_set_object(value, image);
      break;
    }
    case PROP_SERVER_IMAGE:
    {
      g_value_set_object(value, get_server_image(OSSO_ABOOK_CONTACT(object)));
      break;
    }
    case PROP_CAPABILITIES:
    {
      OssoABookCapsFlags caps_flags =
          osso_abook_caps_get_capabilities(OSSO_ABOOK_CAPS(object));

      g_value_set_flags(value, caps_flags);
      break;
    }
    case PROP_DISPLAY_NAME:
    {
      const char *display_name =
          osso_abook_contact_get_display_name(OSSO_ABOOK_CONTACT(object));

      g_value_set_string(value, display_name);
      break;
    }
    case PROP_PRESENCE_TYPE:
    {
      TpConnectionPresenceType presence_type =
          osso_abook_presence_get_presence_type(OSSO_ABOOK_PRESENCE(object));

      g_value_set_uint(value, presence_type);
      break;
    }
    case PROP_PRESENCE_STATUS:
    {
      const char *presence_status =
          osso_abook_presence_get_presence_status(OSSO_ABOOK_PRESENCE(object));

      g_value_set_string(value, presence_status);
      break;
    }
    case PROP_PRESENCE_STATUS_MESSAGE:
    {
      const char *status_message =
          osso_abook_presence_get_presence_status_message(
            OSSO_ABOOK_PRESENCE(object));

      g_value_set_string(value, status_message);
      break;
    }
    case PROP_PRESENCE_LOCATION_STRING:
    {
      G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      const char *location_string =
          osso_abook_presence_get_location_string(OSSO_ABOOK_PRESENCE(object));
      G_GNUC_END_IGNORE_DEPRECATIONS

      g_value_set_string(value, location_string);
      break;
    }
    case PROP_AVATAR_USER_SELECTED:
    {
      gboolean is_user_selected = osso_abook_contact_photo_is_user_selected(
            OSSO_ABOOK_CONTACT(object));

      g_value_set_boolean(value, is_user_selected);
      break;
    }
    case PROP_DONE_LOADING:
    {
      g_value_set_boolean(value, TRUE);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_contact_finalize(GObject *object)
{
  OssoABookContact *contact = OSSO_ABOOK_CONTACT(object);
  OssoABookContactPrivate *priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);

  if (priv->contacts)
    g_hash_table_destroy(priv->contacts);

  osso_abook_string_list_free(priv->master_uids);
  g_free(priv->presence_status_message);
  g_free(priv->presence_status);
  g_free(priv->presence_location_string);
  free_names_and_collate_keys(priv);

  G_OBJECT_CLASS(osso_abook_contact_parent_class)->finalize(object);
}
static void
osso_abook_contact_class_init(OssoABookContactClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  EVCardClass *e_vcard_class = E_VCARD_CLASS(klass);
  GEnumClass *name_order_class = g_type_class_ref(OSSO_ABOOK_TYPE_NAME_ORDER);

  g_assert_cmpint(OSSO_ABOOK_NAME_ORDER_COUNT, ==, name_order_class->n_values);
  g_type_class_unref(name_order_class);

  object_class->dispose = osso_abook_contact_dispose;
  object_class->finalize = osso_abook_contact_finalize;
  object_class->get_property = osso_abook_contact_get_property;

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

const char **
osso_abook_contact_get_collate_keys(OssoABookContact *contact,
                                    OssoABookNameOrder order)
{
  static const char *no_collate_keys[] = {NULL};
  OssoABookContactPrivate *priv;
  gchar *secondary_out;
  gchar *primary_out;
  char **collate_key;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), no_collate_keys);
  g_return_val_if_fail(order < OSSO_ABOOK_NAME_ORDER_COUNT, no_collate_keys);

  priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);

  if (priv->collate_keys[order])
    return (const char **)priv->collate_keys[order];


  osso_abook_contact_get_name_components(E_CONTACT(contact), order, FALSE,
                                         &primary_out, &secondary_out);
  collate_key = g_new0(char *, 3);
  priv->collate_keys[order] = collate_key;

  if (primary_out)
  {
    collate_key[0] = g_utf8_collate_key(primary_out, -1);
    g_free(primary_out);
  }

  if (secondary_out)
  {
    collate_key[1] = g_utf8_collate_key(secondary_out, -1);
    g_free(secondary_out);
  }

  return (const char **)collate_key;
}

gboolean
osso_abook_is_temporary_uid(const char *uid)
{
  if (!uid)
    return FALSE;

  return g_str_has_prefix(uid, "osso-abook-tmc");
}

GList *
osso_abook_contact_get_roster_contacts(OssoABookContact *master_contact)
{
  OssoABookContactPrivate *priv;
  GList *contacts = NULL;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(master_contact), NULL);

  priv = OSSO_ABOOK_CONTACT_PRIVATE(master_contact);

  if (priv->contacts)
  {
    GHashTableIter iter;
    struct roster_contact *c;

    g_hash_table_iter_init(&iter, priv->contacts);

    while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&c))
      contacts = g_list_prepend(contacts, c->contact);
  }

  return contacts;
}

gboolean
osso_abook_contact_is_roster_contact(OssoABookContact *contact)
{

  OssoABookRoster *roster;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), FALSE);

  roster = OSSO_ABOOK_CONTACT_PRIVATE(contact)->roster;

  if (roster)
    return osso_abook_roster_get_account(roster) != NULL;


  return FALSE;
}

int
osso_abook_contact_uid_compare(OssoABookContact *a, OssoABookContact *b)
{
  const char *uid_a = e_contact_get_const(E_CONTACT(a), E_CONTACT_UID);
  const char *uid_b = e_contact_get_const(E_CONTACT(b), E_CONTACT_UID);

  if (uid_a)
  {
    if (uid_b)
      return strverscmp(uid_a, uid_b);
    else
      return 1;
  }
  else if (uid_b)
    return -1;

  return 0;
}

const char *
osso_abook_contact_get_display_name(OssoABookContact *contact)
{
  const char *name;

  name = osso_abook_contact_get_name(contact);

  if (!name || !*name)
    name = g_dgettext("osso-addressbook", "addr_li_unnamed_contact");

  return name;
}

void
osso_abook_contact_get_name_components(EContact *contact,
                                       OssoABookNameOrder order,
                                       gboolean strict,
                                       char **primary_out,
                                       char **secondary_out)
{
  gchar *primary = NULL;
  gchar *secondary = NULL;
  gint primary_match_num = 1;
  gint secondary_match_num = 2;

  switch (order)
  {
    case OSSO_ABOOK_NAME_ORDER_LAST:
    case OSSO_ABOOK_NAME_ORDER_LAST_SPACE:
    {
      gchar *given_name = e_contact_get(contact, E_CONTACT_GIVEN_NAME);
      gchar *family_name = e_contact_get(contact, E_CONTACT_FAMILY_NAME);

      if (strict || (family_name && *family_name))
      {
        primary = family_name;
        secondary = given_name;
      }
      else
      {
        primary = given_name;
        g_free(family_name);
      }

      primary_match_num = 2;
      secondary_match_num = 1;

      break;
    }
    case OSSO_ABOOK_NAME_ORDER_NICK:
    {
      gchar *nickname = e_contact_get(contact, E_CONTACT_NICKNAME);

      if (strict || (nickname && *nickname))
        primary = nickname;
      else
      {
        osso_abook_contact_get_name_components(contact,
                                               OSSO_ABOOK_NAME_ORDER_FIRST,
                                               FALSE, &primary, &secondary);
        g_free(nickname);
      }

      break;
    }
    case OSSO_ABOOK_NAME_ORDER_FIRST:
    {
      gchar *given_name = e_contact_get(contact, E_CONTACT_GIVEN_NAME);
      gchar *family_name = e_contact_get(contact, E_CONTACT_FAMILY_NAME);

      if (strict || (given_name && *given_name))
      {
        primary = given_name;
        secondary = family_name;
      }
      else
      {
        primary = family_name;
        g_free(given_name);
      }

      break;
    }
    default:
      break;
  }

  if ((!primary || !*primary) && (!secondary || !*secondary))
  {
    const gchar *full_name;
    GMatchInfo *match_info;

    g_free(primary);
    primary = NULL;
    g_free(secondary);
    secondary = NULL;
    full_name = e_contact_get_const(contact, E_CONTACT_FULL_NAME);

    if (full_name && *full_name)
    {
      GRegex *regex = g_regex_new("^\\s*(.*)\\s+(\\S+)\\s*$", 0, 0, NULL);

      if (g_regex_match(regex, full_name, 0, &match_info))
      {
        primary = g_match_info_fetch(match_info, primary_match_num);
        secondary = g_match_info_fetch(match_info, secondary_match_num);
      }
      else
        primary = g_strdup(full_name);

      g_match_info_free(match_info);
      g_regex_unref(regex);
    }

    if (!strict)
    {
      if (!primary || !*primary)
      {
        g_free(primary);
        primary = e_contact_get(contact, E_CONTACT_NICKNAME);
      }

      if (!primary || !*primary)
      {
        g_free(primary);
        primary = e_contact_get(contact, E_CONTACT_ORG);
      }
    }
  }

  if (!primary || !*primary)
  {
    g_free(primary);
    primary = NULL;
  }

  if (!secondary || !*secondary)
  {
    g_free(secondary);
    secondary = NULL;
  }

  if (!primary)
  {
    if (OSSO_ABOOK_IS_CONTACT(contact))
    {
      OssoABookContactClass *klass = OSSO_ABOOK_CONTACT_GET_CLASS(contact);

      if (klass->get_default_name)
        primary = klass->get_default_name((OssoABookContact *)contact);
    }
  }

  if (strict || primary)
    goto out;

  if (OSSO_ABOOK_IS_CONTACT(contact))
  {
    GHashTable *contacts =
        OSSO_ABOOK_CONTACT_PRIVATE(OSSO_ABOOK_CONTACT(contact))->contacts;

    if (contacts)
    {
      GHashTableIter iter;
      struct roster_contact *c;

      g_hash_table_iter_init(&iter, contacts);

      while (g_hash_table_iter_next(&iter, NULL, (gpointer *)&c))
      {
        osso_abook_contact_get_name_components(E_CONTACT(c->contact), order,
                                               FALSE, &primary, &secondary);

        if (primary && *primary)
            break;

        g_free(secondary);
      }
    }
  }

  if (!primary)
  {
    GList *emails = e_contact_get(E_CONTACT(contact), E_CONTACT_EMAIL);

    for (GList *l = emails; l; l = l->next)
    {
      gchar *email = l->data;

      if (email && *email)
      {
        primary = g_strdup(email);
        break;
      }
    }

    osso_abook_string_list_free(emails);
  }

  if (!primary)
  {
    GList *attr = e_vcard_get_attributes(E_VCARD(contact));

    while(attr)
    {
      if (osso_abook_account_manager_has_primary_vcard_field(
            NULL, e_vcard_attribute_get_name(attr->data)))
      {
        GList *val = e_vcard_attribute_get_values(attr->data);

        if (val && val->data && *(const char *)(val->data))
        {
          primary = g_strdup(val->data);
          break;
        }
      }

      attr = attr->next;
    }
  }

out:

  if (primary_out)
    *primary_out = primary;
  else
    g_free(primary);

  if (secondary_out)
    *secondary_out = secondary;
  else
    g_free(secondary);
}

int
osso_abook_contact_collate(OssoABookContact *a, OssoABookContact *b,
                           OssoABookNameOrder order)
{
  const char **keys_a = osso_abook_contact_get_collate_keys(a, order);
  const char **keys_b = osso_abook_contact_get_collate_keys(b, order);

  if (!keys_a)
  {
    if (keys_b)
      return -1;

    return 0;
  }

  if (!keys_b)
    return 1;


  while(1)
  {
    int cmpres;
    const char *key_a = *keys_a;
    const char *key_b = *keys_b;

    if (!key_a)
    {
      if (key_b)
        return -1;

      return 0;
    }

    if (!key_b)
      return 1;

    cmpres = strcmp(key_a, key_b);

    if (cmpres)
      return cmpres;

    keys_a++;
    keys_b++;
  }
}

gboolean
osso_abook_contact_photo_is_user_selected(OssoABookContact *contact)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), FALSE);

  if (osso_abook_contact_is_roster_contact(contact) ||
      osso_abook_is_temporary_uid(
        e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID)))
  {
    return FALSE;
  }

  return e_vcard_get_attribute(E_VCARD(contact), "PHOTO") != NULL;
}

EContactPhoto *
osso_abook_contact_get_contact_photo(EContact *contact)
{
  EContactPhoto *photo;

  g_return_val_if_fail(contact != NULL, NULL);
  g_return_val_if_fail(E_IS_CONTACT(contact), NULL);

  photo = e_contact_get(E_CONTACT(contact), E_CONTACT_PHOTO);

  if (photo)
  {
    if (photo->type == E_CONTACT_PHOTO_TYPE_URI &&
        !_is_local_file(photo->data.uri))
    {
      e_contact_photo_free(photo);
      photo = NULL;
    }
  }

  return photo;
}

gboolean
osso_abook_contact_attribute_is_readonly(EVCardAttribute *attribute)
{
  GList *l;

  g_return_val_if_fail(NULL != attribute, FALSE);

  for (l = e_vcard_attribute_get_params(attribute); l; l = l->next)
  {
    const char *name= e_vcard_attribute_param_get_name(l->data);

    if (name && !g_ascii_strcasecmp(name, OSSO_ABOOK_VCP_OSSO_READONLY))
      return TRUE;
  }

  return FALSE;
}

void
osso_abook_contact_reset(OssoABookContact *contact,
                         OssoABookContact *replacement)
{
  OssoABookContactPrivate *priv;
  GList *l;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(contact));
  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(replacement));

  priv = OSSO_ABOOK_CONTACT_PRIVATE(contact);

  g_return_if_fail(/* !priv->disposed */!(priv->flags & 0x40));

  g_object_freeze_notify(G_OBJECT(contact));
  priv->flags |= 1u;

  for (l = e_vcard_get_attributes(E_VCARD(contact)); l; l = l->next)
  {
    if (!osso_abook_contact_attribute_is_readonly(l->data))
      e_vcard_remove_attribute(E_VCARD(contact), l->data);
  }

  for (l = e_vcard_get_attributes(E_VCARD(replacement)); l; l = l->next)
    e_vcard_add_attribute(E_VCARD(contact), e_vcard_attribute_copy(l->data));

  priv->flags &= 0xFEu;
  g_assert(0);
  /*parse_capabilities(contact, priv);
  parse_presence(contact, priv);*/
  g_object_thaw_notify(G_OBJECT(contact));
  g_signal_emit(contact, signals[RESET], 0);
}

/* readonly seems to be ignored, see osso_abook_contact_attribute_is_readonly */
void
osso_abook_contact_attribute_set_readonly(EVCardAttribute *attribute,
                                          gboolean readonly)
{
  EVCardAttributeParam *param;

  g_return_if_fail(NULL != attribute);

  param = e_vcard_attribute_param_new(OSSO_ABOOK_VCP_OSSO_READONLY);
  e_vcard_attribute_add_param(attribute, param);
}

char *
osso_abook_create_temporary_uid()
{
  static guint temp_uid;

  return g_strdup_printf("osso-abook-tmc%d", ++temp_uid);
}

TpAccount *
osso_abook_contact_get_account(OssoABookContact *contact)
{
  OssoABookRoster *roster;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);

  roster = OSSO_ABOOK_CONTACT_PRIVATE(contact)->roster;

  if (roster)
    return osso_abook_roster_get_account(roster);

  return NULL;
}

static EBook *
get_contact_book(OssoABookContact *contact)
{
  OssoABookRoster *roster = osso_abook_contact_get_roster(contact);
  EBook *book;

  if (!roster)
    return osso_abook_system_book_dup_singleton(TRUE, NULL);

  book = osso_abook_roster_get_book(roster);

  if (!book)
    return osso_abook_system_book_dup_singleton(TRUE, NULL);

  g_object_ref(book);

  return book;
}

void
_osso_abook_contact_reject_for_uid_full(OssoABookContact *contact,
                                        const gchar *master_uid,
                                        gboolean always_keep_roster_contact,
                                        GError **error)
{
  const gchar *contact_uid;
  EBook *book;
  GString *id;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT(contact));
  g_return_if_fail(master_uid || !always_keep_roster_contact);

  contact_uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);
  g_return_if_fail(NULL != contact_uid);

  book = get_contact_book(contact);
  g_return_if_fail(NULL != book);

  id = g_string_new(contact_uid);

  g_string_append_c(id, ';');

  if (master_uid)
    g_string_append(id, master_uid);
  else
    g_string_append(id, "if-unused");

  if (always_keep_roster_contact)
  {
    g_string_append_c(id, ';');
    g_string_append(id, "preserve");
  }

  e_book_remove_contact(book, id->str, error);
  g_object_unref(book);
  g_string_free(id, TRUE);
}

static void
parse_master_uids(OssoABookContact *contact, OssoABookContactPrivate *priv)
{
  GHashTable *uids = g_hash_table_new(g_str_hash, g_str_equal);
  GList *attr;

  for (attr = e_vcard_get_attributes(E_VCARD(contact)); attr; attr = attr->next)
  {
    const char *attr_name = e_vcard_attribute_get_name(attr->data);

    if (!strcmp(attr_name, OSSO_ABOOK_VCA_OSSO_MASTER_UID))
    {
      GList *values = e_vcard_attribute_get_values(attr->data);

      if (values)
      {
        g_hash_table_insert(uids, g_strdup(values->data), attr->data);
        g_warn_if_fail(NULL == values->next);
      }
      else
        g_warn_if_fail(NULL != values);
    }
  }

  priv->flags |= 8u; // master_uids_parsed
  priv->master_uids = g_hash_table_get_keys(uids);
  g_hash_table_destroy(uids);
}

GList *
osso_abook_contact_get_master_uids(OssoABookContact *roster_contact)
{
  OssoABookContactPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(roster_contact), NULL);

  priv = OSSO_ABOOK_CONTACT_PRIVATE(roster_contact);

  if (!(priv->flags & 8))
    parse_master_uids(roster_contact, priv);

  return priv->master_uids;
}

const char *
osso_abook_contact_get_bound_name(OssoABookContact *contact)
{
  const char *vcf;
  GList *l;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT(contact), NULL);

  vcf = osso_abook_contact_get_vcard_field(contact);

  if (vcf && (l = osso_abook_contact_get_values(E_CONTACT(contact), vcf)))
    return l->data;

  return NULL;
}
