/*
 * test-avatar-chooser-dialog.c
 *
 * Copyright (C) 2021 Ivaylo Dimitrov <ivo.g.dimitrov.75@gmail.com>
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

#include <hildon/hildon.h>

#include "config.h"

#include "osso-abook-avatar-chooser-dialog.h"
#include "osso-abook-debug.h"

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

static  void
avatar_chooser_response_cb(GtkWidget *dialog, int response_id,
                           gpointer user_data)
{
  printf("%s: response[%d] icon_name[%s]\n",__FUNCTION__, response_id,
         osso_abook_avatar_chooser_dialog_get_icon_name(
           OSSO_ABOOK_AVATAR_CHOOSER_DIALOG(dialog)));

  gtk_widget_destroy(user_data);
}

int
main(int argc, char **argv)
{
  hildon_gtk_init (&argc, &argv);

  setenv("OSSO_ABOOK_DEBUG", "all", TRUE);

  GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

  osso_abook_debug_init();

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWidget *dialog = osso_abook_avatar_chooser_dialog_new(GTK_WINDOW(window));

  g_signal_connect(window, "destroy", G_CALLBACK(loop_quit), main_loop);
  g_signal_connect(dialog, "response",
                   G_CALLBACK(avatar_chooser_response_cb), window);

  gtk_widget_show_all(window);
  gtk_dialog_run(GTK_DIALOG(dialog));
  g_main_loop_run(main_loop);
  g_main_loop_unref(main_loop);
}
