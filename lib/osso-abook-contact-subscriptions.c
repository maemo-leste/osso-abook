/*
 * osso-abook-contact-subscriptions.c
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

#include <gdk/gdk.h>

#include "config.h"

#include "osso-abook-contact-subscriptions.h"
#include "osso-abook-contact-filter.h"

struct _OssoABookContactSubscriptionsPrivate {
  GHashTable *uids;
  guint emit_idle_id;
};
typedef struct _OssoABookContactSubscriptionsPrivate OssoABookContactSubscriptionsPrivate;

static void
osso_abook_contact_subscriptions_contact_filter_iface_init(
    OssoABookContactFilterIface *iface);

#define OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_PRIVATE(subscriptions) \
                ((OssoABookContactSubscriptionsPrivate *)osso_abook_contact_subscriptions_get_instance_private(subscriptions))

G_DEFINE_TYPE_WITH_CODE(
  OssoABookContactSubscriptions,
  osso_abook_contact_subscriptions,
  G_TYPE_OBJECT,
  G_IMPLEMENT_INTERFACE(
      OSSO_ABOOK_TYPE_CONTACT_FILTER,
      osso_abook_contact_subscriptions_contact_filter_iface_init);
  G_ADD_PRIVATE(OssoABookContactSubscriptions);
);

enum {
  CONTACT_FILTER_CHANGED,
  LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {};

static OssoABookContactFilterFlags
osso_abook_contact_subscriptions_contact_filter_get_flags(
    OssoABookContactFilter *filter)
{
  return OSSO_ABOOK_CONTACT_FILTER_ONLY_READS_UID;
}

static gboolean
osso_abook_contact_subscriptions_contact_filter_accept(
    OssoABookContactFilter *filter, const char *uid, OssoABookContact *contact)
{
  OssoABookContactSubscriptions * subscriptions =
      OSSO_ABOOK_CONTACT_SUBSCRIPTIONS(filter);
  OssoABookContactSubscriptionsPrivate *priv =
      OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_PRIVATE(subscriptions);

  return g_hash_table_lookup(priv->uids, uid) != NULL;
}

static void
osso_abook_contact_subscriptions_contact_filter_iface_init(
    OssoABookContactFilterIface *iface)
{
  iface->get_flags = osso_abook_contact_subscriptions_contact_filter_get_flags;
  iface->accept = osso_abook_contact_subscriptions_contact_filter_accept;
}

static void
osso_abook_contact_subscriptions_finalize(GObject *object)
{
  OssoABookContactSubscriptions * subscriptions =
      OSSO_ABOOK_CONTACT_SUBSCRIPTIONS(object);
  OssoABookContactSubscriptionsPrivate *priv =
      OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_PRIVATE(subscriptions);

  g_hash_table_destroy(priv->uids);

  if (priv->emit_idle_id)
    g_source_remove(priv->emit_idle_id);

  G_OBJECT_CLASS(
        osso_abook_contact_subscriptions_parent_class)->finalize(object);
}

static void
osso_abook_contact_subscriptions_class_init(
    OssoABookContactSubscriptionsClass *klass)
{
  G_OBJECT_CLASS(klass)->finalize = osso_abook_contact_subscriptions_finalize;

  signals[CONTACT_FILTER_CHANGED] =
      g_signal_lookup("contact-filter-changed", OSSO_ABOOK_TYPE_CONTACT_FILTER);
}

static void
osso_abook_contact_subscriptions_init(OssoABookContactSubscriptions *self)
{
  OssoABookContactSubscriptionsPrivate *priv =
      OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_PRIVATE(self);

  priv->uids = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
}

static gboolean
emit_contact_filter_changed(gpointer user_data)
{
  OssoABookContactSubscriptions *subscriptions = user_data;
  OssoABookContactSubscriptionsPrivate *priv =
      OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_PRIVATE(subscriptions);

  priv->emit_idle_id = 0;
  g_signal_emit(subscriptions, signals[CONTACT_FILTER_CHANGED], 0);

  return FALSE;
}

static void
idle_emit_contact_filter_changed(OssoABookContactSubscriptions *subscriptions)
{
  OssoABookContactSubscriptionsPrivate *priv =
      OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_PRIVATE(subscriptions);

  if (!priv->emit_idle_id)
  {
    priv->emit_idle_id =
        gdk_threads_add_idle(emit_contact_filter_changed, subscriptions);
  }
}

void
osso_abook_contact_subscriptions_add(
    OssoABookContactSubscriptions *subscriptions, const char *uid)
{
  OssoABookContactSubscriptionsPrivate *priv;

  g_return_if_fail(OSSO_ABOOK_IS_CONTACT_SUBSCRIPTIONS(subscriptions));
  g_return_if_fail(NULL != uid);

  priv = OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_PRIVATE(subscriptions);
  g_hash_table_insert(priv->uids, g_strdup(uid), subscriptions);
  idle_emit_contact_filter_changed(subscriptions);
}

gboolean
osso_abook_contact_subscriptions_remove(
    OssoABookContactSubscriptions *subscriptions, const char *uid)
{
  OssoABookContactSubscriptionsPrivate *priv;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_SUBSCRIPTIONS(subscriptions),
                       FALSE);
  g_return_val_if_fail(NULL != uid, FALSE);

  priv = OSSO_ABOOK_CONTACT_SUBSCRIPTIONS_PRIVATE(subscriptions);

  if (g_hash_table_remove(priv->uids, uid))
  {
    idle_emit_contact_filter_changed(subscriptions);
    return TRUE;
  }

  return FALSE;
}
