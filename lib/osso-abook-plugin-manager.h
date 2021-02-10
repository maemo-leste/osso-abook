/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * Copyright (C) 2009-2010 Nokia Corporation
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_PLUGIN_MANAGER_H__
#define __OSSO_ABOOK_PLUGIN_MANAGER_H__

#include "osso-abook-menu-extension.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_PLUGIN_MANAGER \
                (osso_abook_plugin_manager_get_type ())
#define OSSO_ABOOK_PLUGIN_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_PLUGIN_MANAGER, \
                 OssoABookPluginManager))
#define OSSO_ABOOK_PLUGIN_MANAGER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST((klass), \
                 OSSO_ABOOK_TYPE_PLUGIN_MANAGER, \
                 OssoABookPluginManagerClass))
#define OSSO_ABOOK_IS_PLUGIN_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_PLUGIN_MANAGER))
#define OSSO_ABOOK_IS_PLUGIN_MANAGER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE((klass), \
                 OSSO_ABOOK_TYPE_PLUGIN_MANAGER))
#define OSSO_ABOOK_PLUGIN_MANAGER_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_PLUGIN_MANAGER, \
                 OssoABookPluginManagerClass))

struct _OssoABookPluginManager
{
        GObject parent_instance;
};

struct _OssoABookPluginManagerClass
{
        GObjectClass parent_class;
};


GType
osso_abook_plugin_manager_get_type               (void) G_GNUC_CONST;

OssoABookPluginManager *
osso_abook_plugin_manager_new                    (void);

GList *
osso_abook_plugin_manager_create_menu_extensions (OssoABookPluginManager *manager,
                                                  GType                   extension_type,
                                                  const char             *first_property_name,
                                                  ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS

#endif /* __OSSO_ABOOK_PLUGIN_MANAGER_H__ */
