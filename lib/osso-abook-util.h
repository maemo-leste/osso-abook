/* -*- Mode: C; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 8 -*- */
/*
 * This file is part of libosso-abook
 *
 * Copyright (C) 2008-2009 Nokia Corporation. All rights reserved.
 *
 * Contact: Joergen Scheibengruber <jorgen.scheibengruber@nokia.com>
 */

#ifndef __OSSO_ABOOK_UTIL_H__
#define __OSSO_ABOOK_UTIL_H__

#include <gio/gio.h>
#include <hildon/hildon.h>
#include <libebook/libebook.h>
#include <rtcom-eventlogger/eventlogger.h>
#include <telepathy-glib/telepathy-glib.h>

#include "osso-abook-avatar.h"
#include "osso-abook-settings.h"

G_BEGIN_DECLS

/**
 * OSSO_ABOOK_DEFAULT_MAXIMUM_PIXBUF_SIZE
 *
 * The largest size allowed for avatar images
 */
#define OSSO_ABOOK_DEFAULT_MAXIMUM_PIXBUF_SIZE  (6 * 1024 * 1024) /* 6 Mega Pixels */
/**
 * OSSO_ABOOK_INVALID_FILENAME_CHARS
 *
 * A string of characters that are invalid to use in filenames
 */
#define OSSO_ABOOK_INVALID_FILENAME_CHARS "\\/:*?\"<>|"
/**
 * OSSO_ABOOK_FILENAME_PLACEHOLDER_CHAR
 *
 * A character that should be used in place of the invalid char as listed in
 * OSSO_ABOOK_INVALID_FILENAME_CHARS
 */
#define OSSO_ABOOK_FILENAME_PLACEHOLDER_CHAR '_'

char *
osso_abook_tp_account_get_display_string (TpAccount          *account,
                                          const char         *username,
                                          const char         *format);

char *
osso_abook_tp_account_get_display_markup (TpAccount          *account);

const char *
osso_abook_tp_account_get_bound_name     (TpAccount          *account);

gboolean
osso_abook_screen_is_landscape_mode      (GdkScreen          *screen);

void
osso_abook_attach_screen_size_handler    (GtkWindow          *window);

GtkWidget *
osso_abook_pannable_area_new             (void);

#ifndef OSSO_ABOOK_DISABLE_DEPRECATED

G_GNUC_DEPRECATED
G_GNUC_WARN_UNUSED_RESULT char *
osso_abook_normalize_phone_number        (const char         *phone_number);

#endif /* OSSO_ABOOK_DISABLE_DEPRECATED */

EBookQuery *
osso_abook_query_phone_number            (const char         *phone_number,
                                          gboolean            fuzzy_match);

GList *
osso_abook_sort_phone_number_matches     (GList              *matches,
                                          const char         *phone_number);

void
osso_abook_load_pixbuf_async             (GFile              *file,
                                          gsize               maximum_size,
                                          int                 io_priority,
                                          GCancellable       *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer            user_data);

void
osso_abook_load_pixbuf_at_scale_async    (GFile              *file,
                                          int                 width,
                                          int                 height,
                                          gboolean            preserve_aspect_ratio,
                                          gsize               maximum_size,
                                          int                 io_priority,
                                          GCancellable       *cancellable,
                                          GAsyncReadyCallback callback,
                                          gpointer            user_data);

GdkPixbuf *
osso_abook_load_pixbuf_finish            (GFile              *file,
                                          GAsyncResult       *result,
                                          GError            **error);

char *
osso_abook_concat_names                  (OssoABookNameOrder  order,
                                          const gchar        *primary,
                                          const gchar        *secondary);

EBook *
osso_abook_system_book_dup_singleton     (gboolean            open,
                                          GError            **error);

GtkWidget *
osso_abook_avatar_button_new             (OssoABookAvatar    *avatar,
                                          int                 size);

void
osso_abook_set_portrait_mode_supported   (GtkWindow          *window,
                                          gboolean            flag);

void
osso_abook_set_portrait_mode_requested   (GtkWindow          *window,
                                          gboolean            flag);

void
osso_abook_set_zoom_key_used             (GtkWindow          *window,
                                          gboolean            flag);

gboolean
osso_abook_file_set_contents             (const char         *filename,
                                          const void         *contents,
                                          gssize              length,
                                          GError            **error);

const char *
osso_abook_get_work_dir                  (void);

void
osso_abook_enforce_label_width           (GtkLabel           *label,
                                          int                 width);

inline static void
osso_abook_weak_ref (GObject **object)
{
        g_return_if_fail (NULL != object);
        g_object_add_weak_pointer (*object, (gpointer *) object);
}

inline static void
osso_abook_weak_unref (GObject **object)
{
        g_return_if_fail (NULL != object);
        g_object_remove_weak_pointer (*object, (gpointer *) object);
}

G_GNUC_DEPRECATED gpointer
osso_abook_get_rtcom_el (void);

GtkWidget *
osso_abook_live_search_new_with_filter (OssoABookFilterModel *filter);

gboolean
osso_abook_is_fax_attribute (EVCardAttribute *attribute);

gboolean
osso_abook_is_mobile_attribute (EVCardAttribute *attribute);

const gchar *
osso_abook_strip_sip_prefix (const gchar *address);

EVCardAttribute *
osso_abook_convert_to_tel_attribute (EVCardAttribute *attribute);

gboolean
osso_abook_phone_numbers_equal (const gchar *tel_a,
                                const gchar *tel_b,
                                gboolean     fuzzy_match);

gboolean
osso_abook_attributes_match (EVCardAttribute *attr_a,
                             EVCardAttribute *attr_b,
                             gboolean         fuzzy_match);

G_END_DECLS

#endif

