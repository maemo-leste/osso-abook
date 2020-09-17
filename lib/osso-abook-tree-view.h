#ifndef __OSSO_ABOOK_TREE_VIEW_H_INCLUDED__
#define __OSSO_ABOOK_TREE_VIEW_H_INCLUDED__

#include <gtk/gtk.h>
#include <libebook/libebook.h>

#include "osso-abook-filter-model.h"

G_BEGIN_DECLS

/**
 * OssoABookTreeViewPanMode:
 * @OSSO_ABOOK_TREE_VIEW_PAN_MODE_TOP: pan to the top of the visible area
 * @OSSO_ABOOK_TREE_VIEW_PAN_MODE_MIDDLE: pan to the center of the visible area
 *
 * The various possible places where a contact is to be positioned when
 * panning the treeview.
 */
typedef enum {
        OSSO_ABOOK_TREE_VIEW_PAN_MODE_TOP,
        OSSO_ABOOK_TREE_VIEW_PAN_MODE_MIDDLE,
} OssoABookTreeViewPanMode;

#define OSSO_ABOOK_TYPE_TREE_VIEW \
                (osso_abook_tree_view_get_type ())
#define OSSO_ABOOK_TREE_VIEW(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_TREE_VIEW, \
                 OssoABookTreeView))
#define OSSO_ABOOK_TREE_VIEW_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_TREE_VIEW, \
                 OssoABookTreeViewClass))
#define OSSO_ABOOK_IS_TREE_VIEW(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_TREE_VIEW))
#define OSSO_ABOOK_IS_TREE_VIEW_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_TREE_VIEW))
#define OSSO_ABOOK_TREE_VIEW_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_TREE_VIEW, \
                 OssoABookTreeViewClass))

/**
 * OssoABookTreeView:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookTreeView {
        /*< private >*/
        GtkVBox parent;

        OssoABookTreeViewPrivate *priv;
};

struct _OssoABookTreeViewClass {
        GtkVBoxClass parent_class;

        /* vtable */
        gboolean (* is_row_sensitive)        (OssoABookTreeView  *view,
                                              GtkTreeModel       *tree_model,
                                              GtkTreeIter        *iter);
};

GType
osso_abook_tree_view_get_type                (void) G_GNUC_CONST;

void
osso_abook_tree_view_set_empty_text          (OssoABookTreeView    *view,
                                              const char           *text);

const char *
osso_abook_tree_view_get_empty_text          (OssoABookTreeView    *view);

GtkTreeModel *
osso_abook_tree_view_get_model               (OssoABookTreeView    *view);

OssoABookListStore *
osso_abook_tree_view_get_base_model          (OssoABookTreeView    *view);

void
osso_abook_tree_view_set_base_model          (OssoABookTreeView    *view,
                                              OssoABookListStore   *model);

OssoABookFilterModel *
osso_abook_tree_view_get_filter_model        (OssoABookTreeView    *view);

void
osso_abook_tree_view_set_filter_model        (OssoABookTreeView    *view,
                                              OssoABookFilterModel *model);

GtkTreeView *
osso_abook_tree_view_get_tree_view           (OssoABookTreeView    *view);

GtkWidget *
osso_abook_tree_view_get_pannable_area       (OssoABookTreeView    *view);

GtkTreeSelection *
osso_abook_tree_view_get_tree_selection      (OssoABookTreeView    *view);

void
osso_abook_tree_view_set_avatar_view         (OssoABookTreeView    *view,
                                              gboolean              enable);
void
osso_abook_tree_view_set_aggregation_account (OssoABookTreeView    *view,
                                              TpAccount            *account);

TpAccount *
osso_abook_tree_view_get_aggregation_account (OssoABookTreeView    *view);

void
osso_abook_tree_view_set_ui_mode             (OssoABookTreeView    *view,
                                              HildonUIMode          mode);

HildonUIMode
osso_abook_tree_view_get_ui_mode             (OssoABookTreeView    *view);

#ifndef OSSO_ABOOK_DISABLE_DEPRECATED
G_GNUC_DEPRECATED void
osso_abook_tree_view_select_first            (OssoABookTreeView    *view);
#endif /* OSSO_ABOOK_DISABLE_DEPRECATED */

gboolean
osso_abook_tree_view_pan_to_contact          (OssoABookTreeView        *view,
                                              OssoABookContact         *contact,
                                              OssoABookTreeViewPanMode  pan_mode);

G_END_DECLS

#endif /* __OSSO_ABOOK_TREE_VIEW_H_INCLUDED__ */
