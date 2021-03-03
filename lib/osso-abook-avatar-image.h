/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2007-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_AVATAR_IMAGE_H__
#define __OSSO_ABOOK_AVATAR_IMAGE_H__

#include "osso-abook-types.h"
#include "osso-abook-avatar.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_AVATAR_IMAGE \
                (osso_abook_avatar_image_get_type ())
#define OSSO_ABOOK_AVATAR_IMAGE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_IMAGE, \
                 OssoABookAvatarImage))
#define OSSO_ABOOK_IS_AVATAR_IMAGE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_IMAGE))
#define OSSO_ABOOK_AVATAR_IMAGE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_AVATAR_IMAGE, \
                 OssoABookAvatarImageClass))
#define OSSO_ABOOK_IS_AVATAR_IMAGE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_AVATAR_IMAGE))
#define OSSO_ABOOK_AVATAR_IMAGE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_IMAGE, \
                 OssoABookAvatarImageClass))

/**
 * OssoABookAvatarImage:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookAvatarImage {
        GtkWidget parent_instance;
};

struct _OssoABookAvatarImageClass {
        GtkWidgetClass parent_class;
};

GType
osso_abook_avatar_image_get_type          (void) G_GNUC_CONST;

GtkWidget *
osso_abook_avatar_image_new               (void);

GtkWidget *
osso_abook_avatar_image_new_with_avatar   (OssoABookAvatar      *avatar,
                                           int                   size);

void
osso_abook_avatar_image_set_avatar        (OssoABookAvatarImage *image,
                                           OssoABookAvatar      *avatar);

OssoABookAvatar *
osso_abook_avatar_image_get_avatar        (OssoABookAvatarImage *image);

void
osso_abook_avatar_image_set_pixbuf        (OssoABookAvatarImage *image,
                                           GdkPixbuf            *pixbuf);

GdkPixbuf *
osso_abook_avatar_image_get_pixbuf        (OssoABookAvatarImage *image);

GdkPixbuf *
osso_abook_avatar_image_get_scaled_pixbuf (OssoABookAvatarImage *image);

void
osso_abook_avatar_image_set_fallback_icon (OssoABookAvatarImage *image,
                                           const char           *icon_name);

const char *
osso_abook_avatar_image_get_fallback_icon (OssoABookAvatarImage *image);

void
osso_abook_avatar_image_set_size          (OssoABookAvatarImage *image,
                                           int                   size);

int
osso_abook_avatar_image_get_size          (OssoABookAvatarImage *image);

double
osso_abook_avatar_image_get_minimum_zoom  (OssoABookAvatarImage *image);

double
osso_abook_avatar_image_get_current_zoom  (OssoABookAvatarImage *image);

void
osso_abook_avatar_image_set_maximum_zoom  (OssoABookAvatarImage *image,
                                           double                limit);

double
osso_abook_avatar_image_get_maximum_zoom  (OssoABookAvatarImage *image);

void
osso_abook_avatar_image_set_xadjustment   (OssoABookAvatarImage *image,
                                           GtkAdjustment        *adjustment);

GtkAdjustment *
osso_abook_avatar_image_get_xadjustment   (OssoABookAvatarImage *image);

void
osso_abook_avatar_image_set_yadjustment   (OssoABookAvatarImage *image,
                                           GtkAdjustment        *adjustment);

GtkAdjustment *
osso_abook_avatar_image_get_yadjustment   (OssoABookAvatarImage *image);

void
osso_abook_avatar_image_set_zadjustment   (OssoABookAvatarImage *image,
                                           GtkAdjustment        *adjustment);

GtkAdjustment *
osso_abook_avatar_image_get_zadjustment   (OssoABookAvatarImage *image);

G_END_DECLS

#endif /* __OSSO_ABOOK_AVATAR_IMAGE_H__ */
