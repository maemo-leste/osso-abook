#ifndef __EDS_H_INCLUDED__
#define __EDS_H_INCLUDED__

#include "osso-abook-init.h"

void _osso_abook_book_view_set_freezable(EBookView *view, gboolean freezable);
void osso_ebook_set_backend_died_func(OssoABookBackendDiedFunc func, gpointer user_data);
typedef void (*GetBookViewCb)(EBook *book, EBookStatus status, EBookView *book_view, gpointer user_data);
void _osso_abook_async_get_book_view(EBook *book, EBookQuery *query,
                                     GList *requested_fields, int max_results,
                                     GetBookViewCb callback, gpointer user_data,
                                     GDestroyNotify destroy_notify);

#endif /* __EDS_H_INCLUDED__ */
