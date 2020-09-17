/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_GCONF_CONTACT_H__
#define __OSSO_ABOOK_GCONF_CONTACT_H__

#include "osso-abook-contact.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_GCONF_CONTACT \
                (osso_abook_gconf_contact_get_type ())
#define OSSO_ABOOK_GCONF_CONTACT(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_GCONF_CONTACT, \
                 OssoABookGconfContact))
#define OSSO_ABOOK_GCONF_CONTACT_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_GCONF_CONTACT, \
                 OssoABookGconfContactClass))
#define OSSO_ABOOK_IS_GCONF_CONTACT(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_GCONF_CONTACT))
#define OSSO_ABOOK_IS_GCONF_CONTACT_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_GCONF_CONTACT))
#define OSSO_ABOOK_GCONF_CONTACT_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_GCONF_CONTACT, \
                 OssoABookGconfContactClass))

#define osso_abook_gconf_contact_is_usable(x) \
        (!osso_abook_gconf_contact_is_vcard_empty ((x)) && \
         !osso_abook_gconf_contact_is_deleted ((x)))

/**
 * OssoABookGconfContact:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookGconfContact {
        OssoABookContact parent;
};

struct _OssoABookGconfContactClass {
        OssoABookContactClass parent_class;

        /* virtual methods */
        const char * (* get_gconf_key) (OssoABookGconfContact *self);
};

GType
osso_abook_gconf_contact_get_type         (void) G_GNUC_CONST;

gboolean
osso_abook_gconf_contact_is_vcard_empty   (OssoABookGconfContact *self);

gboolean
osso_abook_gconf_contact_is_deleted       (OssoABookGconfContact *self);

EVCardAttribute *
osso_abook_gconf_contact_add_ro_attribute (OssoABookGconfContact *self,
                                           const gchar           *attr_name,
                                           const gchar           *value);

void
osso_abook_gconf_contact_load (OssoABookGconfContact *self);


G_END_DECLS

#endif /* __OSSO_ABOOK_GCONF_CONTACT_H__ */
