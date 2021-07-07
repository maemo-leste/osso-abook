AC_INIT([osso-abook], [4.0])
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

#define API_VERSION_MAJOR
API_VERSION_MAJOR=1.0
AC_SUBST(API_VERSION_MAJOR)

PKG_CHECK_MODULES(OSSO, libosso)
AC_SUBST(OSSO_CFLAGS)
AC_SUBST(OSSO_LIBS)

PKG_CHECK_MODULES(HILDON, hildon-1)
AC_SUBST(HILDON_CFLAGS)
AC_SUBST(HILDON_LIBS)

PKG_CHECK_MODULES(EBOOK, libebook-contacts-1.2 libebook-1.2)
AC_SUBST(EBOOK_CFLAGS)
AC_SUBST(EBOOK_LIBS)

PKG_CHECK_MODULES(GMODULE, gmodule-2.0)
AC_SUBST(GMODULE_CFLAGS)
AC_SUBST(GMODULE_LIBS)

PKG_CHECK_MODULES(GCONF, gconf-2.0)
AC_SUBST(GCONF_CFLAGS)
AC_SUBST(GCONF_LIBS)

PKG_CHECK_MODULES(TPGLIB, telepathy-glib)
AC_SUBST(TPGLIB_CFLAGS)
AC_SUBST(TPGLIB_LIBS)

PKG_CHECK_MODULES(RTCOM, rtcom-eventlogger)
AC_SUBST(RTCOM_CFLAGS)
AC_SUBST(RTCOM_LIBS)

PKG_CHECK_MODULES(XML, [libxml-2.0 iso-codes])
AC_SUBST(XML_CFLAGS)
AC_SUBST(XML_LIBS)

PKG_CHECK_MODULES(CAIRO, [cairo])
AC_SUBST(CAIRO_CFLAGS)
AC_SUBST(CAIRO_LIBS)

AC_ARG_ENABLE([maemo-launcher],
              [AS_HELP_STRING([--enable-maemo-launcher],
                              [build with maemo-launcher support])],
                              [case "${enableval}" in
                               yes) maemo_launcher=true ;;
                               no)  maemo_launcher=false ;;
                               *) AC_MSG_ERROR([bad value ${enableval} for --enable-maemo-launcher]) ;;
                               esac], [maemo_launcher=false])

MAEMO_LAUNCHER_PACKAGE=""
if test "x$maemo_launcher" = "xtrue"; then
  MAEMO_LAUNCHER_PACKAGE="maemo-launcher-app"
  PKG_CHECK_MODULES(LAUNCHER_DEPS, $MAEMO_LAUNCHER_PACKAGE)
else
  LAUNCHER_DEPS_CFLAGS=""
  LAUNCHER_DEPS_LIBS=""
fi

AC_SUBST(LAUNCHER_DEPS_CFLAGS)
AC_SUBST(LAUNCHER_DEPS_LIBS)

#+++++++++++++++++++
# Directories setup
#+++++++++++++++++++

dnl Localization
GETTEXT_PACKAGE=osso-addressbook
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "${GETTEXT_PACKAGE}", [gettext package])

DGETTEXT="-D'_(msgid)'='g_dgettext(GETTEXT_PACKAGE, msgid)'"
AC_SUBST(DGETTEXT)

CFLAGS="-Wall -Werror -DG_LOG_DOMAIN=GETTEXT_PACKAGE"
AC_SUBST(CFLAGS)

ISO_CODES_PREFIX=`pkg-config iso-codes --variable=prefix`
AC_SUBST(ISO_CODES_PREFIX)

AC_OUTPUT([
Makefile
lib/Makefile
dist/Makefile
pkgconfig/Makefile
pkgconfig/libosso-abook-1.0.pc
])
