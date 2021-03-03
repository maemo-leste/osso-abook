/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_MERGE_H__
#define __OSSO_ABOOK_MERGE_H__

#include "osso-abook-contact.h"

#include <libebook/libebook.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

/**
 * OssoABookMergeWithCb
 * @uid: the uid of the newly-merged contact
 * @user_data: the data registered with osso_abook_merge_with_dialog()
 *
 * The type of callback used with osso_abook_merge_with_dialog()
 */
typedef void (*OssoABookMergeWithCb) (const char *uid,
                                      gpointer user_data);

OssoABookContact *
osso_abook_merge_contacts (GList     *contacts,
                           GtkWindow *parent);

void
osso_abook_merge_contacts_and_save (GList *contacts,
                                    GtkWindow *parent,
                                    OssoABookMergeWithCb cb,
                                    gpointer user_data);

void
osso_abook_merge_with_dialog (OssoABookContact *contact,
                              OssoABookContactModel *model,
                              GtkWindow *parent,
                              OssoABookMergeWithCb cb,
                              gpointer user_data);
void
osso_abook_merge_with_many_dialog (OssoABookContact *contact,
                                   OssoABookContactModel *model,
                                   GtkWindow *parent,
                                   OssoABookMergeWithCb cb,
                                   gpointer user_data);

G_END_DECLS

#endif /* __OSSO_ABOOK_MERGE_H__ */
