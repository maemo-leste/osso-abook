/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* vim:set et sw=8 ts=8 sts=8: */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_SETTINGS_DIALOG_H__
#define __OSSO_ABOOK_SETTINGS_DIALOG_H__

#include "osso-abook-types.h"

#include <hildon/hildon.h>
#include <libebook/libebook.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_SETTINGS_DIALOG \
                (osso_abook_settings_dialog_get_type ())
#define OSSO_ABOOK_SETTINGS_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_SETTINGS_DIALOG, \
                 OssoABookSettingsDialog))
#define OSSO_ABOOK_SETTINGS_DIALOG_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_SETTINGS_DIALOG, \
                 OssoABookSettingsDialogClass))
#define OSSO_ABOOK_IS_SETTINGS_DIALOG(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_SETTINGS_DIALOG))
#define OSSO_ABOOK_IS_SETTINGS_DIALOG_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_SETTINGS_DIALOG))
#define OSSO_ABOOK_SETTINGS_DIALOG_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_SETTINGS_DIALOG, \
                 OssoABookSettingsDialogClass))

/**
 * OssoABookSettingsDialog:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookSettingsDialog {
        /*< private >*/
        GtkDialog parent;
};

struct _OssoABookSettingsDialogClass {
        GtkDialogClass parent_class;
};

GType
osso_abook_settings_dialog_get_type     (void) G_GNUC_CONST;

GtkWidget *
osso_abook_settings_dialog_new          (GtkWindow *parent, EBook *book);

G_END_DECLS

#endif /* __OSSO_ABOOK_SETTINGS_DIALOG_H__ */
