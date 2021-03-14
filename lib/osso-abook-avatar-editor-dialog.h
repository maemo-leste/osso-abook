/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_AVATAR_EDITOR_DIALOG_H__
#define __OSSO_ABOOK_AVATAR_EDITOR_DIALOG_H__

#include "osso-abook-types.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_AVATAR_EDITOR_DIALOG \
                (osso_abook_avatar_editor_dialog_get_type ())
#define OSSO_ABOOK_AVATAR_EDITOR_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR_DIALOG, \
                 OssoABookAvatarEditorDialog))
#define OSSO_ABOOK_AVATAR_EDITOR_DIALOG_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR_DIALOG, \
                 OssoABookAvatarEditorDialogClass))
#define OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR_DIALOG))
#define OSSO_ABOOK_IS_AVATAR_EDITOR_DIALOG_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR_DIALOG))
#define OSSO_ABOOK_AVATAR_EDITOR_DIALOG_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR_DIALOG, \
                 OssoABookAvatarEditorDialogClass))

struct _OssoABookAvatarEditorDialog {
        /*< private >*/
        GtkDialog parent;
};

struct _OssoABookAvatarEditorDialogClass {
        GtkDialogClass parent_class;
};

GType
osso_abook_avatar_editor_dialog_get_type          (void) G_GNUC_CONST;

GtkWidget *
osso_abook_avatar_editor_dialog_new               (GtkWindow                   *parent,
                                                   GdkPixbuf                   *pixbuf);

void
osso_abook_avatar_editor_dialog_set_pixbuf        (OssoABookAvatarEditorDialog *dialog,
                                                   GdkPixbuf                   *pixbuf);

GdkPixbuf *
osso_abook_avatar_editor_dialog_get_pixbuf        (OssoABookAvatarEditorDialog *dialog);

GdkPixbuf *
osso_abook_avatar_editor_dialog_get_scaled_pixbuf (OssoABookAvatarEditorDialog *dialog);

void
osso_abook_avatar_editor_dialog_set_avatar_size   (OssoABookAvatarEditorDialog *dialog,
                                                   int                          size);

int
osso_abook_avatar_editor_dialog_get_avatar_size   (OssoABookAvatarEditorDialog *dialog);

void
osso_abook_avatar_editor_dialog_set_zoom_steps    (OssoABookAvatarEditorDialog *dialog,
                                                   int                          steps);

int
osso_abook_avatar_editor_dialog_get_zoom_steps    (OssoABookAvatarEditorDialog *dialog);

void
osso_abook_avatar_editor_dialog_set_maximum_zoom  (OssoABookAvatarEditorDialog *dialog,
                                                   double                       zoom);

double
osso_abook_avatar_editor_dialog_get_maximum_zoom  (OssoABookAvatarEditorDialog *dialog);

G_END_DECLS

#endif /* __OSSO_ABOOK_AVATAR_EDITOR_DIALOG_H__ */
