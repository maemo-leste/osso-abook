/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_AVATAR_CHOOSER_DIALOG_H__
#define __OSSO_ABOOK_AVATAR_CHOOSER_DIALOG_H__

#include "osso-abook-types.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_AVATAR_CHOOSER_DIALOG \
                (osso_abook_avatar_chooser_dialog_get_type ())
#define OSSO_ABOOK_AVATAR_CHOOSER_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_CHOOSER_DIALOG, \
                 OssoABookAvatarChooserDialog))
#define OSSO_ABOOK_AVATAR_CHOOSER_DIALOG_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_AVATAR_CHOOSER_DIALOG, \
                 OssoABookAvatarChooserDialogClass))
#define OSSO_ABOOK_IS_AVATAR_CHOOSER_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_CHOOSER_DIALOG))
#define OSSO_ABOOK_IS_AVATAR_CHOOSER_DIALOG_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_AVATAR_CHOOSER_DIALOG))
#define OSSO_ABOOK_AVATAR_CHOOSER_DIALOG_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_CHOOSER_DIALOG, \
                 OssoABookAvatarChooserDialogClass))

/**
 * OssoABookAvatarChooserDialog:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookAvatarChooserDialog {
        /*< private >*/
        GtkDialog parent;
};

struct _OssoABookAvatarChooserDialogClass {
        GtkDialogClass parent_class;
};

GType
osso_abook_avatar_chooser_dialog_get_type      (void) G_GNUC_CONST;

GtkWidget *
osso_abook_avatar_chooser_dialog_new           (GtkWindow                    *parent);

GdkPixbuf *
osso_abook_avatar_chooser_dialog_get_pixbuf    (OssoABookAvatarChooserDialog *dialog);

const char *
osso_abook_avatar_chooser_dialog_get_filename  (OssoABookAvatarChooserDialog *dialog);

const char *
osso_abook_avatar_chooser_dialog_get_icon_name (OssoABookAvatarChooserDialog *dialog);

void
osso_abook_avatar_chooser_dialog_set_contact   (OssoABookAvatarChooserDialog *dialog,
                                                OssoABookContact             *contact);

OssoABookContact *
osso_abook_avatar_chooser_dialog_get_contact   (OssoABookAvatarChooserDialog *dialog);

G_END_DECLS

#endif /* __OSSO_ABOOK_AVATAR_CHOOSER_DIALOG_H__ */
