#include <string.h>

#include "osso-abook-debug.h"

guint _osso_abook_debug_flags = 0;

static GDebugKey debug_keys[] =
{
  {"eds", OSSO_ABOOK_DEBUG_EDS},
  {"gtk", OSSO_ABOOK_DEBUG_GTK},
  {"hildon", OSSO_ABOOK_DEBUG_HILDON},
  {"contact-add", OSSO_ABOOK_DEBUG_CONTACT_ADD},
  {"contact-remove", OSSO_ABOOK_DEBUG_CONTACT_REMOVE},
  {"contact-update", OSSO_ABOOK_DEBUG_CONTACT_UPDATE},
  {"vcard", OSSO_ABOOK_DEBUG_VCARD},
  {"avatar", OSSO_ABOOK_DEBUG_AVATAR},
  {"mc", OSSO_ABOOK_DEBUG_MC},
  {"caps", OSSO_ABOOK_DEBUG_CAPS},
  {"list-store", OSSO_ABOOK_DEBUG_LIST_STORE},
  {"startup", OSSO_ABOOK_DEBUG_STARTUP},
  {"dbus", OSSO_ABOOK_DEBUG_DBUS},
  {"aggregator", OSSO_ABOOK_DEBUG_AGGREGATOR},
  {"i18n", OSSO_ABOOK_DEBUG_I18N},
  {"sim", OSSO_ABOOK_DEBUG_SIM},
  {"editor", OSSO_ABOOK_DEBUG_EDITOR },
  {"tree-view", OSSO_ABOOK_DEBUG_TREE_VIEW },
  {"disk-space", OSSO_ABOOK_DEBUG_DISK_SPACE},
  {"todo", OSSO_ABOOK_DEBUG_TODO},
  {"all", OSSO_ABOOK_DEBUG_ALL & ~OSSO_ABOOK_DEBUG_DISK_SPACE}
};

static GTimer *_osso_debug_timer = NULL;
static GQuark _osso_debug_timer_name;
static GQuark _osso_debug_timer_domain;

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
