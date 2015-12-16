/*
    GQ -- a GTK-based LDAP client is
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

/*
  A gq-independent higher level XML parser interface around a SAX parser.

  No code specific to any given parser should be put into this file.

  This parser interface allows to process a XML file in a SAX like
  fashion, but much more comfortably as it provides for different
  callbacks for start and end tags based on the tag name. It also
  provides the start-tag attributes for end-tag handlers, takes care
  of character data assembly for tags and provides for
  context-sensitivity out-of-the box: It is trivial to have different
  handlers for the same tag in different semantic contexts (that is:
  with different parent tags).

  A stack of tag-related data allows handlers to store arbitrary data
  with any tag. That data is available through the tagstack to child
  elements as well. This feature really makes it simple to create a
  XML parser.
*/

#include <string.h>
#include <errno.h>

/* This is the only part specific to GTK, I hope */
#include <gtk/gtk.h>

#include "xmlparse.h"

#define malloc g_malloc
#define calloc(n,s) g_malloc0((n) * (s))

#define TAGSTACK_INCR 20

static void free_tagstack_entry(struct tagstack_entry *e) 
{
    int i;

    if (e->tag) free(e->tag);
    e->tag = NULL;

    if (e->cdata) free(e->cdata);
    e->cdata = NULL;

    if (e->free_data) e->free_data(e->data);
    e->data = NULL;
    e->free_data = NULL;

    if (e->attrs) {
	for (i = 0 ; e->attrs[i] ; i++) {
	    free(e->attrs[i]);
	    e->attrs[i] = NULL;
	}
	free(e->attrs);
	e->attrs = NULL;
    }
}

static struct tagstack *new_tagstack()
{
    struct tagstack *s = calloc(1, sizeof (struct tagstack));
    s->size = TAGSTACK_INCR;
    s->entries = calloc(s->size, sizeof(struct tagstack_entry *));
    return s;
}

static struct tagstack_entry *push_tag(struct tagstack *stack,
				       const xmlChar *tag,
				       const xmlChar **attrs)
{
    struct tagstack_entry *e = calloc(1, sizeof (struct tagstack_entry));
    if (stack->sp >= stack->size) {
	stack->size += TAGSTACK_INCR;
	stack->entries = realloc(stack->entries, 
				 sizeof(struct tagstack_entry*) * stack->size);
    }
    stack->entries[stack->sp++] = e;
    e->tag = (guchar*)strdup((gchar*)tag);
    e->cdata = (guchar*)strdup("");
    e->len = 1;

    return e;
}

static void pop_tag(struct tagstack *stack)
{
    if (stack->sp-- > 0) {
	struct tagstack_entry *e = stack->entries[stack->sp];
	stack->entries[stack->sp] = NULL;
	free_tagstack_entry(e);
    }
}

static void free_tagstack(struct tagstack *stack)
{
    while (stack->sp-- > 0) pop_tag(stack);
}



/* retrieve the n-th tagstack entry. 0 = the top entry, 1 = the
   next-to-top entry, etc... */
struct tagstack_entry *peek_tag(struct tagstack *stack, int n) 
{
    int i = stack->sp - 1 - n;
    if (i >= 0) {
	return stack->entries[i];
    } else {
	return NULL;
    }
}

static void startElementH(struct parser_context *ctx,
			  const xmlChar *name,
			  const xmlChar **attrs)
{
    struct tagstack_entry *e;
    const struct xml_tag *t;
    int i;

#ifdef SKIP_UNKNOWN
    int parent_skip;

    e = peek_tag(ctx->stack, 0);
    parent_skip = e && e->skip;
#endif

    e = push_tag(ctx->stack, name, attrs);

#ifdef SKIP_UNKNOWN
    if (parent_skip) {
	/* we are currently skipping tags recursively */

	e->skip = 1;
	return;
    }
#endif

    e->attrs = NULL;
    if (attrs) {
	for (i = 0 ; attrs[i] ; i++) ;
	e->attrs = calloc(i+1, sizeof(xmlChar *));
	for (i = 0 ; attrs[i] ; i++) {
	    e->attrs[i] = (guchar*)strdup((gchar*)attrs[i]);
	}
	e->attrs[i] = NULL;
    }
    
    /* lookup handler */
    
    for (t = ctx->tags ; t->tag ; t++) {
	if (strcmp((gchar*)name, t->tag) == 0) {
	    /* maybe, maybe, maybe..... (J Joplin) */

	    int ok = 1;
	    /* check context */
	    for (i = 0 ; i < XML_CONTEXT_SIZE && t->context[i] ; i++) {
		struct tagstack_entry *f;
		f = peek_tag(ctx->stack, i + 1);
		if (!f) {
		    ok = 0;
		    break;
		}
		if (strcmp((gchar*)f->tag, t->context[i]) != 0) {
		    ok = 0;
		    break;
		}
	    }

	    if (ok) {
		/* found handler !!! */
		break;
	    }
	}
    }
    if (t->tag) {
	e->handler = t;
	if (e->handler->startElement) {
	    e->handler->startElement(ctx, e);
	}
    } else {
#ifdef SKIP_UNKNOWN
	e->skip = 1;
#else
	XMLhandleFatalError(ctx, "Unknown tag '%s'\n", name);
#endif
    }
}

