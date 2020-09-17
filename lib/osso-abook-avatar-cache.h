/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_AVATAR_CACHE_H__
#define __OSSO_ABOOK_AVATAR_CACHE_H__

#include "osso-abook-avatar.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_AVATAR_CACHE \
                (osso_abook_avatar_cache_get_type())
#define OSSO_ABOOK_AVATAR_CACHE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_CACHE, \
                 OssoABookAvatarCache))
#define OSSO_ABOOK_AVATAR_CACHE_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_AVATAR_CACHE, \
                 OssoABookAvatarCacheClass))
#define OSSO_ABOOK_IS_AVATAR_CACHE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_CACHE))
#define OSSO_ABOOK_IS_AVATAR_CACHE_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_TYPE ((cls), \
                 OSSO_ABOOK_TYPE_AVATAR_CACHE))
#define OSSO_ABOOK_AVATAR_CACHE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_CACHE, \
                 OssoABookAvatarCacheClass))

struct _OssoABookAvatarCache {
        GObject parent;
};

struct _OssoABookAvatarCacheClass {
        GObjectClass parent_class;
};

GType
osso_abook_avatar_cache_get_type      (void) G_GNUC_CONST;

OssoABookAvatarCache *
osso_abook_avatar_cache_new           (void);

OssoABookAvatarCache *
osso_abook_avatar_cache_get_default   (void);

OssoABookAvatarCache *
osso_abook_avatar_cache_get_for_scale (int                   width,
                                       int                   height,
                                       gboolean              crop);

OssoABookAvatarCache *
osso_abook_avatar_cache_get_for_name  (const char           *name);

void
osso_abook_avatar_cache_set_limit     (OssoABookAvatarCache *self,
                                       unsigned              limit);
unsigned
osso_abook_avatar_cache_get_limit     (OssoABookAvatarCache *self);

GdkPixbuf *
osso_abook_avatar_cache_lookup        (OssoABookAvatarCache *self,
                                       OssoABookAvatar      *avatar);

void
osso_abook_avatar_cache_add           (OssoABookAvatarCache *self,
                                       OssoABookAvatar      *avatar,
                                       GdkPixbuf            *pixbuf);

void
osso_abook_avatar_cache_clear         (OssoABookAvatarCache *self);

void
osso_abook_avatar_cache_drop_by_name  (const char           *name);

G_END_DECLS

#endif /* __OSSO_ABOOK_AVATAR_CACHE_H__ */
