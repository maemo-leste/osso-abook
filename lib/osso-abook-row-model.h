/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_ROW_MODEL_H__
#define __OSSO_ABOOK_ROW_MODEL_H__

#include "osso-abook-types.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_ROW_MODEL \
                (osso_abook_row_model_get_type ())
#define OSSO_ABOOK_ROW_MODEL(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_ROW_MODEL, \
                 OssoABookRowModel))
#define OSSO_ABOOK_IS_ROW_MODEL(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_ROW_MODEL))
#define OSSO_ABOOK_ROW_MODEL_GET_IFACE(obj) \
                (G_TYPE_INSTANCE_GET_INTERFACE ((obj), \
                 OSSO_ABOOK_TYPE_ROW_MODEL, \
                 OssoABookRowModelIface))

/* This typedef cannot go in osso-abook-types.h or gtk-doc will not generate
 * cross-reference links correctly */
/**
 * OssoABookRowModel:
 *
 * Dummy type for the #OssoABookRowModel interface.
 */
typedef struct _OssoABookRowModel OssoABookRowModel;

/**
 * OssoABookRowModelIface:
 * @iter_get_row: virtual method for osso_abook_row_model_iter_get_row()
 * @row_get_iter: virtual method for osso_abook_row_model_row_get_iter()
 *
 * Virtual methods of the #OssoABookRowModel interface.
 */
struct _OssoABookRowModelIface
{
  /*< private >*/
  GTypeInterface g_iface;

  /*< public >*/
  gpointer (*iter_get_row) (OssoABookRowModel *model,
                            GtkTreeIter       *iter);
  gboolean (*row_get_iter) (OssoABookRowModel *model,
                            gconstpointer      row,
                            GtkTreeIter       *iter);
};

GType
osso_abook_row_model_get_type     (void) G_GNUC_CONST;

gpointer
osso_abook_row_model_iter_get_row (OssoABookRowModel *model,
                                   GtkTreeIter       *iter);
gboolean
osso_abook_row_model_row_get_iter (OssoABookRowModel *model,
                                   gconstpointer      row,
                                   GtkTreeIter       *iter);

G_END_DECLS

#endif /* __OSSO_ABOOK_ROW_MODEL_H__ */
