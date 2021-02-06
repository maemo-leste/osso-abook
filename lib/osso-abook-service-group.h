/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_SERVICE_GROUP_H__
#define __OSSO_ABOOK_SERVICE_GROUP_H__

#include <libebook/libebook.h>
#include <telepathy-glib/account.h>

#include "osso-abook-group.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_SERVICE_GROUP \
                (osso_abook_service_group_get_type ())
#define OSSO_ABOOK_SERVICE_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_SERVICE_GROUP, \
                 OssoABookServiceGroup))
#define OSSO_ABOOK_SERVICE_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_SERVICE_GROUP, \
                 OssoABookServiceGroupClass))
#define OSSO_ABOOK_IS_SERVICE_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_SERVICE_GROUP))
#define OSSO_ABOOK_IS_SERVICE_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_SERVICE_GROUP))
#define OSSO_ABOOK_SERVICE_GROUP_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_SERVICE_GROUP, \
                 OssoABookServiceGroupClass))

/**
 * OssoABookServiceGroup:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookServiceGroup {
        /*< private >*/
        OssoABookGroup parent;
};

struct _OssoABookServiceGroupClass {
        OssoABookGroupClass parent_class;
};

GType
osso_abook_service_group_get_type      (void) G_GNUC_CONST;

OssoABookGroup *
osso_abook_service_group_get           (TpAccount *account) G_GNUC_CONST;

GList *
osso_abook_service_group_get_all (void);

TpAccount *
osso_abook_service_group_get_account (OssoABookServiceGroup *group);

OssoABookGroup *
osso_abook_service_group_lookup_by_name (const char *unique_name);

G_END_DECLS

#endif /* __OSSO_ABOOK_SERVICE_GROUP_H__ */
