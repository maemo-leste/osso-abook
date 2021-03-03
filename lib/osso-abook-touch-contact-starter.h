/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_TOUCH_CONTACT_STARTER_H__
#define __OSSO_ABOOK_TOUCH_CONTACT_STARTER_H__

#include "osso-abook-types.h"
#include "osso-abook-contact.h"
#include "osso-abook-merge.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER \
                (osso_abook_touch_contact_starter_get_type ())
#define OSSO_ABOOK_TOUCH_CONTACT_STARTER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER, \
                 OssoABookTouchContactStarter))
#define OSSO_ABOOK_TOUCH_CONTACT_STARTER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER, \
                 OssoABookTouchContactStarterClass))
#define OSSO_ABOOK_IS_TOUCH_CONTACT_STARTER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER))
#define OSSO_ABOOK_IS_TOUCH_CONTACT_STARTER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER))
#define OSSO_ABOOK_TOUCH_CONTACT_STARTER_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_TOUCH_CONTACT_STARTER, \
                 OssoABookTouchContactStarterClass))

/**
 * OssoABookTouchContactStarter:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookTouchContactStarter {
        /*< private >*/
        GtkBin parent;
};

struct _OssoABookTouchContactStarterClass {
        GtkBinClass parent_class;

        /* signals */
        void (* action_started)     (OssoABookTouchContactStarter *starter);
        void (* editor_started)     (OssoABookTouchContactStarter *starter,
                                     OssoABookContactEditor       *editor);
};

GType
osso_abook_touch_contact_starter_get_type           (void) G_GNUC_CONST;

GtkWidget *
osso_abook_touch_contact_starter_new_with_contact   (GtkWindow                    *parent,
                                                     OssoABookContact             *contact);

GtkWidget *
osso_abook_touch_contact_starter_new_with_editor    (GtkWindow                    *parent,
                                                     OssoABookContact             *contact);

GtkWidget *
osso_abook_touch_contact_starter_new_with_store     (OssoABookContactDetailStore  *store,
                                                     const OssoABookContactAction *allowed_actions,
                                                     guint                         n_actions);

GtkWidget *
osso_abook_touch_contact_starter_new_not_interactive(GtkWindow                    *parent,
                                                     OssoABookContact             *contact);

GtkWidget *
osso_abook_touch_contact_starter_new_with_single_attribute(OssoABookContact             *contact,
                                                           EVCardAttribute              *attribute,
                                                           const OssoABookContactAction *allowed_actions,
                                                           guint                         n_actions);
GtkWidget *
osso_abook_touch_contact_starter_new_with_single_attribute_full
                                                    (OssoABookContact             *contact,
                                                     EVCardAttribute              *attribute,
                                                     TpProtocol                   *protocol,
                                                     const OssoABookContactAction *allowed_actions,
                                                     guint                         n_actions);
GtkWidget *
osso_abook_touch_contact_starter_new_with_highlighted_attribute
                                                    (OssoABookContact             *contact,
                                                     EVCardAttribute              *attribute);

OssoABookContact *
osso_abook_touch_contact_starter_get_contact        (OssoABookTouchContactStarter *starter);

gboolean
osso_abook_touch_contact_starter_get_editable       (OssoABookTouchContactStarter *starter);

GtkWidget *
osso_abook_touch_contact_starter_dialog_new         (GtkWindow                    *parent,
                                                     OssoABookTouchContactStarter *starter);

OssoABookContactAction
osso_abook_touch_contact_starter_get_started_action (OssoABookTouchContactStarter *starter);

void
osso_abook_touch_contact_starter_start_editor (OssoABookTouchContactStarter *starter);

void
osso_abook_touch_contact_starter_start_merge (OssoABookTouchContactStarter *starter,
                                              OssoABookContactModel        *contact_model,
                                              OssoABookMergeWithCb          cb,
                                              gpointer                      user_data);

G_END_DECLS

#endif /* __OSSO_ABOOK_TOUCH_CONTACT_STARTER_H__ */
