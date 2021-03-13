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
#include <locale.h>
#include <libintl.h>
#include <string.h>

#include <glib.h>

#include "osso-abook-msgids.h"


static GHashTable *msgids_table;
static int mo_magic1 = 0x950412DE;
static int mo_magic2 = 0xDE120495;

const char *
osso_abook_msgids_locate(const char *locale, const char *domain)
{
  gchar *locale_name;
  gchar *cmp_locale_name;
  gchar *mo_file;
  char *basedir;

  if (domain)
  {
    if (!locale)
      locale = setlocale(LC_MESSAGES, NULL);
    locale_name = g_strdup(locale);
    cmp_locale_name = &locale_name[strlen(locale_name) - 1];
    mo_file = g_strconcat(domain, ".mo", NULL);
    basedir = bindtextdomain(domain, NULL);
    if (locale_name >= cmp_locale_name)
    {
      domain = NULL;
    }
    else
    {
      while (1)
      {
        domain = g_build_filename(basedir, locale_name, "LC_MESSAGES", mo_file,
                                  NULL);
        if (g_file_test(domain, G_FILE_TEST_IS_REGULAR))
          break;
        g_free(domain);
        while (!strchr("@._", *cmp_locale_name))
        {
          if (locale_name >= --cmp_locale_name)
          {
            domain = NULL;
            goto out;
          }
        }
        *cmp_locale_name-- = 0;
        if (locale_name >= cmp_locale_name)
        {
          domain = NULL;
          goto out;
        }
      }
    }
out:
    g_free(locale_name);
    g_free(mo_file);
  }
  else
  {
    g_return_if_fail(NULL != domain);
  }
  return domain;
}

static void
free_msgids_table()
{
  if (msgids_table)
  {
    g_hash_table_unref(msgids_table);
    msgids_table = NULL;
  }
}

GHashTable *
osso_abook_msgids_get_table(const char *locale, const char *domain)
{
  GHashTable *ret;
  return ret;
}

const char *
osso_abook_msgids_rfind(const char *locale, const char *domain,
                        const char *msgstr)
{
  GHashTable *hashtable;
  const char *result;

  if (domain)
  {
    if (msgstr)
    {
      hashtable = osso_abook_msgids_get_table(locale, domain);
      if (hashtable)
        result = g_hash_table_lookup(hashtable, msgstr);
      else
        result = NULL;
    }
    else
    {
      g_return_if_fail(NULL != msgstr);
      result = msgstr;
    }
  }
  else
  {
    g_return_if_fail(NULL != domain);
    result = 0;
  }
  return result;
}
