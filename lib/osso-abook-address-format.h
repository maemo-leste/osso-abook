/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2007-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_ADDRESS_FORMAT_H__
#define __OSSO_ABOOK_ADDRESS_FORMAT_H__

typedef struct _OssoABookAddress OssoABookAddress;

struct _OssoABookAddress {
    char *p_o_box;
    char *extension;
    char *street;
    char *city;
    char *region;
    char *postal;
    char *country;
};

char* osso_abook_address_format (OssoABookAddress *address);

#endif
