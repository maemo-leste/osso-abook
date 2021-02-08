/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2009-2010 Nokia Corporation
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_PLUGIN_H__
#define __OSSO_ABOOK_PLUGIN_H__

#include <glib-object.h>
#include "osso-abook-types.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_PLUGIN \
                (osso_abook_plugin_get_type ())
#define OSSO_ABOOK_PLUGIN(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_PLUGIN, \
                 OssoABookPlugin))
#define OSSO_ABOOK_PLUGIN_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST((klass), \
                 OSSO_ABOOK_TYPE_PLUGIN, \
                 OssoABookPluginClass))
#define OSSO_ABOOK_IS_PLUGIN(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_PLUGIN))
#define OSSO_ABOOK_IS_PLUGIN_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_PLUGIN))
#define OSSO_ABOOK_PLUGIN_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_PLUGIN, \
                 OssoABookPluginClass))

struct _OssoABookPlugin
{
        GTypeModule  parent_instance;
};

struct _OssoABookPluginClass
{
        GTypeModuleClass  parent_class;
};

GType
osso_abook_plugin_get_type (void) G_GNUC_CONST;

OssoABookPlugin *
osso_abook_plugin_new      (const char *filename);

G_END_DECLS

#endif /* __OSSO_ABOOK_PLUGIN_H_ */
