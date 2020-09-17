#ifndef __OSSO_ABOOK_CONTACT_MODEL_H_INCLUDED__
#define __OSSO_ABOOK_CONTACT_MODEL_H_INCLUDED__

#include "osso-abook-list-store.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CONTACT_MODEL \
                (osso_abook_contact_model_get_type ())
#define OSSO_ABOOK_CONTACT_MODEL(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_MODEL, \
                 OssoABookContactModel))
#define OSSO_ABOOK_CONTACT_MODEL_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_MODEL, \
                 OssoABookContactModelClass))
#define OSSO_ABOOK_IS_CONTACT_MODEL(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_MODEL))
#define OSSO_ABOOK_IS_CONTACT_MODEL_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_MODEL))
#define OSSO_ABOOK_CONTACT_MODEL_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_MODEL, \
                 OssoABookContactModelClass))

/**
 * OssoABookContactModel:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookContactModel {
        /*< private >*/
        OssoABookListStore parent;
};

struct _OssoABookContactModelClass {
        OssoABookListStoreClass parent_class;
};

GType
osso_abook_contact_model_get_type     (void) G_GNUC_CONST;

OssoABookContactModel *
osso_abook_contact_model_new          (void);

gboolean
osso_abook_contact_model_is_default   (OssoABookContactModel *model);

OssoABookContactModel *
osso_abook_contact_model_get_default  (void);

OssoABookListStoreRow *
osso_abook_contact_model_find_contact (OssoABookContactModel *model,
                                       const char            *uid,
                                       GtkTreeIter           *iter);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_MODEL_H_INCLUDED__ */
