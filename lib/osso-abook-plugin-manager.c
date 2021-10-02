/*
 * osso-abook-plugin-manager.c
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
#include <gobject/gvaluecollector.h>

#include "config.h"

#include "osso-abook-plugin-manager.h"
#include "osso-abook-plugin.h"

/* FIXME multiarch, pkgconfig variable? */
#define PLUGINS_DIR "/usr/lib/osso-addressbook/plugins"

struct _OssoABookPluginManagerPrivate
{
  GHashTable *menu_plugins;
};

typedef struct _OssoABookPluginManagerPrivate OssoABookPluginManagerPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookPluginManager,
  osso_abook_plugin_manager,
  G_TYPE_OBJECT
);

#define OSSO_ABOOK_PLUGIN_MANAGER_PRIVATE(manager) \
  ((OssoABookPluginManagerPrivate *) \
   osso_abook_plugin_manager_get_instance_private(manager))

static GObject *plugin_manager = NULL;

static void
menu_plugins_destroy(gpointer data)
{
  g_array_free(data, TRUE);
}

static void
osso_abook_plugin_manager_load_plugins(OssoABookPluginManager *manager)
{
  OssoABookPluginManagerPrivate *priv =
    OSSO_ABOOK_PLUGIN_MANAGER_PRIVATE(manager);
  GDir *dir;
  const gchar *filename;
  int i;
  GType *all_types;
  GList *plugins = NULL;
  GList *l;
  GError *error = NULL;
  guint n_children;

  g_assert(priv->menu_plugins);

  dir = g_dir_open(PLUGINS_DIR, 0, &error);

  if (!dir)
  {
    if (error->code != G_FILE_ERROR_NOENT)
      g_printerr("Error while opening module dir: %s\n", error->message);

    g_clear_error(&error);
    return;
  }

  while ((filename = g_dir_read_name(dir)))
  {
    if (g_str_has_suffix(filename, ".so"))
    {
      gchar *plugin_filename = g_build_filename(PLUGINS_DIR, filename, NULL);
      OssoABookPlugin *plugin = osso_abook_plugin_new(plugin_filename);

      if (g_type_module_use(G_TYPE_MODULE(plugin)))
      {
        g_free(plugin_filename);
        plugins = g_list_prepend(plugins, plugin);
      }
      else
      {
        g_warning("Failed to load module: %s\n", plugin_filename);
        g_free(plugin_filename);
      }
    }
  }

  g_dir_close(dir);

  priv->menu_plugins = g_hash_table_new_full(g_str_hash, g_str_equal, g_free,
                                             menu_plugins_destroy);

  all_types = g_type_children(OSSO_ABOOK_TYPE_MENU_EXTENSION, &n_children);

  for (i = 0; i < n_children; i++)
  {
    GType *type = &all_types[i];
    OssoABookMenuExtensionClass *klass = g_type_class_ref(*type);

    if (klass->name)
    {
      GStrv names = g_strsplit(klass->name, ";", -1);
      GStrv name = names;

      while (*name)
      {
        gchar *s = *name;
        GArray *types = g_hash_table_lookup(priv->menu_plugins, s);

        if (!types)
        {
          types = g_array_new(FALSE, FALSE, sizeof(*type));
          g_hash_table_insert(priv->menu_plugins, g_strdup(s), types);
        }

        g_array_append_vals(types, type, 1);

        name++;
      }

      g_strfreev(names);
    }
    else
    {
      g_warning("%s: menu extension class %s doesn't provide a name",
                __FUNCTION__, g_type_name(*type));
    }

    g_type_class_unref(klass);
  }

  for (l = plugins; l; l = l->next)
    g_type_module_unuse(G_TYPE_MODULE(l->data));

  g_list_free(plugins);
}

static GObject *
osso_abook_plugin_manager_constructor(GType type,
                                      guint n_construct_properties,
                                      GObjectConstructParam *construct_properties)
{
  if (plugin_manager)
  {
    g_object_ref(plugin_manager);
    return plugin_manager;
  }

  plugin_manager = G_OBJECT_CLASS(osso_abook_plugin_manager_parent_class)->
    constructor(type, n_construct_properties, construct_properties);

  g_object_add_weak_pointer(plugin_manager, (gpointer *)&plugin_manager);

  osso_abook_plugin_manager_load_plugins(
    (OssoABookPluginManager *)plugin_manager);

  return plugin_manager;
}

