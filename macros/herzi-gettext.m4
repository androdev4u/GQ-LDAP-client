# herzi-gettext.m4
#
# File that contains autoconf macros that initialize gettext parameters
# (GETTEXT_PACKAGE and PACKAGE_LOCALE_DIR)

dnl HERZI_GETTEXT
AC_DEFUN([HERZI_GETTEXT],[
	AC_REQUIRE([AM_GLIB_GNU_GETTEXT])
	
	GETTEXT_PACKAGE=$1
	AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",
		                [The package name for gettext])
	AC_SUBST(GETTEXT_PACKAGE)
	
	PACKAGE_LOCALE_DIR="${PACKAGE_PREFIX}/${DATADIRNAME}/locale"
	AC_DEFINE_UNQUOTED(PACKAGE_LOCALE_DIR,"$PACKAGE_LOCALE_DIR",
		                [The package's locale path for gettext])
	AC_SUBST(PACKAGE_LOCALE_DIR)
])

