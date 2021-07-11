/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_AVATAR_EDITOR_H__
#define __OSSO_ABOOK_AVATAR_EDITOR_H__

#include "osso-abook-types.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_AVATAR_EDITOR \
                (osso_abook_avatar_editor_get_type ())
#define OSSO_ABOOK_AVATAR_EDITOR(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR, \
                 OssoABookAvatarEditor))
#define OSSO_ABOOK_AVATAR_EDITOR_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR, \
                 OssoABookAvatarEditorClass))
#define OSSO_ABOOK_IS_AVATAR_EDITOR(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR))
#define OSSO_ABOOK_IS_AVATAR_EDITOR_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR))
#define OSSO_ABOOK_AVATAR_EDITOR_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_AVATAR_EDITOR, \
                 OssoABookAvatarEditorClass))

struct _OssoABookAvatarEditor {
        /*< private >*/
        GtkEventBox parent;
};

struct _OssoABookAvatarEditorClass {
        GtkEventBoxClass parent_class;
};

GType
osso_abook_avatar_editor_get_type  (void) G_GNUC_CONST;

GtkWidget *
osso_abook_avatar_editor_new       (void);

OssoABookAvatarImage *
osso_abook_avatar_editor_get_image (OssoABookAvatarEditor *editor);

G_END_DECLS

#endif /* __OSSO_ABOOK_AVATAR_EDITOR_H__ */
