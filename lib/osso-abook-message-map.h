/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_I18N_H__
#define __OSSO_ABOOK_I18N_H__

#include "osso-abook-types.h"

#include <glib.h>

G_BEGIN_DECLS

struct _OssoABookMessageMapping
{
        const char *generic_id;
        const char *context_id;
};

GHashTable *
osso_abook_message_map_new    (const OssoABookMessageMapping *mappings);

const char *
osso_abook_message_map_lookup (GHashTable                    *map,
                               const char                    *msgid);

G_END_DECLS

#endif /* __OSSO_ABOOK_I18N_H__ */
