/*
 * osso-abook-profile-group.c
 *
 * Copyright (C) 2022 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
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

#include "osso-abook-utils-private.h"

#include "osso-abook-profile-group.h"

struct _OssoABookProfileGroupPrivate
{
  TpProtocol *protocol;
};

typedef struct _OssoABookProfileGroupPrivate OssoABookProfileGroupPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookProfileGroup,
  osso_abook_profile_group,
  OSSO_ABOOK_TYPE_GROUP
);

#define PRIVATE(group) \
  ((OssoABookProfileGroupPrivate *) \
   osso_abook_profile_group_get_instance_private( \
     (OssoABookProfileGroup *)(group)))

enum
{
  PROP_PROTOCOL = 1
};

static GHashTable *groups = NULL;

static void
osso_abook_profile_group_set_property(GObject *object, guint property_id,
                                      const GValue *value, GParamSpec *pspec)
{
  OssoABookProfileGroupPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_PROTOCOL:
    {
      priv->protocol = g_value_dup_object(value);
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
osso_abook_profile_group_get_property(GObject *object, guint property_id,
                                      GValue *value, GParamSpec *pspec)
{
  OssoABookProfileGroupPrivate *priv = PRIVATE(object);

  switch (property_id)
  {
    case PROP_PROTOCOL:
    {
      g_value_set_object(value, priv->protocol);
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
osso_abook_profile_group_dispose(GObject *object)
{
  OssoABookProfileGroupPrivate *priv = PRIVATE(object);

  if (priv->protocol)
  {
    g_object_unref(priv->protocol);
    priv->protocol = NULL;
  }

  G_OBJECT_CLASS(osso_abook_profile_group_parent_class)->dispose(object);
}

static const char *
osso_abook_profile_group_get_icon_name(OssoABookGroup *group)
{
  OssoABookProfileGroupPrivate *priv = PRIVATE(group);

  return tp_protocol_get_icon_name(priv->protocol);
}

static const char *
osso_abook_profile_group_get_name(OssoABookGroup *group)
{
  OssoABookProfileGroupPrivate *priv = PRIVATE(group);

  return tp_protocol_get_english_name(priv->protocol);
}

static gboolean
osso_abook_profile_group_includes_contact(OssoABookGroup *group,
                                          OssoABookContact *contact)
{
  OssoABookProfileGroupPrivate *priv;
  EVCardAttribute *vcf;
  gboolean rv = FALSE;

  g_return_val_if_fail(OSSO_ABOOK_IS_PROFILE_GROUP(group), FALSE);

  priv = PRIVATE(group);

  if (osso_abook_contact_get_blocked(contact))
    return FALSE;

  vcf = e_vcard_get_attribute(E_VCARD(contact),
                              tp_protocol_get_vcard_field(priv->protocol));

  if (vcf)
  {
    gchar *v = e_vcard_attribute_get_value(vcf);

    if (!IS_EMPTY(v))
      rv = TRUE;

    g_free(v);
  }

  return rv;
}

static int
osso_abook_profile_group_get_sort_weight(OssoABookGroup *group)
{
  return -60;
}

static void
osso_abook_profile_group_class_init(OssoABookProfileGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  OssoABookGroupClass *group_class = OSSO_ABOOK_GROUP_CLASS(klass);

  object_class->set_property = osso_abook_profile_group_set_property;
  object_class->get_property = osso_abook_profile_group_get_property;
  object_class->dispose = osso_abook_profile_group_dispose;

  group_class->get_icon_name = osso_abook_profile_group_get_icon_name;
  group_class->get_name = osso_abook_profile_group_get_name;
  group_class->includes_contact = osso_abook_profile_group_includes_contact;
  group_class->get_sort_weight = osso_abook_profile_group_get_sort_weight;

  g_object_class_install_property(
    object_class, PROP_PROTOCOL,
    g_param_spec_object(
      "protocol",
      "The protocol",
      "The protocol of this group",
      TP_TYPE_PROTOCOL,
      GTK_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

static void
osso_abook_profile_group_init(OssoABookProfileGroup *group)
{}

__attribute__((destructor)) static void
osso_abook_profile_group_destructor()
{
  if (groups)
  {
    g_hash_table_destroy(groups);
    groups = NULL;
  }
}

OssoABookGroup *
osso_abook_profile_group_get(TpProtocol *protocol)
{
  OssoABookGroup *group;
  const gchar *vcard_field;

  g_return_val_if_fail(TP_IS_PROTOCOL(protocol), NULL);

  if (!groups)
  {
    groups = g_hash_table_new_full((GHashFunc)&g_str_hash,
                                   (GEqualFunc)&g_str_equal,
                                   (GDestroyNotify)&g_free,
                                   (GDestroyNotify)&g_object_unref);
  }

  vcard_field = tp_protocol_get_vcard_field(protocol);
  group = g_hash_table_lookup(groups, vcard_field);

  if (!group)
  {
    group = g_object_new(OSSO_ABOOK_TYPE_PROFILE_GROUP,
                         "protocol", protocol,
                         NULL);
    g_hash_table_insert(groups, g_strdup(vcard_field), group);
  }

  return group;
}
