#ifndef __OSSO_ABOOK_INIT_H_INCLUDED__
#define __OSSO_ABOOK_INIT_H_INCLUDED__

#include <libebook/libebook.h>
#include <libosso.h>

G_BEGIN_DECLS

gboolean
osso_abook_init(int *argc, char ***argv, osso_context_t *osso_context);

gboolean
osso_abook_init_with_args(int *argc, char ***argv, osso_context_t *osso_context,
                          char *parameter_string, GOptionEntry *entries,
                          char *translation_domain, GError **error);

gboolean
osso_abook_init_with_name(const char *name, osso_context_t *osso_context);

osso_context_t *
osso_abook_get_osso_context (void);

void
osso_abook_make_resident(void);


/**
 * OssoABookBackendDiedFunc:
 * @book: the #EBook for which the backend died
 * @user_data: user data passed to osso_abook_set_backend_died_func()
 *
 * Specifies the type of functions passed to osso_abook_set_backend_died_func().
 */
typedef gboolean (*OssoABookBackendDiedFunc)(EBook    *book,
                                             gpointer  user_data);
void
osso_abook_set_backend_died_func (OssoABookBackendDiedFunc func,
                                  gpointer user_data);

G_END_DECLS

#endif /* __OSSO_ABOOK_INIT_H_INCLUDED__ */
