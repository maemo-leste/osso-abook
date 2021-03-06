/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_PRESENCE_LABEL_H__
#define __OSSO_ABOOK_PRESENCE_LABEL_H__

#include <gtk/gtk.h>

#include "osso-abook-presence.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_PRESENCE_LABEL \
                (osso_abook_presence_label_get_type ())
#define OSSO_ABOOK_PRESENCE_LABEL(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_PRESENCE_LABEL, \
                 OssoABookPresenceLabel))
#define OSSO_ABOOK_PRESENCE_LABEL_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_PRESENCE_LABEL, \
                 OssoABookPresenceLabelClass))
#define OSSO_ABOOK_IS_PRESENCE_LABEL(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_PRESENCE_LABEL))
#define OSSO_ABOOK_IS_PRESENCE_LABEL_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_PRESENCE_LABEL))
#define OSSO_ABOOK_PRESENCE_LABEL_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_PRESENCE_LABEL, \
                 OssoABookPresenceLabelClass))

/**
 * OssoABookPresenceLabel:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookPresenceLabel {
        /*< private >*/
        GtkLabel parent;
};

struct _OssoABookPresenceLabelClass {
        GtkLabelClass parent_class;
};

GType
osso_abook_presence_label_get_type     (void) G_GNUC_CONST;

GtkWidget *
osso_abook_presence_label_new          (OssoABookPresence      *presence);

void
osso_abook_presence_label_set_presence (OssoABookPresenceLabel *label,
                                        OssoABookPresence      *presence);

OssoABookPresence *
osso_abook_presence_label_get_presence (OssoABookPresenceLabel *label);
  
G_END_DECLS

#endif /* __OSSO_ABOOK_PRESENCE_LABEL_H__ */
