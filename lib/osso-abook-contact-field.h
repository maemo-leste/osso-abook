/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_CONTACT_FIELD_H__
#define __OSSO_ABOOK_CONTACT_FIELD_H__

#include "osso-abook-types.h"
#include "osso-abook-contact.h"

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_CONTACT_FIELD \
                (osso_abook_contact_field_get_type ())
#define OSSO_ABOOK_CONTACT_FIELD(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD, \
                 OssoABookContactField))
#define OSSO_ABOOK_CONTACT_FIELD_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD, \
                 OssoABookContactFieldClass))
#define OSSO_ABOOK_IS_CONTACT_FIELD(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD))
#define OSSO_ABOOK_IS_CONTACT_FIELD_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD))
#define OSSO_ABOOK_CONTACT_FIELD_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_CONTACT_FIELD, \
                 OssoABookContactFieldClass))

#define OSSO_ABOOK_CONTACT_FIELD_GROUP_STEP 100
#define OSSO_ABOOK_CONTACT_FIELD_GROUP_DIFF(weight1, weight2) \
        ((weight1)/OSSO_ABOOK_CONTACT_FIELD_GROUP_STEP !=         \
        (weight2)/OSSO_ABOOK_CONTACT_FIELD_GROUP_STEP)


/**
 * OssoABookContactFieldPredicate
 *
 * The signature of a function that can be used to select or reject fields in
 * osso_abook_contact_field_list_supported_fields()
 */
typedef gboolean (* OssoABookContactFieldPredicate) (OssoABookContactField *field,
                                                     gpointer               user_data);

/**
 * OssoABookContactFieldFlags
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE: No flags
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_HOME: The field is related to the contact's home
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_WORK: The field is related to the contact's work
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL: The field is related to the contact's mobile device
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE: The field is related to voice communication
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_FAX: The field is for sending Fax messages
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER: The field is related to something other than those previously described
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_DEVICE_MASK: A mask for fields that describe the device itself
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_USAGE_MASK: A mask for fields that describe the usage of the device
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_TYPE_MASK: A mask of all previous flags
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON: Only one field of this type is allowed
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_MANDATORY: The field must exist for every contact
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_DYNAMIC: The field is dynamic
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_NO_LABEL: The field has no label
 * @OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED: The field has additional details
 * (e.g. "Phone (home)")
 *
 * A set of flags that describe various properties about a
 * #OssoABookContactField
 */
typedef enum {
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_NONE        = (0),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_HOME        = (1 << 0),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_WORK        = (1 << 1),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL        = (1 << 2),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE       = (1 << 3),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER       = (1 << 4),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_FAX         = (1 << 5),

        OSSO_ABOOK_CONTACT_FIELD_FLAGS_USAGE_MASK  = (OSSO_ABOOK_CONTACT_FIELD_FLAGS_HOME | OSSO_ABOOK_CONTACT_FIELD_FLAGS_WORK),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_DEVICE_MASK = (OSSO_ABOOK_CONTACT_FIELD_FLAGS_CELL | OSSO_ABOOK_CONTACT_FIELD_FLAGS_VOICE | OSSO_ABOOK_CONTACT_FIELD_FLAGS_FAX),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_TYPE_MASK   = (OSSO_ABOOK_CONTACT_FIELD_FLAGS_DEVICE_MASK | OSSO_ABOOK_CONTACT_FIELD_FLAGS_USAGE_MASK | OSSO_ABOOK_CONTACT_FIELD_FLAGS_OTHER),

        OSSO_ABOOK_CONTACT_FIELD_FLAGS_SINGLETON   = (1 << 10),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_MANDATORY   = (1 << 11),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_DYNAMIC     = (1 << 12),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_NO_LABEL    = (1 << 13),
        OSSO_ABOOK_CONTACT_FIELD_FLAGS_DETAILED    = (1 << 14),
} OssoABookContactFieldFlags;

