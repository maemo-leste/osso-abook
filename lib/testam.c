#include <glib.h>
#include <glib-object.h>
#include <libedata-book/libedata-book.h>
#include <hildon/hildon.h>
#include <stdlib.h>

#include "config.h"

#include "osso-abook-contact-chooser.h"
#include "osso-abook-avatar-chooser-dialog.h"
#include "osso-abook-debug.h"
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

GtkWidget *window;

static void
pixbuf_ready_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf = osso_abook_load_pixbuf_finish(G_FILE(source_object),
                                                    res, &error);
  GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);

  gtk_container_add(GTK_CONTAINER(user_data), image);
  gtk_widget_show(image);

  g_object_unref(pixbuf);
}

static  void
avatar_chooser_response_cb(GtkWidget *dialog, int response_id,
                           gpointer user_data)
{
  printf("%s: response[%d] icon_name[%s]\n",__FUNCTION__, response_id,
         osso_abook_avatar_chooser_dialog_get_icon_name(
           OSSO_ABOOK_AVATAR_CHOOSER_DIALOG(dialog)));

  gtk_widget_destroy(dialog);
}

int
main(int argc, char **argv)
{
  hildon_gtk_init (&argc, &argv);

  setenv("OSSO_ABOOK_DEBUG", "all", TRUE);

  GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

  osso_abook_debug_init();

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWidget *chooser = osso_abook_contact_chooser_new(GTK_WINDOW(window), NULL);
  GtkWidget *dialog = osso_abook_avatar_chooser_dialog_new(GTK_WINDOW(window));

  g_signal_connect(window, "destroy", G_CALLBACK(loop_quit), main_loop);

  gtk_widget_show_all(window);
  gtk_widget_show_all(chooser);

  g_signal_connect(dialog, "response",
                   G_CALLBACK(avatar_chooser_response_cb), NULL);

  gtk_dialog_run(GTK_DIALOG(dialog));

  GFile *file = g_file_new_for_uri("file:///home/user/pic.png");

  osso_abook_load_pixbuf_async(file, 0, 0, NULL, pixbuf_ready_cb, window);

  g_object_unref(file);
  g_main_loop_run(main_loop);


  g_main_loop_unref(main_loop);
}
