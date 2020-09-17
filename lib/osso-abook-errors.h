/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_ERROR_H__
#define __OSSO_ABOOK_ERROR_H__

#include <gtk/gtk.h>
#include <libebook/libebook.h>

G_BEGIN_DECLS

/**
 * osso_abook_check_disc_space
 * @parent: parent #GtkWindow for error banner
 *
 * This function checks whether there is a 'reasonable' amount of disk space
 * available.  As a side-effect, if the disk space is too low, a warning banner
 * will be displayed.
 *
 * <emphasis>NOTE:</emphasis> It is recommended that applications avoid this function
 *
 * Returns: TRUE if there is a "reasonable" amount of disk space available,
 * FALSE otherwise.
 */
#define osso_abook_check_disc_space(parent) \
        _osso_abook_check_disc_space ((parent), G_STRLOC, NULL)
/**
 * osso_abook_check_disc_space_full
 * @parent: parent #GtkWindow for error banner
 * @path: the path to 
 *
 * This function is like #osso_abook_check_disc_space, but checks for available
 * space on the partition that contains @path.  Like
 * #osso_abook_check_disc_space, a warning banner will be displayed if
 * sufficient disk space is not available.
 *
 * <emphasis>NOTE:</emphasis> It is recommended that applications avoid this function
 *
 * Returns: TRUE if there is a "reasonable" amount of disk space available,
 * FALSE otherwise.
 */
#define osso_abook_check_disc_space_full(parent, path) \
        _osso_abook_check_disc_space ((parent), G_STRLOC, (path))

/**
 * osso_abook_handle_gerror
 * @parent: The parent #GtkWindow for error banner
 * @error: the #GError object to be handled
 *
 * Displays a general error banner with a message appropriate for the specified
 * #GError.  An error message will also be logged to stdout.  In addition,
 * @error will be freed within this function, so you don't need to explicitly
 * free it.
 */
#define osso_abook_handle_gerror(parent, error) \
        _osso_abook_handle_gerror ((parent), G_STRLOC, (error))

/**
 * osso_abook_handle_estatus
 * @parent: the parent #GtkWindow for error banner
 * @error: the #EBookStatus variable to handle
 * @book: optional #EBook associated with the error
 *
 * Displays a general error banner with a message appropriate for the specified
 * #EBookStatus.  An additional error message will be logged to stdout
 */
#define osso_abook_handle_estatus(parent, error, book) \
        _osso_abook_handle_estatus ((parent), G_STRLOC, (error), (book))

#define OSSO_ABOOK_ERROR osso_abook_error_quark ()
typedef enum {
        OSSO_ABOOK_ERROR_OFFLINE,
        OSSO_ABOOK_ERROR_EBOOK,
        OSSO_ABOOK_ERROR_CANCELLED,
        OSSO_ABOOK_ERROR_MAPS_FAILED,
} OssoABookError;

GQuark
osso_abook_error_quark (void);

/**
 * osso_abook_error_new_from_estatus
 * @status: an #EBookStatus value
 *
 * Creates a #GError of type OSSO_ABOOK_ERROR_EBOOK that represents the error
 * status specified by @status
 *
 * Returns: A newly-allocated #GError, or NULL if status is E_BOOK_ERROR_OK.
 */
GError *
osso_abook_error_new_from_estatus (EBookStatus status);

gboolean
_osso_abook_check_disc_space (GtkWindow  *parent,
                              const char *strloc,
                              const char *path);

void
_osso_abook_handle_gerror    (GtkWindow  *parent,
                              const char *strloc,
                              GError     *error);

void
_osso_abook_handle_estatus   (GtkWindow  *parent,
                              const char *strloc,
                              EBookStatus status,
                              EBook      *book);

G_END_DECLS

#endif /* __ERROR_H__ */
