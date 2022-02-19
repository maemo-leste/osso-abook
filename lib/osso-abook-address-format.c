/*
 * osso-abook-address-format.c
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

#include <glib.h>

#include "osso-abook-address-format.h"
#include "osso-abook-log.h"

#define FALLBACK_FORMAT ", [%E|%S|PB %B| [%P|%L]|%R|%C]"
#define FORMAT_FILE "/usr/share/libosso-abook/address_formats"

static char *format = NULL;

/*
 * fmg - does more or less the same what original code was doing, LMK if it
 * misbehaves
 */
static gchar *
format_address(OssoABookAddress *address, const char *fmt)
{
  const char *start;
  const char *end;
  const char *last_sep = NULL;
  gchar *sep;
  GString *formatted_address;
  int bracket_count = 1;

  g_return_val_if_fail(address, NULL);
  g_return_val_if_fail(fmt, NULL);

  start = strchr(fmt, '[');

  if (!start)
  {
    g_warning("Group did not start with [ in: '%s'", fmt);
    return NULL;
  }

  if (start[strlen(start) - 1] != ']')
  {
    g_warning("Group did not end with ] in: '%s'", fmt);
    return NULL;
  }

  sep = g_strndup(fmt, start - fmt);
  formatted_address = g_string_new("");
  start++;
  end = start;

  do
  {
    gchar *token = NULL;

    while (*end)
    {
      char c = *end;

      if ((c == '[') || (c == ']') || (c == '|') || (c == '%'))
        break;

      end++;
    }

    if (end - start && (*end != '['))
    {
      token = g_strndup(start, end - start);

      g_string_append(formatted_address, token);
      g_free(token);
      token = NULL;
    }
    else
    {
      switch (*end)
      {
        case '[':
        {
          end = strchr(end, ']');

          if (end)
          {
            gchar *group;

            if (last_sep)
            {
              group = g_strndup(last_sep + 1, end - last_sep);
              last_sep = NULL;
            }
            else
              group = g_strndup(start, end - start + 1);

            token = format_address(address, group);
            g_free(group);
          }

          break;
        }
        case ']':
        {
          token = g_strndup(start, end - start);
          bracket_count--;

          break;
        }
        case '|':
        {
          token = g_strdup(sep);
          last_sep = end;
          break;
        }
        case '%':
        {
          char *data = "";

          switch (*(end + 1))
          {
            case 'B':
            {
              data = address->p_o_box;
              break;
            }
            case 'C':
            {
              data = address->country;
              break;
            }
            case 'E':
            {
              data = address->extension;
              break;
            }
            case 'L':
            {
              data = address->city;
              break;
            }
            case 'P':
            {
              data = address->postal;
              break;
            }
            case 'R':
            {
              data = address->region;
              break;
            }
            case 'S':
            {
              data = address->street;
              break;
            }
            default:
            {
              g_warning("Unsupported format '%c'",
                        g_utf8_get_char(g_utf8_next_char(end)));
            }
          }

          token = g_strdup_printf("%s", data);
          end++;
          last_sep = end;
        }
      }

      end++;
    }

    if (token)
    {
      g_string_append(formatted_address, token);
      g_free(token);
      token = NULL;
    }

    start = end;
  }
  while (*start);

  g_free(sep);

  if (bracket_count < 0)
    g_warning("unmatched ]");

  if (formatted_address->len)
    return g_string_free(formatted_address, FALSE);

  g_string_free(formatted_address, TRUE);

  return NULL;
}

char *
osso_abook_address_format(OssoABookAddress *address)
{
  char *formated_address;
  gchar *tmp_format = NULL;

  if (!format)
  {
    GError *error = NULL;
    GKeyFile *key_file = g_key_file_new();

    if (g_key_file_load_from_file(
          key_file, FORMAT_FILE, G_KEY_FILE_NONE, &error))
    {
      tmp_format = g_key_file_get_locale_string(key_file, "Address Formats",
                                                "format", NULL, &error);

      if (error)
        OSSO_ABOOK_WARN("Could not get address format: %s", error->message);

      g_clear_error(&error);
    }
    else
    {
      OSSO_ABOOK_WARN("Could not open address format file '%s': %s",
                      FORMAT_FILE, error ? error->message : "Unknown error");
      g_clear_error(&error);
    }

    g_key_file_free(key_file);

    if (g_utf8_validate(tmp_format, -1, NULL))
    {
      if (!tmp_format)
        tmp_format = FALLBACK_FORMAT;
    }
    else
    {
      g_free(tmp_format);
      tmp_format = FALLBACK_FORMAT;
    }

    format = tmp_format;
  }

  formated_address = format_address(address, format);

  if (!formated_address)
  {
    format = FALLBACK_FORMAT;
    formated_address = format_address(address, FALLBACK_FORMAT);
  }

  return formated_address;
}
