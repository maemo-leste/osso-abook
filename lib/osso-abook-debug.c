#include <string.h>

#include "osso-abook-debug.h"

#define PHOTO_ID "PHOTO"

guint _osso_abook_debug_flags = 0;

static GDebugKey debug_keys[] =
{
  { "eds", OSSO_ABOOK_DEBUG_EDS },
  { "gtk", OSSO_ABOOK_DEBUG_GTK },
  { "hildon", OSSO_ABOOK_DEBUG_HILDON },
  { "contact-add", OSSO_ABOOK_DEBUG_CONTACT_ADD },
  { "contact-remove", OSSO_ABOOK_DEBUG_CONTACT_REMOVE },
  { "contact-update", OSSO_ABOOK_DEBUG_CONTACT_UPDATE },
  { "vcard", OSSO_ABOOK_DEBUG_VCARD },
  { "avatar", OSSO_ABOOK_DEBUG_AVATAR },
  { "tp", OSSO_ABOOK_DEBUG_TP },
  { "caps", OSSO_ABOOK_DEBUG_CAPS },
  { "list-store", OSSO_ABOOK_DEBUG_LIST_STORE },
  { "startup", OSSO_ABOOK_DEBUG_STARTUP },
  { "dbus", OSSO_ABOOK_DEBUG_DBUS },
  { "aggregator", OSSO_ABOOK_DEBUG_AGGREGATOR },
  { "i18n", OSSO_ABOOK_DEBUG_I18N },
  { "sim", OSSO_ABOOK_DEBUG_SIM },
  { "editor", OSSO_ABOOK_DEBUG_EDITOR },
  { "tree-view", OSSO_ABOOK_DEBUG_TREE_VIEW },
  { "disk-space", OSSO_ABOOK_DEBUG_DISK_SPACE },
  { "todo", OSSO_ABOOK_DEBUG_TODO }
};

static GTimer *_osso_debug_timer = NULL;
static GQuark _osso_debug_timer_name;
static GQuark _osso_debug_timer_domain;

double
_osso_abook_debug_timestamp()
{
  return g_timer_elapsed(_osso_debug_timer, NULL);
}

void
osso_abook_debug_init(void)
{
  const gchar *env_debug;

  if (_osso_debug_timer)
    return;

  env_debug = g_getenv("OSSO_ABOOK_DEBUG");
  _osso_debug_timer_name = g_quark_from_static_string("osso-debug-timer-name");
  _osso_debug_timer_domain =
    g_quark_from_static_string("osso-debug-timer-domain");
  _osso_debug_timer = g_timer_new();

  if (env_debug)
  {
    gchar *debug_string = g_strstrip(g_strdup(env_debug));

    if (!*debug_string || !strcmp(debug_string, "none"))
      _osso_abook_debug_flags = 0;
    else
    {
      _osso_abook_debug_flags = g_parse_debug_string(debug_string, debug_keys,
                                                     G_N_ELEMENTS(debug_keys));
    }

    g_free(debug_string);
  }
}

void
_osso_abook_log(const char *domain, const char *strloc, const char *strfunc,
                const char *strtype, OssoABookDebugFlags type, const char *format, ...)
{
  gchar *msg;
  const char *debug_type;
  gchar *log_domain;
  double ts;
  va_list va;

  va_start(va, format);
  msg = g_strdup_vprintf(format, va);

  if (type == OSSO_ABOOK_DEBUG_ALL)
    debug_type = NULL;
  else
    debug_type = strtype;

  log_domain = g_strconcat(domain, debug_type, NULL);

  ts = _osso_abook_debug_timestamp();

  g_log(log_domain, G_LOG_LEVEL_DEBUG, "%.3f s:\n---- %s(%s):\n---- %s",
        ts, strfunc, strloc, msg);
  g_free(log_domain);
  g_free(msg);
}

void
_osso_abook_dump_vcard_string(const char *domain, const char *strloc,
                              const char *strfunc, const char *strtype,
                              OssoABookDebugFlags type, const char *note,
                              const char *vcard)
{
  gchar *msg;

  if (vcard)
  {
    gchar *chunk = msg = g_strescape(vcard, NULL);
    gchar *p;

    while ((p = strstr(chunk, PHOTO_ID)))
    {
      p += sizeof(PHOTO_ID) - 1;

      chunk = strstr(p, "\\n");

      if (!chunk)
        chunk = p + strlen(p);

      if (strchr(":;,", *p))
      {
        while (*p && *p != ':')
          p++;

        if ((chunk > p) && (*p == ':'))
        {
          do
          {
            p++;
          }
          while (*p == '\t' || *p == ' ');

          if (chunk - p > 30)
          {
            memcpy(p + 10, "[...]", 5);
            memmove(p + 15, chunk - 10, strlen(chunk) + 11);
          }
        }
      }
    }
  }
  else
    msg = g_strdup("<none>");

  if (!note)
    note = "vcard";

  _osso_abook_log(domain, strloc, strfunc, strtype, type, "%s: %s", note, msg);

  g_free(msg);
}

void
_osso_abook_dump_vcard(const char *domain, const char *strloc,
                       const char *strfunc, const char *strtype,
                       OssoABookDebugFlags type, const char *note,
                       EVCard *vcard)
{
  gchar *vcard_string = e_vcard_to_string(vcard, EVC_FORMAT_VCARD_30);

  _osso_abook_dump_vcard_string(domain, strloc, strfunc, strtype, type, note,
                                vcard_string);
  g_free(vcard_string);
}

void
_osso_abook_timer_start(const char *domain, const char *strfunc,
                        const char *strtype, OssoABookDebugFlags type,
                        GTimer *timer, const char *name)
{
  gchar *timer_name;
  GDestroyNotify destroy;

  if (!timer)
    return;

  if (!(type & _osso_abook_debug_flags))
    return;

  if (type == OSSO_ABOOK_DEBUG_ALL)
    strtype = NULL;

  if (!name)
    name = strfunc;

  g_dataset_id_set_data_full(timer, _osso_debug_timer_domain, g_strdup(name),
                             (GDestroyNotify)&g_free);

  timer_name = g_strconcat(domain, strtype, NULL);

  if (timer_name)
    destroy = g_free;
  else
    destroy = NULL;

  g_dataset_id_set_data_full(
    timer, _osso_debug_timer_name, timer_name, destroy);
}

void
_osso_abook_timer_mark(const char *strloc, const char *strfunc, GTimer *timer)
{
  if (timer)
  {
    const gchar *domain = g_dataset_id_get_data(timer,
                                                _osso_debug_timer_domain);

    if (domain)
    {
      const gchar *name = g_dataset_id_get_data(timer, _osso_debug_timer_name);

      g_log(name, G_LOG_LEVEL_DEBUG,
            "%.3f s:\n---- %s(%s):\n---- %s: %.3f s elapsed",
            _osso_abook_debug_timestamp(), strfunc, strloc, domain,
            g_timer_elapsed(timer, NULL));
    }
  }
}
