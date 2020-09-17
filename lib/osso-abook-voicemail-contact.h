/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/* vim:set et sw=8 ts=8 sts=8: */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_VOICEMAIL_CONTACT_H__
#define __OSSO_ABOOK_VOICEMAIL_CONTACT_H__

#include "osso-abook-gconf-contact.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_VOICEMAIL_CONTACT \
                (osso_abook_voicemail_contact_get_type ())
#define OSSO_ABOOK_VOICEMAIL_CONTACT(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_CONTACT, \
                 OssoABookVoicemailContact))
#define OSSO_ABOOK_VOICEMAIL_CONTACT_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_CONTACT, \
                 OssoABookVoicemailContactClass))
#define OSSO_ABOOK_IS_VOICEMAIL_CONTACT(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_CONTACT))
#define OSSO_ABOOK_IS_VOICEMAIL_CONTACT_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_CONTACT))
#define OSSO_ABOOK_VOICEMAIL_CONTACT_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_VOICEMAIL_CONTACT, \
                 OssoABookVoicemailContactClass))

#define OSSO_ABOOK_VOICEMAIL_CONTACT_UID        "osso-abook-vmbx"
#define OSSO_ABOOK_VOICEMAIL_CONTACT_ICON_NAME  "general_voicemail_avatar"


/**
 * OssoABookVoicemailContact:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookVoicemailContact {
        OssoABookGconfContact parent;
};

struct _OssoABookVoicemailContactClass {
        OssoABookGconfContactClass parent_class;
};

GType
osso_abook_voicemail_contact_get_type                (void) G_GNUC_CONST;

OssoABookVoicemailContact *
osso_abook_voicemail_contact_new                     (void);

OssoABookVoicemailContact *
osso_abook_voicemail_contact_get_default             (void);

char *
osso_abook_voicemail_contact_get_preferred_number    (OssoABookVoicemailContact *contact);

G_END_DECLS

#endif /* __OSSO_ABOOK_VOICEMAIL_CONTACT_H__ */
