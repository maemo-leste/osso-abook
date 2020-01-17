#include "config.h"

#include "osso-abook-waitable.h"

typedef OssoABookWaitableIface OssoABookWaitableInterface;

struct _OssoABookWaitableClosure
{
  OssoABookWaitableCallback callback;
  gpointer user_data;
  GDestroyNotify destroy;
};


G_DEFINE_INTERFACE(
    OssoABookWaitable,
    osso_abook_waitable,
    G_TYPE_INVALID
);

static void
osso_abook_waitable_default_init(OssoABookWaitableIface *iface)
{
}

static void
osso_abook_waitable_destroy_closure(OssoABookWaitableClosure *closure)
{
  if (closure->destroy)
    closure->destroy(closure->user_data);

  g_slice_free(OssoABookWaitableClosure, closure);
}

gboolean
osso_abook_waitable_cancel(OssoABookWaitable *waitable,
                           OssoABookWaitableClosure *closure)
{
  OssoABookWaitableIface *iface;

  g_return_val_if_fail(OSSO_ABOOK_IS_WAITABLE (waitable), FALSE);
  g_return_val_if_fail(closure != NULL, FALSE);

  iface = OSSO_ABOOK_WAITABLE_GET_IFACE(waitable);

  g_return_val_if_fail(iface->pop != NULL, FALSE);

  if (!iface->pop(waitable, closure))
    return FALSE;

  osso_abook_waitable_destroy_closure(closure);

  return TRUE;
}

OssoABookWaitableClosure *
osso_abook_waitable_call_when_ready(OssoABookWaitable *waitable,
                                    OssoABookWaitableCallback callback,
                                    gpointer user_data, GDestroyNotify destroy)
{
  OssoABookWaitableIface *iface;
  OssoABookWaitableClosure *closure;
  const GError *error = NULL;


  g_return_val_if_fail(OSSO_ABOOK_IS_WAITABLE (waitable), NULL);
  g_return_val_if_fail(callback != NULL, NULL);

  iface = OSSO_ABOOK_WAITABLE_GET_IFACE(waitable);

  g_return_val_if_fail(iface->is_ready != NULL, NULL);

  if (iface->is_ready(waitable, &error))
  {
    callback(waitable, error, user_data);
    return FALSE;
  }

  g_return_val_if_fail(iface->push != NULL, NULL);
  g_return_val_if_fail(iface->pop != NULL, NULL);

  closure = g_slice_new0(OssoABookWaitableClosure);
  closure->callback = callback;
  closure->user_data = user_data;
  closure->destroy = destroy;
  iface->push(waitable, closure);

  return closure;
}

gboolean
osso_abook_waitable_is_ready(OssoABookWaitable *waitable, GError **error)
{
  OssoABookWaitableIface *iface;
  const GError *err = NULL;
  gboolean rv;

  g_return_val_if_fail(OSSO_ABOOK_IS_WAITABLE (waitable), FALSE);

  iface = OSSO_ABOOK_WAITABLE_GET_IFACE(waitable);

  g_return_val_if_fail(iface->is_ready != NULL, FALSE);

  rv = iface->is_ready(waitable, &err);

  if (error)
  {
    g_return_val_if_fail(*error == NULL, rv);

    if (err)
      *error = g_error_copy(err);
  }

  return rv;
}
