/*
 * osso-abook-entry.h
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

#ifndef __OSSO_ABOOK_ENTRY_H_INCLUDED__
#define __OSSO_ABOOK_ENTRY_H_INCLUDED__

void _osso_abook_entry_set_error_style(GtkEntry *entry);
void _osso_abook_entry_add_icon(GtkEntry *entry, gchar *icon_name);

#endif /* __OSSO_ABOOK_ENTRY_H_INCLUDED__ */
