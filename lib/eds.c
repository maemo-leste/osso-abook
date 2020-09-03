#include <libebook/libebook.h>

#include "osso-abook-debug.h"
#include "eds.h"

#include "config.h"

static OssoABookBackendDiedFunc _backend_died_func = NULL;
static gpointer _backend_died_user_data = NULL;

void
osso_ebook_set_backend_died_func(OssoABookBackendDiedFunc func,
                                 gpointer user_data)
{
  _backend_died_func = func;
  _backend_died_user_data = user_data;
}

void
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
      OSSO_ABOOK_NOTE(EDS, "activating freeze/thaw support on %s",
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
      OSSO_ABOOK_NOTE(EDS, "deactivating freeze/thaw support on %s",
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

typedef struct
{
  EBook *book;
  EBookQuery *query;
  GList *requested_fields;
  int max_results;
  GetBookViewCb callback;
  gpointer user_data;
  GDestroyNotify destroy_notify;
} GetBookViewClosure;

static void
get_book_view_cb(EBook *book, EBookStatus status, EBookView *book_view,
                 gpointer user_data)
{
  GetBookViewClosure *closure = user_data;

  closure->callback(book, status, book_view, closure->user_data);
  g_list_free(closure->requested_fields);
  e_book_query_unref(closure->query);
  g_object_unref(closure->book);

  if (closure->destroy_notify)
    closure->destroy_notify(closure->user_data);

  g_slice_free(GetBookViewClosure, closure);
}

static void
backend_died_cb(EBook *book)
{
  if (!_backend_died_func || !_backend_died_func(book, _backend_died_user_data))
  {
    ESource *source = e_book_get_source(book);

    g_critical("%s: Backend of %s died. Prepare for trouble.",
               __FUNCTION__, e_source_get_uid(source));
  }
}

static void
book_open_cb(EBook *book, EBookStatus status, gpointer user_data)
{
  GetBookViewClosure *closure = user_data;

  if (status == E_BOOK_ERROR_OK)
  {
    gboolean res;

    g_signal_connect(book, "backend-died",
                     G_CALLBACK(backend_died_cb), closure);
    res = e_book_async_get_book_view(book, closure->query,
                                     closure->requested_fields,
                                     closure->max_results, get_book_view_cb,
                                     closure);

    if (res == FALSE)
      status = E_BOOK_ERROR_INVALID_ARG;
  }

  if (status != E_BOOK_ERROR_OK)
    get_book_view_cb(book, status, NULL, closure);
}

void
_osso_abook_async_get_book_view(EBook *book, EBookQuery *query,
                                GList *requested_fields, int max_results,
                                GetBookViewCb callback, gpointer user_data,
                                GDestroyNotify destroy_notify)
{
  GetBookViewClosure *closure;

  g_return_if_fail(E_IS_BOOK(book));
  g_return_if_fail(NULL != callback);

  closure = g_slice_new0(GetBookViewClosure);
  closure->book = g_object_ref(book);
  closure->max_results = max_results;
  closure->callback = callback;
  closure->user_data = user_data;
  closure->destroy_notify = destroy_notify;
  closure->requested_fields = g_list_copy(requested_fields);

  if (query)
    closure->query = e_book_query_ref(query);
  else
    closure->query = e_book_query_any_field_contains("");

  if (e_book_is_opened(book))
    book_open_cb(book, E_BOOK_ERROR_OK, closure);
  else if (!e_book_async_open(book, FALSE, book_open_cb, closure))
    book_open_cb(book, E_BOOK_ERROR_INVALID_ARG, closure);
}
