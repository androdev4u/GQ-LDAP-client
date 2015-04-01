/*
    GQ -- a GTK-based LDAP client is
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 Peter Stamfest
    
    This file is
    Copyright (c) 2003 by Peter Stamfest <peter@stamfest.at>

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
# include  <config.h>
#endif /* HAVE_CONFIG_H */
#include <string.h>

#include <glib/gi18n.h>

#include "xmlutil.h"
#include "errorchain.h"

/* gq-specific XML parser utilities */

void XMLmessageHandler(struct parser_context *ctx, 
		       int severity,
		       const char *type, 
		       const char *format, va_list ap)
{
    struct parser_comm *comm = ctx->user_data;
    if (comm) {
	int len = 1024, pos = 0, i;
	char *buf = g_malloc(len);

	g_snprintf(buf, len, "%s: ", type);
	i = strlen(buf);
	pos += i; /* OK for UTF-8 */

	g_vsnprintf(buf + pos, len - pos, format, ap);
	error_push(comm->error_context, buf);

	fprintf(stderr, "%s\n", buf);

	g_free(buf);

	if (severity > comm->severity) {
	    comm->severity = severity;
	}
    }
}

void XMLwarningHandler(struct parser_context *ctx, 
		       const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    XMLmessageHandler(ctx, SEVERITY_WARNING, _("Warning"), fmt, args);
    va_end(args);
}

void XMLerrorHandler(struct parser_context *ctx,
			    const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    XMLmessageHandler(ctx, SEVERITY_ERROR, _("Error"), fmt, args);
    va_end(args);
}

void XMLfatalErrorHandler(struct parser_context *ctx,
			  const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    XMLmessageHandler(ctx, SEVERITY_FATAL_ERROR, _("Fatal Error"), fmt, args);
    va_end(args);
}
