#include "config.h"

#include "osso-abook-row-model.h"

GType
osso_abook_row_model_get_type (void)
{
  static volatile gsize g_define_type_id__volatile = 0;

  if (g_once_init_enter (&g_define_type_id__volatile))
  {
    GType g_define_type_id =
        g_type_register_static_simple (
          G_TYPE_INTERFACE,
          g_intern_static_string("OssoABookRowModel"),
          sizeof (struct _OssoABookRowModelIface),
          (GClassInitFunc)NULL,
          0,
          (GInstanceInitFunc)NULL,
          (GTypeFlags) 0);
    g_type_interface_add_prerequisite(g_define_type_id, GTK_TYPE_TREE_MODEL);
    g_once_init_leave (&g_define_type_id__volatile, g_define_type_id);
  }

  return g_define_type_id__volatile;
}

gpointer
osso_abook_row_model_iter_get_row(OssoABookRowModel *model, GtkTreeIter *iter)
{
  OssoABookRowModelIface *iface = OSSO_ABOOK_ROW_MODEL_GET_IFACE(model);

  g_return_val_if_fail(iface != NULL, NULL);
  g_return_val_if_fail(iter != NULL, NULL);

  return iface->iter_get_row(model, iter);
}

gboolean
osso_abook_row_model_row_get_iter(OssoABookRowModel *model, gconstpointer row,
                                  GtkTreeIter *iter)
{
  OssoABookRowModelIface *iface = OSSO_ABOOK_ROW_MODEL_GET_IFACE(model);

  g_return_val_if_fail(iface != NULL, FALSE);
  g_return_val_if_fail(iter != NULL, FALSE);
  g_return_val_if_fail(row != NULL, FALSE);

  return iface->row_get_iter(model, row, iter);
}
