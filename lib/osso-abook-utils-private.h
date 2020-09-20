#ifndef __OSSO_ABOOK_UTILS_PRIVATE_H_INCLUDED__
#define __OSSO_ABOOK_UTILS_PRIVATE_H_INCLUDED__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <telepathy-glib/telepathy-glib.h>

G_BEGIN_DECLS

#define IS_EMPTY(s) (!((s) && (((const char *)(s))[0])))

void
disconnect_signal_if_connected(gpointer instance, gulong handler);

gchar *
_osso_abook_avatar_get_cache_name(int width, int height, gboolean crop,
                                  int radius, const guint8 border_color[4]);

void
osso_abook_list_push(GList **list, gpointer data);
gpointer
osso_abook_list_pop(GList **list, gpointer data);

GdkPixbuf *
_osso_abook_scale_pixbuf_and_crop(const GdkPixbuf *image, int width, int height,
                                  int crop, int border_size);
void
_osso_abook_pixbuf_cut_corners(GdkPixbuf *pixbuf, const int radius,
                               const guint8 border_color[4]);

gchar *
_osso_abook_flags_to_string(GType flags_type, guint value);

gboolean
_osso_abook_is_addressbook(void);

TpConnectionPresenceType
default_presence_convert(TpConnectionPresenceType presence_type);

G_END_DECLS

#endif /* __OSSO_ABOOK_UTILS_PRIVATE_H_INCLUDED__ */
