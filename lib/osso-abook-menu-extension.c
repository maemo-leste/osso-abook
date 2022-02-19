/*
 * osso-abook-menu-extension.c
 *
 * Copyright (C) 2021 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#include <gtk/gtkprivate.h>

#include "osso-abook-contact.h"
#include "osso-abook-menu-extension.h"

enum
{
  PROP_PARENT = 1,
  PROP_MENU_NAME = 2,
  PROP_CONTACT = 3
};

struct _OssoABookMenuExtensionPrivate
{
  gpointer parent;
  gchar *menu_name;
  gpointer contact;
};

typedef struct _OssoABookMenuExtensionPrivate OssoABookMenuExtensionPrivate;

G_DEFINE_ABSTRACT_TYPE_WITH_PRIVATE(
  OssoABookMenuExtension,
  osso_abook_menu_extension,
  G_TYPE_OBJECT
);

#define OSSO_ABOOK_MENU_EXTENSION_PRIVATE(me) \
  ((OssoABookMenuExtensionPrivate *) \
   osso_abook_menu_extension_get_instance_private(me))

static void
osso_abook_menu_extension_set_property(GObject *object,
                                       guint property_id,
                                       const GValue *value,
                                       GParamSpec *pspec)
{
  OssoABookMenuExtensionPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_MENU_EXTENSION(object));

  priv = OSSO_ABOOK_MENU_EXTENSION_PRIVATE(OSSO_ABOOK_MENU_EXTENSION(object));

  switch (property_id)
  {
    case PROP_MENU_NAME:
    {
      priv->menu_name = g_value_dup_string(value);
      break;
    }
    case PROP_CONTACT:
    {
      priv->contact = g_value_dup_object(value);
      break;
    }
    case PROP_PARENT:
    {
      priv->parent = g_value_get_object(value);

      if (priv->parent)
        g_object_add_weak_pointer(priv->parent, (gpointer *)&priv->parent);

      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_menu_extension_get_property(GObject *object, guint property_id,
                                       GValue *value, GParamSpec *pspec)
{
  OssoABookMenuExtensionPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_MENU_EXTENSION(object));

  priv = OSSO_ABOOK_MENU_EXTENSION_PRIVATE(OSSO_ABOOK_MENU_EXTENSION(object));

  switch (property_id)
  {
    case PROP_MENU_NAME:
    {
      g_value_set_string(value, priv->menu_name);
      break;
    }
    case PROP_CONTACT:
    {
      g_value_set_object(value, priv->contact);
      break;
    }
    case PROP_PARENT:
    {
      g_value_set_object(value, priv->parent);
      break;
    }
    default:
    {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec);
      break;
    }
  }
}

static void
osso_abook_menu_extension_finalize(GObject *object)
{
  OssoABookMenuExtensionPrivate *priv =
    OSSO_ABOOK_MENU_EXTENSION_PRIVATE(OSSO_ABOOK_MENU_EXTENSION(object));

  if (priv->parent)
    g_object_remove_weak_pointer(priv->parent, (gpointer *)&priv->parent);

  if (priv->contact)
    g_object_unref(priv->contact);

  g_free(priv->menu_name);

  G_OBJECT_CLASS(osso_abook_menu_extension_parent_class)->finalize(object);
}

static void
osso_abook_menu_extension_class_init(OssoABookMenuExtensionClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->set_property = osso_abook_menu_extension_set_property;
  object_class->get_property = osso_abook_menu_extension_get_property;
  object_class->finalize = osso_abook_menu_extension_finalize;

  g_object_class_install_property(
    object_class, PROP_PARENT,
    g_param_spec_object(
      "parent", "Parent",
      "Transient parent for extension widgets",
      GTK_TYPE_WINDOW,
      GTK_PARAM_READWRITE | GTK_ARG_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_MENU_NAME,
    g_param_spec_string(
      "menu-name", "Menu Name",
      "Name of the menu for which this extension is created",
      NULL,
      GTK_PARAM_READWRITE | GTK_ARG_CONSTRUCT_ONLY));
  g_object_class_install_property(
    object_class, PROP_CONTACT,
    g_param_spec_object(
      "contact", "Contact",
      "Contact for which this this extension is created",
      OSSO_ABOOK_TYPE_CONTACT,
      GTK_PARAM_READWRITE | GTK_ARG_CONSTRUCT_ONLY));
}

static void
osso_abook_menu_extension_init(OssoABookMenuExtension *extension)
{}

int
osso_abook_menu_extension_get_n_menu_entries(OssoABookMenuExtension *extension)
{
  OssoABookMenuExtensionClass *extension_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_MENU_EXTENSION(extension), 0);

  extension_class = OSSO_ABOOK_MENU_EXTENSION_GET_CLASS(extension);

  if (extension_class->get_n_menu_entries)
    return extension_class->get_n_menu_entries(extension);

  g_warning(
    "%s: menu extension class %s doesn't implement get_n_menu_entries()",
    __FUNCTION__, g_type_name(G_TYPE_FROM_CLASS(extension_class)));

  return 0;
}

const OssoABookMenuEntry *
osso_abook_menu_extension_get_menu_entries(OssoABookMenuExtension *extension)
{
  OssoABookMenuExtensionClass *extension_class;

  g_return_val_if_fail(OSSO_ABOOK_IS_MENU_EXTENSION(extension), NULL);

  extension_class = OSSO_ABOOK_MENU_EXTENSION_GET_CLASS(extension);

  if (extension_class->get_menu_entries)
    return extension_class->get_menu_entries(extension);

  g_warning("%s: menu extension class %s doesn't implement get_menu_entries()",
            __FUNCTION__, g_type_name(G_TYPE_FROM_CLASS(extension_class)));

  return NULL;
}

GtkWindow *
osso_abook_menu_extension_get_parent(OssoABookMenuExtension *extension)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_MENU_EXTENSION(extension), NULL);

  return OSSO_ABOOK_MENU_EXTENSION_PRIVATE(extension)->parent;
}

const char *
osso_abook_menu_extension_get_menu_name(OssoABookMenuExtension *extension)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_MENU_EXTENSION(extension), NULL);

  return OSSO_ABOOK_MENU_EXTENSION_PRIVATE(extension)->menu_name;
}

OssoABookContact *
osso_abook_menu_extension_get_contact(OssoABookMenuExtension *extension)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_MENU_EXTENSION(extension), NULL);

  return OSSO_ABOOK_MENU_EXTENSION_PRIVATE(extension)->contact;
}
