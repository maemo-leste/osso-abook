#include <gtk/gtkprivate.h>

#include "osso-abook-avatar-cache.h"
#include "osso-abook-utils-private.h"

#include "config.h"

struct _OssoABookAvatarCachePrivate
{
  GQueue queue;
  GHashTable *cached_images;
  guint limit;
};

typedef struct _OssoABookAvatarCachePrivate OssoABookAvatarCachePrivate;

struct _CachedImage
{
  OssoABookAvatarCache *cache;
  OssoABookAvatar *avatar;
  GdkPixbuf *pixbuf;
  gpointer image_token;
};

typedef struct _CachedImage CachedImage;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookAvatarCache,
  osso_abook_avatar_cache,
  G_TYPE_OBJECT
);

#define OSSO_ABOOK_AVATAR_CACHE_PRIVATE(cache) \
                osso_abook_avatar_cache_get_instance_private(cache)

enum {
  PROP_CACHE_LIMIT = 1
};

#define DEFAULT_CACHE_LIMIT 100

static GHashTable *cache_by_name = NULL;

static void
prune_n_items(OssoABookAvatarCachePrivate *priv, guint n_items)
{
  guint queue_length = g_queue_get_length(&priv->queue);
  guint i;

  g_warn_if_fail(queue_length != g_hash_table_size (priv->cached_images));

  if (queue_length < n_items)
    n_items = queue_length;

  for (i = 0; i < n_items; i++)
    g_hash_table_remove(priv->cached_images, g_queue_pop_head(&priv->queue));
}

