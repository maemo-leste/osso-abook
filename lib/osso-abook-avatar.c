#include <gtk/gtkprivate.h>

#include "osso-abook-avatar.h"
#include "osso-abook-avatar-cache.h"
#include "osso-abook-utils-private.h"

#include "config.h"

typedef OssoABookAvatarIface OssoABookAvatarInterface;

G_DEFINE_INTERFACE(
    OssoABookAvatar,
    osso_abook_avatar,
    G_TYPE_OBJECT
);

static void
osso_abook_avatar_default_init(OssoABookAvatarIface *iface)
{
  g_object_interface_install_property(
        iface,
        g_param_spec_object(
                 "avatar-image",
                 "Avtar Image",
                 "The avatar image",
                 GDK_TYPE_PIXBUF,
                 GTK_PARAM_READABLE));
  g_object_interface_install_property(
        iface,
        g_param_spec_object(
                 "server-image",
                 "Server Image",
                 "The server provided avatar image",
                 GDK_TYPE_PIXBUF,
                 GTK_PARAM_READABLE));
  g_object_interface_install_property(
        iface,
        g_param_spec_boolean(
                 "avatar-user-selected",
                 "Avatar is User-Selected",
                 "Whether the avatar was selected by the user",
                 FALSE,
                 GTK_PARAM_READABLE));
  g_object_interface_install_property(
        iface,
        g_param_spec_boolean(
                 "done-loading",
                 "Done Loading",
                 "Whether the avatar has been loaded",
                 FALSE,
                 GTK_PARAM_READABLE));
  g_signal_new("load-error",
               G_TYPE_FROM_CLASS(iface), G_SIGNAL_RUN_LAST,
               G_STRUCT_OFFSET(OssoABookAvatarIface, load_error),
               0, NULL,
               g_cclosure_marshal_VOID__POINTER,
               G_TYPE_NONE,
               1, G_TYPE_POINTER);
}

gpointer
osso_abook_avatar_get_image_token(OssoABookAvatar *avatar)
{
  GdkPixbuf *image;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR(avatar), NULL);

  g_object_get(avatar, "avatar-image", &image, NULL);

  if (image)
  {
    volatile gint image_ref_count = G_OBJECT(image)->ref_count;

    g_return_val_if_fail(image_ref_count > 1, image);

    g_object_unref(image);
  }

  return image;
}

GdkPixbuf *
osso_abook_avatar_get_image(OssoABookAvatar *avatar)
{
  OssoABookAvatarCache *cache;
  GdkPixbuf *image;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR(avatar), NULL);

  cache = osso_abook_avatar_cache_get_default();
  image = osso_abook_avatar_cache_lookup(cache, avatar);

  if (!image)
  {
    g_object_get((gpointer)avatar, "avatar-image", &image, NULL);

    if (image)
    {
      volatile gint image_ref_count = G_OBJECT(image)->ref_count;

      osso_abook_avatar_cache_add(cache, avatar, image);

      g_return_val_if_fail(image_ref_count > 1, image);

      g_object_unref((gpointer)image);
    }
  }

  return image;
}

const char *
osso_abook_avatar_get_fallback_icon_name(OssoABookAvatar *avatar)
{
  OssoABookAvatarIface *iface;
  const char *icon_name = NULL;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR(avatar), NULL);

  iface = OSSO_ABOOK_AVATAR_GET_IFACE(avatar);

  if (iface->get_fallback_icon_name)
    icon_name = iface->get_fallback_icon_name(avatar);

  if (!icon_name)
    icon_name = "general_default_avatar";

  return icon_name;
}

GdkPixbuf *
osso_abook_avatar_get_image_rounded(OssoABookAvatar *avatar, int width,
                                    int height, gboolean crop, int radius,
                                    const guint8 border_color[4])
{
  gchar *name;
  GdkPixbuf *image;
  OssoABookAvatarCache *cache;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR(avatar), NULL);

  name = _osso_abook_avatar_get_cache_name(width, height, crop, radius,
                                           border_color);
  cache = osso_abook_avatar_cache_get_for_name(name);
  image = osso_abook_avatar_cache_lookup(cache, avatar);
  g_free(name);

  if (image)
    image = g_object_ref(image);
  else
  {
    OssoABookAvatarIface *iface = OSSO_ABOOK_AVATAR_GET_IFACE(avatar);

    if (iface->get_image_scaled)
      image = iface->get_image_scaled(avatar, width, height, crop);
    else
    {
      image = osso_abook_avatar_get_image(avatar);

      if (image)
      {
        image = _osso_abook_scale_pixbuf_and_crop(image, width, height, crop,
                                                  border_color ? 1 : 0);
        _osso_abook_pixbuf_cut_corners(image, radius, border_color);
        osso_abook_avatar_cache_add(cache, avatar, image);
      }
    }
  }

  return image;
}

gboolean
osso_abook_avatar_is_user_selected(OssoABookAvatar *avatar)
{
  gboolean user_selected = FALSE;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR(avatar), FALSE);

  g_object_get(G_OBJECT(avatar),
               "avatar-user-selected", &user_selected,
               NULL);

  return user_selected;
}

gboolean
osso_abook_avatar_is_done_loading(OssoABookAvatar *avatar)
{
  gboolean done_loading = FALSE;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR(avatar), FALSE);

  g_object_get(G_OBJECT(avatar),
               "done-loading", &done_loading,
               NULL);

  return done_loading;
}