/**
 * OssoABookContactFieldActionLayoutFlags
 * @OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_NORMAL: Action should be placed in
 * the same row than previous actions, or in a new row if the layout don't have
 * enough columns. This is the normal behaviour to fill completely the table.
 * @OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY: Action should be placed in
 * a new row, even if previous row was not completely filled.
 * @OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_EXPANDABLE: Like PRIMARY if the
 * action is alone in its group. Like NORMAL otherwise.
 * @OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_EXTRA: Action should be placed after
 * all other actions from #OssoABookContactField having the same sort weight.
 *
 * Hints telling how a #OssoABookContactFieldAction should be placed in UI like
 * #OssoABookTouchContactStarter.
 *
 * Examples:
 *  - Cell-call action is PRIMARY.
 *  - SMS action is EXPANDABLE, to be placed in the same row than Cell-call if
 *    there are more than one phone number, on a new row otherwise.
 *  - SkypeOut/SipOut-call action is EXTRA|PRIMARY, to be placed after all other
 *    cell-call and SMS actions, in a new row.
 *  - SipOut-SMS action is EXTRA, to be placed after all other
 *    cell-call and SMS actions and on the same row than SipOut-call action.
 */
typedef enum {
        OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_NORMAL     = 0,
        OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_PRIMARY    = (1 << 0),
        OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_EXPANDABLE = (1 << 1),
        OSSO_ABOOK_CONTACT_FIELD_ACTION_LAYOUT_EXTRA      = (1 << 2),
} OssoABookContactFieldActionLayoutFlags;

struct _OssoABookContactField {
        GObject parent_instance;
};

struct _OssoABookContactFieldClass {
        GObjectClass parent_class;

        void (* update_label) (OssoABookContactField *field);

        /* Padding for future expansion */
        void (* _reserved0) (void);
        void (* _reserved1) (void);
        void (* _reserved2) (void);
        void (* _reserved3) (void);
        void (* _reserved4) (void);
        void (* _reserved5) (void);
        void (* _reserved6) (void);
        void (* _reserved7) (void);
};

GType
osso_abook_contact_field_get_type                 (void) G_GNUC_CONST;

OssoABookContactField *
osso_abook_contact_field_new_full                 (GHashTable                  *message_map,
                                                   OssoABookContact            *master_contact,
                                                   OssoABookContact            *roster_contact,
                                                   EVCardAttribute             *attribute);

OssoABookContactField *
osso_abook_contact_field_new                      (GHashTable                  *message_map,
                                                   OssoABookContact            *master_contact,
                                                   EVCardAttribute             *attribute);

GHashTable *
osso_abook_contact_field_get_message_map          (OssoABookContactField       *field);

void
osso_abook_contact_field_set_message_map          (OssoABookContactField       *field,
                                                   GHashTable                  *mapping);

const char *
osso_abook_contact_field_get_name                 (OssoABookContactField       *field);

const char *
osso_abook_contact_field_get_path                 (OssoABookContactField       *field);

OssoABookContactFieldFlags
osso_abook_contact_field_get_flags                (OssoABookContactField       *field);

OssoABookContactField *
osso_abook_contact_field_get_parent               (OssoABookContactField       *field);

GList *
osso_abook_contact_field_get_children             (OssoABookContactField       *field);

gboolean
osso_abook_contact_field_has_children             (OssoABookContactField       *field);

EVCardAttribute *
osso_abook_contact_field_get_attribute            (OssoABookContactField       *field);

EVCardAttribute *
osso_abook_contact_field_get_borrowed_attribute   (OssoABookContactField       *field);

GList *
osso_abook_contact_field_get_secondary_attributes (OssoABookContactField       *field);

const char *
osso_abook_contact_field_get_display_title        (OssoABookContactField       *field);

const char *
osso_abook_contact_field_get_secondary_title      (OssoABookContactField       *field);

const char *
osso_abook_contact_field_get_display_value        (OssoABookContactField       *field);

const char *
osso_abook_contact_field_get_icon_name            (OssoABookContactField       *field);

