/*
 * osso-abook-recent-group.c
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

#include "osso-abook-roster.h"
#include "osso-abook-utils-private.h"

#include "osso-abook-recent-group.h"

struct _OssoABookRecentGroupPrivate
{
  OssoABookRoster *roster;
  guint contacts_added_id;
  guint contacts_changed_master_id;
  guint contacts_removed_id;
  GHashTable *recent_contacts;
  OssoABookContact *contact;
};

typedef struct _OssoABookRecentGroupPrivate OssoABookRecentGroupPrivate;

G_DEFINE_TYPE_WITH_PRIVATE(
  OssoABookRecentGroup,
  osso_abook_recent_group,
  OSSO_ABOOK_TYPE_GROUP
);

#define PRIVATE(group) \
  ((OssoABookRecentGroupPrivate *) \
   osso_abook_recent_group_get_instance_private( \
     (OssoABookRecentGroup *)(group)))

enum
{
  PROP_ROSTER = 1
};

static OssoABookGroup *recent_group_singleton = NULL;
static guint refilter_contact_signal = 0;

static void
osso_abook_recent_group_set_property(GObject *object, guint property_id,
                                     const GValue *value, GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_ROSTER:
    {
      osso_abook_recent_group_set_roster(OSSO_ABOOK_RECENT_GROUP(object),
                                         g_value_get_object(value));
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
osso_abook_recent_group_get_property(GObject *object,
                                     guint property_id, GValue *value,
                                     GParamSpec *pspec)
{
  switch (property_id)
  {
    case PROP_ROSTER:
    {
      g_value_set_object(value, PRIVATE(object)->roster);
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
osso_abook_recent_group_dispose(GObject *object)
{
  osso_abook_recent_group_set_roster(OSSO_ABOOK_RECENT_GROUP(object), NULL);

  G_OBJECT_CLASS(osso_abook_recent_group_parent_class)->dispose(object);
}

static void
osso_abook_recent_group_finalize(GObject *object)
{
  OssoABookRecentGroupPrivate *priv = PRIVATE(object);

  g_hash_table_destroy(priv->recent_contacts);

  G_OBJECT_CLASS(osso_abook_recent_group_parent_class)->finalize(object);
}

static const char *
osso_abook_recent_group_get_icon_name(OssoABookGroup *group)
{
  return "addressbook_recent_contacts_group";
}

static const char *
osso_abook_recent_group_get_name(OssoABookGroup *group)
{
  return "addr_ti_main_view_recent";
}

static gboolean
osso_abook_recent_group_is_visible(OssoABookGroup *group)
{
  return TRUE;
}

static gboolean
osso_abook_recent_group_includes_contact(OssoABookGroup *group,
                                         OssoABookContact *contact)
{
  OssoABookRecentGroupPrivate *priv = PRIVATE(group);
  const gchar *uid = e_contact_get_const(E_CONTACT(contact), E_CONTACT_UID);

  if (g_hash_table_lookup_extended(priv->recent_contacts, uid, NULL, NULL))
    return !osso_abook_contact_get_blocked(contact);

  return FALSE;
}

static int
osso_abook_recent_group_get_sort_weight(OssoABookGroup *group)
{
  return -300;
}

static void
osso_abook_recent_group_class_init(OssoABookRecentGroupClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS(klass);
  OssoABookGroupClass *group_class = OSSO_ABOOK_GROUP_CLASS(klass);

  object_class->set_property = osso_abook_recent_group_set_property;
  object_class->get_property = osso_abook_recent_group_get_property;
  object_class->dispose = osso_abook_recent_group_dispose;
  object_class->finalize = osso_abook_recent_group_finalize;

  group_class->get_icon_name = osso_abook_recent_group_get_icon_name;
  group_class->get_name = osso_abook_recent_group_get_name;
  group_class->is_visible = osso_abook_recent_group_is_visible;
  group_class->includes_contact = osso_abook_recent_group_includes_contact;
  group_class->get_sort_weight = osso_abook_recent_group_get_sort_weight;

  g_object_class_install_property(
    object_class, PROP_ROSTER,
    g_param_spec_object(
      "roster",
      "Roster",
      "The monitored contact roster.",
      OSSO_ABOOK_TYPE_ROSTER,
      GTK_PARAM_READWRITE));

  refilter_contact_signal = g_signal_lookup("refilter-contact",
                                            OSSO_ABOOK_TYPE_GROUP);
}

__attribute__((destructor)) static void
osso_abook_recentl_group_at_exit()
{
  if (recent_group_singleton)
  {
    g_object_unref(recent_group_singleton);
    recent_group_singleton = NULL;
  }
}

OssoABookGroup *
osso_abook_recent_group_get(void)
{
  if (!recent_group_singleton)
    recent_group_singleton = g_object_new(OSSO_ABOOK_TYPE_RECENT_GROUP, NULL);

  return recent_group_singleton;
}

void
osso_abook_recent_group_init(OssoABookRecentGroup *group)
{
  OssoABookRecentGroupPrivate *priv = PRIVATE(group);

  priv->roster = NULL;
  priv->contact = 0;
  priv->recent_contacts = g_hash_table_new_full((GHashFunc)&g_str_hash,
                                         (GEqualFunc)&g_str_equal,
                                         NULL,
                                         (GDestroyNotify)&g_object_unref);
}

static void
contact_changed(OssoABookRecentGroup *group, OssoABookContact **contacts,
                gboolean master_changed)
{
  OssoABookRecentGroupPrivate *priv = PRIVATE(group);
  const gchar *uid;

  while (*contacts)
  {
    uid = e_contact_get_const(E_CONTACT(*contacts), E_CONTACT_UID);

    if (osso_abook_contact_get_blocked(*contacts))
    {
      if (master_changed && g_hash_table_remove(priv->recent_contacts, uid))
        g_signal_emit(group, refilter_contact_signal, 0, *contacts);
    }
    else if (g_hash_table_size(priv->recent_contacts) < 10)
    {
      if (!master_changed || !g_hash_table_lookup(priv->recent_contacts, uid))
      {
        /* FIXME - umm.. what? For sure I REed that correctly...
         * Looks like remnants of some functionality. */
        g_object_ref(priv->contact);
        g_hash_table_remove(priv->recent_contacts,
                            e_contact_get_const(E_CONTACT(priv->contact),
                                                E_CONTACT_UID));
        g_signal_emit(group, refilter_contact_signal, 0, priv->contact);
        g_object_unref(priv->contact);
      }

      g_hash_table_replace(priv->recent_contacts, (gchar *)uid,
                           g_object_ref(*contacts));
      g_signal_emit(group, refilter_contact_signal, 0, *contacts);
    }

    contacts++;
  }
}

