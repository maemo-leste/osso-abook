bin_PROGRAMS = \
		test_voicemail_selector \
		test_avatar_editor_dialog \
		test_avatar_chooser_dialog \
		test_contact_chooser \
		test_contact_editor \
		test_settings

COMMON_CFLAGS = \
		-D_GNU_SOURCE $(OSSO_CFLAGS) $(HILDON_CFLAGS) $(EBOOK_CFLAGS) \
		$(GMODULE_CFLAGS) $(GCONF_CFLAGS) $(TPGLIB_CFLAGS) \
		$(CAIRO_CFLAGS) $(RTCOM_CFLAGS) $(XML_CFLAGS) $(GOFONO_CFLAGS) \
		$(MODEST_DBUS_CFLAGS) -I$(srcdir)/../include -Wall -Werror \
		-DOSSO_ABOOK_DEBUG -DOSSOLOG_COMPILE=1 \
		-DISO_CODES_PREFIX=\"$(ISO_CODES_PREFIX)\" \
		-DMBPI_DATABASE=\"$(MBPI_DATABASE)\" -Wno-format-extra-args \
		$(DGETTEXT)

COMMON_LIBS = \
		$(OSSO_LIBS) $(HILDON_LIBS) $(EBOOK_LIBS) $(RTCOM_LIBS) \
		$(GMODULE_LIBS) $(GCONF_LIBS) $(TPGLIB_LIBS) $(XML_LIBS) \
		$(CAIRO_LIBS) $(GOFONO_LIBS) $(MODEST_DBUS_LIBS) -lm

TEST_LIBS = $(COMMON_LIBS) libosso-abook-@API_VERSION_MAJOR@.la

test_avatar_editor_dialog_SOURCES = test-avatar-editor-dialog.c
test_avatar_editor_dialog_CFLAGS = $(COMMON_CFLAGS) -DTEST
test_avatar_editor_dialog_LDADD = $(TEST_LIBS)

test_avatar_chooser_dialog_SOURCES = test-avatar-chooser-dialog.c
test_avatar_chooser_dialog_CFLAGS = $(COMMON_CFLAGS) -DTEST
test_avatar_chooser_dialog_LDADD = $(TEST_LIBS)

test_contact_chooser_SOURCES = test-contact-chooser.c
test_contact_chooser_CFLAGS = $(COMMON_CFLAGS) -DTEST
test_contact_chooser_LDADD = $(TEST_LIBS)

test_voicemail_selector_SOURCES = test-voicemail-selector.c
test_voicemail_selector_CFLAGS = $(COMMON_CFLAGS) -DTEST
test_voicemail_selector_LDADD = $(TEST_LIBS)

test_contact_editor_SOURCES = test-contact-editor.c
test_contact_editor_CFLAGS = $(COMMON_CFLAGS) -DTEST
test_contact_editor_LDADD = $(TEST_LIBS)

test_settings_SOURCES = test-settings.c
test_settings_CFLAGS = $(COMMON_CFLAGS) -DTEST
test_settings_LDADD = $(TEST_LIBS)

lib_LTLIBRARIES = libosso-abook-@API_VERSION_MAJOR@.la

libosso_abook_@API_VERSION_MAJOR@_includedir = \
		$(includedir)/$(PACKAGE)-$(API_VERSION_MAJOR)/$(PACKAGE)

libosso_abook_@API_VERSION_MAJOR@_la_CFLAGS = \
		$(COMMON_CFLAGS)

libosso_abook_@API_VERSION_MAJOR@_la_LDFLAGS = -Wl,--no-undefined
libosso_abook_@API_VERSION_MAJOR@_la_LIBADD = \
		$(COMMON_LIBS)

libosso_abook_@API_VERSION_MAJOR@_la_SOURCES = \
		osso-abook-enums.c \
		osso-abook-marshal.c \
		tp-glib-enums.c \
		osso-abook-init.c \
		osso-abook-debug.c \
		osso-abook-errors.c \
		osso-abook-string-list.c \
		osso-abook-address-format.c \
		osso-abook-contact-chooser.c \
		osso-abook-filter-model.c \
		osso-abook-row-model.c \
		osso-abook-list-store.c \
		osso-abook-waitable.c \
		osso-abook-util.c \
		osso-abook-contact-view.c \
		osso-abook-tree-view.c \
		osso-abook-contact-model.c \
		osso-abook-presence.c \
		osso-abook-caps.c \
		osso-abook-settings.c \
		osso-abook-avatar.c \
		osso-abook-group.c \
		osso-abook-all-group.c \
		osso-abook-avatar-cache.c \
		eds.c \
		osso-abook-roster.c \
		osso-abook-gconf-contact.c \
		osso-abook-voicemail-contact.c \
		osso-abook-voicemail-selector.c \
		osso-abook-aggregator.c \
		osso-abook-contact.c \
		osso-abook-utils-private.c \
		osso-abook-roster-manager.c \
		osso-abook-contact-filter.c \
		osso-abook-eventlogger.c \
		osso-abook-msgids.c \
		osso-abook-quarks.c \
		osso-abook-account-manager.c \
		osso-abook-service-group.c \
		osso-abook-contact-subscriptions.c \
		osso-abook-plugin.c \
		osso-abook-menu-extension.c \
		osso-abook-plugin-manager.c \
		osso-abook-presence-label.c \
		osso-abook-message-map.c \
		osso-abook-avatar-editor.c \
		osso-abook-avatar-editor-dialog.c \
		osso-abook-touch-contact-starter.c \
		osso-abook-entry.c \
		osso-abook-button.c \
		osso-abook-avatar-image.c \
		osso-abook-icon-sizes.c \
		osso-abook-avatar-chooser-dialog.c \
		osso-abook-contact-field.c \
		osso-abook-contact-editor.c \
		osso-abook-contact-field-selector.c \
		osso-abook-contact-detail-store.c \
		osso-abook-dialogs.c \
		osso-abook-tp-account-model.c \
		osso-abook-tp-account-selector.c \
		avatar.c \
		osso-abook-send-contacts.c \
		osso-abook-temporary-contact-dialog.c \
		osso-abook-merge.c \
		osso-abook-alpha-shortcuts.c \
		osso-abook-self-contact.c \
		osso-abook-presence-icon.c \
		osso-abook-contact-detail-selector.c \
		osso-abook-recent-group.c \
		osso-abook-settings-dialog.c

