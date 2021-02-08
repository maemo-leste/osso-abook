/*
 * osso-abook-plugin.c
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

#include <glib-2.0/gmodule.h>
#include <gtk/gtkprivate.h>

#include "config.h"

#include "osso-abook-plugin.h"

struct _OssoABookPluginPrivate
{
  gchar *filename;
  GModule *module;
  void (*load)(GTypeModule *module);
  void (*unload)(GTypeModule *module);
};

typedef struct _OssoABookPluginPrivate OssoABookPluginPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookPlugin,
  osso_abook_plugin,
  G_TYPE_TYPE_MODULE
);

#define OSSO_ABOOK_PLUGIN_PRIVATE(group) \
                osso_abook_plugin_get_instance_private(group)

enum {
  PROP_FILENAME = 1
};

static void
osso_abook_plugin_finalize(GObject *object)
{
  OssoABookPluginPrivate *priv =
      OSSO_ABOOK_PLUGIN_PRIVATE(OSSO_ABOOK_PLUGIN(object));

  g_free(priv->filename);

  G_OBJECT_CLASS(osso_abook_plugin_parent_class)->finalize(object);
}

static void
osso_abook_plugin_get_property(GObject *object, guint property_id,
                               GValue *value, GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_FILENAME:
    {
      OssoABookPlugin *plugin = OSSO_ABOOK_PLUGIN(object);
      OssoABookPluginPrivate *priv = OSSO_ABOOK_PLUGIN_PRIVATE(plugin);

      g_value_set_string(value, priv->filename);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static void
osso_abook_plugin_set_property(GObject *object, guint property_id,
                               const GValue *value, GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_FILENAME:
    {
      OssoABookPlugin *plugin = OSSO_ABOOK_PLUGIN(object);
      OssoABookPluginPrivate *priv = OSSO_ABOOK_PLUGIN_PRIVATE(plugin);

      g_free(priv->filename);
      priv->filename = g_value_dup_string(value);
      break;
    }
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
  }
}

static gboolean
osso_abook_plugin_load(GTypeModule *module)
{
  OssoABookPlugin *plugin = OSSO_ABOOK_PLUGIN(module);
  OssoABookPluginPrivate *priv = OSSO_ABOOK_PLUGIN_PRIVATE(plugin);

  if (!priv->filename)
  {
    g_warning("Module path not set");
    return FALSE;
  }


  priv->module = g_module_open(priv->filename, 0);

  if (!priv->module)
  {
    g_warning("%s", g_module_error());
    return FALSE;
  }

  if (g_module_symbol(priv->module, "osso_abook_menu_plugin_load",
                      (gpointer *)&priv->load) &&
      g_module_symbol(priv->module, "osso_abook_menu_plugin_unload",
                      (gpointer *)&priv->unload) )
  {
    priv->load(module);
    return TRUE;
  }

  g_warning("%s", g_module_error());
  g_module_close(priv->module);

  return FALSE;
}

static void
osso_abook_plugin_unload(GTypeModule *module)
{
  OssoABookPlugin *plugin = OSSO_ABOOK_PLUGIN(module);
  OssoABookPluginPrivate *priv = OSSO_ABOOK_PLUGIN_PRIVATE(plugin);

  priv->unload(module);
  g_module_close(priv->module);
  priv->module = NULL;
  priv->load = NULL;
  priv->unload = NULL;
}

static void
osso_abook_plugin_class_init(OssoABookPluginClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  GTypeModuleClass *type_module_class = G_TYPE_MODULE_CLASS(klass);

  object_class->finalize = osso_abook_plugin_finalize;
  object_class->get_property = osso_abook_plugin_get_property;
  object_class->set_property = osso_abook_plugin_set_property;
  type_module_class->load = osso_abook_plugin_load;
  type_module_class->unload = osso_abook_plugin_unload;

  g_object_class_install_property(
        object_class, PROP_FILENAME,
        g_param_spec_string(
          "filename", "Filename", "The filaname of the module", NULL,
          GTK_PARAM_READWRITE | GTK_ARG_CONSTRUCT_ONLY));
}

static void
osso_abook_plugin_init(OssoABookPlugin *plugin)
{
  OssoABookPluginPrivate *priv = OSSO_ABOOK_PLUGIN_PRIVATE(plugin);

  priv->filename = NULL;
  priv->module = NULL;
  priv->load = NULL;
  priv->unload = NULL;
}

OssoABookPlugin *
osso_abook_plugin_new(const char *filename)
{
  g_return_val_if_fail(filename != NULL, NULL);

  return g_object_new(v1, "filename", filename, NULL);
}
