#include "config.h"

#include <glib-object.h>
#include <glib.h>
#include <hildon/hildon.h>
#include <libedata-book/libedata-book.h>
#include <stdlib.h>

#include "osso-abook-debug.h"
#include "osso-abook-settings-dialog.h"
#include "osso-abook-util.h"

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
  hildon_gtk_init(&argc, &argv);

  setenv("OSSO_ABOOK_DEBUG", "all,disk-space", TRUE);

  GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

  osso_abook_debug_init();

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  EBook *book = osso_abook_system_book_dup_singleton(TRUE, NULL);

  GtkWidget *dialog = osso_abook_settings_dialog_new(GTK_WINDOW(window), book);
  g_signal_connect_swapped(dialog, "response",
                           G_CALLBACK(gtk_widget_destroy), window);
  g_signal_connect(window, "destroy", G_CALLBACK(loop_quit), main_loop);

  gtk_widget_show_all(window);
  gtk_widget_show_all(dialog);

  g_main_loop_run(main_loop);
  g_object_unref(book);
  g_main_loop_unref(main_loop);

  return 0;
}
