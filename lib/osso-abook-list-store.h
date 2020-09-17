#ifndef __OSSO_ABOOK_LIST_STORE_H_INCLUDED__
#define __OSSO_ABOOK_LIST_STORE_H_INCLUDED__

#include <gtk/gtk.h>
#include <libebook/libebook.h>

#include "osso-abook-avatar.h"
#include "osso-abook-caps.h"
#include "osso-abook-contact.h"
#include "osso-abook-presence.h"
#include "osso-abook-settings.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_LIST_STORE_ROW \
                (osso_abook_list_store_row_get_type ())

#define OSSO_ABOOK_TYPE_LIST_STORE \
                (osso_abook_list_store_get_type ())
#define OSSO_ABOOK_LIST_STORE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_LIST_STORE, \
                 OssoABookListStore))
#define OSSO_ABOOK_LIST_STORE_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_LIST_STORE, \
                 OssoABookListStoreClass))
#define OSSO_ABOOK_IS_LIST_STORE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_LIST_STORE))
#define OSSO_ABOOK_IS_LIST_STORE_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_LIST_STORE))
#define OSSO_ABOOK_LIST_STORE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_LIST_STORE, \
                 OssoABookListStoreClass))

/**
 * OssoABookListStoreCompareFunc:
 * @row_a: a #OssoABookListStoreRow
 * @row_b: another row to compare with
 * @user_data: user data to pass to comparison function
 *
 * The type of a comparison function used to compare two #OssoABookListStore
 * rows. The function should return a negative integer if the first row comes
 * before the second, 0 if they are equal, or a positive integer if the first
 * row comes after the second.
 *
 * Returns: negative value if @row_a < @row_b; zero if @row_a = @row_b;
 * positive value if @row_a > @row_b.
 */
typedef int (*OssoABookListStoreCompareFunc) (const OssoABookListStoreRow *row_a,
                                              const OssoABookListStoreRow *row_b,
                                              gpointer                     user_data);

/**
 * OssoABookListStoreColumn:
 * @OSSO_ABOOK_LIST_STORE_COLUMN_CONTACT: The contact column, containing an #OssoABookContact instance
 * @OSSO_ABOOK_LIST_STORE_COLUMN_LAST: The first column id available for child classes
 *
 * The columns in a #OssoABookListStore.
 */
typedef enum {
        OSSO_ABOOK_LIST_STORE_COLUMN_CONTACT,
        OSSO_ABOOK_LIST_STORE_COLUMN_LAST,
} OssoABookListStoreColumn;

/**
 * OssoABookListStore:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookListStore
{
        /*< private >*/
        GObject                    parent;
};

struct _OssoABookListStoreClass
{
        GObjectClass parent_class;
        GType        row_type;

        /* vtable */
        void     (*clear)            (OssoABookListStore      *store);

        void     (*contacts_added)   (OssoABookListStore      *store,
                                      OssoABookContact       **contacts);

        void     (*row_added)        (OssoABookListStore      *store,
                                      OssoABookListStoreRow   *added);
        void     (*row_removed)      (OssoABookListStore      *store,
                                      OssoABookListStoreRow   *removed);

        void     (*contact_changed)  (OssoABookListStore      *store,
                                      OssoABookListStoreRow  **changed);
        void     (*presence_changed) (OssoABookListStore      *store,
                                      OssoABookListStoreRow   *changed);
        void     (*caps_changed)     (OssoABookListStore      *store,
                                      OssoABookListStoreRow   *changed);
};

/**
 * OssoABookListStoreRow:
 * @offset: The position of this row
 * @contact: The #OssoABookContact associated with this row
 * @presence: The #OssoABookPresence associated with this row
 * @caps: The #OssoABookCaps associated with this row
 *
 * One single row of a #OssoABookListStore.
 */
struct _OssoABookListStoreRow
{
        int                offset;
        OssoABookContact  *contact;
};

GType
osso_abook_list_store_get_type            (void) G_GNUC_CONST;

void
osso_abook_list_store_set_roster          (OssoABookListStore           *store,
                                           OssoABookRoster              *roster);

OssoABookRoster *
osso_abook_list_store_get_roster          (OssoABookListStore           *store);

void
osso_abook_list_store_set_book_view       (OssoABookListStore           *store,
                                           EBookView                    *book_view);
EBookView *
osso_abook_list_store_get_book_view       (OssoABookListStore           *store);

void
osso_abook_list_store_set_contact_order   (OssoABookListStore           *store,
                                           OssoABookContactOrder         order);

OssoABookContactOrder
osso_abook_list_store_get_contact_order   (OssoABookListStore           *store);

void
osso_abook_list_store_set_name_order      (OssoABookListStore           *store,
                                           OssoABookNameOrder            order);
OssoABookNameOrder
osso_abook_list_store_get_name_order      (OssoABookListStore           *store);

void
osso_abook_list_store_set_sort_func       (OssoABookListStore           *store,
                                           OssoABookListStoreCompareFunc callback,
                                           gpointer                      user_data,
                                           GDestroyNotify                destroy_data);

void
osso_abook_list_store_set_group_sort_func (OssoABookListStore           *store,
                                           OssoABookListStoreCompareFunc callback,
                                           gpointer                      user_data,
                                           GDestroyNotify                destroy_data);

void
osso_abook_list_store_pre_allocate_rows   (OssoABookListStore           *store,
                                           int                           n_rows);

void
osso_abook_list_store_merge_rows          (OssoABookListStore           *store,
                                           GList                        *rows);

void
osso_abook_list_store_remove_rows         (OssoABookListStore           *store,
                                           OssoABookListStoreRow       **rows,
                                           gssize                        n_rows);

gboolean
osso_abook_list_store_is_loading          (OssoABookListStore           *store);
void
osso_abook_list_store_cancel_loading      (OssoABookListStore           *store);

OssoABookListStoreRow *
osso_abook_list_store_iter_get_row        (OssoABookListStore           *store,
                                           GtkTreeIter                  *iter);
gboolean
osso_abook_list_store_row_get_iter        (OssoABookListStore           *store,
                                           const OssoABookListStoreRow  *row,
                                           GtkTreeIter                  *iter);

OssoABookListStoreRow **
osso_abook_list_store_find_contacts       (OssoABookListStore           *store,
                                           const char                   *uid);

void
osso_abook_list_store_contact_changed     (OssoABookListStore           *store,
                                           OssoABookContact             *contact);


GType
osso_abook_list_store_row_get_type        (void) G_GNUC_CONST;

OssoABookListStoreRow *
osso_abook_list_store_row_new             (OssoABookContact             *contact);

OssoABookListStoreRow *
osso_abook_list_store_row_copy            (OssoABookListStoreRow        *row);

void
osso_abook_list_store_row_free            (OssoABookListStoreRow        *row);

G_END_DECLS

#endif /* __OSSO_ABOOK_LIST_STORE_H_INCLUDED__ */
