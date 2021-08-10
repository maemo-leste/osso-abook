/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_DIALOGS_H__
#define __OSSO_ABOOK_DIALOGS_H__

#include "osso-abook-contact-model.h"
#include "osso-abook-util.h"

G_BEGIN_DECLS

/* Contact dialogs */
gboolean
osso_abook_delete_contact_dialog_run     (GtkWindow                *parent,
                                          OssoABookRoster          *roster,
                                          OssoABookContact         *contact);

void
osso_abook_confirm_delete_contacts_dialog_run
                                         (GtkWindow                *parent,
                                          OssoABookRoster          *roster,
                                          GList                    *contacts);

void
osso_abook_delete_contacts_dialog_run    (GtkWindow                *parent,
                                          OssoABookRoster          *roster,
                                          OssoABookContactModel    *model);

/* Others */
GList *
osso_abook_choose_email_dialog_run       (GtkWindow                *parent,
                                          EContact                 *contact);

EVCardAttribute *
osso_abook_choose_im_dialog_run          (GtkWindow                *parent,
                                          OssoABookContact         *contact,
                                          OssoABookCapsFlags        type);

char *
osso_abook_choose_url_dialog_run         (GtkWindow                *parent,
                                          EContact                 *contact);

void
osso_abook_add_contact_dialog_run        (GtkWindow                *parent);

void
osso_abook_add_im_account_dialog_run     (GtkWindow                *parent);

gboolean
osso_abook_launch_applet                 (GtkWindow                *parent,
                                          const char               *applet);

G_END_DECLS

#endif /* __OSSO_ABOOK_DIALOGS_H__ */
