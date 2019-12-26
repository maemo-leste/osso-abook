lib_LTLIBRARIES = libosso-abook-@API_VERSION_MAJOR@.la

libosso_abook_@API_VERSION_MAJOR@_includedir = \
		$(includedir)/$(PACKAGE)-$(API_VERSION_MAJOR)/$(PACKAGE)

libosso_abook_@API_VERSION_MAJOR@_la_CFLAGS = \
		$(OSSO_CFLAGS) $(HILDON_CFLAGS) $(EBOOK_CFLAGS) \
		$(GMODULE_CFLAGS) $(GCONF_CFLAGS) $(TPGLIB_CFLAGS) \
		-I$(srcdir)/../include -Wall -Werror \
		-DOSSOLOG_COMPILE=1 -Wno-format-extra-args $(DGETTEXT)

libosso_abook_@API_VERSION_MAJOR@_la_LDFLAGS = -Wl,--no-undefined
libosso_abook_@API_VERSION_MAJOR@_la_LIBADD = \
		$(OSSO_LIBS) $(HILDON_LIBS) $(EBOOK_LIBS) \
		$(GMODULE_LIBS) $(GCONF_LIBS) $(TPGLIB_LIBS)

libosso_abook_@API_VERSION_MAJOR@_la_SOURCES = \
		osso-abook-enum-types.c \
		osso-abook-marshal.c \
		osso-abook-init.c \
		osso-abook-debug.c \
		osso-abook-string-list.c \
		osso-abook-contact-chooser.c

#
#		osso-abook-contact.c

#		osso-abook-list-store.c


libosso_abook_@API_VERSION_MAJOR@_public_headers = \
		osso-abook-settings.h \
		osso-abook-caps.h \
		osso-abook-debug.h

libosso_abook_@API_VERSION_MAJOR@_built_public_headers  = \
		osso-abook-enum-types.h

libosso_abook_@API_VERSION_MAJOR@_include_HEADERS = \
		$(libosso_abook_@API_VERSION_MAJOR@_public_headers) \
		$(libosso_abook_@API_VERSION_MAJOR@_built_public_headers)

osso-abook-enum-types.h: $(libosso_abook_$(API_VERSION_MAJOR)_public_headers) Makefile
	(cd $(srcdir) && glib-mkenums					\
	--fhead "#ifndef __OSSO_ABOOK_ENUM_TYPES_H_INCLUDED__\n"	\
	--fhead "#define __OSSO_ABOOK_ENUM_TYPES_H_INCLUDED__\n"	\
	--fhead "\n" 							\
	--fhead "#include <glib-object.h>\n"				\
	--fhead "\n"							\
	--fhead "G_BEGIN_DECLS\n" 					\
	--fhead "\n"							\
	--fprod "/* Enumerations from \"@filename@\" */\n" 		\
	--vhead "GType @enum_name@_get_type (void);\n#define @ENUMPREFIX@_TYPE_@ENUMSHORT@ (@enum_name@_get_type())\n" \
	--ftail "\nG_END_DECLS\n\n#endif /* __OSSO_ABOOK_ENUM_TYPES_H_INCLUDED__ */" \
	$(libosso_abook_$(API_VERSION_MAJOR)_public_headers)) > $@

osso-abook-enum-types.c: $(libosso_abook_$(API_VERSION_MAJOR)_public_headers) Makefile
	(cd $(srcdir) && glib-mkenums 				\
	--fhead "#include \"osso-abook-enum-types.h\"\n"	\
	--fprod "\n/* enumerations from \"@filename@\" */" 	\
	--fprod "#include \"@filename@\"\n" 			\
	--vhead "GType\n@enum_name@_get_type (void)\n{\n  static GType etype = 0;\n  if (etype == 0) {\n    static const G@Type@Value values[] = {" 	\
	--vprod "      { @VALUENAME@, \"@VALUENAME@\", \"@valuenick@\" }," \
	--vtail "      { 0, NULL, NULL }\n    };\n    etype = g_@type@_register_static (\"@EnumName@\", values);\n  }\n  return etype;\n}\n" \
	$(libosso_abook_$(API_VERSION_MAJOR)_public_headers)) > $@

osso-abook-marshal.h: osso-abook-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --header --prefix=osso_abook_marshal \
	$< > $@

osso-abook-marshal.c: osso-abook-marshal.list
	$(AM_V_GEN)$(GLIB_GENMARSHAL) --body --prefix=osso_abook_marshal $< > $@

BUILT_SOURCES = \
	osso-abook-enum-types.h \
	osso-abook-enum-types.c \
	osso-abook-marshal.h \
	osso-abook-marshal.c

CLEANFILES = $(BUILT_SOURCES)

MAINTAINERCLEANFILES = Makefile.in
