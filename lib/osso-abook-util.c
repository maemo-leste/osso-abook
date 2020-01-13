#include "config.h"

#include "osso-abook-util.h"

gboolean
osso_abook_screen_is_landscape_mode(GdkScreen *screen)
{
  g_return_val_if_fail(GDK_IS_SCREEN (screen), TRUE);

  return gdk_screen_get_width(screen) > gdk_screen_get_height(screen);
}

static void
osso_abook_pannable_area_size_request(GtkWidget *widget,
                                      GtkRequisition *requisition,
                                      gpointer user_data)
{
  GtkWidget *child = gtk_bin_get_child(GTK_BIN(widget));

  if (!child)
    return;

  if (gtk_widget_get_sensitive(child))
  {
    GdkScreen *screen = gtk_widget_get_screen(widget);

    if (screen)
    {
      gint screen_height = gdk_screen_get_height(screen);
      GtkRequisition child_requisition;

      gtk_widget_size_request(child, &child_requisition);

      if (screen_height > child_requisition.height + requisition->height)
        requisition->height = child_requisition.height + requisition->height;
      else
        requisition->height = screen_height;
    }
  }
}

GtkWidget *
osso_abook_pannable_area_new(void)
{
  GtkWidget *area = hildon_pannable_area_new();

  g_object_set(area,
               "hscrollbar-policy", GTK_POLICY_NEVER,
               "mov-mode", HILDON_MOVEMENT_MODE_VERT,
               NULL);
  g_signal_connect(area, "size-request",
                   G_CALLBACK(osso_abook_pannable_area_size_request),NULL);

  return area;
}
