#ifndef _OSSO_ABOOK_STRING_LIST_H_INCLUDED__
#define _OSSO_ABOOK_STRING_LIST_H_INCLUDED__

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_STRING_LIST \
                (osso_abook_string_list_get_type ())

typedef GList *OssoABookStringList;

GType osso_abook_string_list_get_type(void) G_GNUC_CONST;

GList *osso_abook_string_list_copy(GList *list);
void osso_abook_string_list_free(OssoABookStringList list);
GList *osso_abook_string_list_sort(GList *list);
GList *osso_abook_string_list_chug(GList *list);
GList *osso_abook_string_list_find(GList *list, const char *s);

G_END_DECLS

#endif /* _OSSO_ABOOK_STRING_LIST_H_INCLUDED__ */
