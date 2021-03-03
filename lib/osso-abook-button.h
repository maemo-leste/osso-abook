/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_BUTTON_H__
#define __OSSO_ABOOK_BUTTON_H__

#include "osso-abook-types.h"
#include "osso-abook-presence.h"

#include <hildon/hildon.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_BUTTON \
                (osso_abook_button_get_type ())
#define OSSO_ABOOK_BUTTON(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_BUTTON, \
                 OssoABookButton))
#define OSSO_ABOOK_IS_BUTTON(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_BUTTON))
#define OSSO_ABOOK_BUTTON_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_BUTTON, \
                 OssoABookButtonClass))
#define OSSO_ABOOK_IS_BUTTON_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_BUTTON))
#define OSSO_ABOOK_BUTTON_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_BUTTON, \
                 OssoABookButtonClass))

/**
 * OssoABookButtonStyle:
 * @OSSO_ABOOK_BUTTON_STYLE_NORMAL: The button will look like a #HildonButton
 * @OSSO_ABOOK_BUTTON_STYLE_PICKER: The button will look like a #HildonPickerButton
 * @OSSO_ABOOK_BUTTON_STYLE_LABEL: The button will look and behave like a
 * single line label rather than a button
 * @OSSO_ABOOK_BUTTON_STYLE_NOTE: The button will look and behave like a
 * multiline label rather than a button
 */
typedef enum {
        OSSO_ABOOK_BUTTON_STYLE_NORMAL,
        OSSO_ABOOK_BUTTON_STYLE_PICKER,
        OSSO_ABOOK_BUTTON_STYLE_LABEL,
        OSSO_ABOOK_BUTTON_STYLE_NOTE,
} OssoABookButtonStyle;

/**
 * OssoABookButton:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookButton {
        GtkButton parent_instance;
};

struct _OssoABookButtonClass {
        GtkButtonClass parent_class;
};

GType
osso_abook_button_get_type             (void) G_GNUC_CONST;

GtkWidget *
osso_abook_button_new                  (HildonSizeType       size);

GtkWidget *
osso_abook_button_new_with_text        (HildonSizeType       size,
                                        const char          *title,
                                        const char          *value);

GtkWidget *
osso_abook_button_new_with_presence    (HildonSizeType       size,
                                        const char          *icon_name,
                                        const char          *title,
                                        const char          *value,
                                        OssoABookPresence   *presence);

void
osso_abook_button_set_style            (OssoABookButton     *button,
                                        OssoABookButtonStyle style);

OssoABookButtonStyle
osso_abook_button_get_style            (OssoABookButton     *button);

void
osso_abook_button_set_title            (OssoABookButton     *button,
                                        const char          *title);

const char *
osso_abook_button_get_title            (OssoABookButton     *button);

void
osso_abook_button_set_value            (OssoABookButton     *button,
                                        const char          *value);

const char *
osso_abook_button_get_value            (OssoABookButton     *button);

void
osso_abook_button_set_icon_name        (OssoABookButton     *button,
                                        const char          *icon_name);

const char *
osso_abook_button_get_icon_name        (OssoABookButton     *button);

void
osso_abook_button_set_icon_visible     (OssoABookButton     *button,
                                        gboolean             visible);

gboolean
osso_abook_button_get_icon_visible     (OssoABookButton     *button);

void
osso_abook_button_set_presence         (OssoABookButton     *button,
                                        OssoABookPresence   *presence);

OssoABookPresence *
osso_abook_button_get_presence         (OssoABookButton     *button);

void
osso_abook_button_set_presence_visible (OssoABookButton     *button,
                                        gboolean             visible);

gboolean
osso_abook_button_get_presence_visible (OssoABookButton     *button);

G_END_DECLS

#endif /* __OSSO_ABOOK_BUTTON_H__ */