static void
osso_abook_avatar_cache_set_property(GObject *object, guint property_id,
                                     const GValue *value, GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_CACHE_LIMIT:
    {
      OssoABookAvatarCache *cache = OSSO_ABOOK_AVATAR_CACHE(object);
      OssoABookAvatarCachePrivate *priv =
          OSSO_ABOOK_AVATAR_CACHE_PRIVATE(cache);
      guint new_limit = g_value_get_uint(value);
      guint limit = g_hash_table_size(priv->cached_images);

      if (new_limit < limit)
        prune_n_items(priv, limit - new_limit);

      priv->limit = new_limit;
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_avatar_cache_get_property(GObject *object, guint property_id,
                                     GValue *value, GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_CACHE_LIMIT:
    {
      OssoABookAvatarCache *cache = OSSO_ABOOK_AVATAR_CACHE(object);
      OssoABookAvatarCachePrivate *priv =
          OSSO_ABOOK_AVATAR_CACHE_PRIVATE(cache);

      g_value_set_uint(value, priv->limit);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_avatar_cache_dispose(GObject *object)
{
  OssoABookAvatarCache *cache = OSSO_ABOOK_AVATAR_CACHE(object);
  OssoABookAvatarCachePrivate *priv =
      OSSO_ABOOK_AVATAR_CACHE_PRIVATE(cache);

  if (priv->cached_images)
  {
    g_hash_table_destroy(priv->cached_images);
    priv->cached_images = NULL;
  }

  g_queue_clear(&priv->queue);
  G_OBJECT_CLASS(osso_abook_avatar_cache_parent_class)->dispose(object);
}

static void
osso_abook_avatar_cache_class_init(OssoABookAvatarCacheClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_avatar_cache_set_property;
  object_class->dispose = osso_abook_avatar_cache_dispose;
  object_class->get_property = osso_abook_avatar_cache_get_property;

  g_object_class_install_property(
        object_class, PROP_CACHE_LIMIT,
        g_param_spec_uint(
                 "cache-limit",
                 "Cache Limit",
                 "Maximum number of pixbufs to cache",
                 0,
                 G_MAXUINT,
                 DEFAULT_CACHE_LIMIT,
                 GTK_PARAM_READWRITE));
}

static void
cached_image_free(CachedImage *cached_image, gboolean keep_avatar)
{
  OssoABookAvatarCachePrivate *priv =
      OSSO_ABOOK_AVATAR_CACHE_PRIVATE(cached_image->cache);
  OssoABookAvatar *avatar = cached_image->avatar;

  g_queue_remove(&priv->queue, avatar);

  if (keep_avatar)
    cached_image->avatar = NULL;

  g_hash_table_remove(priv->cached_images, avatar);
}

static void
avatar_finalyzed_cb(gpointer data, GObject *where_the_object_was)
{
  cached_image_free(data, TRUE);
}

static void
cached_image_destroy(gpointer data)
{
  CachedImage *cached_image = data;

  if (cached_image->avatar)
  {
    g_object_weak_unref(G_OBJECT(cached_image->avatar), avatar_finalyzed_cb,
                        cached_image);
  }

  if (cached_image->pixbuf)
    g_object_unref(cached_image->pixbuf);

  g_slice_free(CachedImage, cached_image);
}

static void
osso_abook_avatar_cache_init(OssoABookAvatarCache *cache)
{
  OssoABookAvatarCachePrivate *priv =
      OSSO_ABOOK_AVATAR_CACHE_PRIVATE(cache);

  priv->cached_images = g_hash_table_new_full(g_direct_hash, g_direct_equal,
                                              NULL, cached_image_destroy);
  g_queue_init(&priv->queue);
}

OssoABookAvatarCache *
osso_abook_avatar_cache_new(void)
{
  return g_object_new(OSSO_ABOOK_TYPE_AVATAR_CACHE, NULL);
}

OssoABookAvatarCache *osso_abook_avatar_cache_get_default()
{
  return osso_abook_avatar_cache_get_for_name("default");
}

OssoABookAvatarCache *
osso_abook_avatar_cache_get_for_scale(int width, int height, gboolean crop)
{
  gchar *name = _osso_abook_avatar_get_cache_name(width, height, crop, 0, NULL);
  OssoABookAvatarCache *cache = osso_abook_avatar_cache_get_for_name(name);

  g_free(name);

  return cache;
}

OssoABookAvatarCache *
osso_abook_avatar_cache_get_for_name(const char *name)
{
  OssoABookAvatarCache *cache;

  g_return_val_if_fail(name != NULL, NULL);

  if (!cache_by_name)
  {
    cache_by_name =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_object_unref);
  }

  cache = g_hash_table_lookup(cache_by_name, name);

  if (!cache)
  {
    cache = osso_abook_avatar_cache_new();
    g_hash_table_insert(cache_by_name, g_strdup(name), cache);
  }

  return cache;
}

void
osso_abook_avatar_cache_set_limit(OssoABookAvatarCache *self,
                                  unsigned int limit)
{
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_CACHE(self));

  g_object_set(self, "cache-limit", limit, NULL);
}

unsigned int
osso_abook_avatar_cache_get_limit(OssoABookAvatarCache *self)
{
  OssoABookAvatarCachePrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_CACHE(self), DEFAULT_CACHE_LIMIT);

  priv = OSSO_ABOOK_AVATAR_CACHE_PRIVATE(self);

  return priv->limit;
}

void
osso_abook_avatar_cache_add(OssoABookAvatarCache *self, OssoABookAvatar *avatar,
                            GdkPixbuf *pixbuf)
{
  OssoABookAvatarCachePrivate *priv;
  guint table_size;
  guint limit;
  CachedImage *cached_image;

  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_CACHE(self));
  g_return_if_fail(OSSO_ABOOK_IS_AVATAR(avatar));
  g_return_if_fail(GDK_IS_PIXBUF(pixbuf));

  priv = OSSO_ABOOK_AVATAR_CACHE_PRIVATE(self);

  table_size = g_hash_table_size(priv->cached_images);
  limit = priv->limit;

  if (table_size >= limit)
    prune_n_items(priv, table_size - limit + 1);

  cached_image = g_slice_new0(CachedImage);
  cached_image->cache = self;
  cached_image->avatar = avatar;
  cached_image->pixbuf = g_object_ref(pixbuf);
  cached_image->image_token = osso_abook_avatar_get_image_token(avatar);

  g_object_weak_ref(G_OBJECT(avatar), avatar_finalyzed_cb, cached_image);
  g_hash_table_insert(priv->cached_images, avatar, cached_image);
  g_queue_push_tail(&priv->queue, cached_image->avatar);
}

GdkPixbuf *
osso_abook_avatar_cache_lookup(OssoABookAvatarCache *self,
                               OssoABookAvatar *avatar)
{
  OssoABookAvatarCachePrivate *priv;
  CachedImage *cached_image;

  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR_CACHE(self), NULL);
  g_return_val_if_fail(OSSO_ABOOK_IS_AVATAR(avatar), NULL);

  priv = OSSO_ABOOK_AVATAR_CACHE_PRIVATE(self);
  cached_image = g_hash_table_lookup(priv->cached_images, avatar);

  if (cached_image)
  {
    if (cached_image->image_token == osso_abook_avatar_get_image_token(avatar))
    {
      g_queue_remove(&priv->queue, cached_image->avatar);
      g_queue_push_tail(&priv->queue, cached_image->avatar);
      return cached_image->pixbuf;
    }
    else
      cached_image_free(cached_image, FALSE);
  }

  return NULL;
}

void
osso_abook_avatar_cache_clear(OssoABookAvatarCache *self)
{
  OssoABookAvatarCachePrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_AVATAR_CACHE(self));

  priv = OSSO_ABOOK_AVATAR_CACHE_PRIVATE(self);
  prune_n_items(priv, g_hash_table_size(priv->cached_images));
}

void
osso_abook_avatar_cache_drop_by_name(const char *name)
{
  g_return_if_fail(name != NULL);

  if (cache_by_name)
    g_hash_table_remove(cache_by_name, name);
}
