#include <hildon/hildon.h>

#include <libintl.h>

#include "eds.h"
#include "osso-abook-debug.h"
#include "osso-abook-init.h"

#include "config.h"

static osso_context_t *osso_abook_osso_context = NULL;

static gboolean
_osso_abook_init_with_args(int *argc, char ***argv,
                           osso_context_t *osso_context, char *parameter_string,
                           GOptionEntry *entries,
                           char *translation_domain, GError **error)
{
  gboolean rv;

  bindtextdomain(GETTEXT_PACKAGE, "/usr/share/locale");
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");
  osso_abook_osso_context = osso_context;

#if !GLIB_CHECK_VERSION(2, 31, 0)

  if (!g_thread_supported())
    g_thread_init(NULL);

#endif

  osso_abook_debug_init();
  gtk_rc_add_default_file("/usr/share/libosso-abook/gtkrc.libosso-abook");
  rv = gtk_init_with_args(argc, argv, parameter_string, entries,
                          translation_domain, error);

  if (rv)
  {
    if (gtk_icon_size_from_name("hildon-finger") == GTK_ICON_SIZE_INVALID)
      hildon_init();
  }

  return rv;
}

gboolean
osso_abook_init(int *argc, char ***argv, osso_context_t *osso_context)
{
  g_return_val_if_fail(argc != NULL, FALSE);
  g_return_val_if_fail(argv != NULL, FALSE);

  return _osso_abook_init_with_args(argc, argv, osso_context, 0, 0, 0, 0);
}

gboolean
osso_abook_init_with_args(int *argc, char ***argv, osso_context_t *osso_context,
                          char *parameter_string, GOptionEntry *entries,
                          char *translation_domain, GError **error)
{
  g_return_val_if_fail(argc != NULL, FALSE);
  g_return_val_if_fail(argv != NULL, FALSE);

  return _osso_abook_init_with_args(argc, argv, osso_context, parameter_string,
                                    entries, translation_domain, error);
}

gboolean
osso_abook_init_with_name(const char *name, osso_context_t *osso_context)
{
  char **argv = (char **)&name;

  g_return_val_if_fail(name != NULL, FALSE);

  return _osso_abook_init_with_args(
    0, &argv, osso_context, NULL, NULL, NULL, NULL);
}

osso_context_t *
osso_abook_get_osso_context()
{
  g_warn_if_fail(NULL != osso_abook_osso_context);

  return osso_abook_osso_context;
}

void
osso_abook_make_resident()
{
#pragma message("FIXME!!! - get lib name from config.h")
  GModule *module = g_module_open("/usr/lib/libosso-abook-1.0.so.0", 0);

  if (!module)
  {
    g_critical("%s: g_module_open() failed: %s",
               __FUNCTION__, g_module_error());
  }
  else
    g_module_make_resident(module);
}

void
osso_abook_set_backend_died_func(OssoABookBackendDiedFunc func,
                                 gpointer user_data)
{
  osso_ebook_set_backend_died_func(func, user_data);
}
