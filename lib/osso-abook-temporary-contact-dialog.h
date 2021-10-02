/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_TEMPORARY_CONTACT_DIALOG_H__
#define __OSSO_ABOOK_TEMPORARY_CONTACT_DIALOG_H__

#include "osso-abook-contact.h"
#include <libebook/libebook.h>
#include <telepathy-glib/account.h>
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_TEMPORARY_CONTACT_DIALOG \
                (osso_abook_temporary_contact_dialog_get_type ())
#define OSSO_ABOOK_TEMPORARY_CONTACT_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_TEMPORARY_CONTACT_DIALOG, \
                 OssoABookTemporaryContactDialog))
#define OSSO_ABOOK_TEMPORARY_CONTACT_DIALOG_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_TEMPORARY_CONTACT_DIALOG, \
                 OssoABookTemporaryContactDialogClass))
#define OSSO_ABOOK_IS_TEMPORARY_CONTACT_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_TEMPORARY_CONTACT_DIALOG))
#define OSSO_ABOOK_IS_TEMPORARY_CONTACT_DIALOG_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_TEMPORARY_CONTACT_DIALOG))
#define OSSO_ABOOK_TEMPORARY_CONTACT_DIALOG_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_TEMPORARY_CONTACT_DIALOG, \
                 OssoABookTemporaryContactDialogClass))

/**
 * OssoABookTemporaryContactDialog:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookTemporaryContactDialog {
        /*< private >*/
        GtkDialog parent;
};

struct _OssoABookTemporaryContactDialogClass {
        GtkDialogClass parent_class;

        gboolean (* contact_saved) (OssoABookTemporaryContactDialog *dialog,
                                    const char                      *uid);
};

GType
osso_abook_temporary_contact_dialog_get_type         (void) G_GNUC_CONST;

GtkWidget *
osso_abook_temporary_contact_dialog_new              (GtkWindow        *parent,
                                                      EBook            *book,
                                                      EVCardAttribute  *attribute,
						      TpAccount        *account);

GtkWidget *
osso_abook_temporary_contact_dialog_new_with_contact (GtkWindow        *parent,
                                                      OssoABookContact *contact);

G_END_DECLS

#endif /* __OSSO_ABOOK_TEMPORARY_CONTACT_DIALOG_H__ */