static void endElementH(struct parser_context *ctx,
			const xmlChar *name)
{
    struct tagstack_entry *e = peek_tag(ctx->stack, 0);


    if (e->cdata && e->handler && !(e->handler->flags & NO_TRIM_CDATA)) {
	gunichar c;
	xmlChar *p = e->cdata, *n = NULL;

	/* trim leading whitespace */
	for(c = g_utf8_get_char((gchar*)p) ; c ;
	    p = (guchar*)g_utf8_next_char((gchar*)p), c = g_utf8_get_char((gchar*)p)) {
	    if (!g_unichar_isspace(c)) {
		memmove(e->cdata, p, e->len - (p - e->cdata) + 1);
		break;
	    }
	}

	/* trim trailing whitespace */
	p = e->cdata;
	for(c = g_utf8_get_char((gchar*)p) ; c ;
	    p = (guchar*)g_utf8_next_char((gchar*)p), c = g_utf8_get_char((gchar*)p)) {
	    if (g_unichar_isspace(c)) {
		if (n == NULL) n = p;
	    } else {
		n = NULL;
	    }
	}
	if (n) {
	    *n = 0;
	}
    }

    if (!e->skip && e->handler) {
	if (e->handler->endElement) {
	    e->handler->endElement(ctx, e);
	}
    }

    pop_tag(ctx->stack);
}

static void charactersH(struct parser_context *ctx, 
			const xmlChar *ch, int len)
{
    struct tagstack_entry *e = peek_tag(ctx->stack, 0);
    if (e->skip) return;





    if (e->cdata) {
	e->len += len;
	e->cdata = realloc(e->cdata, e->len + 1);
    } else {
	e->len += len;
	e->cdata = malloc(e->len + 1);
	*(e->cdata) = 0;
    }

    strncat((gchar*)e->cdata, (gchar*)ch, len);
    e->cdata[e->len] = 0;
}

static int inputReadCallback(FILE *context,
			     char *buffer,
			     int len)
{
    int rc = fread(buffer, 1, len, context);
    if (rc == 0) {
	if (ferror(context)) return -1;
    }
    return rc;
}

/* A dummy callback doing nothing */
static int inputCloseCallbackDummy(FILE *fp)
{
    return 0;
}

int XMLparse(struct xml_tag *tags,
	     xmlSAXHandler *handler,
	     void *user_data, const char *file)
{
    struct parser_context ctx;
    struct tagstack *stack = new_tagstack();
    int rc;
    int handler_is_local = 0;
    FILE *fp;

    if (handler == NULL) {
	handler = calloc(1, sizeof(xmlSAXHandler));
	handler_is_local = 1;
    }

    if (!handler->startElement) {
	handler->startElement = (startElementSAXFunc) startElementH;
    }
    if (!handler->endElement) {
	handler->endElement   = (endElementSAXFunc)   endElementH;
    }
    if (!handler->characters) {
	handler->characters   = (charactersSAXFunc)   charactersH;
    }

    ctx.tags = tags;
    ctx.stack = stack;
    ctx.user_data = user_data;
    ctx.XMLhandler = handler;

    fp = fopen(file, "r");
    if (fp) {
	xmlParserCtxtPtr parser =
	    xmlCreateIOParserCtxt(handler,
				  &ctx,
				  (xmlInputReadCallback)  inputReadCallback,
				  (xmlInputCloseCallback) inputCloseCallbackDummy,
				  fp, 0);
	rc = xmlParseDocument(parser);
	xmlFreeParserCtxt(parser);
    } else {
	XMLhandleFatalError(&ctx, "Cannot open file '%s': %s",
			    file, strerror(errno));
	rc = -1;
    }
    if (fp) fclose(fp); /* inputCloseCallbackDummy does not close fp. That way
			   we can be sure that fp gets closed only once. */

    free_tagstack(stack);
    if (handler_is_local) free(handler);

    return rc;
}