static void
contact_added_cb(OssoABookRoster *roster, OssoABookContact **contacts,
                 OssoABookRecentGroup *group)
{
  contact_changed(group, contacts, FALSE);
}

static void
contacts_changed_master_cb(OssoABookRoster *roster, OssoABookContact **contacts,
                           OssoABookRecentGroup *group)
{
  contact_changed(group, contacts, TRUE);
}

static void
contacts_removed_cb(OssoABookRoster *roster, const char **uids,
                    OssoABookRecentGroup *group)
{
  OssoABookRecentGroupPrivate *priv = PRIVATE(group);

  while (*uids)
  {
    g_hash_table_remove(priv->recent_contacts, *uids);
    uids++;
  }
}

void
osso_abook_recent_group_set_roster(OssoABookRecentGroup *group,
                                   OssoABookRoster *roster)
{
  OssoABookRecentGroupPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_RECENT_GROUP(group));
  g_return_if_fail(!roster || OSSO_ABOOK_IS_ROSTER(roster));

  priv = PRIVATE(group);

  if (priv->roster)
  {
    disconnect_signal_if_connected(&priv->roster, priv->contacts_added_id);
    disconnect_signal_if_connected(&priv->roster,
                                   priv->contacts_changed_master_id);
    disconnect_signal_if_connected(&priv->roster, priv->contacts_removed_id);
    g_object_unref(priv->roster);
    priv->roster = NULL;
  }

  if (roster)
  {
    priv->roster = (OssoABookRoster *)g_object_ref(roster);
    priv->contacts_added_id =
      g_signal_connect_object(priv->roster, "contacts-added",
                              G_CALLBACK(contact_added_cb), group, 0);
    priv->contacts_changed_master_id =
      g_signal_connect_object(priv->roster, "contacts-changed::master",
                              G_CALLBACK(contacts_changed_master_cb), group,
                              0);
    priv->contacts_removed_id =
      g_signal_connect_object(priv->roster, "contacts-removed",
                              G_CALLBACK(contacts_removed_cb), group, 0);
  }

  g_object_notify(G_OBJECT(group), "roster");
}

OssoABookRoster *
osso_abook_recent_group_get_roster(OssoABookRecentGroup *group)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_RECENT_GROUP(group), NULL);

  return PRIVATE(group)->roster;
 }
