/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 by Peter Stamfest

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

/* $Id: xmlparse.h 960 2006-09-02 20:42:46Z herzi $ */

#ifndef GQ_XMLPARSE_H_INCLUDED
#define GQ_XMLPARSE_H_INCLUDED

#include <libxml/SAX.h>
#include <libxml/parserInternals.h>

#define XML_CONTEXT_SIZE	10


#define NO_TRIM_CDATA		 1

struct xml_tag;

typedef void (*free_func)(void *);

struct tagstack_entry {
    xmlChar *tag;
    xmlChar **attrs;

    xmlChar *cdata;
    int len;

    const struct xml_tag *handler;

    /* if skip is set to 1, all tags below will be skipped (no xml_tag
       handlers will be called) */
    int skip;

    void *data;
    free_func free_data; /* void (*free_data)(void *); */
};


struct tagstack {
    int size;
    int sp;
    struct tagstack_entry **entries;
};


struct parser_context;

struct xml_tag {
    const char *tag;
    int flags;
    void (*startElement)(struct parser_context *ctx, 
			 struct tagstack_entry *stack);
    void (*endElement)  (struct parser_context *ctx,
			 struct tagstack_entry *stack);
    const char *context[XML_CONTEXT_SIZE];
};


struct parser_context {
    struct tagstack *stack;
    const struct xml_tag *tags;
    void *user_data;
    xmlSAXHandler *XMLhandler;
};



#define XMLhandleFatalError(ctx, ...) \
	if ((ctx)->XMLhandler->fatalError) { \
	    (ctx)->XMLhandler->fatalError((ctx), __VA_ARGS__); \
	} else { \
	    fprintf(stderr, "Unhandled fatal error: "); fprintf(stderr, __VA_ARGS__); \
	    exit(1); \
	}

#define XMLhandleError(ctx, ...) \
	if ((ctx)->XMLhandler->error) { \
	    (ctx)->XMLhandler->error(ctx, __VA_ARGS__); \
	} else { \
	    fprintf(stderr, "Unhandled error: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); \
	    exit(1); \
	}

struct tagstack_entry *peek_tag(struct tagstack *stack, int n);

int XMLparse(struct xml_tag *tags, 
	     xmlSAXHandler *handler,
	     void *user_data, const char *file);

#endif

/* 
   Local Variables:
   c-basic-offset: 4
   End:
 */
