/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 * Author:  Travis Reitter
 */

#ifndef __OSSO_ABOOK_TP_ACCOUNT_MODEL_H__
#define __OSSO_ABOOK_TP_ACCOUNT_MODEL_H__

#include <gtk/gtkliststore.h>
#include <libebook/libebook.h>

#include "osso-abook-types.h"
#include "osso-abook-caps.h"

G_BEGIN_DECLS

/**
 * OssoABookTpAccountModelColumn:
 * @OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ACCOUNT: a #TpAccount
 *
 * The columns indicies for #OssoABookTpAccountModel.
 */
typedef enum {
        OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ACCOUNT,
        OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_PROTOCOL_AND_UID,
        OSSO_ABOOK_TP_ACCOUNT_MODEL_COL_ICON_NAME,
} OssoABookTpAccountModelColumn;

#define OSSO_ABOOK_TYPE_TP_ACCOUNT_MODEL \
                (osso_abook_tp_account_model_get_type ())
#define OSSO_ABOOK_TP_ACCOUNT_MODEL(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_MODEL, \
                 OssoABookTpAccountModel))
#define OSSO_ABOOK_TP_ACCOUNT_MODEL_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_MODEL, \
                 OssoABookTpAccountModelClass))
#define OSSO_ABOOK_IS_TP_ACCOUNT_MODEL(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_MODEL))
#define OSSO_ABOOK_IS_TP_ACCOUNT_MODEL_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_MODEL))
#define OSSO_ABOOK_TP_ACCOUNT_MODEL_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_MODEL, \
                 OssoABookTpAccountModelClass))

/**
 * OssoABookTpAccountModel:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookTpAccountModel {
        /*< private >*/
        GtkListStore parent;
};

struct _OssoABookTpAccountModelClass {
        GtkListStoreClass parent_class;
};

GType
osso_abook_tp_account_model_get_type       (void) G_GNUC_CONST;

OssoABookTpAccountModel *
osso_abook_tp_account_model_new            (void);

const char *
osso_abook_tp_account_model_get_account_protocol
        (OssoABookTpAccountModel *model);

void
osso_abook_tp_account_model_set_account_protocol
        (OssoABookTpAccountModel *model,
         const char              *protocol);

OssoABookCapsFlags
osso_abook_tp_account_model_get_required_capabilities
        (OssoABookTpAccountModel *model);

void
osso_abook_tp_account_model_set_required_capabilities
        (OssoABookTpAccountModel *model,
         OssoABookCapsFlags       caps);

GList *
osso_abook_tp_account_model_get_allowed_accounts
        (OssoABookTpAccountModel *model);

void
osso_abook_tp_account_model_set_allowed_accounts
        (OssoABookTpAccountModel *model,
         GList                   *accounts);

gboolean
osso_abook_tp_account_model_get_iter       (OssoABookTpAccountModel *model,
                                            TpAccount               *account,
                                            GtkTreeIter             *iter);

G_END_DECLS

#endif /* __OSSO_ABOOK_TP_ACCOUNT_MODEL_H__ */
