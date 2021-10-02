#include <hildon/hildon.h>

#include "config.h"

#include "osso-abook-avatar-editor-dialog.h"
#include "osso-abook-debug.h"
#include "osso-abook-log.h"
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

static void
avatar_editor_response_cb(GtkWidget *dialog, int response_id,
                          gpointer user_data)
{
  GdkPixbuf *pixbuf = osso_abook_avatar_editor_dialog_get_scaled_pixbuf(
    OSSO_ABOOK_AVATAR_EDITOR_DIALOG(dialog));
  GtkWidget *image = gtk_image_new_from_pixbuf(pixbuf);

  g_object_unref(pixbuf);
  gtk_container_add(GTK_CONTAINER(user_data), image);
  gtk_widget_show(image);

  gtk_widget_destroy(dialog);
}

static void
pixbuf_ready_cb(GObject *source_object, GAsyncResult *res, gpointer user_data)
{
  GError *error = NULL;
  GdkPixbuf *pixbuf = osso_abook_load_pixbuf_finish(G_FILE(source_object),
                                                    res, &error);

  if (!error)
  {
    GtkWidget *dialog = osso_abook_avatar_editor_dialog_new(
      GTK_WINDOW(user_data), pixbuf);

    g_signal_connect(dialog, "response",
                     G_CALLBACK(avatar_editor_response_cb), user_data);

    gtk_dialog_run(GTK_DIALOG(dialog));
  }
  else
  {
    OSSO_ABOOK_WARN("Unable to load test pixbuf, %s", error->message);
    gtk_widget_destroy(user_data);
  }
}

int
main(int argc, char **argv)
{
  hildon_gtk_init(&argc, &argv);

  setenv("OSSO_ABOOK_DEBUG", "all,disk-space", TRUE);

  GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

  osso_abook_debug_init();

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  g_signal_connect(window, "destroy", G_CALLBACK(loop_quit), main_loop);

  gtk_widget_show_all(window);

  GFile *file = g_file_new_for_path("./Macaca_nigra_self-portrait.jpg");

  osso_abook_load_pixbuf_async(file, 0, 0, NULL, pixbuf_ready_cb, window);
  g_object_unref(file);
  g_main_loop_run(main_loop);
  g_main_loop_unref(main_loop);
}
