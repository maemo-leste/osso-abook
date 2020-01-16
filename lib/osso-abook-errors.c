#include <hildon/hildon.h>

#include "osso-abook-errors.h"

#include "config.h"

GQuark
osso_abook_error_quark()
{
  static GQuark _osso_abook_error_quark = 0;

  if (!_osso_abook_error_quark)
  {
    _osso_abook_error_quark =
        g_quark_from_static_string("osso-abook-error-quark");
  }

  return _osso_abook_error_quark;
}

void
_osso_abook_handle_gerror(GtkWindow *parent, const char *strloc, GError *error)
{
  const char *banner_text;
  GtkWidget *widget = NULL;

  if (!error)
    return;

  g_warning("%s: GError (%s) %d: %s", strloc, g_quark_to_string(error->domain),
            error->code, error->message);

  if (error->domain == GDK_PIXBUF_ERROR)
  {
    if (error->code == GDK_PIXBUF_ERROR_INSUFFICIENT_MEMORY)
      banner_text = _("addr_ib_not_enough_memory");
    else
      banner_text = _("addr_ib_cannot_load_picture");
  }
  else if (error->domain == OSSO_ABOOK_ERROR)
    banner_text = error->message;
  else
  {
    if (g_error_matches(error, G_FILE_ERROR, G_FILE_ERROR_NOSPC))
      banner_text = _("addr_ib_disc_full");
    else
      banner_text = _("addr_ib_system_error");
  }

  if (parent)
    widget = GTK_WIDGET(parent);

  hildon_banner_show_information(widget, NULL, banner_text);
  g_error_free(error);
}
