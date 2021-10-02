/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_MECARD_VIEW_H__
#define __OSSO_ABOOK_MECARD_VIEW_H__

#include "osso-abook-types.h"

#include <hildon/hildon.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_MECARD_VIEW \
                (osso_abook_mecard_view_get_type ())
#define OSSO_ABOOK_MECARD_VIEW(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_MECARD_VIEW, \
                 OssoABookMecardView))
#define OSSO_ABOOK_MECARD_VIEW_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_MECARD_VIEW, \
                 OssoABookMecardViewClass))
#define OSSO_ABOOK_IS_MECARD_VIEW(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_MECARD_VIEW))
#define OSSO_ABOOK_IS_MECARD_VIEW_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_MECARD_VIEW))
#define OSSO_ABOOK_MECARD_VIEW_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_MECARD_VIEW, \
                 OssoABookMecardViewClass))

/**
 * OssoABookMecardView:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookMecardView {
        /*< private >*/
        HildonStackableWindow parent;
};

struct _OssoABookMecardViewClass {
        HildonStackableWindowClass parent_class;
};

GType
osso_abook_mecard_view_get_type         (void) G_GNUC_CONST;

GtkWidget *
osso_abook_mecard_view_new              (void);

G_END_DECLS

#endif /* __OSSO_ABOOK_MECARD_VIEW_H__ */
