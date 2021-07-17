/*
 * osso-abook-quarks.c
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

#include "osso-abook-quarks.h"
#include "osso-abook-contact.h"

#define QUARK(name, string) \
GQuark osso_abook_quark_vca_##name (void) \
{ \
  static GQuark quark = 0; \
  \
  if (!quark) \
    quark = g_quark_from_static_string(string); \
  \
  return quark; \
}

QUARK(email, EVC_EMAIL)
QUARK(tel, EVC_TEL)
QUARK(nickname, EVC_NICKNAME)
QUARK(org, EVC_ORG)
QUARK(fn, EVC_FN)
QUARK(n, EVC_N)
QUARK(photo, EVC_PHOTO)
QUARK(note, EVC_NOTE)
QUARK(bday, EVC_BDAY)
QUARK(title, EVC_TITLE)
QUARK(url, EVC_URL)
QUARK(adr, EVC_ADR)

QUARK(osso_master_uid, OSSO_ABOOK_VCA_OSSO_MASTER_UID)
QUARK(telepathy_presence, OSSO_ABOOK_VCA_TELEPATHY_PRESENCE)
QUARK(telepathy_capabilities, OSSO_ABOOK_VCA_TELEPATHY_CAPABILITIES)
QUARK(gender, OSSO_ABOOK_VCA_GENDER)
