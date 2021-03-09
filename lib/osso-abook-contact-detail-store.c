/*
 * osso-abook-presence-label.c
 *
 * Copyright (C) 2021 Merlijn Wajer <merlijn@wizzup.org>
 *
 * This library is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License
 * for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library. If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include <gtk/gtkprivate.h>
#include <hildon/hildon.h>

#include "config.h"

#include "osso-abook-enums.h"
#include "osso-abook-marshal.h"
#include "osso-abook-contact.h"

#include "osso-abook-contact-detail-store.h"

struct _OssoABookContactDetailStorePrivate {
	GHashTable *message_map;
	OssoABookContact *contact;
	OssoABookContactDetailFilters details_filters_flags;
	GSequence *fields;
	int sequence_is_empty;
	int probably_initialised;
};

typedef struct _OssoABookContactDetailStorePrivate
 OssoABookContactDetailStorePrivate;

G_DEFINE_TYPE_WITH_PRIVATE(OssoABookContactDetailStore,
			   osso_abook_contact_detail_store, G_TYPE_OBJECT);

enum {
	MESSAGE_MAP = 1,
	CONTACT = 2,
	FILTERS = 3,
};

static int changed_signal = 0;
static int contact_changed_signal = 0;

#define OSSO_ABOOK_CONTACT_DETAIL_STORE_PRIVATE(store) \
                ((OssoABookContactDetailStorePrivate *)osso_abook_detail_store_get_instance_private(store))

void
osso_abook_contact_detail_store_class_init(OssoABookContactDetailStoreClass
					   * klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS(klass);

#if 0
	object_class->set_property =
	    osso_abook_contact_detail_store_set_property;
	object_class->get_property =
	    osso_abook_contact_detail_store_get_property;
	object_class->constructed = osso_abook_contact_detail_store_constructed;
	object_class->dispose = osso_abook_contact_detail_store_dispose;
#endif

	g_object_class_install_property(object_class,
					MESSAGE_MAP,
					g_param_spec_boxed("message-map",
							   "Message Map",
							   "Mapping for generic message ids to context ids",
							   G_TYPE_HASH_TABLE,
							   GTK_PARAM_READWRITE));

	g_object_class_install_property(object_class, CONTACT,
					g_param_spec_object("contact",
							    "Contact",
							    "The contact of which to show details",
							    OSSO_ABOOK_TYPE_CONTACT,
							    GTK_PARAM_READWRITE));

	g_object_class_install_property(object_class, FILTERS,
                    g_param_spec_flags("filters", "Filters",
                        "The contact field filters to use",
                        OSSO_ABOOK_TYPE_CONTACT_DETAIL_FILTERS,
                        0,	/* TODO: check this */
                        GTK_PARAM_READWRITE));

	changed_signal = g_signal_new("changed",
            OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE, G_SIGNAL_RUN_LAST,
            0,	/* XXX: offset? */
            0, NULL, g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

	contact_changed_signal = g_signal_new("contact-changed",
            OSSO_ABOOK_TYPE_CONTACT_DETAIL_STORE, G_SIGNAL_RUN_LAST,
            0, /* XXX: offset? */
            0, NULL, osso_abook_marshal_VOID__OBJECT_OBJECT,
            G_TYPE_NONE, 2u,	/* XXX: What is this 2u? two args? */
            OSSO_ABOOK_TYPE_CONTACT,
            OSSO_ABOOK_TYPE_CONTACT);
}

void osso_abook_contact_detail_store_init(OssoABookContactDetailStore * store)
{
}
