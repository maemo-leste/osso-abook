/*
 * osso-abook-service-group.c
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

#include <gtk/gtkprivate.h>

#include "osso-abook-service-group.h"

#include "config.h"

struct _OssoABookServiceGroupPrivate
{
  TpAccount *account;
  gchar *service_name;
  gchar *name;
  gchar *icon_name;
  gchar *vcard_field;
};

typedef struct _OssoABookServiceGroupPrivate OssoABookServiceGroupPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookServiceGroup,
  osso_abook_service_group,
  OSSO_ABOOK_TYPE_SERVICE_GROUP
);

#define OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group) \
  ((OssoABookServiceGroupPrivate *)osso_abook_service_group_get_instance_private( \
     group))

enum
{
  PROP_SERVICE_NAME = 1,
  PROP_DISPLAY_NAME
};

enum
{
  REFILTER_GROUP,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};
static GHashTable *service_groups_by_name = NULL;

static void
osso_abook_service_group_set_property(GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec)
{
  OssoABookServiceGroup *group = OSSO_ABOOK_SERVICE_GROUP(object);
  OssoABookServiceGroupPrivate *priv = OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group);

  switch (property_id)
  {
    case PROP_SERVICE_NAME:
    {
      if (priv->service_name)
        g_free(priv->service_name);

      priv->service_name = g_value_dup_string(value);
      break;
    }
    case PROP_DISPLAY_NAME:
    {
      if (priv->name)
        g_free(priv->name);

      priv->name = g_value_dup_string(value);
      g_object_ref(group);
      g_object_notify(G_OBJECT(group), "name");
      g_object_unref(group);
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
osso_abook_service_group_get_property(GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec)
{
  OssoABookServiceGroup *group = OSSO_ABOOK_SERVICE_GROUP(object);
  OssoABookServiceGroupPrivate *priv = OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group);

  switch (property_id)
  {
    case PROP_SERVICE_NAME:
    {
      g_value_set_string(value, priv->service_name);
      break;
    }
    case PROP_DISPLAY_NAME:
    {
      g_value_set_string(value, priv->name);
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
osso_abook_service_group_dispose(GObject *object)
{
  G_OBJECT_CLASS(osso_abook_service_group_parent_class)->dispose(object);
}

static void
osso_abook_service_group_finalize(GObject *object)
{
  OssoABookServiceGroup *group = OSSO_ABOOK_SERVICE_GROUP(object);
  OssoABookServiceGroupPrivate *priv = OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group);

  g_free(priv->service_name);
  g_free(priv->name);
  g_free(priv->icon_name);
  g_free(priv->vcard_field);

  G_OBJECT_CLASS(osso_abook_service_group_parent_class)->finalize(object);
}

static const char *
osso_abook_service_group_get_icon_name(OssoABookGroup *grp)
{
  OssoABookServiceGroup *group = OSSO_ABOOK_SERVICE_GROUP(grp);
  OssoABookServiceGroupPrivate *priv = OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group);

  return priv->icon_name;
}

static const char *
osso_abook_service_group_get_name(OssoABookGroup *grp)
{
  OssoABookServiceGroup *group = OSSO_ABOOK_SERVICE_GROUP(grp);
  OssoABookServiceGroupPrivate *priv = OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group);

  return priv->name;
}

static gboolean
osso_abook_service_group_is_visible(OssoABookGroup *group)
{
  return TRUE;
}

static gboolean
osso_abook_service_group_includes_contact(OssoABookGroup *grp,
                                          OssoABookContact *contact)
{
  OssoABookServiceGroup *group;
  OssoABookServiceGroupPrivate *priv;
  GList *contacts;
  GList *l;
  gboolean rv = FALSE;

  g_return_val_if_fail(OSSO_ABOOK_IS_SERVICE_GROUP(grp), FALSE);

  group = OSSO_ABOOK_SERVICE_GROUP(grp);
  priv = OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group);

  if (osso_abook_contact_get_blocked(contact))
    return FALSE;

  if (osso_abook_contact_is_roster_contact(contact))
    return osso_abook_contact_get_account(contact) == priv->account;

  contacts = osso_abook_contact_get_roster_contacts(contact);

  for (l = contacts; l; l = l->next)
  {
    if (osso_abook_contact_get_account(l->data) == priv->account)
    {
      rv = TRUE;
      break;
    }
  }

  g_list_free(contacts);

  return rv;
}

static int
osso_abook_service_group_get_sort_weight(OssoABookGroup *group)
{
  return -50;
}

static int
osso_abook_service_group_sort_func(const OssoABookListStoreRow *row_a,
                                   const OssoABookListStoreRow *row_b,
                                   gpointer user_data)
{
  return osso_abook_presence_compare_for_display(
    OSSO_ABOOK_PRESENCE(row_a->contact),
    OSSO_ABOOK_PRESENCE(row_b->contact));
}

static OssoABookListStoreCompareFunc
osso_abook_service_group_get_sort_func(OssoABookGroup *group)
{
  return osso_abook_service_group_sort_func;
}

static void
osso_abook_service_group_class_init(OssoABookServiceGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  OssoABookGroupClass *group_class = OSSO_ABOOK_GROUP_CLASS(klass);

  object_class->set_property = osso_abook_service_group_set_property;
  object_class->get_property = osso_abook_service_group_get_property;
  object_class->dispose = osso_abook_service_group_dispose;
  object_class->finalize = osso_abook_service_group_finalize;

  group_class->get_icon_name = osso_abook_service_group_get_icon_name;
  group_class->get_name = osso_abook_service_group_get_name;
  group_class->is_visible = osso_abook_service_group_is_visible;
  group_class->includes_contact = osso_abook_service_group_includes_contact;
  group_class->get_sort_weight = osso_abook_service_group_get_sort_weight;
  group_class->get_sort_func = osso_abook_service_group_get_sort_func;

  g_object_class_install_property(
    object_class, PROP_SERVICE_NAME,
    g_param_spec_string(
      "service-name",
      "Service Name",
      "The name of the service",
      "",
      GTK_PARAM_READWRITE));

  g_object_class_install_property(
    object_class, PROP_DISPLAY_NAME,
    g_param_spec_string(
      "display-name",
      "Display Name",
      "The name to be shown in the UI",
      "",
      GTK_PARAM_READWRITE));

  signals[REFILTER_GROUP] = g_signal_lookup("refilter-group",
                                            OSSO_ABOOK_TYPE_GROUP);
}

static void
osso_abook_service_group_init(OssoABookServiceGroup *group)
{
  OssoABookServiceGroupPrivate *priv = OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group);

  priv->name = NULL;
  priv->service_name = NULL;
}

TpAccount *
osso_abook_service_group_get_account(OssoABookServiceGroup *group)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_SERVICE_GROUP(group), FALSE);

  return OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group)->account;
}

OssoABookGroup *
osso_abook_service_group_lookup_by_name(const char *unique_name)
{
  OssoABookServiceGroup *group;

  if (!service_groups_by_name)
    return NULL;

  group = g_hash_table_lookup(service_groups_by_name, unique_name);

  if (!group)
    return NULL;

  return OSSO_ABOOK_GROUP(group);
}
