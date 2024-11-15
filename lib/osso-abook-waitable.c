/*
 * osso-abook-waitable.c
 *
 * Copyright (C) 2020 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */

/**
 * SECTION:osso-abook-waitable
 * @short_description: Generic interfaces for object with asynchronous
 * initialization.
 *
 * This interface provides a generic method for object with asynchronous
 * initialization to inform their clients that they finished initialization.
 *
 * In order to implement #OssoABookWaitable, derived classes should implement a
 * stack of opaque closures #OssoABookWaitableClosure and the virtual
 * `push()` and `pop()` methods for adding or removing closures from the stack.
 * Inherited classes should indicate that they have become ready by calling
 * osso_abook_waitable_notify().
 *
 */

#include "config.h"

#include <gdk/gdk.h>

#include "osso-abook-waitable.h"

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

/**
 * osso_abook_waitable_cancel:
 * @waitable: a #OssoABookWaitable
 * @closure: a return value from osso_abook_waitable_call_when_ready()
 *
 * Returns: %TRUE if cancel was successful, else %FALSE (for example if @closure
 * was already called or cancelled)
 */
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

/**
 * osso_abook_waitable_call_when_ready:
 * @waitable: a #OssoABookWaitable
 * @callback: a function to call when ready
 * @user_data: additional data to pass to @callback
 * @destroy: an optional function to free @user_data when no longer needed
 *
 * Registers the specified @callback to be executed when @waitable becomes
 * ready. The meaning of 'ready' is dependant on the concrete implementation
 * classes, but once a #OssoABookWaitable object becomes ready it does not
 * become 'unready' again. If @waitable is already ready when this function is
 * called, @callback will be executed immediately.
 *
 * Returns: an opaque closure that can be used later to cancel this
 * registration by calling osso_abook_waitable_cancel().
 */
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

/**
 * osso_abook_waitable_is_ready:
 * @waitable: a #OssoABookWaitable
 * @error: a return location for error reporting
 *
 * Checks whether @waitable is ready or not.
 *
 * Returns: %TRUE if @waitable is ready, else %FALSE
 */
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

/**
 * osso_abook_waitable_reset:
 * @waitable : a #OssoABookWaitable
 *
 * Resets @waitable by removing all registered callbacks without executing them.
 **/
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

/**
 * osso_abook_waitable_notify:
 * @waitable: a #OssoABookWaitable
 * @error: a parameter for indicating an error
 *
 * Derived classes will call this function in order to indicate that they have
 * transitioned to 'ready'. This function should never be called from outside
 * the implementation of a derived class (to protect agains this, this function
 * will call osso_abook_waitable_is_ready() and if it returns %FALSE, waitable
 * will not be considered ready and the registered callbacks will not be
 * executed).
 *
 * This function will return before the registered callbacks are executed. The
 * callbacks will be executed in an idle handler, and the @error parameter will
 * be passed to each callback.
 *
 */
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

/**
 * osso_abook_waitable_run:
 * @waitable: a #OssoABookWaitable
 * @context: a #GMainContext to run in
 * @error: return location for an error or %NULL on success
 *
 * This function simply spins a main loop in @context until @waitable has become
 * ready.
 */
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
