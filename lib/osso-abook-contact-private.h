/*
 * osso-abook-contact-private.h
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

#ifndef __OSSO_ABOOK_CONTACT_PRIVATE_H__
#define __OSSO_ABOOK_CONTACT_PRIVATE_H__

G_BEGIN_DECLS

void _osso_abook_contact_reject_for_uid_full(
    OssoABookContact *contact, const gchar *master_uid,
    gboolean always_keep_roster_contact, GError **error);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_PRIVATE_H__ */
