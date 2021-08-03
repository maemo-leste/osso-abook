/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_ICON_SIZES_H__
#define __OSSO_ABOOK_ICON_SIZES_H__

#include <gtk/gtk.h>

/**
 * OSSO_ABOOK_PIXEL_SIZE_AVATAR_CHOOSER:
 *
 * The pixel size of the avatar image as used by the the avatar chooser dialog
 */
#define OSSO_ABOOK_PIXEL_SIZE_AVATAR_CHOOSER    (106)

/**
 * OSSO_ABOOK_PIXEL_SIZE_AVATAR_MEDIUM
 *
 * The pixel size of a medium avatar image as used by the contact editor
 */
#define OSSO_ABOOK_PIXEL_SIZE_AVATAR_MEDIUM     (128)

/**
 * OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT
 *
 * The pixel size of a default avatar image.  This size is used in the contact
 * starter and is the preferred size for storing avatars
 */
#define OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT    (144)

/**
 * OSSO_ABOOK_PIXEL_SIZE_AVATAR_LARGE
 *
 * The pixel size of a large avatar image as used by the avatar crop tool
 */
#define OSSO_ABOOK_PIXEL_SIZE_AVATAR_LARGE      (216)

/**
 * OSSO_ABOOK_ICON_SIZE_AVATAR_CHOOSER
 *
 * Returns: A #GtkIconSize for the the avatar image as used by the avatar
 * chooser dialog
 */
#define OSSO_ABOOK_ICON_SIZE_AVATAR_CHOOSER     (osso_abook_icon_size_avatar_chooser ())

/**
 * OSSO_ABOOK_ICON_SIZE_AVATAR_MEDIUM
 *
 * Returns: A #GtkIconSize for a medium avatar image
 */
#define OSSO_ABOOK_ICON_SIZE_AVATAR_MEDIUM      (osso_abook_icon_size_avatar_medium  ())

/**
 * OSSO_ABOOK_ICON_SIZE_AVATAR_DEFAULT
 *
 * Returns: A #GtkIconSize for a default avatar image
 */
#define OSSO_ABOOK_ICON_SIZE_AVATAR_DEFAULT     (osso_abook_icon_size_avatar_default ())

/**
 * OSSO_ABOOK_ICON_SIZE_AVATAR_LARGE
 *
 * Returns: A #GtkIconSize for a large avatar image
 */
#define OSSO_ABOOK_ICON_SIZE_AVATAR_LARGE       (osso_abook_icon_size_avatar_large   ())

GtkIconSize
osso_abook_icon_size_avatar_chooser (void) G_GNUC_CONST;

GtkIconSize
osso_abook_icon_size_avatar_medium  (void) G_GNUC_CONST;

GtkIconSize
osso_abook_icon_size_avatar_default (void) G_GNUC_CONST;

GtkIconSize
osso_abook_icon_size_avatar_large   (void) G_GNUC_CONST;

#endif /* __OSSO_ABOOK_ICON_SIZES_H__ */
