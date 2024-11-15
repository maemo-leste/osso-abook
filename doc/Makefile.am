MAINTAINERCLEANFILES 				= Makefile.in \
						  version.xml \
						  tmpl/*.sgml \
						  gtk-doc.make

DOC_MODULE					= libosso-abook

DOC_MAIN_SGML_FILE				= $(DOC_MODULE)-docs.sgml

#SCAN_OPTIONS					=--deprecated-guards="HILDON_DISABLE_DEPRECATED"

SCANOBJ_OPTIONS					=--type-init-func="gtk_type_init(0)"

DOC_SOURCE_DIR					= $(top_srcdir)/lib
DOC_BUILD_DIR 					= $(top_builddir)/libosso-abook


HFILE_GLOB					= $(top_srcdir)/lib/*.h

CFILE_GLOB					= $(top_srcdir)/lib/*.c

IGNORE_HFILES 					=

AM_CPPFLAGS 					= -I$(top_srcdir)/lib

MKDB_OPTIONS					=--sgml-mode --output-format=xml

GTKDOC_CFLAGS 					= $(MAEMO_GTK)

GTKDOC_LIBS 					= $(top_builddir)/lib/libosso-abook-1.0.la

version.xml: ../configure.ac
	@echo $(VERSION) > version.xml

html: html-build.stamp

FIXXREF_OPTIONS					=

include $(top_srcdir)/doc/gtk-doc.make

