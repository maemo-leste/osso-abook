/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_CONTACT_DETAIL_STORE_H__
#define __OSSO_ABOOK_CONTACT_DETAIL_STORE_H__

#include <gtk/gtk.h>
#include "osso-abook-contact.h"
#include "osso-abook-contact-field.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE \
                (osso_abook_contact_detail_store_get_type ())
#define OSSO_ABOOK_CONTACT_DETAIL_STORE(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE, \
                 OssoABookContactDetailStore))
#define OSSO_ABOOK_CONTACT_DETAIL_STORE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_CAST ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE, \
                 OssoABookContactDetailStoreClass))
#define OSSO_ABOOK_IS_CONTACT_DETAIL_STORE(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE))
#define OSSO_ABOOK_IS_CONTACT_DETAIL_STORE_CLASS(klass) \
                (G_TYPE_CHECK_CLASS_TYPE ((klass), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE))
#define OSSO_ABOOK_CONTACT_DETAIL_STORE_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE, \
                 OssoABookContactDetailStoreClass))

/**
 * OssoABookContactDetailStore:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookContactDetailStore {
        /*< private >*/
        GObject parent;
};

struct _OssoABookContactDetailStoreClass {
        GObjectClass parent_class;
};

/**
 * OssoABookContactDetailFilters:
 * @OSSO_ABOOK_CONTACT_DETAIL_NONE: No filters
 * @OSSO_ABOOK_CONTACT_DETAIL_EMAIL: match email details
 * @OSSO_ABOOK_CONTACT_DETAIL_PHONE: match telephone details
 * @OSSO_ABOOK_CONTACT_DETAIL_IM_VOICE: match IM accounts with audio capability
 * @OSSO_ABOOK_CONTACT_DETAIL_IM_VIDEO: match IM accounts with video capability
 * @OSSO_ABOOK_CONTACT_DETAIL_IM_CHAT: match IM accounts with text chat capability
 * @OSSO_ABOOK_CONTACT_DETAIL_FULLNAME: match fullname detail
 * @OSSO_ABOOK_CONTACT_DETAIL_NICKNAME: match nickname detail
 * @OSSO_ABOOK_CONTACT_DETAIL_OTHERS: match all other details
 * @OSSO_ABOOK_CONTACT_DETAIL_SMS: match SMS details
 * @OSSO_ABOOK_CONTACT_DETAIL_ALL: match all details
 *
 * Flags to specify filters for the details in the #OssoABookContactDetailStore
 */
typedef enum {
        OSSO_ABOOK_CONTACT_DETAIL_NONE     = 0,
        OSSO_ABOOK_CONTACT_DETAIL_EMAIL    = 1 << 0,
        OSSO_ABOOK_CONTACT_DETAIL_PHONE    = 1 << 1,
        OSSO_ABOOK_CONTACT_DETAIL_IM_VOICE = 1 << 2,
        OSSO_ABOOK_CONTACT_DETAIL_IM_VIDEO = 1 << 3,
        OSSO_ABOOK_CONTACT_DETAIL_IM_CHAT  = 1 << 4,
        OSSO_ABOOK_CONTACT_DETAIL_FULLNAME = 1 << 5,
        OSSO_ABOOK_CONTACT_DETAIL_NICKNAME = 1 << 6,
        OSSO_ABOOK_CONTACT_DETAIL_OTHERS   = 1 << 7,
        OSSO_ABOOK_CONTACT_DETAIL_SMS      = 1 << 8,
        OSSO_ABOOK_CONTACT_DETAIL_ALL      = (1 << 9) - 1,
} OssoABookContactDetailFilters;

/**
 * OSSO_ABOOK_CONTACT_DETAIL_CALL:
 *
 * A filter mask for any type of call, either telephone or VoIP (audio or video)
 */
#define OSSO_ABOOK_CONTACT_DETAIL_CALL       \
        OSSO_ABOOK_CONTACT_DETAIL_PHONE |    \
        OSSO_ABOOK_CONTACT_DETAIL_IM_VOICE | \
        OSSO_ABOOK_CONTACT_DETAIL_IM_VIDEO

GType
osso_abook_contact_detail_store_get_type        (void) G_GNUC_CONST;

OssoABookContactDetailStore *
osso_abook_contact_detail_store_new             (OssoABookContact              *contact,
                                                 OssoABookContactDetailFilters  filters);

GHashTable *
osso_abook_contact_detail_store_get_message_map (OssoABookContactDetailStore   *self);

void
osso_abook_contact_detail_store_set_message_map (OssoABookContactDetailStore   *self,
                                                 const OssoABookMessageMapping *message_map);

OssoABookContact *
osso_abook_contact_detail_store_get_contact     (OssoABookContactDetailStore   *self);

void
osso_abook_contact_detail_store_set_contact     (OssoABookContactDetailStore   *self,
                                                 OssoABookContact              *contact);

void
osso_abook_contact_detail_store_set_filters     (OssoABookContactDetailStore   *self,
                                                 OssoABookContactDetailFilters  filters);

GSequence *
osso_abook_contact_detail_store_get_fields      (OssoABookContactDetailStore   *self);

gboolean
osso_abook_contact_detail_store_is_empty        (OssoABookContactDetailStore   *self);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_DETAIL_STORE_H__ */
