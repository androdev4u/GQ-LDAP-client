dnl
dnl    $Id: acinclude.m4 876 2006-05-02 23:10:24Z herzi $
dnl
dnl    GQ -- a GTK-based LDAP client
dnl    Copyright (C) 1998-2003 Bert Vermeulen
dnl    Copyright (c) 2002-2003 Peter Stamfest <peter@stamfest.at>
dnl
dnl    This program is released under the Gnu General Public License with
dnl    the additional exemption that compiling, linking, and/or using
dnl    OpenSSL is allowed.
dnl
dnl    This program is free software; you can redistribute it and/or modify
dnl    it under the terms of the GNU General Public License as published by
dnl    the Free Software Foundation; either version 2 of the License, or
dnl    (at your option) any later version.
dnl
dnl    This program is distributed in the hope that it will be useful,
dnl    but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl    GNU General Public License for more details.
dnl
dnl    You should have received a copy of the GNU General Public License
dnl    along with this program; if not, write to the Free Software
dnl    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
dnl


AC_DEFUN([ARG_YESNO], [
if test -z "$1" -o "x$1" = "xno"; then
    echo "no"
else
    echo "yes"
fi
])

dnl
dnl idea taken from the autoconf mailing list, posted by
dnl Timur I. Bakeyev  timur@gnu.org, 
dnl http://mail.gnu.org/pipermail/autoconf/1999-October/008311.html
dnl partly rewritten by Peter Stamfest <peter@stamfest.at>
dnl
dnl This determines, if struct tm containes tm_gmtoff field
dnl or we should use extern long int timezone.
dnl

AC_DEFUN([GC_TIMEZONE],
	[AC_REQUIRE([AC_STRUCT_TM])dnl

	AC_CACHE_CHECK([tm_gmtoff in struct tm], gq_cv_have_tm_gmtoff,
		gq_cv_have_tm_gmtoff=no
		AC_TRY_COMPILE([#include <time.h>
				#include <$ac_cv_struct_tm>
				],
			       [struct tm t;
				t.tm_gmtoff = 0;
				exit(0);
				],
			       gq_cv_have_tm_gmtoff=yes
			)
	)

	AC_CACHE_CHECK([__tm_gmtoff in struct tm], gq_cv_have___tm_gmtoff,
		gq_cv_have___tm_gmtoff=no
		AC_TRY_COMPILE([#include <time.h>
				#include <$ac_cv_struct_tm>
				],
			       [struct tm t;
				t.__tm_gmtoff = 0;
				exit(0);
				],
			       gq_cv_have___tm_gmtoff=yes
			)
	)

	if test "$gq_cv_have_tm_gmtoff" = yes ; then
		AC_DEFINE(HAVE_TM_GMTOFF,1,
			  [Define if your struct tm contains tm_gmtoff or __tm_gmtoff])
	        AC_DEFINE(TM_GMTOFF, tm_gmtoff, 
			  [Define to the name of the tm_gmtoff member of struct tm])
	elif test "$gq_cv_have___tm_gmtoff" = yes ; then
		AC_DEFINE(HAVE_TM_GMTOFF,1)
	        AC_DEFINE(TM_GMTOFF, __tm_gmtoff)
	else
		AC_CACHE_CHECK(for timezone, ac_cv_var_timezone,
			       [AC_TRY_LINK([
					     #include <time.h>
					     extern long int timezone;
				],
			       [long int l = timezone;], 
				ac_cv_var_timezone=yes, 
				ac_cv_var_timezone=no)])
		if test $ac_cv_var_timezone = yes; then
			AC_DEFINE(HAVE_TIMEZONE,1,
				  [Define if your libc has a global timezone variable])
		fi
	fi
])
