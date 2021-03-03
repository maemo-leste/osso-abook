/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2006-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_CONTACT_EDITOR_H__
#define __OSSO_ABOOK_CONTACT_EDITOR_H__

#include "osso-abook-types.h"

#include <hildon/hildon.h>
#include <libebook/libebook.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CONTACT_EDITOR \
                (osso_abook_contact_editor_get_type ())
#define OSSO_ABOOK_CONTACT_EDITOR(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_EDITOR, \
                 OssoABookContactEditor))
#define OSSO_ABOOK_CONTACT_EDITOR_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_EDITOR, \
                 OssoABookContactEditorClass))
#define OSSO_ABOOK_IS_CONTACT_EDITOR(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_EDITOR))
#define OSSO_ABOOK_IS_CONTACT_EDITOR_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_EDITOR))
#define OSSO_ABOOK_CONTACT_EDITOR_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_EDITOR, \
                 OssoABookContactEditorClass))

/**
 * OssoABookContactEditorMode:
 * @OSSO_ABOOK_CONTACT_EDITOR_CREATE: Create a new contact.
 * @OSSO_ABOOK_CONTACT_EDITOR_EDIT: Modify an existing contact.
 *
 * The mode defines the contact editor's behavior.
 */
typedef enum {
        OSSO_ABOOK_CONTACT_EDITOR_CREATE,
        OSSO_ABOOK_CONTACT_EDITOR_EDIT,
        OSSO_ABOOK_CONTACT_EDITOR_CREATE_SELF,
        OSSO_ABOOK_CONTACT_EDITOR_EDIT_SELF,
} OssoABookContactEditorMode;

/**
 * OssoABookContactEditor:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookContactEditor {
        /*< private >*/
        GtkDialog parent;
};

struct _OssoABookContactEditorClass {
        GtkDialogClass parent_class;

        gboolean (*contact_saved) (OssoABookContactEditor *editor,
                                   const char             *uid);
};

GType
osso_abook_contact_editor_get_type         (void) G_GNUC_CONST;

GtkWidget *
osso_abook_contact_editor_new              (void);

GtkWidget *
osso_abook_contact_editor_new_with_contact (GtkWindow                      *parent,
                                            OssoABookContact               *contact,
                                            OssoABookContactEditorMode      mode);

void
osso_abook_contact_editor_set_contact      (OssoABookContactEditor         *editor,
                                            OssoABookContact               *contact);

OssoABookContact *
osso_abook_contact_editor_get_contact      (OssoABookContactEditor         *editor);

void
osso_abook_contact_editor_set_mode         (OssoABookContactEditor         *editor,
                                            OssoABookContactEditorMode      mode);

OssoABookContactEditorMode
osso_abook_contact_editor_get_mode         (OssoABookContactEditor         *editor);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_EDITOR_H__ */
