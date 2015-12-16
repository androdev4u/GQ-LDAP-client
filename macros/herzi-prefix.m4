# herzi-cflags.m4
#
# File that contains autoconf macros that perfom checks with modified CFLAGS
# and CPPFLAGS

dnl HERZI_PACKAGE_PREFIX
AC_DEFUN([HERZI_PACKAGE_PREFIX],[
	AC_MSG_CHECKING(prefix)
	if test "x${prefix}" = "xNONE"; then
		PACKAGE_PREFIX="${ac_default_prefix}"
	else
		PACKAGE_PREFIX="${prefix}"
	fi
	AC_SUBST(PACKAGE_PREFIX)
	AC_MSG_RESULT($PACKAGE_PREFIX)
])

