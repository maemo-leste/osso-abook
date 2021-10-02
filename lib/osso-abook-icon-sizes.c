/*
 * osso-abook-icon-sizes.c
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

#include "osso-abook-icon-sizes.h"

#define ICON_SIZE(f, name, size) \
  GtkIconSize f(void) \
  { \
    static GtkIconSize icon_size = GTK_ICON_SIZE_INVALID; \
\
    if (icon_size == GTK_ICON_SIZE_INVALID) \
    { \
      icon_size = gtk_icon_size_register(name, size, size); \
    } \
\
    return icon_size; \
  }

ICON_SIZE(osso_abook_icon_size_avatar_chooser,
          "osso-abook-icon-size-avatar-chooser",
          OSSO_ABOOK_PIXEL_SIZE_AVATAR_CHOOSER)
ICON_SIZE(osso_abook_icon_size_avatar_medium,
          "osso-abook-icon-size-avatar-medium",
          OSSO_ABOOK_PIXEL_SIZE_AVATAR_MEDIUM)
ICON_SIZE(osso_abook_icon_size_avatar_default,
          "osso-abook-icon-size-avatar-default",
          OSSO_ABOOK_PIXEL_SIZE_AVATAR_DEFAULT)
ICON_SIZE(osso_abook_icon_size_avatar_large,
          "osso-abook-icon-size-avatar-large",
          OSSO_ABOOK_PIXEL_SIZE_AVATAR_LARGE)