libosso_abook_@API_VERSION_MAJOR@_public_headers = \
		osso-abook.h \
		osso-abook-aggregator.h \
		osso-abook-settings.h \
		osso-abook-caps.h \
		osso-abook-debug.h \
		osso-abook-presence.h \
		osso-abook-contact.h \
		osso-abook-errors.h \
		osso-abook-init.h \
		osso-abook-contact-model.h \
		osso-abook-list-store.h \
		osso-abook-avatar.h \
		osso-abook-avatar-image.h \
		osso-abook-avatar-chooser-dialog.h \
		osso-abook-avatar-editor.h \
		osso-abook-avatar-editor-dialog.h \
		osso-abook-icon-sizes.h \
		osso-abook-types.h \
		osso-abook-roster-manager.h \
		osso-abook-account-manager.h \
		osso-abook-tree-view.h \
		osso-abook-filter-model.h \
		osso-abook-all-group.h \
		osso-abook-group.h \
		osso-abook-roster.h \
		osso-abook-contact-filter.h \
		osso-abook-service-group.h \
		osso-abook-waitable.h \
		osso-abook-gconf-contact.h \
		osso-abook-contact-subscriptions.h \
		osso-abook-util.h \
		osso-abook-log.h \
		osso-abook-contact-view.h \
		osso-abook-menu-extension.h \
		osso-abook-plugin.h \
		osso-abook-plugin-manager.h \
		osso-abook-button.h \
		osso-abook-contact-editor.h \
		osso-abook-contact-field.h \
		osso-abook-contact-detail-store.h \
		osso-abook-row-model.h \
		osso-abook-tp-account-model.h \
		osso-abook-tp-account-selector.h \
		osso-abook-touch-contact-starter.h \
		osso-abook-merge.h \
		osso-abook-voicemail-contact.h \
		osso-abook-send-contacts.h \
		osso-abook-avatar-cache.h \
		osso-abook-contact-chooser.h \
		osso-abook-contact-detail-selector.h \
		osso-abook-contact-field-selector.h \
		osso-abook-dialogs.h \
		osso-abook-mecard-view.h \
		osso-abook-message-map.h \
		osso-abook-presence-icon.h \
		osso-abook-presence-label.h \
		osso-abook-profile-group.h \
		osso-abook-quarks.h \
		osso-abook-recent-group.h \
		osso-abook-live-search.h \
		osso-abook-alpha-shortcuts.h \
		osso-abook-self-contact.h \
		osso-abook-settings-dialog.h \
		osso-abook-temporary-contact-dialog.h \
		osso-abook-voicemail-selector.h

libosso_abook_@API_VERSION_MAJOR@_built_public_headers  = \
		osso-abook-enums.h

libosso_abook_@API_VERSION_MAJOR@_include_HEADERS = \
		$(libosso_abook_@API_VERSION_MAJOR@_public_headers) \
		$(libosso_abook_@API_VERSION_MAJOR@_built_public_headers)

osso-abook-enums.h: $(libosso_abook_$(API_VERSION_MAJOR)_public_headers)
	(cd $(srcdir) && glib-mkenums --identifier-prefix OssoABook \
	--symbol-prefix OSSO_ABOOK --template osso-abook-enums.h.in \
	--output $@ \
	$(libosso_abook_$(API_VERSION_MAJOR)_public_headers))

osso-abook-enums.c: $(libosso_abook_$(API_VERSION_MAJOR)_public_headers)
	(cd $(srcdir) && glib-mkenums --output $@ \
	--template osso-abook-enums.c.in \
	$(libosso_abook_$(API_VERSION_MAJOR)_public_headers))

osso-abook-marshal.h: osso-abook-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=osso_abook_marshal \
	$< > $@

osso-abook-marshal.c: osso-abook-marshal.list
	(cd $(srcdir) && echo "#include \"config.h\"" > $@) && \
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --body --prefix=osso_abook_marshal \
	$< >> $@

tp-glib-enums.tmp.h:
	(cd $(srcdir) && (echo | \
	gcc -E -P -include telepathy-glib/_gen/telepathy-enums.h \
	$(TPGLIB_CFLAGS) -o $@ -))

tp-glib-enums.h: tp-glib-enums.tmp.h
	(cd $(srcdir) && glib-mkenums --identifier-prefix Tp \
	--symbol-prefix TP --template tp-glib-enums.h.in --output $@ $<)

tp-glib-enums.c: tp-glib-enums.tmp.h
	(cd $(srcdir) && glib-mkenums --identifier-prefix Tp \
	--symbol-prefix TP --template tp-glib-enums.c.in --output $@ $<)

BUILT_SOURCES = \
	osso-abook-enums.h \
	osso-abook-enums.c \
	osso-abook-marshal.h \
	osso-abook-marshal.c \
	tp-glib-enums.tmp.h \
	tp-glib-enums.h \
	tp-glib-enums.c

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in
