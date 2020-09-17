#ifndef __OSSO_ABOOK_CONTACT_VIEW_H_INCLUDED__
#define __OSSO_ABOOK_CONTACT_VIEW_H_INCLUDED__

#include "osso-abook-tree-view.h"
#include "osso-abook-contact-model.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CONTACT_VIEW \
                (osso_abook_contact_view_get_type ())
#define OSSO_ABOOK_CONTACT_VIEW(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_VIEW, \
                 OssoABookContactView))
#define OSSO_ABOOK_CONTACT_VIEW_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_VIEW, \
                 OssoABookContactViewClass))
#define OSSO_ABOOK_IS_CONTACT_VIEW(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_VIEW))
#define OSSO_ABOOK_IS_CONTACT_VIEW_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_VIEW))
#define OSSO_ABOOK_CONTACT_VIEW_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_VIEW, \
                 OssoABookContactViewClass))

/**
 * OssoABookContactView:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookContactView {
        /*< private >*/
        OssoABookTreeView parent;
};

struct _OssoABookContactViewClass {
        OssoABookTreeViewClass parent_class;

        void (* contact_activated) (OssoABookContactView *view,
                                    OssoABookContact     *contact);

        void (* selection_changed) (OssoABookContactView *view,
                                    guint                 n_selected_rows);
};

GType
osso_abook_contact_view_get_type           (void) G_GNUC_CONST;

GtkWidget *
osso_abook_contact_view_new                (HildonUIMode           mode,
                                            OssoABookContactModel *model,
                                            OssoABookFilterModel  *filter_model);
GtkWidget *
osso_abook_contact_view_new_basic          (HildonUIMode           mode,
                                            OssoABookContactModel *model);
GList *
osso_abook_contact_view_get_selection      (OssoABookContactView  *view);

#ifndef OSSO_ABOOK_DISABLE_DEPRECATED

G_GNUC_DEPRECATED OssoABookContact *
osso_abook_contact_view_get_focus          (OssoABookContactView  *view);

G_GNUC_DEPRECATED OssoABookAvatar *
osso_abook_contact_view_get_focus_avatar   (OssoABookContactView  *view);

G_GNUC_DEPRECATED OssoABookPresence *
osso_abook_contact_view_get_focus_presence (OssoABookContactView  *view);

G_GNUC_DEPRECATED OssoABookCaps *
osso_abook_contact_view_get_focus_caps     (OssoABookContactView  *view);

#endif /* OSSO_ABOOK_DISABLE_DEPRECATED */

unsigned int
osso_abook_contact_view_get_minimum_selection (OssoABookContactView *view);

unsigned int
osso_abook_contact_view_get_maximum_selection (OssoABookContactView *view);

void
osso_abook_contact_view_set_minimum_selection (OssoABookContactView *view,
                                               unsigned int          limit);

void
osso_abook_contact_view_set_maximum_selection (OssoABookContactView *view,
                                               unsigned int          limit);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_VIEW_H_INCLUDED__ */
