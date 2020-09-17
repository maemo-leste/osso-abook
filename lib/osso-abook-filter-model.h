#ifndef __OSSO_ABOOK_FILTER_MODEL_H_INCLUDED__
#define __OSSO_ABOOK_FILTER_MODEL_H_INCLUDED__

#include "osso-abook-group.h"
#include "osso-abook-list-store.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_FILTER_MODEL \
                (osso_abook_filter_model_get_type ())
#define OSSO_ABOOK_FILTER_MODEL(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_FILTER_MODEL, \
                 OssoABookFilterModel))
#define OSSO_ABOOK_FILTER_MODEL_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_FILTER_MODEL, \
                 OssoABookFilterModelClass))
#define OSSO_ABOOK_IS_FILTER_MODEL(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_FILTER_MODEL))
#define OSSO_ABOOK_IS_FILTER_MODEL_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_FILTER_MODEL))
#define OSSO_ABOOK_FILTER_MODEL_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_FILTER_MODEL, \
                 OssoABookFilterModelClass))

/**
 * OssoABookFilterModel:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookFilterModel {
        /*< private >*/
        GtkTreeModelFilter parent;
};

struct _OssoABookFilterModelClass {
        GtkTreeModelFilterClass parent_class;
};

GType
osso_abook_filter_model_get_type         (void) G_GNUC_CONST;

OssoABookFilterModel *
osso_abook_filter_model_new              (OssoABookListStore           *child_model);

void
osso_abook_filter_model_set_visible_func (OssoABookFilterModel         *model,
                                          GtkTreeModelFilterVisibleFunc func,
                                          gpointer                      data,
                                          GDestroyNotify                destroy);

void
osso_abook_filter_model_set_group        (OssoABookFilterModel         *model,
                                          OssoABookGroup               *group);

OssoABookGroup *
osso_abook_filter_model_get_group        (OssoABookFilterModel         *model);

void
osso_abook_filter_model_set_text         (OssoABookFilterModel         *model,
                                          const char                   *text);
void
osso_abook_filter_model_set_prefix       (OssoABookFilterModel         *model,
                                          const char                   *prefix);
const char *
osso_abook_filter_model_get_text         (OssoABookFilterModel         *model);

gboolean
osso_abook_filter_model_get_prefix       (OssoABookFilterModel         *model);

void
osso_abook_filter_model_freeze_refilter  (OssoABookFilterModel         *model);

void
osso_abook_filter_model_thaw_refilter    (OssoABookFilterModel         *model);

PangoAttrList *
osso_abook_filter_model_get_markup       (OssoABookFilterModel         *model,
                                          const char                   *text);

gboolean
osso_abook_filter_model_is_row_visible   (OssoABookFilterModel         *model,
                                          GtkTreeIter                  *iter);

G_END_DECLS

#endif /* __OSSO_ABOOK_FILTER_MODEL_H_INCLUDED__ */
