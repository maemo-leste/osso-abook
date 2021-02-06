/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_H__
#define __OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_H__

#include "osso-abook-types.h"
#include <glib-object.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CONTACT_SUBSCRIPTIONS \
                (osso_abook_contact_subscriptions_get_type ())
#define OSSO_ABOOK_CONTACT_SUBSCRIPTIONS(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_SUBSCRIPTIONS, \
                 OssoABookContactSubscriptions))
#define OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_SUBSCRIPTIONS, \
                 OssoABookContactSubscriptionsClass))
#define OSSO_ABOOK_IS_CONTACT_SUBSCRIPTIONS(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_SUBSCRIPTIONS))
#define OSSO_ABOOK_IS_CONTACT_SUBSCRIPTIONS_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_SUBSCRIPTIONS))
#define OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_SUBSCRIPTIONS, \
                 OssoABookContactSubscriptionsClass))

/**
 * OssoABookContactSubscriptions:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookContactSubscriptions {
        /*< private >*/
        GObject parent;
};

struct _OssoABookContactSubscriptionsClass {
        GObjectClass parent_class;
};

GType
osso_abook_contact_subscriptions_get_type (void) G_GNUC_CONST;

OssoABookContactSubscriptions *
osso_abook_contact_subscriptions_new      (void);

void
osso_abook_contact_subscriptions_add      (OssoABookContactSubscriptions *subscriptions,
                                           const char                    *uid);

gboolean
osso_abook_contact_subscriptions_remove   (OssoABookContactSubscriptions *subscriptions,
                                           const char                    *uid);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_H__ */
