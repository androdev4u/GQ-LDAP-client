/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen
    Copyright (C) 2002 Bert Vermeulen and Peter Stamfest <peter@stamfest.at>

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

/* $Id: xmlutil.h 960 2006-09-02 20:42:46Z herzi $ */

#ifndef GQ_XMLUTIL_H_INCLUDED
#define GQ_XMLUTIL_H_INCLUDED

#include <stdarg.h>
#include "xmlparse.h" 

/* gq specific XML utils */


#define SEVERITY_NONE		0
#define SEVERITY_WARNING	1
#define SEVERITY_ERROR		2
#define SEVERITY_FATAL_ERROR	3

/* the user_data for any parser used by gq */
struct parser_comm {
    int error_context;	/* error_context for the various warning/error
			   handlers */
    int severity;	/* The gq-specific warning/error handlers set
			   this to the maximum severity of the
			   warning/error */
    void *result;	/* A pointer to whatever result the parser provides */
};

void XMLmessageHandler(struct parser_context *ctx, 
		       int severity,
		       const char *type, 
		       const char *format, va_list ap);
void XMLwarningHandler(struct parser_context *ctx, 
		       const char *fmt, ...);
void XMLerrorHandler(struct parser_context *ctx,
		     const char *fmt, ...);
void XMLfatalErrorHandler(struct parser_context *ctx,
			  const char *fmt, ...);


#endif

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
