/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_PROFILE_GROUP_H__
#define __OSSO_ABOOK_PROFILE_GROUP_H__

#include "osso-abook-group.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_PROFILE_GROUP \
                (osso_abook_profile_group_get_type ())
#define OSSO_ABOOK_PROFILE_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_PROFILE_GROUP, \
                 OssoABookProfileGroup))
#define OSSO_ABOOK_PROFILE_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_PROFILE_GROUP, \
                 OssoABookProfileGroupClass))
#define OSSO_ABOOK_IS_PROFILE_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_PROFILE_GROUP))
#define OSSO_ABOOK_IS_PROFILE_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_PROFILE_GROUP))
#define OSSO_ABOOK_PROFILE_GROUP_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_PROFILE_GROUP, \
                 OssoABookProfileGroupClass))

/**
 * OssoABookProfileGroup:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookProfileGroup {
        /*< private >*/
        OssoABookGroup parent;
};

struct _OssoABookProfileGroupClass {
        OssoABookGroupClass parent_class;
};

GType
osso_abook_profile_group_get_type (void) G_GNUC_CONST;

OssoABookGroup *
osso_abook_profile_group_get      (TpProtocol *protocol) G_GNUC_CONST;

G_END_DECLS

#endif /* __OSSO_ABOOK_PROFILE_GROUP_H__ */
