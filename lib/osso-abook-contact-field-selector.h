/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_CONTACT_FIELD_SELECTOR_H__
#define __OSSO_ABOOK_CONTACT_FIELD_SELECTOR_H__

#include "osso-abook-contact-field.h"
#include <hildon/hildon.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CONTACT_FIELD_SELECTOR \
                (osso_abook_contact_field_selector_get_type ())
#define OSSO_ABOOK_CONTACT_FIELD_SELECTOR(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD_SELECTOR, \
                 OssoABookContactFieldSelector))
#define OSSO_ABOOK_CONTACT_FIELD_SELECTOR_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD_SELECTOR, \
                 OssoABookContactFieldSelectorClass))
#define OSSO_ABOOK_IS_CONTACT_FIELD_SELECTOR(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD_SELECTOR))
#define OSSO_ABOOK_IS_CONTACT_FIELD_SELECTOR_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD_SELECTOR))
#define OSSO_ABOOK_CONTACT_FIELD_SELECTOR_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD_SELECTOR, \
                 OssoABookContactFieldSelectorClass))

struct _OssoABookContactFieldSelector
{
        HildonTouchSelector parent_instance;
};

struct _OssoABookContactFieldSelectorClass
{
        HildonTouchSelectorClass parent_class;
};

GType
osso_abook_contact_field_selector_get_type            (void) G_GNUC_CONST;

GtkWidget *
osso_abook_contact_field_selector_new                 (void);

void
osso_abook_contact_field_selector_load                (OssoABookContactFieldSelector *selector,
                                                       OssoABookContact              *contact,
                                                       OssoABookContactFieldPredicate accept_field,
                                                       gpointer                       user_data);

void
osso_abook_contact_field_selector_append              (OssoABookContactFieldSelector *selector,
                                                       OssoABookContactField         *field);

OssoABookContactField *
osso_abook_contact_field_selector_get_selected        (OssoABookContactFieldSelector *selector);

int
osso_abook_contact_field_selector_find_custom         (OssoABookContactFieldSelector *selector,
                                                       OssoABookContactFieldPredicate accept_field,
                                                       gpointer                       user_data);

G_GNUC_WARN_UNUSED_RESULT GList *
osso_abook_contact_field_selector_get_selected_fields (OssoABookContactFieldSelector *selector);

void
osso_abook_contact_field_selector_set_show_values     (OssoABookContactFieldSelector *selector,
                                                       gboolean                       value);

gboolean
osso_abook_contact_field_selector_get_show_values     (OssoABookContactFieldSelector *selector);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_FIELD_SELECTOR_H__ */
