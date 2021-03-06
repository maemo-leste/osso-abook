/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_PRESENCE_ICON_H__
#define __OSSO_ABOOK_PRESENCE_ICON_H__

#include <gtk/gtkimage.h>

#include "osso-abook-presence.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_PRESENCE_ICON \
                (osso_abook_presence_icon_get_type ())
#define OSSO_ABOOK_PRESENCE_ICON(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_PRESENCE_ICON, \
                 OssoABookPresenceIcon))
#define OSSO_ABOOK_PRESENCE_ICON_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_PRESENCE_ICON, \
                 OssoABookPresenceIconClass))
#define OSSO_ABOOK_IS_PRESENCE_ICON(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_PRESENCE_ICON))
#define OSSO_ABOOK_IS_PRESENCE_ICON_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_PRESENCE_ICON))
#define OSSO_ABOOK_PRESENCE_ICON_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_PRESENCE_ICON, \
                 OssoABookPresenceIconClass))

/**
 * OssoABookPresenceIcon:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookPresenceIcon {
        /*< private >*/
        GtkImage parent;
};

struct _OssoABookPresenceIconClass {
        GtkImageClass parent_class;
};

GType
osso_abook_presence_icon_get_type     (void) G_GNUC_CONST; 

GtkWidget *
osso_abook_presence_icon_new          (OssoABookPresence     *presence);

void
osso_abook_presence_icon_set_presence (OssoABookPresenceIcon *icon,
                                       OssoABookPresence     *presence);

OssoABookPresence *
osso_abook_presence_icon_get_presence (OssoABookPresenceIcon *icon);
  
G_END_DECLS

#endif /* __OSSO_ABOOK_PRESENCE_ICON_H__ */
