/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_SEND_CONTACTS_H__
#define __OSSO_ABOOK_SEND_CONTACTS_H__

#include "osso-abook-contact.h"
#include "osso-abook-contact-detail-selector.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_SEND_CONTACTS_DIALOG \
                (osso_abook_send_contacts_dialog_get_type ())
#define OSSO_ABOOK_SEND_CONTACTS_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_SEND_CONTACTS_DIALOG, \
                 OssoABookSendContactsDialog))
#define OSSO_ABOOK_SEND_CONTACTS_DIALOG_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_SEND_CONTACTS_DIALOG, \
                 OssoABookSendContactsDialogClass))
#define OSSO_ABOOK_IS_SEND_CONTACTS_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_SEND_CONTACTS_DIALOG))
#define OSSO_ABOOK_IS_SEND_CONTACTS_DIALOG_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_SEND_CONTACTS_DIALOG))
#define OSSO_ABOOK_SEND_CONTACTS_DIALOG_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_SEND_CONTACTS_DIALOG, \
                 OssoABookSendContactsDialogClass))

/**
 * OssoABookSendContactsDialog:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookSendContactsDialog
{
        GtkDialog parent_instance;
};

struct _OssoABookSendContactsDialogClass
{
        GtkDialogClass parent_class;
};

GType
osso_abook_send_contacts_dialog_get_type (void) G_GNUC_CONST;

GtkWidget *
osso_abook_send_contacts_dialog_new (GtkWindow *parent,
                                     OssoABookContact *contact,
                                     gboolean is_detail);

void osso_abook_send_contacts_bluetooth (OssoABookContact *contact,
                                         gboolean send_avatar);
void osso_abook_send_contacts_email (OssoABookContact *contact,
                                     gboolean send_avatar);
void osso_abook_send_contacts_sms (OssoABookContact *contact);

void osso_abook_send_contacts_detail_dialog (OssoABookContact              *contact,
                                             OssoABookContactDetailFilters  filters,
                                             GtkWindow                     *parent_window);
void osso_abook_send_contacts_detail_dialog_default (OssoABookContact *contact,
                                                     GtkWindow        *parent_window);


G_END_DECLS

#endif /* __OSSO_ABOOK_SEND_CONTACTS_H__ */
