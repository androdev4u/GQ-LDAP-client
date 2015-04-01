/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "common.h"
#include "iconv-helpers.h"
#include "util.h"
#include "formfill.h"
#include "ldif.h"
#include "encode.h"
#include "debug.h"

const char *gq_codeset = GQ_CODESET;

const char *decode_string(char *native_string, const gchar *ldap_string,
			  size_t len)
{
#if defined(HAVE_ICONV)
     char *in;
     char *out;
     size_t outlen;
     iconv_t conv;

     conv = iconv_open(gq_codeset, LDAP_CODESET);
     if (conv != (iconv_t) -1) {
	  in = (char *) ldap_string;
	  out = native_string;
	  /*       len = strlen(in); */
	  outlen = len;
	  
	  while(len > 0 && outlen > 0) {
	       if(iconv(conv, &in, &len, &out, &outlen) != 0) {
		    in++;		/* *** */
		    len--;
	       }
	  }
	  iconv_close(conv);
	  *out = '\0';
     } else {
	  strncpy(native_string, ldap_string, len);
	  *(native_string + len) = 0;
     }
#else /* HAVE_ICONV */
     strncpy(native_string, ldap_string, len);
     *(native_string + len - 1) = 0;
#endif /* HAVE_ICONV */
#ifdef DEBUG
     if (debug & GQ_DEBUG_ENCODE) {
	  fprintf(stderr, "decode_string \"%s\" (%d) -> \"%s\"\n", 
		  ldap_string, strlen(ldap_string), native_string);
     }
#endif
     return native_string;
}

const gchar *encode_string(gchar *ldap_string, const gchar *native_string, 
			   size_t len)
{
#if defined(HAVE_ICONV)
     char *in;
     char *out;
     size_t outlen;
     iconv_t conv;

     in = (char *) native_string;
     out = ldap_string;
/*       len = strlen(in); */
     outlen = len * 2 + 1; /* Worst case */
     conv = iconv_open(LDAP_CODESET, gq_codeset);

     if (conv != (iconv_t) (-1)) {
	  while(len > 0 && outlen > 0) {
	       if(iconv(conv, &in, &len, &out, &outlen) != 0) {
		    in++;		/* *** */
		    len--;
	       }
	  }
	  iconv_close(conv);
	  *out = '\0';
     } else {
 	  strncpy(ldap_string, native_string, len);
	  *(ldap_string + len) = 0;
     }
#else /* HAVE_ICONV */
/*       memcpy(ldap_string, native_string, len); */
     strncpy(ldap_string, native_string, len);
     *(ldap_string + len) = 0;
#endif /* HAVE_ICONV */
  
#ifdef DEBUG
     if (debug & GQ_DEBUG_ENCODE) {
	  fprintf(stderr, "encode_string \"%s\" -> \"%s\"\n", 
		  native_string, ldap_string);
     }
#endif

     return ldap_string;
}

gchar *encoded_string(const gchar *string)
{
     /* FIXME: check whether this code is still used (it might have been dropped out with the GTK+1.2 removal) */
     char *ldap_string;

     if (!string) return NULL;

     ldap_string = calloc(2 * strlen(string) + 1, sizeof(char));
  
     encode_string(ldap_string, string, strlen(string));

     return ldap_string;
}


/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
