/*
 * osso-abook-msgids.h
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

#ifndef OSSOABOOKMSGIDS_H
#define OSSOABOOKMSGIDS_H

const char *
osso_abook_msgids_rfind(const char *locale, const char *domain,
                        const char *msgstr);

GStrv
osso_abook_msgids_get_countries();

#endif // OSSOABOOKMSGIDS_H
