#ifndef __OSSO_ABOOK_UTILS_PRIVATE_H_INCLUDED__
#define __OSSO_ABOOK_UTILS_PRIVATE_H_INCLUDED__

#include <glib-object.h>
#include <gtk/gtk.h>
#include <telepathy-glib/telepathy-glib.h>
#include <libebook/libebook.h>
#include <hildon/hildon.h>

G_BEGIN_DECLS

#define IS_EMPTY(s) (!((s) && (((const char *)(s))[0])))

#define FILE_SCHEME "file://"

#define OSSO_ABOOK_BACKEND_TELEPATHY "tp"
#define OSSO_ABOOK_BACKEND_SIM "sim"

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

void
osso_abook_e_vcard_attribute_param_merge_value(EVCardAttributeParam *param,
                                               const char *value,
                                               GCompareFunc cmp_func);

void
_osso_abook_button_set_date_style(HildonButton *button);

GdkPixbuf *
_osso_abook_get_cached_icon(gpointer widget, const gchar *icon_name, gint size);

gchar *
_osso_abook_get_safe_folder(const char *folder);

gchar *
_osso_abook_get_operator_id(const char *modem_path, GError **error);

gchar *
_osso_abook_get_operator_name(const char *modem_path, const char *imsi,
                              GError **error);

gboolean
_osso_abook_e_vcard_attribute_has_value(EVCardAttribute *attr);

gchar *
_osso_abook_get_delete_confirmation_string(GList *contacts,
                                           gboolean show_contact_name,
                                           const gchar *no_im_format,
                                           gchar *single_service_format,
                                           gchar *mutiple_services_format);

gboolean
_osso_abook_tp_protocol_has_rosters(TpProtocol *protocol);

gchar *
_osso_abook_tp_account_get_vcard_field(TpAccount *account);

gboolean
_is_local_file(const gchar *uri);

ESource *
_osso_abook_create_source(const gchar *uid, const gchar *backend_name);

void
_osso_abook_unref_registry_idle(ESourceRegistry *registry);

G_END_DECLS

#endif /* __OSSO_ABOOK_UTILS_PRIVATE_H_INCLUDED__ */
