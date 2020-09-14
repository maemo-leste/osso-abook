/*
 * osso-abook-contact-filter.c
 *
 * Copyright (C) 2020 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
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

#include "osso-abook-contact-filter.h"

#include "config.h"

typedef OssoABookContactFilterIface OssoABookContactFilterInterface;

G_DEFINE_INTERFACE(
    OssoABookContactFilter,
    osso_abook_contact_filter,
    G_TYPE_OBJECT
);

static void
osso_abook_contact_filter_default_init(OssoABookContactFilterIface *iface)
{
  g_signal_new("contact-filter-changed",
               G_TYPE_FROM_CLASS(iface), G_SIGNAL_RUN_FIRST,
               G_STRUCT_OFFSET(OssoABookContactFilterIface,
                               contact_filter_changed),
               0, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

gboolean
osso_abook_contact_filter_accept(OssoABookContactFilter *filter,
                                 const char *uid, OssoABookContact *contact)
{
  OssoABookContactFilterIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FILTER(filter), TRUE);

  iface = OSSO_ABOOK_CONTACT_FILTER_GET_IFACE(filter);

  g_return_val_if_fail(NULL != iface->accept, TRUE);

  return iface->accept(filter, uid, contact);
}

OssoABookContactFilterFlags
osso_abook_contact_filter_get_flags(OssoABookContactFilter *filter)
{
  OssoABookContactFilterIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_FILTER(filter), TRUE);

  iface = OSSO_ABOOK_CONTACT_FILTER_GET_IFACE(filter);

  if (iface->get_flags)
    return iface->get_flags(filter);

  return 0;
}
