/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 Peter Stamfest

    This program is released under the Gnu General Public License with
    the additional exemption that compiling, linking, and/or using
    OpenSSL is allowed.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

/* $Id: encode.h 945 2006-08-24 13:45:22Z herzi $ */

#ifndef GQ_ENCODE_H_INCLUDED
#define GQ_ENCODE_H_INCLUDED

#include <glib.h>
#include <stdlib.h>		/* size_t */

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */
#include "formfill.h"

#if ENABLE_NLS
#  if HAVE_LANGINFO_CODESET
#    include <langinfo.h>
#  endif /* HAVE_LANGINFO_CODESET */
#endif /* ENABLE_NLS */

#define LDAP_CODESET	"UTF-8"

#ifdef DEFAULT_CODESET
#define GQ_CODESET	DEFAULT_CODESET;
#else
#define GQ_CODESET	ISO8859_1
#endif


extern const char *gq_codeset;

const gchar *decode_string(gchar *native_string, const gchar *ldap_string,
			   size_t len);
const gchar *encode_string(gchar *ldap_string, const gchar *native_string,
			   size_t len);
gchar *decoded_string(const gchar *string);
gchar *encoded_string(const gchar *string);

#endif

/*
   Local Variables:
   c-basic-offset: 5
   End:
 */
