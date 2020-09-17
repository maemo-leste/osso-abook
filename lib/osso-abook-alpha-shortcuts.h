/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef _OSSO_ABOOK_ALPHA_SHORTCUTS
#define _OSSO_ABOOK_ALPHA_SHORTCUTS

#include <gtk/gtk.h>
#include <hildon/hildon.h>

#include "osso-abook-types.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_ALPHA_SHORTCUTS osso_abook_alpha_shortcuts_get_type()

#define OSSO_ABOOK_ALPHA_SHORTCUTS(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_ALPHA_SHORTCUTS, \
                 OssoABookAlphaShortcuts))
#define OSSO_ABOOK_ALPHA_SHORTCUTS_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_ALPHA_SHORTCUTS, \
                 OssoABookAlphaShortcutsClass))
#define OSSO_ABOOK_IS_ALPHA_SHORTCUTS(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_ALPHA_SHORTCUTS))
#define OSSO_ABOOK_IS_ALPHA_SHORTCUTS_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_ALPHA_SHORTCUTS))
#define OSSO_ABOOK_ALPHA_SHORTCUTS_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_ALPHA_SHORTCUTS, \
                 OssoABookAlphaShortcutsClass))

/**
 * OssoABookAlphaShortcuts:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookAlphaShortcuts {
        /*< private >*/
        GtkVBox parent;
};

struct _OssoABookAlphaShortcutsClass {
        GtkVBoxClass parent_class;
};

GType
osso_abook_alpha_shortcuts_get_type      (void);

GtkWidget *
osso_abook_alpha_shortcuts_new           (void);

void
osso_abook_alpha_shortcuts_show          (OssoABookAlphaShortcuts *shortcuts);

void
osso_abook_alpha_shortcuts_hide          (OssoABookAlphaShortcuts *shortcuts);

void
osso_abook_alpha_shortcuts_widget_hook  (OssoABookAlphaShortcuts *shortcuts,
                                         OssoABookContactView *contact_view);

void
osso_abook_alpha_shortcuts_widget_unhook (OssoABookAlphaShortcuts *shortcuts);

G_END_DECLS

#endif /* _OSSO_ABOOK_ALPHA_SHORTCUTS */
