#ifndef __OSSO_ABOOK_CONTACT_CHOOSER_H_INCLUDED__
#define __OSSO_ABOOK_CONTACT_CHOOSER_H_INCLUDED__

#include "osso-abook-types.h"
#include "osso-abook-contact.h"
#include "osso-abook-caps.h"
#include "osso-abook-settings.h"

#include <hildon/hildon.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CONTACT_CHOOSER \
                (osso_abook_contact_chooser_get_type ())
#define OSSO_ABOOK_CONTACT_CHOOSER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_CHOOSER, \
                 OssoABookContactChooser))
#define OSSO_ABOOK_CONTACT_CHOOSER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_CHOOSER, \
                 OssoABookContactChooserClass))
#define OSSO_ABOOK_IS_CONTACT_CHOOSER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_CHOOSER))
#define OSSO_ABOOK_IS_CONTACT_CHOOSER_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_CHOOSER))
#define OSSO_ABOOK_CONTACT_CHOOSER_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_CHOOSER, \
                 OssoABookContactChooserClass))

/**
 * OSSO_ABOOK_CONTACT_CHOOSER_DBUS_IFACE:
 *
 * Name of the #OssoABookContactChooser<!-- -->'s D-Bus interface as #GQuark.
 */
#define OSSO_ABOOK_CONTACT_CHOOSER_DBUS_IFACE \
                (osso_abook_contact_chooser_dbus_iface_quark ())

/**
 * OSSO_ABOOK_CONTACT_CHOOSER_DBUS_IFACE_STRING:
 *
 * Name of the #OssoABookContactChooser<!-- -->'s D-Bus interface as string.
 */
#define OSSO_ABOOK_CONTACT_CHOOSER_DBUS_IFACE_STRING \
                (g_quark_to_string (OSSO_ABOOK_CONTACT_CHOOSER_DBUS_IFACE))

/**
 * OssoABookContactChooser:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookContactChooser {
        /*< private >*/
        GtkDialog parent;
};

struct _OssoABookContactChooserClass {
        GtkDialogClass parent_class;
};


typedef gboolean (*OssoABookContactChooserVisibleFunc) (OssoABookContactChooser *chooser,
                                                        OssoABookContact        *contact,
                                                        gpointer                 user_data);


GQuark
osso_abook_contact_chooser_dbus_iface_quark      (void) G_GNUC_CONST;

GType
osso_abook_contact_chooser_get_type              (void) G_GNUC_CONST;

GtkWidget *
osso_abook_contact_chooser_new                   (GtkWindow               *parent,
                                                  const gchar             *title);

GtkWidget *
osso_abook_contact_chooser_new_with_capabilities (GtkWindow               *parent,
                                                  const gchar             *title,
                                                  OssoABookCapsFlags       caps,
                                                  OssoABookContactOrder    order);

GList *
osso_abook_contact_chooser_get_selection         (OssoABookContactChooser *chooser);

void
osso_abook_contact_chooser_select                (OssoABookContactChooser *chooser,
                                                  OssoABookContact        *contact);

void
osso_abook_contact_chooser_select_contacts       (OssoABookContactChooser *chooser,
                                                  GList                   *contacts);

void
osso_abook_contact_chooser_deselect_all          (OssoABookContactChooser *chooser);


void
osso_abook_contact_chooser_set_capabilities      (OssoABookContactChooser *chooser,
                                                  OssoABookCapsFlags       caps);

void
osso_abook_contact_chooser_set_contact_order     (OssoABookContactChooser *chooser,
                                                  OssoABookContactOrder    order);

void
osso_abook_contact_chooser_set_excluded_contacts (OssoABookContactChooser *chooser,
                                                  GList                   *contacts);

void
osso_abook_contact_chooser_set_minimum_selection (OssoABookContactChooser *chooser,
                                                  unsigned int             limit);

void
osso_abook_contact_chooser_set_maximum_selection (OssoABookContactChooser *chooser,
                                                  unsigned int             limit);

void
osso_abook_contact_chooser_set_done_label        (OssoABookContactChooser *chooser,
                                                  const char              *label);

void
osso_abook_contact_chooser_set_model             (OssoABookContactChooser *chooser,
                                                  OssoABookContactModel   *model);

void
osso_abook_contact_chooser_set_hide_offline_contacts
                                                 (OssoABookContactChooser *chooser,
                                                  gboolean                 state);
void
osso_abook_contact_chooser_set_show_empty_note   (OssoABookContactChooser *chooser,
                                                  gboolean                 state);


OssoABookCapsFlags
osso_abook_contact_chooser_get_capabilities      (OssoABookContactChooser *chooser);

OssoABookContactOrder
osso_abook_contact_chooser_get_contact_order     (OssoABookContactChooser *chooser);

GList *
osso_abook_contact_chooser_get_excluded_contacts (OssoABookContactChooser *chooser);

unsigned int
osso_abook_contact_chooser_get_minimum_selection (OssoABookContactChooser *chooser);

unsigned int
osso_abook_contact_chooser_get_maximum_selection (OssoABookContactChooser *chooser);

const char *
osso_abook_contact_chooser_get_done_label        (OssoABookContactChooser *chooser);

OssoABookContactModel *
osso_abook_contact_chooser_get_model             (OssoABookContactChooser *chooser);

GtkWidget *
osso_abook_contact_chooser_get_contact_view      (OssoABookContactChooser *chooser);

gboolean
osso_abook_contact_chooser_get_hide_offline_contacts
                                                 (OssoABookContactChooser *chooser);
gboolean
osso_abook_contact_chooser_get_show_empty_note   (OssoABookContactChooser *chooser);


void
osso_abook_contact_chooser_set_visible_func      (OssoABookContactChooser *chooser,
                                                  OssoABookContactChooserVisibleFunc func,
                                                  gpointer                 user_data,
                                                  GDestroyNotify           destroy);

void
osso_abook_contact_chooser_refilter              (OssoABookContactChooser *chooser);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_CHOOSER_H_INCLUDED__ */
