/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_VOICEMAIL_SELECTOR_H__
#define __OSSO_ABOOK_VOICEMAIL_SELECTOR_H__

#include "osso-abook-types.h"
#include <hildon/hildon.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_VOICEMAIL_SELECTOR \
                (osso_abook_voicemail_selector_get_type ())
#define OSSO_ABOOK_VOICEMAIL_SELECTOR(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_SELECTOR, \
                 OssoABookVoicemailSelector))
#define OSSO_ABOOK_VOICEMAIL_SELECTOR_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_SELECTOR, \
                 OssoABookVoicemailSelectorClass))
#define OSSO_ABOOK_IS_VOICEMAIL_SELECTOR(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_SELECTOR))
#define OSSO_ABOOK_IS_VOICEMAIL_SELECTOR_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_SELECTOR))
#define OSSO_ABOOK_VOICEMAIL_SELECTOR_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_SELECTOR, \
                 OssoABookVoicemailSelectorClass))

/**
 * OssoABookVoicemailSelector:
 *
 * A #HildonTouchSelector for chosing the prefered voicemail box number.
 */
struct _OssoABookVoicemailSelector {
        HildonTouchSelectorEntry parent;
};

struct _OssoABookVoicemailSelectorClass {
        HildonTouchSelectorEntryClass parent_class;
};

GType
osso_abook_voicemail_selector_get_type (void) G_GNUC_CONST;

GtkWidget *
osso_abook_voicemail_selector_new      (void);

void
osso_abook_voicemail_selector_apply    (OssoABookVoicemailSelector *selector);

gboolean
osso_abook_voicemail_selector_save     (OssoABookVoicemailSelector *selector);

G_END_DECLS

#endif /* __OSSO_ABOOK_VOICEMAIL_SELECTOR_H__ */

