/*
 * avatar.h
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

#ifndef AVATAR_H
#define AVATAR_H

typedef struct _avatar_data avatar_data;

gboolean
_osso_abook_avatar_read_file(GFile *file, char **contents, gsize *length,
                             GError **error);

EContactPhoto *
_osso_abook_avatar_data_to_photo(avatar_data *avatar);

avatar_data *
_osso_abook_avatar_data_new_from_contact(EContact *ec, gboolean only_uri);

avatar_data *
_osso_abook_avatar_data_new_from_uri(const char *uri);

void
_osso_abook_avatar_data_free(avatar_data *avatar);

avatar_data *
_osso_abook_avatar_data_new();

#endif // AVATAR_H
