/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_MENU_EXTENSION_H__
#define __OSSO_ABOOK_MENU_EXTENSION_H__

#include "osso-abook-types.h"

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_MENU_EXTENSION \
                (osso_abook_menu_extension_get_type ())
#define OSSO_ABOOK_MENU_EXTENSION(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_MENU_EXTENSION, \
                 OssoABookMenuExtension))
#define OSSO_ABOOK_MENU_EXTENSION_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST((klass), \
                 OSSO_ABOOK_TYPE_MENU_EXTENSION, \
                 OssoABookMenuExtensionClass))
#define OSSO_ABOOK_IS_MENU_EXTENSION(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_MENU_EXTENSION))
#define OSSO_ABOOK_IS_MENU_EXTENSION_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_MENU_EXTENSION))
#define OSSO_ABOOK_MENU_EXTENSION_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_MENU_EXTENSION, \
                 OssoABookMenuExtensionClass))

/**
 * OSSO_ABOOK_DEFINE_MENU_PLUGIN:
 * @TN: the name of the new type, in Camel case
 * @t_n: the name of hte new type, in lowercase, with words separated by '_'
 * @TP: the GType of the parent type
 * @LC: code to execute when the module is loaded
 * @UC: code to execute when the module is unloaded
 *
 * Registers a class as a menu extension.  The arguments @TN, @t_n, and @TP
 * correspond directly to the parameters of G_DEFINE_DYNAMIC_TYPE() (which is
 * called internally)
 */
#define OSSO_ABOOK_DEFINE_MENU_PLUGIN(TN, t_n, TP, LC, UC)              \
                G_DEFINE_DYNAMIC_TYPE (TN, t_n, TP)                     \
                OSSO_ABOOK_DEFINE_MENU_PLUGIN_SYMBOLS (t_n, LC, UC)

#define OSSO_ABOOK_DEFINE_MENU_PLUGIN_SYMBOLS(t_n, LC, UC)              \
                                                                        \
G_MODULE_EXPORT void                                                    \
osso_abook_menu_plugin_load (GTypeModule *module);                      \
                                                                        \
void                                                                    \
osso_abook_menu_plugin_load (GTypeModule *module)                       \
{                                                                       \
        t_n##_register_type (module);                                   \
        {LC}                                                            \
}                                                                       \
                                                                        \
G_MODULE_EXPORT void                                                    \
osso_abook_menu_plugin_unload (GTypeModule *module);                    \
                                                                        \
void                                                                    \
osso_abook_menu_plugin_unload (GTypeModule *module)                     \
{                                                                       \
        {UC}                                                            \
}

#define OSSO_ABOOK_MENU_NAME_MAIN_VIEW          "osso-abook-main-view"
#define OSSO_ABOOK_MENU_NAME_CONTACT_VIEW       "osso-abook-contact-view"
#define OSSO_ABOOK_MENU_NAME_MECARD_VIEW        "osso-abook-mecard-view"
#define OSSO_ABOOK_MENU_NAME_SIM_VIEW           "osso-abook-sim-view"

/**
 * OssoABookMenuEntry:
 * @label: a label for the menu item
 * @accel_key: an Accel key for the menu item
 * @accel_mods: modifier keys to use for the menu item
 * @callback: a function to call when the menu item is activated
 * @name: the name of the menu item.
 */
struct _OssoABookMenuEntry {
        const char      *label;
        unsigned         accel_key;
        GdkModifierType  accel_mods;
        GCallback        callback;
        const char      *name;
};

/**
 * OssoABookMenuExtension:
 *
 * Base class for menu extensions.  To implement a menu extension, create a
 * class that derives from #OssoABookMenuExtension, implement the virtual
 * methods get_n_menu_entries() and get_menu_entries(), and register the
 * extension with OSSO_ABOOK_DEFINE_MENU_PLUGIN()
 */
struct _OssoABookMenuExtension
{
        GObject  parent_instance;
};

/**
 * OssoABookMenuExtensionClass:
 * @name: The name of an account profile (e.g. "google-talk"), or a well kown menu
 * @get_n_menu_entries: Virtual function for osso_abook_menu_extension_get_n_menu_entries()
 * @get_menu_entries: Virtual function for osso_abook_menu_extension_get_menu_entries()
 *
 * Well known menu names are: "osso-abook-main-view",
 * "osso-abook-contact-view","osso-abook-mecard-view" and
 * "osso-abook-sim-view".
 */
struct _OssoABookMenuExtensionClass
{
        GObjectClass  parent_class;
        const char   *name;

        /* extension interface */
        int                        (* get_n_menu_entries) (OssoABookMenuExtension *extension);
        const OssoABookMenuEntry * (* get_menu_entries)   (OssoABookMenuExtension *extension);
};

GType
osso_abook_menu_extension_get_type           (void) G_GNUC_CONST;

int
osso_abook_menu_extension_get_n_menu_entries (OssoABookMenuExtension *extension);

const OssoABookMenuEntry *
osso_abook_menu_extension_get_menu_entries   (OssoABookMenuExtension *extension);

GtkWindow *
osso_abook_menu_extension_get_parent         (OssoABookMenuExtension *extension);

const char *
osso_abook_menu_extension_get_menu_name      (OssoABookMenuExtension *extension);

OssoABookContact *
osso_abook_menu_extension_get_contact        (OssoABookMenuExtension *extension);

G_END_DECLS

#endif /* __OSSO_ABOOK_MENU_EXTENSION_H_ */