int
osso_abook_contact_field_get_sort_weight          (OssoABookContactField       *field);

OssoABookContact *
osso_abook_contact_field_get_master_contact       (OssoABookContactField       *field);

OssoABookContact *
osso_abook_contact_field_get_roster_contact       (OssoABookContactField       *field);

TpAccount *
osso_abook_contact_field_get_account              (OssoABookContactField       *field);

GList *
osso_abook_contact_field_get_actions_full         (OssoABookContactField       *field,
                                                   gboolean                     button);

GList *
osso_abook_contact_field_get_actions              (OssoABookContactField       *field);

TpAccount *
osso_abook_contact_field_action_request_account   (OssoABookContactFieldAction *action,
                                                   GtkWindow                   *parent,
                                                   gboolean                    *aborted);

gboolean
osso_abook_contact_field_action_start             (OssoABookContactFieldAction *action,
                                                   GtkWindow                   *parent);

gboolean
osso_abook_contact_field_action_start_with_callback (OssoABookContactFieldAction   *action,
                                                     GtkWindow                     *parent,
                                                     OssoABookContactActionStartCb  callback,
                                                     gpointer                       callback_data);

OssoABookContactFieldAction *
osso_abook_contact_field_action_ref               (OssoABookContactFieldAction *action);

void
osso_abook_contact_field_action_unref             (OssoABookContactFieldAction *action);

OssoABookContactAction
osso_abook_contact_field_action_get_action        (OssoABookContactFieldAction *action);

OssoABookContactField *
osso_abook_contact_field_action_get_field         (OssoABookContactFieldAction *action);

GtkWidget *
osso_abook_contact_field_action_get_widget        (OssoABookContactFieldAction *action);

/* replaces osso_abook_contact_field_action_get_profile */
TpProtocol *
osso_abook_contact_field_action_get_protocol      (OssoABookContactFieldAction *action);

OssoABookContactFieldActionLayoutFlags
osso_abook_contact_field_action_get_layout_flags  (OssoABookContactFieldAction *action);

G_GNUC_DEPRECATED gboolean
osso_abook_contact_field_action_is_basic          (OssoABookContactFieldAction *action);

GtkWidget *
osso_abook_contact_field_create_button            (OssoABookContactField       *field,
                                                   const char                  *title,
                                                   const char                  *icon_name);

GtkWidget *
osso_abook_contact_field_get_label_widget         (OssoABookContactField       *field);

GtkWidget *
osso_abook_contact_field_get_editor_widget        (OssoABookContactField       *field);

gboolean
osso_abook_contact_field_has_editor_widget        (OssoABookContactField       *field);

gboolean
osso_abook_contact_field_is_readonly              (OssoABookContactField       *field);

gboolean
osso_abook_contact_field_is_bound_im              (OssoABookContactField       *field);

gboolean
osso_abook_contact_field_is_modified              (OssoABookContactField       *field);

gboolean
osso_abook_contact_field_is_empty                 (OssoABookContactField       *field);

gboolean
osso_abook_contact_field_is_mandatory             (OssoABookContactField       *field);

void
osso_abook_contact_field_set_mandatory            (OssoABookContactField       *field,
                                                   gboolean                     mandatory);

gboolean
osso_abook_contact_field_activate                 (OssoABookContactField       *field);

int
osso_abook_contact_field_cmp                      (OssoABookContactField       *a,
                                                   OssoABookContactField       *b);

G_GNUC_WARN_UNUSED_RESULT GList *
osso_abook_contact_field_list_supported_fields    (GHashTable                  *message_map,
                                                   OssoABookContact            *master_contact,
                                                   OssoABookContactFieldPredicate
                                                                                accept_field,
                                                   gpointer                     user_data);

G_GNUC_WARN_UNUSED_RESULT GList *
osso_abook_contact_field_list_account_fields      (GHashTable                  *message_map,
                                                   OssoABookContact            *master_contact);

G_END_DECLS

#endif /* __OSSO_ABOOK_CONTACT_FIELD_H__ */

