/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 * Author:  Travis Reitter
 */

#ifndef __OSSO_ABOOK_TP_ACCOUNT_SELECTOR_H__
#define __OSSO_ABOOK_TP_ACCOUNT_SELECTOR_H__

#include "osso-abook-types.h"
#include "osso-abook-caps.h"

#include <gtk/gtk.h>
#include <hildon/hildon-picker-dialog.h>
#include <hildon/hildon-touch-selector.h>
#include <libebook/libebook.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_TP_ACCOUNT_SELECTOR \
                (osso_abook_tp_account_selector_get_type())
#define OSSO_ABOOK_TP_ACCOUNT_SELECTOR(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST((obj), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_SELECTOR, \
                 OssoABookTpAccountSelector))
#define OSSO_ABOOK_TP_ACCOUNT_SELECTOR_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST((klass), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_SELECTOR, \
                 OssoABookTpAccountSelectorClass))
#define OSSO_ABOOK_IS_TP_ACCOUNT_SELECTOR(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE((obj), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_SELECTOR))
#define OSSO_ABOOK_IS_TP_ACCOUNT_SELECTOR_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE((klass), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_SELECTOR))
#define OSSO_ABOOK_TP_ACCOUNT_SELECTOR_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS((obj), \
                 OSSO_ABOOK_TYPE_TP_ACCOUNT_SELECTOR, \
                 OssoABookTpAccountSelectorClass))

/**
 * OssoABookTpAccountSelector:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookTpAccountSelector {
        HildonPickerDialog      parent;
};

struct _OssoABookTpAccountSelectorClass {
        HildonPickerDialogClass parent_class;
};

GType
osso_abook_tp_account_selector_get_type    (void) G_GNUC_CONST;

GtkWidget *
osso_abook_tp_account_selector_new         (GtkWindow               *parent,
                                            OssoABookTpAccountModel *model);

TpAccount *
osso_abook_tp_account_selector_get_account (OssoABookTpAccountSelector *dialog);

GList *
osso_abook_tp_account_selector_get_accounts
        (OssoABookTpAccountSelector *dialog);

OssoABookTpAccountModel *
osso_abook_tp_account_selector_get_model (OssoABookTpAccountSelector *dialog);

void
osso_abook_tp_account_selector_set_model (OssoABookTpAccountSelector *dialog,
                                          OssoABookTpAccountModel    *model);

HildonTouchSelectorSelectionMode
osso_abook_tp_account_selector_get_selection_mode
        (OssoABookTpAccountSelector *dialog);

void
osso_abook_tp_account_selector_set_selection_mode
        (OssoABookTpAccountSelector       *dialog,
         HildonTouchSelectorSelectionMode  mode);


G_END_DECLS

#endif /* __OSSO_ABOOK_TP_ACCOUNT_SELECTOR_H__ */
