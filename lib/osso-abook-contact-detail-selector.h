/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_CONTACT_DETAIL_SELECTOR_H__
#define __OSSO_ABOOK_CONTACT_DETAIL_SELECTOR_H__

#include "osso-abook-contact.h"
#include "osso-abook-contact-detail-store.h"

#include <hildon/hildon.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CONTACT_DETAIL_SELECTOR \
                (osso_abook_contact_detail_selector_get_type ())
#define OSSO_ABOOK_CONTACT_DETAIL_SELECTOR(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_SELECTOR, \
                 OssoABookContactDetailSelector))
#define OSSO_ABOOK_CONTACT_DETAIL_SELECTOR_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_SELECTOR, \
                 OssoABookContactDetailSelectorClass))
#define OSSO_ABOOK_IS_CONTACT_DETAIL_SELECTOR(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_SELECTOR))
#define OSSO_ABOOK_IS_CONTACT_DETAIL_SELECTOR_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_SELECTOR))
#define OSSO_ABOOK_CONTACT_DETAIL_SELECTOR_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_SELECTOR, \
                 OssoABookContactDetailSelectorClass))

/**
 * OssoABookContactDetailSelector:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookContactDetailSelector {
        /*< private >*/
        GtkDialog parent;
};

struct _OssoABookContactDetailSelectorClass {
        GtkDialogClass parent_class;
};

GType
osso_abook_contact_detail_selector_get_type           (void) G_GNUC_CONST;

GtkWidget *
osso_abook_contact_detail_selector_new                (GtkWindow                      *parent,
                                                       OssoABookContactDetailStore    *details);

GtkWidget *
osso_abook_contact_detail_selector_new_for_contact    (GtkWindow                      *parent,
                                                       OssoABookContact               *contact,
                                                       OssoABookContactDetailFilters   filters);

EVCardAttribute *
osso_abook_contact_detail_selector_get_detail         (OssoABookContactDetailSelector *self);

OssoABookContactField *
osso_abook_contact_detail_selector_get_selected_field (OssoABookContactDetailSelector *self);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_DETAIL_SELECTOR_H__ */
