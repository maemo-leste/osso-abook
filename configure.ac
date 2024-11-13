AC_INIT([libosso-abook], [4.0])
AC_CANONICAL_SYSTEM

AM_CONFIG_HEADER(config.h)

AM_MAINTAINER_MODE

AM_INIT_AUTOMAKE

AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_LIBTOOL

AC_HEADER_STDC

AC_PATH_X
AC_PATH_XTRA
AC_SUBST(X_CFLAGS)

PKG_PROG_PKG_CONFIG([0.28])
PKG_CHECK_VAR([GLIB_MKENUMS], [glib-2.0], [glib_mkenums])
PKG_CHECK_VAR([GLIB_GENMARSHAL], [glib-2.0], [glib_genmarshal])

#+++++++++++++++
# Misc programs 
#+++++++++++++++

GTK_DOC_CHECK([1.3])

#define API_VERSION_MAJOR
API_VERSION_MAJOR=1.0
AC_SUBST(API_VERSION_MAJOR)

PKG_CHECK_MODULES(OSSO, libosso)
PKG_CHECK_MODULES(HILDON, hildon-1 hildon-fm-2 libhildonmime)
PKG_CHECK_MODULES(EBOOK, libebook-contacts-1.2 libebook-1.2)
PKG_CHECK_MODULES(GMODULE, gmodule-2.0)
PKG_CHECK_MODULES(GCONF, gconf-2.0)
PKG_CHECK_MODULES(TPGLIB, telepathy-glib)
PKG_CHECK_MODULES(RTCOM, rtcom-eventlogger rtcom-accounts-ui-client)
PKG_CHECK_MODULES(XML, [libxml-2.0 iso-codes])
PKG_CHECK_MODULES(CAIRO, [cairo])
PKG_CHECK_MODULES(GOFONO, [libgofono libglibutil])
PKG_CHECK_MODULES(MBPI, [mobile-broadband-provider-info])
PKG_CHECK_MODULES(ISO_CODES, [iso-codes])
PKG_CHECK_MODULES(MODEST_DBUS, [libmodest-dbus-client-1.0])

#+++++++++++++++++++
# Directories setup
#+++++++++++++++++++

dnl Localization
GETTEXT_PACKAGE=osso-addressbook
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "${GETTEXT_PACKAGE}", [gettext package])

DGETTEXT="-D'_(msgid)'='g_dgettext(GETTEXT_PACKAGE, msgid)'"
AC_SUBST(DGETTEXT)

AC_DEFINE_UNQUOTED([G_LOG_DOMAIN], "$PACKAGE_NAME", [Default logging facility])

ISO_CODES_PREFIX=`pkg-config iso-codes --variable=prefix`
AC_SUBST(ISO_CODES_PREFIX)

MBPI_DATABASE=`pkg-config mobile-broadband-provider-info --variable=database`
AC_SUBST(MBPI_DATABASE)

AC_OUTPUT([
Makefile
lib/Makefile
dist/Makefile
pkgconfig/Makefile
pkgconfig/libosso-abook-1.0.pc
doc/Makefile
])
