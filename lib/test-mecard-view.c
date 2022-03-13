#include "config.h"

#include <glib-object.h>
#include <glib.h>
#include <hildon/hildon.h>
#include <libedata-book/libedata-book.h>
#include <stdlib.h>

#include "osso-abook-init.h"
#include "osso-abook-mecard-view.h"

static void
loop_quit (GtkWindow *window, gpointer data)
{
  GMainLoop *loop = data;

  g_return_if_fail(loop != NULL);

  gtk_widget_hide(GTK_WIDGET(window));

  while (gtk_events_pending())
    gtk_main_iteration();

  g_main_loop_quit(loop);
}

int
main(int argc, char **argv)
{
  osso_context_t *osso_context;

  /* Initialise osso */
  osso_context = osso_initialize (PACKAGE_NAME, VERSION, TRUE, NULL);

  if (osso_context == NULL)
  {
    g_critical ("Error initializing osso");
    return 1;
  }

  hildon_gtk_init(&argc, &argv);

  setenv("OSSO_ABOOK_DEBUG", "all,disk-space", TRUE);
  osso_abook_init(&argc, &argv, osso_context);

  GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

  GtkWidget *view = osso_abook_mecard_view_new();

  g_signal_connect(view, "destroy", G_CALLBACK(loop_quit), main_loop);

  gtk_widget_show_all(view);

  g_main_loop_run(main_loop);
  g_main_loop_unref(main_loop);

  return 0;
}