static void
osso_abook_plugin_manager_finalize(GObject *object)
{
  OssoABookPluginManagerPrivate *priv =
    OSSO_ABOOK_PLUGIN_MANAGER_PRIVATE(OSSO_ABOOK_PLUGIN_MANAGER(object));
  GHashTable *menu_plugins = priv->menu_plugins;

  if (menu_plugins)
    g_hash_table_destroy(menu_plugins);

  G_OBJECT_CLASS(osso_abook_plugin_manager_parent_class)->finalize(object);
}

static void
osso_abook_plugin_manager_class_init(OssoABookPluginManagerClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);

  object_class->constructor = osso_abook_plugin_manager_constructor;
  object_class->finalize = osso_abook_plugin_manager_finalize;
}

static void
osso_abook_plugin_manager_init(OssoABookPluginManager *plugin_manager)
{}

OssoABookPluginManager *
osso_abook_plugin_manager_new(void)
{
  return g_object_new(OSSO_ABOOK_TYPE_PLUGIN_MANAGER, NULL);
}

struct extension_parameters
{
  int num;
  const char **names;
  GValue *values;
};

static struct extension_parameters *
collect_params(GType extension_type, const char *property_name, va_list va)
{
  int i = 0;
  GParamSpec *property;
  GParamSpec *pspec;
  gchar *error;
  GValue *value;
  int num_params = 16;
  struct extension_parameters *parameters =
    g_new(struct extension_parameters, 1);

  parameters->names = g_new(const char *, num_params);
  parameters->values = g_new(GValue, num_params);

  GObjectClass *klass = g_type_class_ref(extension_type);
  ;

  do
  {
    property = g_object_class_find_property(klass, property_name);

    if (!property)
    {
      g_warning("%s: object class `%s' has no property named `%s'",
                __FUNCTION__, g_type_name(extension_type), property_name);
      goto out;
    }

    if (num_params <= parameters->num)
    {
      num_params += 16;
      parameters->names = g_renew(const char *, parameters->names, num_params);
      parameters->values = g_renew(GValue, parameters->values, num_params);
    }

    pspec = G_PARAM_SPEC(property);
    parameters->names[i] = property_name;
    value = &parameters->values[i];
    value->g_type = 0;
    g_value_init(value, pspec->value_type);
    G_VALUE_COLLECT(value, va, 0, &error);

    if (error)
    {
      g_warning("%s: %s", "collect_params", error);
      g_value_unset(value);
      g_free(error);
      break;
    }

    parameters->num++;
    i++;
    property_name = va_arg(va, const char *);
  }
  while (property_name);

out:
  g_type_class_unref(klass);

  return parameters;
}

GList *
osso_abook_plugin_manager_create_menu_extensions(
  OssoABookPluginManager *manager,
  GType extension_type,
  const char *first_property_name,
  ...)
{
  struct extension_parameters *parameters = NULL;
  const gchar *menu_name = NULL;
  OssoABookPluginManagerPrivate *priv;
  GArray *types = NULL;
  GList *extenstions = NULL;
  va_list va;
  int i;

  g_return_val_if_fail(
    g_type_is_a(extension_type, OSSO_ABOOK_TYPE_MENU_EXTENSION), NULL);
  g_return_val_if_fail(OSSO_ABOOK_IS_PLUGIN_MANAGER(manager), NULL);

  va_start(va, first_property_name);

  priv = OSSO_ABOOK_PLUGIN_MANAGER_PRIVATE(manager);

  if (first_property_name)
  {
    parameters = collect_params(extension_type, first_property_name, va);

    for (i = 0; i < parameters->num; i++)
    {
      if (!strcmp(parameters->names[i], "menu-name"))
      {
        menu_name = g_value_get_string(&parameters->values[i]);

        if (priv->menu_plugins)
          types = g_hash_table_lookup(priv->menu_plugins, menu_name);
      }
    }
  }

  g_warn_if_fail(NULL != menu_name);

  if (types)
  {
    for (i = 0; i < types->len; i++)
    {
      GType type = types->data[i];

      if (g_type_is_a(type, extension_type))
      {
        GObject *object = g_object_new_with_properties(
          type, parameters->num, parameters->names, parameters->values);

        extenstions = g_list_prepend(extenstions, object);
      }
    }
  }

  for (i = 0; i < parameters->num; i++)
    g_value_unset(&parameters->values[i]);

  g_free(parameters->names);
  g_free(parameters->values);
  g_free(parameters);

  va_end(va);

  return extenstions;
}
