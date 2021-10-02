#include <gdk/gdk.h>

#include "osso-abook-waitable.h"

#include "config.h"

struct waitable_notify_data
{
  gpointer waitable;
  GError *error;
};

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
{}

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

  g_return_val_if_fail(OSSO_ABOOK_IS_WAITABLE(waitable), FALSE);
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

  g_return_val_if_fail(OSSO_ABOOK_IS_WAITABLE(waitable), NULL);
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

  g_return_val_if_fail(OSSO_ABOOK_IS_WAITABLE(waitable), FALSE);

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

static void
osso_abook_waitable_for_each(OssoABookWaitable *waitable, gboolean call_closure,
                             const GError *error)
{
  OssoABookWaitableIface *iface = OSSO_ABOOK_WAITABLE_GET_IFACE(waitable);
  OssoABookWaitableClosure *closure;

  g_return_if_fail(iface->pop != NULL);

  while ((closure = iface->pop(waitable, NULL)))
  {
    if (call_closure)
      closure->callback(waitable, error, closure->user_data);

    osso_abook_waitable_destroy_closure(closure);
  }
}

void
osso_abook_waitable_reset(OssoABookWaitable *waitable)
{
  g_return_if_fail(OSSO_ABOOK_IS_WAITABLE(waitable));

  osso_abook_waitable_for_each(waitable, FALSE, NULL);
}

static gboolean
waitable_notify_cb(gpointer user_data)
{
  struct waitable_notify_data *data = user_data;

  osso_abook_waitable_for_each(data->waitable, TRUE, data->error);
  g_object_unref(data->waitable);

  if (data->error)
    g_error_free(data->error);

  g_slice_free(struct waitable_notify_data, data);

  return FALSE;
}

void
osso_abook_waitable_notify(OssoABookWaitable *waitable, const GError *error)
{
  g_return_if_fail(OSSO_ABOOK_IS_WAITABLE(waitable));

  if (osso_abook_waitable_is_ready(waitable, NULL))
  {
    struct waitable_notify_data *data =
      g_slice_new0(struct waitable_notify_data);

    data->waitable = g_object_ref(waitable);

    if (error)
      data->error = g_error_copy(error);

    gdk_threads_add_idle(waitable_notify_cb, data);
  }
}

typedef struct
{
  GMainLoop *loop;
  GError **error;
} WaitableRunClosure;

static void
ready_cb(OssoABookWaitable *waitable, const GError *error, gpointer data)
{
  WaitableRunClosure *closure = data;

  if (error && closure->error)
    *closure->error = g_error_copy(error);

  g_main_loop_quit(closure->loop);
}

void
osso_abook_waitable_run(OssoABookWaitable *waitable, GMainContext *context,
                        GError **error)
{
  WaitableRunClosure closure;

  g_return_if_fail(OSSO_ABOOK_IS_WAITABLE(waitable));

  closure.error = error;
  closure.loop = g_main_loop_new(context, TRUE);

  osso_abook_waitable_call_when_ready(waitable, ready_cb, &closure, NULL);

  if (g_main_loop_is_running(closure.loop))
  {
    GDK_THREADS_LEAVE();
    g_main_loop_run(closure.loop);
    GDK_THREADS_ENTER();
  }

  g_main_loop_unref(closure.loop);
}
