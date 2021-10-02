/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2007-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_ACCOUNT_MANAGER_H__
#define __OSSO_ABOOK_ACCOUNT_MANAGER_H__

#include "osso-abook-caps.h"
#include <telepathy-glib/account-manager.h>
#include <libebook/libebook.h>

G_BEGIN_DECLS

#define OSSO_ABOOK_TYPE_ACCOUNT_MANAGER \
                (osso_abook_account_manager_get_type ())
#define OSSO_ABOOK_ACCOUNT_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_CAST ((obj), \
                 OSSO_ABOOK_TYPE_ACCOUNT_MANAGER, \
                 OssoABookAccountManager))
#define OSSO_ABOOK_ACCOUNT_MANAGER_CLASS(cls) \
                (G_TYPE_CHECK_CLASS_CAST ((cls), \
                 OSSO_ABOOK_TYPE_ACCOUNT_MANAGER, \
                 OssoABookAccountManagerClass))
#define OSSO_ABOOK_IS_ACCOUNT_MANAGER(obj) \
                (G_TYPE_CHECK_INSTANCE_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_ACCOUNT_MANAGER))
#define OSSO_ABOOK_IS_ACCOUNT_MANAGER_CLASS(obj) \
                (G_TYPE_CHECK_CLASS_TYPE ((obj), \
                 OSSO_ABOOK_TYPE_ACCOUNT_MANAGER))
#define OSSO_ABOOK_ACCOUNT_MANAGER_GET_CLASS(obj) \
                (G_TYPE_INSTANCE_GET_CLASS ((obj), \
                 OSSO_ABOOK_TYPE_ACCOUNT_MANAGER, \
                 OssoABookAccountManagerClass))

typedef gboolean (* TpAccountFilterFunc)(TpAccount *account,
                                         gpointer user_data);

/**
 * OssoABookAccountManagerReadyCallback:
 * @manager: a #OssoABookAccountManager that has become ready
 * @error: %NULL on success, else a #GError to report error status
 * @user_data: additional user data
 *
 * A function type that you can register to have called when @manager becomes
 * ready
 */
typedef void (* OssoABookAccountManagerReadyCallback)
                                        (OssoABookAccountManager *manager,
                                         const GError            *error,
                                         gpointer                 user_data);

/**
 * OssoABookAccountManager:
 *
 * All the fields of this structure are private to the object's
 * implementation and should never be accessed directly.
 */
struct _OssoABookAccountManager {
        GObject parent_instance;
};

struct _OssoABookAccountManagerClass {
        GObjectClass parent_class;

        void (*account_created) (OssoABookAccountManager *manager,
                                 TpAccount               *account);
        void (*account_changed) (OssoABookAccountManager *manager,
                                 TpAccount               *account,
                                 GQuark                   property,
                                 const GValue            *value);
        void (*account_removed) (OssoABookAccountManager *manager,
                                 TpAccount               *account);
};

GType
osso_abook_account_manager_get_type         (void) G_GNUC_CONST;

OssoABookAccountManager *
osso_abook_account_manager_get_default      (void) G_GNUC_CONST;

OssoABookAccountManager *
osso_abook_account_manager_new              (void);

TpDBusDaemon *
osso_abook_account_manager_get_dbus_daemon  (OssoABookAccountManager *manager) G_GNUC_PURE;

TpConnectionPresenceType
osso_abook_account_manager_get_presence     (OssoABookAccountManager *manager);

void
osso_abook_account_manager_set_active_accounts_only
                                            (OssoABookAccountManager *manager,
                                             gboolean                 setting);

gboolean
osso_abook_account_manager_is_active_accounts_only
                                            (OssoABookAccountManager *manager);

void
osso_abook_account_manager_set_allowed_accounts
                                            (OssoABookAccountManager *manager,
                                             GList                   *accounts);

GList *
osso_abook_account_manager_get_allowed_accounts
                                            (OssoABookAccountManager *manager);

void
osso_abook_account_manager_set_allowed_capabilities
                                            (OssoABookAccountManager *manager,
                                             OssoABookCapsFlags       caps);

