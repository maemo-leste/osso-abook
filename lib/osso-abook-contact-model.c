#include "osso-abook-contact-model.h"
#include "osso-abook-aggregator.h"
#include "osso-abook-log.h"

#include "config.h"

G_DEFINE_TYPE(
  OssoABookContactModel,
  osso_abook_contact_model,
  OSSO_ABOOK_TYPE_LIST_STORE
);

static OssoABookContactModel *osso_abook_contact_model_default = NULL;

static void
osso_abook_contact_model_init(OssoABookContactModel *model)
{
}

static void
osso_abook_contact_model_contacts_added(OssoABookListStore *store,
                                        OssoABookContact **contacts)
{
  OssoABookContact **contact;
  GList *rows = NULL;

  for (contact = contacts; *contact; contact++)
    rows = g_list_prepend(rows, osso_abook_list_store_row_new(*contact));

  osso_abook_list_store_merge_rows(store, rows);
  g_list_free(rows);
}

static void
osso_abook_contact_model_class_init(OssoABookContactModelClass *klass)
{
  OssoABookListStoreClass *store_class = OSSO_ABOOK_LIST_STORE_CLASS(klass);

  store_class->contacts_added = osso_abook_contact_model_contacts_added;
  store_class->row_type = OSSO_ABOOK_TYPE_LIST_STORE;
}

OssoABookContactModel *
osso_abook_contact_model_new(void)
{
  return g_object_new(OSSO_ABOOK_TYPE_CONTACT_MODEL, NULL);
}

gboolean
osso_abook_contact_model_is_default(OssoABookContactModel *model)
{
  g_return_val_if_fail(OSSO_ABOOK_IS_CONTACT_MODEL(model), FALSE);

  return model == osso_abook_contact_model_default;
}

static void
osso_abook_contact_model_at_exit(void)
{
  if (osso_abook_contact_model_default)
  {
    g_object_unref(osso_abook_contact_model_default);
    osso_abook_contact_model_default = NULL;
  }
}

OssoABookContactModel *
osso_abook_contact_model_get_default()
{
  OssoABookRoster *aggr;

  if (!osso_abook_contact_model_default)
  {
    GError *error = NULL;

    osso_abook_contact_model_default = osso_abook_contact_model_new();
    atexit(osso_abook_contact_model_at_exit);
    aggr = osso_abook_aggregator_get_default(&error);

    if (error)
    {
      OSSO_ABOOK_WARN("%s", error->message);
      g_clear_error(&error);
    }

    if (aggr)
    {
      osso_abook_list_store_set_roster(
            OSSO_ABOOK_LIST_STORE(osso_abook_contact_model_default), aggr);
    }
  }

  return osso_abook_contact_model_default;
}

OssoABookListStoreRow *
osso_abook_contact_model_find_contact(OssoABookContactModel *model,
                                      const char *uid, GtkTreeIter *iter)
{
  OssoABookListStore *store = (OssoABookListStore *)model;
  OssoABookListStoreRow **rows =
      osso_abook_list_store_find_contacts(store, uid);
  OssoABookListStoreRow *row = NULL;

  if (rows && (!iter || osso_abook_list_store_row_get_iter(store, *rows, iter)))
    row = *rows;

  return row;
}
