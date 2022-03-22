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

#include "config.h"

#include <gtk/gtkprivate.h>

#include "osso-abook-account-manager.h"
#include "osso-abook-log.h"
#include "osso-abook-roster-manager.h"
#include "osso-abook-roster.h"
#include "osso-abook-util.h"
#include "osso-abook-utils-private.h"

#include "osso-abook-service-group.h"

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
  OSSO_ABOOK_TYPE_GROUP
);

#define OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group) \
  ((OssoABookServiceGroupPrivate *) \
   osso_abook_service_group_get_instance_private(group))

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

__attribute__((destructor)) static void
osso_abook_service_group_destructor()
{
  if (service_groups_by_name)
  {
    g_hash_table_destroy(service_groups_by_name);
    service_groups_by_name = NULL;
  }
}

static void
_account_changed_cb(OssoABookAccountManager *manager, TpAccount *account,
                    GQuark property, const GValue *value, gpointer user_data)
{
  OssoABookServiceGroup *group =
    g_hash_table_lookup(service_groups_by_name,
                        tp_account_get_path_suffix(account));

  if (group)
  {
    OssoABookServiceGroupPrivate *priv =
      OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group);
    gchar *display_name = osso_abook_tp_account_get_display_string(
        account, NULL, "%s - %s");

    if (strcmp(display_name, priv->name))
    {
      g_object_set(G_OBJECT(group),
                   "display-name", display_name,
                   NULL);
    }

    g_free(display_name);
    g_signal_emit(group, signals[REFILTER_GROUP], 0);
  }
}

static void
_roster_created_cb(OssoABookRosterManager *manager, OssoABookRoster *roster,
                   gpointer user_data)
{
  if (roster)
  {
    TpAccount *account = osso_abook_roster_get_account(roster);

    if (account)
      _account_changed_cb(NULL, account, 0, NULL, user_data);
  }
}

static void
_roster_removed_cb(OssoABookRosterManager *manager, OssoABookRoster *roster,
                   gpointer user_data)
{
  if (roster)
  {
    TpAccount *account = osso_abook_roster_get_account(roster);

    if (account)
    {
      g_hash_table_remove(service_groups_by_name,
                          tp_account_get_path_suffix(account));
    }
  }
}

OssoABookGroup *
osso_abook_service_group_get(TpAccount *account)
{
  OssoABookServiceGroup *group;
  const char *protocol;
  const char *icon_name;
  gchar *display_name;
  OssoABookServiceGroupPrivate *priv;
  OssoABookAccountManager *manager;
  TpProtocol *protocol_object = NULL;
  static gulong account_changed_id = 0;
  static gulong roster_created_id = 0;
  static gulong roster_removed_id = 0;

  if (!service_groups_by_name)
  {
    service_groups_by_name = g_hash_table_new_full(
        (GHashFunc)&g_str_hash,
        (GEqualFunc)&g_str_equal,
        (GDestroyNotify)&g_free,
        (GDestroyNotify)&g_object_unref);
  }

  group = g_hash_table_lookup(service_groups_by_name,
                              tp_account_get_path_suffix(account));

  if (group)
    return OSSO_ABOOK_GROUP(group);

  protocol = tp_account_get_protocol_name(account);

  if (protocol)
  {
    protocol_object =
      osso_abook_account_manager_get_protocol_object(NULL, protocol);
  }

  if (!protocol_object)
  {
    OSSO_ABOOK_WARN("Cannot get protocol");
    return NULL;
  }

  if (!tp_protocol_get_vcard_field(protocol_object))
    return NULL;

  display_name = osso_abook_tp_account_get_display_string(
      account, osso_abook_tp_account_get_bound_name(account), "%s - %s");

  group = g_object_new(OSSO_ABOOK_TYPE_SERVICE_GROUP,
                       "service-name", tp_account_get_path_suffix(account),
                       "display-name", display_name,
                       NULL);
  g_free(display_name);
  g_hash_table_insert(service_groups_by_name,
                      g_strdup(tp_account_get_path_suffix(account)), group);

  priv = OSSO_ABOOK_SERVICE_GROUP_PRIVATE(group);
  priv->account = account;

  icon_name = tp_account_get_icon_name(account);

  if (!IS_EMPTY(icon_name))
    icon_name = tp_protocol_get_icon_name(protocol_object);

  priv->icon_name = g_strdup(icon_name);
  priv->vcard_field = g_strdup(tp_protocol_get_vcard_field(protocol_object));

  manager = osso_abook_account_manager_get_default();

  if (!account_changed_id)
  {
    account_changed_id =
      g_signal_connect(manager, "account-changed",
                       G_CALLBACK(_account_changed_cb), NULL);
  }

  if (!roster_created_id)
  {
    roster_created_id = g_signal_connect(manager, "roster-created",
                                         G_CALLBACK(_roster_created_cb), NULL);
  }

  if (!roster_removed_id)
  {
    roster_removed_id = g_signal_connect(manager, "roster-removed",
                                         G_CALLBACK(_roster_removed_cb), NULL);
  }

  return OSSO_ABOOK_GROUP(group);
}