OssoABookCapsFlags
osso_abook_account_manager_get_allowed_capabilities
                                            (OssoABookAccountManager *manager);

void
osso_abook_account_manager_set_required_capabilities
                                            (OssoABookAccountManager *manager,
                                             OssoABookCapsFlags       caps);

OssoABookCapsFlags
osso_abook_account_manager_get_required_capabilities
                                            (OssoABookAccountManager *manager);

void
osso_abook_account_manager_set_account_protocol
                                            (OssoABookAccountManager *manager,
                                             const char              *protocol);

const char*
osso_abook_account_manager_get_account_protocol
                                            (OssoABookAccountManager *manager);

TpProtocol *
osso_abook_account_manager_get_account_protocol_object
                                            (OssoABookAccountManager *manager,
                                             TpAccount *account);
TpAccount *
osso_abook_account_manager_lookup_by_name   (OssoABookAccountManager *manager,
                                             const char              *name);

TpAccount *
osso_abook_account_manager_lookup_by_vcard_field
                                            (OssoABookAccountManager *manager,
                                             const char              *vcard_field,
                                             const char              *vcard_value);

G_GNUC_WARN_UNUSED_RESULT GList *
osso_abook_account_manager_list_accounts    (OssoABookAccountManager *manager,
                                             TpAccountFilterFunc      filter,
                                             gpointer                 user_data);
G_GNUC_WARN_UNUSED_RESULT GList *
osso_abook_account_manager_list_enabled_accounts
                                            (OssoABookAccountManager *manager);

/* replaces osso_abook_account_manager_list_by_profile */
G_GNUC_WARN_UNUSED_RESULT GList *
osso_abook_account_manager_list_by_protocol  (OssoABookAccountManager *manager,
                                              const char              *protocol);

G_GNUC_WARN_UNUSED_RESULT GList *
osso_abook_account_manager_list_by_vcard_field
                                            (OssoABookAccountManager *manager,
                                             const char              *vcard_field);

G_GNUC_WARN_UNUSED_RESULT GList *
osso_abook_account_manager_list_by_secondary_vcard_field
                                            (OssoABookAccountManager *manager,
                                             const char              *vcard_field);

const GList *
osso_abook_account_manager_get_primary_vcard_fields
                                            (OssoABookAccountManager *manager);

gboolean
osso_abook_account_manager_has_primary_vcard_field
                                            (OssoABookAccountManager *manager,
                                             const char              *vcard_field);

const GList *
osso_abook_account_manager_get_secondary_vcard_fields
                                            (OssoABookAccountManager *manager);

gboolean
osso_abook_account_manager_has_secondary_vcard_field
                                            (OssoABookAccountManager *manager,
                                             const char              *vcard_field);

const char *
osso_abook_account_manager_get_vcard_field  (OssoABookAccountManager *manager,
                                             const char              *account_name);

GList *
osso_abook_account_manager_list_protocols   (OssoABookAccountManager *manager,
                                             const gchar             *attr_name,
                                             gboolean                 no_roster_only);

void
osso_abook_account_manager_call_when_ready  (OssoABookAccountManager *manager,
                                             OssoABookAccountManagerReadyCallback
                                                                      callback,
                                             gpointer                 user_data,
                                             GDestroyNotify           destroy);

void
osso_abook_account_manager_wait_until_ready (OssoABookAccountManager *manager,
                                             GMainContext            *context,
                                             GError                 **error);

void
osso_abook_account_manager_set_roster_query (OssoABookAccountManager *manager,
                                             EBookQuery              *query);

TpProtocol *
osso_abook_account_manager_get_protocol_object
                                            (OssoABookAccountManager *manager,
                                             const char              *protocol);

TpProtocol *
osso_abook_account_manager_get_protocol_object_by_vcard_field
                                            (OssoABookAccountManager *manager,
                                             const char              *vcard_field);

GList *
osso_abook_account_manager_get_protocols    (OssoABookAccountManager *manager);

G_END_DECLS

#endif /* __OSSO_ABOOK_ACCOUNT_MANAGER_H__ */
