/*
 * osso-abook-msgids.c
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

#include <libintl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <libxml/xmlreader.h>

#include "osso-abook-msgids.h"

#define ISO_CODES_DIR "/share/xml/iso-codes"
#define ISO_3166_XML_PATH ISO_CODES_PREFIX ISO_CODES_DIR "/iso_3166.xml"

struct _mo_file_header
{
  guint32 magic;
  guint32 revision;
  guint32 nstrings;
  guint32 orig_tab_offset;
  guint32 trans_tab_offset;
};
typedef struct _mo_file_header mo_file_header;

struct _mo_string_desc
{
  guint32 length;
  guint32 offset;
};
typedef struct _mo_string_desc mo_string_desc;

static GHashTable *msgids_table = NULL;
static const char *mo_magic1 = "\xDE\x12\x04\x95";
static const char *mo_magic2 = "\x95\x04\x12\xDE";

gchar *
osso_abook_msgids_locate(const char *locale, const char *domain)
{
  gchar *loc;
  gchar *p;
  gchar *mo_file;
  gchar *path = NULL;
  char *loc_dir;

  g_return_val_if_fail(NULL != domain, NULL);

  if (!locale)
    locale = setlocale(LC_MESSAGES, NULL);

  loc = g_strdup(locale);
  p = &loc[strlen(loc) - 1];
  mo_file = g_strconcat(domain, ".mo", NULL);
  loc_dir = bindtextdomain(domain, NULL);

  while (p > loc)
  {
    path = g_build_filename(loc_dir, loc, "LC_MESSAGES", mo_file, NULL);

    if (g_file_test(path, G_FILE_TEST_IS_REGULAR))
      break;

    g_free(path);
    path = NULL;

    while (!strchr("@._", *p) && (p-- > loc))
      ;

    if (p >= loc)
      *p-- = 0;
  }

  g_free(loc);
  g_free(mo_file);

  return path;
}

__attribute__((destructor)) static void
osso_abook_msgids_at_exit(void)
{
  if (msgids_table)
  {
    g_hash_table_unref(msgids_table);
    msgids_table = NULL;
  }
}

static GHashTable *
osso_abook_msgids_get_table(const char *locale, const char *domain)
{
  gchar *mo_file_path;
  GHashTable *mo_strings = NULL;
  FILE *fp;
  guint32 i;
  mo_file_header mo_hdr;

  g_return_val_if_fail(NULL != domain, NULL);

  mo_file_path = osso_abook_msgids_locate(locale, domain);

  if (!mo_file_path)
    return NULL;

  if (msgids_table)
    mo_strings = g_hash_table_lookup(msgids_table, mo_file_path);

  if (mo_strings)
  {
    g_free(mo_file_path);
    return mo_strings;
  }

  fp = fopen(mo_file_path, "rb");

  if (fp)
  {
    if ((fread(&mo_hdr, sizeof(mo_hdr), 1, fp) == 1) &&
        (!memcmp(&mo_hdr.magic, mo_magic1, sizeof(mo_hdr.magic)) ||
         !memcmp(&mo_hdr.magic, mo_magic2, sizeof(mo_hdr.magic))) &&
        (fseek(fp, mo_hdr.orig_tab_offset, SEEK_SET) != -1))
    {
      mo_string_desc *orig_tab = g_newa(mo_string_desc, mo_hdr.nstrings);
      mo_string_desc *trans_tab = g_newa(mo_string_desc, mo_hdr.nstrings);
      guint32 ns = mo_hdr.nstrings;

      if ((fread(orig_tab, sizeof(orig_tab[0]), ns, fp) == ns) &&
          (fseek(fp, mo_hdr.trans_tab_offset, 0) != -1) &&
          (fread(trans_tab, sizeof(trans_tab[0]), ns, fp) == ns))
      {
        if (!msgids_table)
        {
          msgids_table = g_hash_table_new_full(
            g_str_hash, g_str_equal, g_free,
            (GDestroyNotify)g_hash_table_unref);
        }

        mo_strings = g_hash_table_new_full(
          g_str_hash, g_str_equal, g_free, (GDestroyNotify)g_free);

        g_hash_table_insert(msgids_table, g_strdup(mo_file_path), mo_strings);

        for (i = 0; i < mo_hdr.nstrings; i++)
        {
          if (orig_tab[i].length)
          {
            guint32 orig_len = orig_tab[i].length;
            guint32 trans_len = trans_tab[i].length;
            gchar *orig = g_malloc(orig_len + 1);
            gchar *trans = g_malloc(trans_len + 1);

            if ((fseek(fp, orig_tab[i].offset, SEEK_SET) != -1) &&
                (fread(orig, 1, orig_len, fp) == orig_len) &&
                (fseek(fp, trans_tab[i].offset, SEEK_SET) != -1) &&
                (fread(trans, 1, trans_len, fp) == trans_len))
            {
              orig[orig_len] = 0;
              trans[trans_len] = 0;
              g_hash_table_insert(mo_strings, trans, orig);
            }
            else
            {
              g_free(orig);
              g_free(trans);
            }
          }
        }
      }
    }

    fclose(fp);
  }

  g_free(mo_file_path);

  return mo_strings;
}

const char *
osso_abook_msgids_rfind(const char *locale, const char *domain,
                        const char *msgstr)
{
  GHashTable *hashtable;

  g_return_val_if_fail(NULL != domain, NULL);
  g_return_val_if_fail(NULL != msgstr, NULL);

  hashtable = osso_abook_msgids_get_table(locale, domain);

  if (hashtable)
    return g_hash_table_lookup(hashtable, msgstr);

  return NULL;
}

static void
process_iso_3166_node(xmlTextReaderPtr reader, GHashTable *iso_3166)
{
  const xmlChar *name = xmlTextReaderConstName(reader);

  if (!name || xmlStrcmp(name, BAD_CAST "iso_3166_entry"))
    return;

  while (xmlTextReaderMoveToNextAttribute(reader) == 1)
  {
    if (!xmlStrcmp(xmlTextReaderConstName(reader), BAD_CAST "name"))
    {
      g_hash_table_insert(
        iso_3166,
        g_strdup((const gchar *)xmlTextReaderConstValue(reader)),
        GINT_TO_POINTER(1));
      break;
    }
  }
}

static gboolean
parse_iso_3166(GHashTable *iso_3166)
{
  xmlTextReaderPtr reader;
  int ret;

  reader = xmlReaderForFile(ISO_3166_XML_PATH, NULL, 0);

  g_return_val_if_fail(reader != NULL, FALSE);

  while ((ret = xmlTextReaderRead(reader)) == 1)
    process_iso_3166_node(reader, iso_3166);

  xmlFreeTextReader(reader);

  g_return_val_if_fail(ret == 0, FALSE);

  return TRUE;
}

struct _country_name
{
  const gchar *name;
  gchar *name_coll;
};

typedef struct _country_name country_name;

static int
country_name_cmp(country_name *n1, country_name *n2)
{
  return strcmp(n1->name_coll, n2->name_coll);
}

static void
sort_countries(country_name *names, size_t count)
{
  qsort(names, count, sizeof(*names), (__compar_fn_t)country_name_cmp);
}

GStrv
osso_abook_msgids_get_countries()
{
  GHashTable *iso_3166 =
    g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
  country_name *names;
  country_name *name;
  const gchar *msgid;
  GHashTableIter iter;
  GStrv countries;
  int num;
  int i;

  if (!parse_iso_3166(iso_3166))
  {
    g_hash_table_destroy(iso_3166);
    return NULL;
  }

  num = g_hash_table_size(iso_3166);
  names = g_new0(country_name, num);
  name = names;

  g_hash_table_iter_init(&iter, iso_3166);

  while (g_hash_table_iter_next(&iter, (gpointer *)&msgid, NULL))
  {
    name->name = g_dgettext("iso_3166", msgid);
    name->name_coll = g_utf8_collate_key(name->name, -1);
    name++;
  }

  sort_countries(names, num);
  countries = g_new(gchar *, num + 1);

  for (i = 0; i < num; i++)
  {
    countries[i] = g_strdup(names[i].name);
    g_free(names[i].name_coll);
  }

  g_free(names);
  countries[i] = NULL;

  g_hash_table_destroy(iso_3166);

  return countries;
}
