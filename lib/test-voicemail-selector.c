/*
 * test-voicemail_selector.c
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

#include <libintl.h>

#include "osso-abook-debug.h"
#include "osso-abook-voicemail-selector.h"

#include "config.h"

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
voicemail_response_cb(GtkWidget *dialog, gint response_id, gpointer user_data)
{
  if (response_id == GTK_RESPONSE_OK)
  {
    HildonTouchSelector *selector =
        hildon_picker_dialog_get_selector(HILDON_PICKER_DIALOG(dialog));

    osso_abook_voicemail_selector_apply(
          OSSO_ABOOK_VOICEMAIL_SELECTOR(selector));
    osso_abook_voicemail_selector_save(OSSO_ABOOK_VOICEMAIL_SELECTOR(selector));
  }

  gtk_widget_destroy(dialog);
}

static gboolean
idle_run_dialog(gpointer user_data)
{
  GtkWidget *dialog = hildon_picker_dialog_new(user_data);
  GtkWidget *selector = osso_abook_voicemail_selector_new();

  hildon_picker_dialog_set_selector(HILDON_PICKER_DIALOG(dialog),
                                    HILDON_TOUCH_SELECTOR(selector));
  hildon_picker_dialog_set_done_label(
        HILDON_PICKER_DIALOG(dialog),
        dgettext("hildon-libs", "wdgt_bd_save"));
  g_signal_connect(dialog, "response",
                   G_CALLBACK(voicemail_response_cb), user_data);

  gtk_dialog_run(GTK_DIALOG(dialog));

  return FALSE;
}

int
main(int argc, char **argv)
{
  hildon_gtk_init (&argc, &argv);

  setenv("OSSO_ABOOK_DEBUG", "all,disk-space", TRUE);

  GMainLoop *main_loop = g_main_loop_new(NULL, FALSE);

  osso_abook_debug_init();

  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  g_signal_connect(window, "destroy", G_CALLBACK(loop_quit), main_loop);

  g_idle_add(idle_run_dialog, window);
  gtk_widget_show_all(window);
  g_main_loop_run(main_loop);
  g_main_loop_unref(main_loop);
}
