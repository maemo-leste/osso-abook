#include <libebook/libebook.h>

#include "osso-abook-debug.h"
#include "eds.h"

#include "config.h"

__attribute__ ((visibility ("hidden"))) void
_osso_abook_book_view_set_freezable(EBookView *view, gboolean freezable)
{
#if 0
  EBook *book;

  g_return_if_fail(E_IS_BOOK_VIEW (view));

  book = e_book_view_get_book(view);

  if (e_book_check_static_capability(book, "freezable"))
  {
    if (freezable)
    {
      OSSO_ABOOK_NOTE(OSSO_ABOOK_DEBUG_EDS,
                      "activating freeze/thaw support on %s",
                      e_book_get_uri(book));

      g_signal_connect_after(view, "contacts-added",
                             G_CALLBACK(contacts_modified_cb), 0);
      g_signal_connect_after(view, "contacts-changed",
                             G_CALLBACK(contacts_modified_cb), 0);
      g_signal_connect_after(view, "contacts-removed",
                             G_CALLBACK(contacts_modified_cb), 0);
      e_book_view_set_freezable(view, TRUE);
    }
    else
    {
      OSSO_ABOOK_NOTE(OSSO_ABOOK_DEBUG_EDS,
                      "deactivating freeze/thaw support on %s",
                      e_book_get_uri(book));
      g_signal_handlers_disconnect_matched(
            view,
            G_SIGNAL_MATCH_DATA | G_SIGNAL_MATCH_FUNC,
            0, 0, 0, contacts_modified_cb, NULL);
      e_book_view_set_freezable(view, FALSE);
    }
  }
#else
#pragma message("FIXME!!! - upstream eds does not support e_book_view_set_freezable()")
#endif
}
