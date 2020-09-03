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

const char *
estatus_to_string(const EBookStatus status)
{
  if (status == E_BOOK_ERROR_PERMISSION_DENIED)
    return _("addr_ib_permission_denied");

  if (status == E_BOOK_ERROR_NO_SPACE)
    return _("addr_ib_disc_full");

  return _("addr_ib_system_error");
}

void
_osso_abook_handle_estatus(GtkWindow *parent, const char *strloc,
                           EBookStatus status, EBook *book)
{
  gchar *uid;
  const char *on;
  const char *msg;

  if (!status)
    return;

  if (E_IS_BOOK(book))
  {
    ESource *source = e_book_get_source(book);

    uid = e_source_dup_uid(source);
    on = " on ";

  }
  else
  {
    uid = g_strdup("");
    on = "";
  }

  g_message("%s: EBook error %d%s%s", strloc, status, on, uid);
  g_free(uid);

  msg = estatus_to_string(status);
  hildon_banner_show_information(parent ? GTK_WIDGET(parent) : NULL, 0, msg);
}
