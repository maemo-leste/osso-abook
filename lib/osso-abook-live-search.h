/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef OSSO_ABOOK_DISABLE_DEPRECATED

#ifndef _OSSO_ABOOK_LIVE_SEARCH
#define _OSSO_ABOOK_LIVE_SEARCH

#include "osso-abook-filter-model.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_LIVE_SEARCH osso_abook_live_search_get_type()

#define OSSO_ABOOK_LIVE_SEARCH(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_LIVE_SEARCH, \
                 OssoABookLiveSearch))
#define OSSO_ABOOK_LIVE_SEARCH_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_LIVE_SEARCH, \
                 OssoABookLiveSearchClass))
#define OSSO_ABOOK_IS_LIVE_SEARCH(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_LIVE_SEARCH))
#define OSSO_ABOOK_IS_LIVE_SEARCH_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_LIVE_SEARCH))
#define OSSO_ABOOK_LIVE_SEARCH_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_LIVE_SEARCH, \
                 OssoABookLiveSearchClass))

/**
 * OssoABookLiveSearch:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookLiveSearch {
        /*< private >*/
        GtkToolbar parent;
};

struct _OssoABookLiveSearchClass {
        GtkToolbarClass parent_class;
};

G_GNUC_DEPRECATED GType
osso_abook_live_search_get_type      (void);

G_GNUC_DEPRECATED GtkWidget *
osso_abook_live_search_new           (void);

G_GNUC_DEPRECATED void
osso_abook_live_search_append_text   (OssoABookLiveSearch *livesearch,
                                      const char           *utf8);

G_GNUC_DEPRECATED const char *
osso_abook_live_search_get_text      (OssoABookLiveSearch *livesearch);

G_GNUC_DEPRECATED void
osso_abook_live_search_set_filter    (OssoABookLiveSearch *livesearch,
                                      OssoABookFilterModel *filter);

G_GNUC_DEPRECATED void
osso_abook_live_search_widget_hook   (OssoABookLiveSearch *livesearch,
                                      GtkWidget            *widget,
                                      GtkTreeView          *kb_focus);

G_GNUC_DEPRECATED void
osso_abook_live_search_widget_unhook (OssoABookLiveSearch *livesearch);

G_GNUC_DEPRECATED void
osso_abook_live_search_save_state    (OssoABookLiveSearch *livesearch,
                                      GKeyFile             *key_file);

G_GNUC_DEPRECATED void
osso_abook_live_search_restore_state (OssoABookLiveSearch *livesearch,
                                      GKeyFile             *key_file);

G_END_DECLS

#endif /* _OSSO_ABOOK_LIVE_SEARCH */
#endif /* OSSO_ABOOK_DISABLE_DEPRECATED */
