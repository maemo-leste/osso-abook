#ifndef __OSSO_ABOOK_GROUP_H_INCLUDED__
#define __OSSO_ABOOK_GROUP_H_INCLUDED__

#include "osso-abook-presence.h"
#include "osso-abook-caps.h"
#include "osso-abook-list-store.h"

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_GROUP \
                (osso_abook_group_get_type ())
#define OSSO_ABOOK_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_GROUP, \
                 OssoABookGroup))
#define OSSO_ABOOK_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_GROUP, \
                 OssoABookGroupClass))
#define OSSO_ABOOK_IS_GROUP(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_GROUP))
#define OSSO_ABOOK_IS_GROUP_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_GROUP))
#define OSSO_ABOOK_GROUP_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_GROUP, \
                 OssoABookGroupClass))

/**
 * OssoABookGroup:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookGroup {
        /*< private >*/
        GObject parent;
        OssoABookGroupPrivate *priv;
};

struct _OssoABookGroupClass {
        GObjectClass parent_class;

        /* signals */
        void (* refilter_contact)   (OssoABookGroup    *group,
                                     OssoABookContact  *contact);

        void (* refilter_group)     (OssoABookGroup    *group);

        /* vtable */
        const char * (* get_name)         (OssoABookGroup    *group);

        void         (* set_name)         (OssoABookGroup    *group,
                                           const char        *name);

        const char * (* get_icon_name)    (OssoABookGroup    *group);

        void         (* set_icon_name)    (OssoABookGroup    *group,
                                           const char        *name);

        GtkWidget  * (* get_empty_widget) (OssoABookGroup    *group);

        gboolean     (* is_visible)       (OssoABookGroup    *group);

        void         (* set_visible)      (OssoABookGroup    *group,
                                           gboolean           visible);

        gboolean     (* includes_contact) (OssoABookGroup    *group,
                                           OssoABookContact  *contact);

        int          (* get_sort_weight)  (OssoABookGroup    *group);

        void         (* set_sort_weight)  (OssoABookGroup    *group,
                                           int                weight);

        gboolean     (* get_sensitive)    (OssoABookGroup    *group);

        void         (* set_sensitive)    (OssoABookGroup    *group,
                                           gboolean           sensitive);


        char *       (* get_display_title) (OssoABookGroup   *group);

        OssoABookListStoreCompareFunc
                     (* get_sort_func)    (OssoABookGroup    *group);
        OssoABookListStore *
                     (* get_model)        (OssoABookGroup    *group);
};

GType
osso_abook_group_get_type               (void) G_GNUC_CONST;

const char *
osso_abook_group_get_name               (OssoABookGroup    *group);

const char *
osso_abook_group_get_display_name       (OssoABookGroup    *group);

const char *
osso_abook_group_get_display_title      (OssoABookGroup    *group);

const char *
osso_abook_group_get_icon_name          (OssoABookGroup    *group);

GtkWidget *
osso_abook_group_get_empty_widget       (OssoABookGroup    *group);

gboolean
osso_abook_group_is_visible             (OssoABookGroup    *group);

gboolean
osso_abook_group_includes_contact       (OssoABookGroup    *group,
                                         OssoABookContact  *contact);

int
osso_abook_group_get_sort_weight        (OssoABookGroup    *group);
gboolean
osso_abook_group_get_sensitive          (OssoABookGroup    *group);

OssoABookListStoreCompareFunc
osso_abook_group_get_sort_func          (OssoABookGroup    *group);

int
osso_abook_group_compare                (OssoABookGroup    *a,
                                         OssoABookGroup    *b);

OssoABookListStore *
osso_abook_group_get_model              (OssoABookGroup    *group);

G_END_DECLS

#endif /* __OSSO_ABOOK_GROUP_H_INCLUDED__ */
