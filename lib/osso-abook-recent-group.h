/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_RECENT_GROUP_H__
#define __OSSO_ABOOK_RECENT_GROUP_H__

#include "osso-abook-group.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_RECENT_GROUP \
                (osso_abook_recent_group_get_type ())
#define OSSO_ABOOK_RECENT_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_RECENT_GROUP, \
                 OssoABookRecentGroup))
#define OSSO_ABOOK_RECENT_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_RECENT_GROUP, \
                 OssoABookRecentGroupClass))
#define OSSO_ABOOK_IS_RECENT_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_RECENT_GROUP))
#define OSSO_ABOOK_IS_RECENT_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_RECENT_GROUP))
#define OSSO_ABOOK_RECENT_GROUP_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_RECENT_GROUP, \
                 OssoABookRecentGroupClass))

/**
 * OssoABookRecentGroup:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookRecentGroup {
        /*< private >*/
        OssoABookGroup parent;
        OssoABookRecentGroupPrivate *priv;
};

struct _OssoABookRecentGroupClass {
        OssoABookGroupClass parent_class;
};

GType
osso_abook_recent_group_get_type   (void) G_GNUC_CONST;

OssoABookGroup *
osso_abook_recent_group_get        (void) G_GNUC_CONST;

void
osso_abook_recent_group_set_roster (OssoABookRecentGroup *group,
                                    OssoABookRoster      *roster);

OssoABookRoster *
osso_abook_recent_group_get_roster (OssoABookRecentGroup *group);

G_END_DECLS

#endif /* __OSSO_ABOOK_RECENT_GROUP_H__ */
