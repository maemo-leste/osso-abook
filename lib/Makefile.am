bin_PROGRAMS = \
		test_avatar_editor_dialog \
		test_avatar_chooser_dialog \
		test_contact_chooser

# this shall be removed once we are able to build the whole library
COMMON_SOURCES = \
		$(BUILT_SOURCES) \
		osso-abook-init.c \
		osso-abook-debug.c \
		osso-abook-util.c \
		osso-abook-errors.c \
		osso-abook-string-list.c \
		osso-abook-utils-private.c \
		osso-abook-list-store.c \
		osso-abook-row-model.c \
		eds.c \
		osso-abook-contact-filter.c \
		osso-abook-filter-model.c \
		osso-abook-gconf-contact.c \
		osso-abook-voicemail-contact.c \
		osso-abook-group.c \
		osso-abook-all-group.c \
		osso-abook-presence.c \
		osso-abook-caps.c \
		osso-abook-settings.c \
		osso-abook-roster.c \
		osso-abook-roster-manager.c \
		osso-abook-msgids.c \
		osso-abook-quarks.c \
		osso-abook-account-manager.c \
		osso-abook-waitable.c \
		osso-abook-aggregator.c \
		osso-abook-contact.c \
		osso-abook-eventlogger.c \
		osso-abook-avatar-image.c \
		osso-abook-avatar.c \
		osso-abook-avatar-cache.c \
		osso-abook-icon-sizes.c

test_avatar_editor_dialog_SOURCES = \
		$(COMMON_SOURCES) \
		test-avatar-editor-dialog.c \
		osso-abook-avatar-editor.c \
		osso-abook-avatar-editor-dialog.c

test_avatar_editor_dialog_CFLAGS = \
		-D_GNU_SOURCE $(OSSO_CFLAGS) $(HILDON_CFLAGS) $(EBOOK_CFLAGS) \
		$(GMODULE_CFLAGS) $(GCONF_CFLAGS) $(TPGLIB_CFLAGS) \
		-I$(srcdir)/../include -Wall -Werror -DOSSO_ABOOK_DEBUG \
		$(RTCOM_CFLAGS) -DOSSOLOG_COMPILE=1 -Wno-format-extra-args \
		$(DGETTEXT) $(XML_CFLAGS) \
		-DISO_CODES_PREFIX=\"$(ISO_CODES_PREFIX)\" -g -DTEST

test_avatar_editor_dialog_LDADD = \
		$(OSSO_LIBS) $(HILDON_LIBS) $(EBOOK_LIBS) $(RTCOM_LIBS) \
		$(GMODULE_LIBS) $(GCONF_LIBS) $(TPGLIB_LIBS) $(XML_LIBS) -lm

test_avatar_chooser_dialog_SOURCES = \
		$(COMMON_SOURCES) \
		test-avatar-chooser-dialog.c \
		osso-abook-avatar-chooser-dialog.c

test_avatar_chooser_dialog_CFLAGS = \
		-D_GNU_SOURCE $(OSSO_CFLAGS) $(HILDON_CFLAGS) $(EBOOK_CFLAGS) \
		$(GMODULE_CFLAGS) $(GCONF_CFLAGS) $(TPGLIB_CFLAGS) \
		-I$(srcdir)/../include -Wall -Werror -DOSSO_ABOOK_DEBUG \
		$(RTCOM_CFLAGS) -DOSSOLOG_COMPILE=1 -Wno-format-extra-args \
		$(DGETTEXT) $(XML_CFLAGS) \
		-DISO_CODES_PREFIX=\"$(ISO_CODES_PREFIX)\" -g -DTEST

test_avatar_chooser_dialog_LDADD = \
		$(OSSO_LIBS) $(HILDON_LIBS) $(EBOOK_LIBS) $(RTCOM_LIBS) \
		$(GMODULE_LIBS) $(GCONF_LIBS) $(TPGLIB_LIBS) $(XML_LIBS) -lm

test_contact_chooser_SOURCES = \
		$(COMMON_SOURCES) \
		test-contact-chooser.c \
		osso-abook-contact-view.c \
		osso-abook-tree-view.c \
		osso-abook-contact-model.c \
		osso-abook-contact-chooser.c

test_contact_chooser_CFLAGS = \
		-D_GNU_SOURCE $(OSSO_CFLAGS) $(HILDON_CFLAGS) $(EBOOK_CFLAGS) \
		$(GMODULE_CFLAGS) $(GCONF_CFLAGS) $(TPGLIB_CFLAGS) \
		-I$(srcdir)/../include -Wall -Werror -DOSSO_ABOOK_DEBUG \
		$(RTCOM_CFLAGS) -DOSSOLOG_COMPILE=1 -Wno-format-extra-args \
		$(DGETTEXT) $(XML_CFLAGS) \
		-DISO_CODES_PREFIX=\"$(ISO_CODES_PREFIX)\" -g -DTEST

test_contact_chooser_LDADD = \
		$(OSSO_LIBS) $(HILDON_LIBS) $(EBOOK_LIBS) $(RTCOM_LIBS) \
		$(GMODULE_LIBS) $(GCONF_LIBS) $(TPGLIB_LIBS) $(XML_LIBS) -lm

lib_LTLIBRARIES = libosso-abook-@API_VERSION_MAJOR@.la

libosso_abook_@API_VERSION_MAJOR@_includedir = \
		$(includedir)/lib$(PACKAGE)-$(API_VERSION_MAJOR)/lib$(PACKAGE)

libosso_abook_@API_VERSION_MAJOR@_la_CFLAGS = \
		-D_GNU_SOURCE $(OSSO_CFLAGS) $(HILDON_CFLAGS) $(EBOOK_CFLAGS) \
		$(GMODULE_CFLAGS) $(GCONF_CFLAGS) $(TPGLIB_CFLAGS) \
		$(CAIRO_CFLAGS) $(RTCOM_CFLAGS) $(XML_CFLAGS) \
		-I$(srcdir)/../include -Wall -Werror -DOSSO_ABOOK_DEBUG \
		-DOSSOLOG_COMPILE=1 -DISO_CODES_PREFIX=\"$(ISO_CODES_PREFIX)\" \
		-Wno-format-extra-args $(DGETTEXT)

libosso_abook_@API_VERSION_MAJOR@_la_LDFLAGS = -Wl,--no-undefined
libosso_abook_@API_VERSION_MAJOR@_la_LIBADD = \
		$(OSSO_LIBS) $(HILDON_LIBS) $(EBOOK_LIBS) $(RTCOM_LIBS) \
		$(GMODULE_LIBS) $(GCONF_LIBS) $(TPGLIB_LIBS) $(XML_LIBS) \
		$(CAIRO_LIBS) -lm

libosso_abook_@API_VERSION_MAJOR@_la_SOURCES = \
		osso-abook-enums.c \
		osso-abook-marshal.c \
		tp-glib-enums.c \
		osso-abook-init.c \
		osso-abook-debug.c \
		osso-abook-errors.c \
		osso-abook-string-list.c \
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
		osso-abook-entry.c \
		osso-abook-button.c \
		osso-abook-avatar-image.c \
		osso-abook-icon-sizes.c \
		osso-abook-avatar-chooser-dialog.c \
		osso-abook-contact-detail-store.c

libosso_abook_@API_VERSION_MAJOR@_public_headers = \
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
		osso-abook-contact-field.h \
		osso-abook-contact-detail-store.h \
		osso-abook-row-model.h

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
