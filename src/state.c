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

/* $Id: state.c 975 2006-09-07 18:44:41Z herzi $ */

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#include <glib/gi18n.h>
/* #include <glib/gmessages.h> */
#include <gtk/gtk.h>

#include "xmlparse.h"
#include "configfile.h"
#include "util.h"
#include "errorchain.h"
#include "xmlutil.h"
#include "state.h"
#include "encode.h"		/* gq_codeset */

enum state_value_type {
    SV_int = 1,
    SV_char = 2,
    SV_list = 3,
};

static struct tokenlist token_value_type_names[] = {
    { SV_int,   "integer",	NULL },
    { SV_char,  "string",	NULL },
    { SV_list,	"list",		NULL },
    { 0, "",			NULL }
};


/* Oh-uh, It looks like a registry! I would't wonder if this file
   would always end up having 666 lines... */

struct state_entity {
    GHashTable *values;
    GHashTable *entities;	/* sub entities */
};

typedef void (*sv_free_list_func)(void);

struct state_value {
    enum state_value_type type;
    union {
	int *int_val;
	char *char_val;
	GList *list_val;
    } val;
    sv_free_list_func free_list_element;
};

static GHashTable *entities = NULL;

static struct state_value *new_state_value(enum state_value_type t)
{
    struct state_value *v = g_malloc0(sizeof(struct state_value));
    v->type = t;
    switch (t) {
    case SV_int: 
	v->val.int_val = g_malloc0(sizeof(int));
	break;
    case SV_char: 
	v->val.char_val = g_strdup("");
	break;
    case SV_list: 
	v->val.list_val = NULL;
	break;
    default: 
	abort();
    }
    return v;
}

static void free_state_value(struct state_value *v) 
{
    g_assert(v);
    switch (v->type) {
    case SV_int: 
	g_free(v->val.int_val);
	break;
    case SV_char: 
	if (v->val.char_val) g_free(v->val.char_val);
	break;
    case SV_list: 
	if (v->val.list_val && v->free_list_element) {
	    g_list_foreach(v->val.list_val, (GFunc) v->free_list_element, NULL);
	}
	if (v->val.list_val) {
	    g_list_free(v->val.list_val);
	}
	break;
    default: 
	abort();
    }
    g_free(v);
}


static struct state_entity *new_state_entity() 
{
    struct state_entity *e = g_malloc0(sizeof(struct state_entity));
    e->values   = g_hash_table_new(g_str_hash, g_str_equal);
    e->entities = g_hash_table_new(g_str_hash, g_str_equal);
    return e;
}

static gboolean ghr_free_value(char *key, struct state_value *v, gpointer ud)
{
    g_free(key);
    free_state_value(v);
    return TRUE;
}

void free_state_entity(struct state_entity *e);

static gboolean ghr_free_entity(char *key, 
				struct state_entity *e, gpointer ud)
{
    g_free(key);
    free_state_entity(e);
    return TRUE;
}

void free_state_entity(struct state_entity *e)
{
    g_assert(e);
    g_hash_table_foreach_remove(e->values, (GHRFunc) ghr_free_value, NULL);
    g_hash_table_foreach_remove(e->entities, (GHRFunc) ghr_free_entity, NULL);
    g_hash_table_destroy(e->values);
    g_hash_table_destroy(e->entities);
}


static struct state_entity *lookup_entity_with_root(GHashTable *root, 
						    const char *entity_name,
						    gboolean create) 
{
    struct state_entity *e;
    const char *c;

    c = strchr(entity_name, '.'); /* UTF-8 OK: we do not use non-ASCII
				     entity names */
    if (c) {
	/* look for subentities! */
	char *d = g_strdup(entity_name);
	char *f = d + (c - entity_name);
	*f = 0;
	f++;

	e = g_hash_table_lookup(root, d);

	if (!e) {
	    if (create) {
		e = new_state_entity();
		g_hash_table_insert(root, g_strdup(d), e);
	    } else {
		return NULL;
	    }
	}

	e = lookup_entity_with_root(e->entities, f, create);
	return e;
    }

    e = g_hash_table_lookup(root, entity_name);
    if (e) {
	return e;
    }
    if (create) {
	e = new_state_entity();
	g_hash_table_insert(root, g_strdup(entity_name), e);
    }
    return e;
}

gboolean exists_entity(const char *entity_name) 
{
    return lookup_entity_with_root(entities, entity_name, FALSE) != NULL;
}

struct state_entity *lookup_entity(const char *entity_name) 
{
    if (!entities) {
	entities = g_hash_table_new(g_str_hash, g_str_equal);
    }

    return lookup_entity_with_root(entities, entity_name, TRUE);
}

int state_value_get_int(const char *state_name,
			const char *value_name,
			int def) 
{
    struct state_value *val;
    struct state_entity *e = lookup_entity(state_name);
    g_assert(e);
    g_assert(e->values);

    val = g_hash_table_lookup(e->values, value_name);
    if (val) {
	if (val->type == SV_int) {
	    return *(val->val.int_val);
	}
    } else {
	val = new_state_value(SV_int);
	*(val->val.int_val) = def;
	g_hash_table_insert(e->values, g_strdup(value_name), val);
    }
    return def;
}

void state_value_set_int(const char *state_name,
			 const char *value_name,
			 int n) 
{
    struct state_value *val;
    struct state_entity *e = lookup_entity(state_name);
    g_assert(e);
    g_assert(e->values);

    val = g_hash_table_lookup(e->values, value_name);
    if (val) {
	if (val->type == SV_int) {
	    *(val->val.int_val) = n;
	}
    } else {
	val = new_state_value(SV_int);
	*(val->val.int_val) = n;
	g_hash_table_insert(e->values, g_strdup(value_name), val);
    }
}

const char *state_value_get_string(const char *state_name,
				   const char *value_name,
				   const char *def) 
{
    struct state_value *val;
    struct state_entity *e = lookup_entity(state_name);
    g_assert(e);
    g_assert(e->values);

    val = g_hash_table_lookup(e->values, value_name);
    if (val) {
	if (val->type == SV_char) {
	    return val->val.char_val;
	}
    } else {
	if (def) {
	    val = new_state_value(SV_char);
	    val->val.char_val = g_strdup(def);
	    g_hash_table_insert(e->values, g_strdup(value_name), val);
	}
    }
    return def;
}

void state_value_set_string(const char *state_name,
			    const char *value_name,
			    const char *c) 
{
    struct state_value *val;
    struct state_entity *e = lookup_entity(state_name);
    g_assert(e);
    g_assert(e->values);

    val = g_hash_table_lookup(e->values, value_name);
    if (val) {
	if (val->type == SV_char) {
	    g_free_and_dup(val->val.char_val, c);
	}
    } else {
	val = new_state_value(SV_char);
	g_free_and_dup(val->val.char_val, c);
	g_hash_table_insert(e->values, g_strdup(value_name), val);
    }
}

const GList *state_value_get_list(const char *state_name,
				  const char *value_name)
{
    struct state_value *val;
    struct state_entity *e = lookup_entity(state_name);
    g_assert(e);
    g_assert(e->values);

    val = g_hash_table_lookup(e->values, value_name);
    if (val) {
	if (val->type == SV_list) {
	    return val->val.list_val;
	}
    }
    return NULL;
}

GList *copy_list_of_strings(const GList *src)
{
    GList *l = NULL;
    const GList *I;
    for (I = src ; I ; I = g_list_next(I) ) {
	l = g_list_append(l, g_strdup(I->data));
    }
    return l;
}

GList *free_list_of_strings(GList *l)
{
    if (l) {
	g_list_foreach(l, (GFunc) g_free, NULL);
	g_list_free(l);
    }
    return NULL;
}


void state_value_set_list(const char *state_name,
			  const char *value_name,
			  const GList *n) 
{
    struct state_value *val;
    struct state_entity *e = lookup_entity(state_name);
    const GList *I;

    g_assert(e);
    g_assert(e->values);

    val = g_hash_table_lookup(e->values, value_name);
    if (val) {
	if (val->type == SV_list) {
	    if (val->val.list_val && val->free_list_element) {
		g_list_foreach(val->val.list_val,
			       (GFunc) val->free_list_element, NULL);
	    }
	    if (val->val.list_val) {
		g_list_free(val->val.list_val);
	    }
	    val->val.list_val = NULL;
	}
    } else {
	val = new_state_value(SV_list);
	val->val.list_val = NULL;
	g_hash_table_insert(e->values, g_strdup(value_name), val);
    }
    
    for (I = n ; I ; I = g_list_next(I)) {
	val->val.list_val = g_list_append(val->val.list_val,
					  g_strdup(I->data));
    }
    val->free_list_element = (sv_free_list_func) g_free;
}

static void rm_entity_with_root(GHashTable *root, 
				const char *entity_name) 
{
    struct state_entity *e;
    const char *c;

    c = strchr(entity_name, '.'); /* UTF-8 OK: we do not use non-ASCII
				     entity names */
    if (c) {
	/* look for subentities! */
	char *d = g_strdup(entity_name);
	char *f = d + (c - entity_name);
	*f = 0;
	f++;

	e = g_hash_table_lookup(root, d);

	if (!e) {
	    return;
	}

	rm_entity_with_root(e->entities, f);
	return;
    }

    e = g_hash_table_lookup(root, entity_name);
    if (e) {
	free_state_entity(e);
	g_hash_table_remove(root, entity_name);
    }
}

void rm_value(const char *state_name)
{
    if (entities != NULL) {
	rm_entity_with_root(entities, state_name);
    }
}


static void width_height_state(GtkWidget *w,
			       GtkAllocation *allocation,
			       char *name)
{
    state_value_set_int(name, "width",  allocation->width);
    state_value_set_int(name, "height", allocation->height);
}

static void window_realized(GtkWidget *w,
			    char *name)
{
    GdkWindow *win = w->window;
    if (win && config->restore_window_positions) {
	int x = state_value_get_int(name, "x", -1);
	int y = state_value_get_int(name, "y", -1);
	
	if (x >= 0 && y >= 0) gdk_window_move(win, x, y);
    }
}

static void window_unrealized(GtkWidget *w, char *name)
{
    GdkWindow *win = w->window;

    if (win) {
	int x, y;
	gdk_window_get_position(win, &x, &y);

	state_value_set_int(name, "x", x);
	state_value_set_int(name, "y", y);
    }
}

GtkWidget *stateful_gtk_window_new(GtkWindowType type,
				   const char *name,
				   int w, int h)
{
    GtkWidget *wid = gtk_window_new(type);
    char *ud = g_strdup(name);

    gtk_object_set_data_full(GTK_OBJECT(wid), "name", ud, g_free);

    if (config->restore_window_sizes) {
	w = state_value_get_int(name, "width",  w);
	h = state_value_get_int(name, "height", h);
    }

    if (w != -1 && h != -1) {
	gtk_window_set_default_size(GTK_WINDOW(wid), w, h);
    }

    g_signal_connect(wid, "size-allocate",
		       G_CALLBACK(width_height_state),
		       ud);

    g_signal_connect_after(wid, "realize",
			     G_CALLBACK(window_realized),
			     ud);

    g_signal_connect(wid, "unrealize",
		       G_CALLBACK(window_unrealized),
		       ud);
    
    return wid;
}

/* filename_config returns the name of the config file. The returned
   pointer must g_free'd. */
static gchar *filename_state(int context)
{
     gchar *rcpath = NULL;
     char *home;
     char *env = getenv("GQSTATE");

     if (env) {
	 return g_strdup(env);
     } else {
	 home = homedir();
	 if(home == NULL) {
	     error_push(context, _("You have no home directory!"));
	     return(NULL);
	 }
	 
	 /* need add'l "/", thus add some extra chars */
	 rcpath = g_malloc(strlen(home) + strlen(STATEFILE) + 3);
	 
	 sprintf(rcpath, "%s/%s", home, STATEFILE);
	 g_free(home);
	 
	 return(rcpath);
     }
}

static void save_single_value(const char *value_name,
			      struct state_value *v,
			      struct writeconfig *wc)
{
    GList *l;
    GHashTable *attr = g_hash_table_new(g_str_hash, g_str_equal);

    g_hash_table_insert(attr, "name", (void *) value_name);
    g_hash_table_insert(attr, "type", 
			(void *) detokenize(token_value_type_names, v->type));

    switch (v->type) {
    case SV_int: 
	config_write_int(wc, *(v->val.int_val), "state-value", attr);
	break;
    case SV_char: 
	config_write_string(wc, v->val.char_val, "state-value", attr);
	break;
    case SV_list: 
	config_write_start_tag(wc, "state-value", attr);
	wc->indent++;

	for ( l = v->val.list_val ; l ; l = g_list_next(l)) {
	    config_write_string(wc, l->data, "list-item", NULL);
	}

	wc->indent--;
	config_write_end_tag(wc, "state-value");

	break;
    default: 
	abort();
    }

    g_hash_table_destroy(attr);
}

static void save_single_entity(const char *state_name,
			       struct state_entity *e,
			       struct writeconfig *wc)
{
    GHashTable *attr = g_hash_table_new(g_str_hash, g_str_equal);

    g_hash_table_insert(attr, "name", (void *) state_name);
    config_write_start_tag(wc, "entity", attr);

    g_hash_table_destroy(attr);

    wc->indent++;
    g_hash_table_foreach(e->values,
			 (GHFunc) save_single_value,
			 wc);

    g_hash_table_foreach(e->entities,
			 (GHFunc) save_single_entity,
			 wc);
    wc->indent--;

    config_write_end_tag(wc, "entity");
}

void save_state() {
    int save_context = error_new_context(_("Error saving statefile"), NULL);
    struct writeconfig *wc;
    gchar *statefile = filename_state(save_context);
    struct stat sfile;
    int mode = S_IRUSR|S_IWUSR;
    int fd;
    char *tmpstatefile = NULL;

    if (!statefile) {
	error_flush(save_context);
	return;
    }

    wc = new_writeconfig();
    
    /* write to temp file... */
    tmpstatefile = g_malloc(strlen(statefile) + 10);
    strcpy(tmpstatefile, statefile);
    strcat(tmpstatefile, ".new");


    /* check mode of original file. */
    if(stat(statefile, &sfile) == 0) {
	mode = sfile.st_mode & (S_IRUSR|S_IWUSR);
    }

    fd = open(tmpstatefile, O_CREAT | O_WRONLY, mode);
    if (fd < 0) {
	error_push(save_context,
		   _("Unable to open '%1$s' for writing: %2$s"), 
		   tmpstatefile,
		   strerror(errno));
	g_free(tmpstatefile);
	return;
    }
    wc->outfile = fdopen(fd, "w");

    config_write(wc, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");

    config_write_start_tag(wc, "gq-state", NULL);
    wc->indent++;

    g_hash_table_foreach(entities,
			 (GHFunc) save_single_entity,
			 wc);

    wc->indent--;
    config_write_end_tag(wc, "gq-state");

    free_writeconfig(wc);

    if (rename(tmpstatefile, statefile) != 0) {
	error_push(save_context,
		   _("Could not replace old configuration (%1$s) with the new one (%2$s):\n%3$s\n"),
		   statefile, tmpstatefile, strerror(errno));
    }

    g_free(tmpstatefile);
}

static void gq_stateS(struct parser_context *ctx,
		      struct tagstack_entry *e)
{
    e->data = g_hash_table_new(g_str_hash, g_str_equal);
    e->free_data = (free_func) g_hash_table_destroy;
}

static void gq_stateE(struct parser_context *ctx,
		      struct tagstack_entry *e)
{
    if (ctx->user_data) {
	struct parser_comm *comm =  (struct parser_comm *)ctx->user_data;

	comm->result = e->data;

	e->data = NULL;
	e->free_data = NULL;
    } else {
	/* FIXME: the free callback should clean up */
    }
}


static void entityS(struct parser_context *ctx,
		    struct tagstack_entry *e)
{
    struct state_entity *ent = new_state_entity();
    e->data = ent;
    e->free_data = (free_func) free_state_entity;
}

static void entityE(struct parser_context *ctx,
		    struct tagstack_entry *e)
{
    GHashTable *ents = peek_tag(ctx->stack, 1)->data;
    struct state_entity *ent = e->data;
    int i;

    for (i = 0 ; e->attrs[i] ; i += 2) {
	if (strcmp("name", (gchar*)e->attrs[i]) == 0) {
	    g_hash_table_insert(ents, g_strdup((gchar*)e->attrs[i+1]), 
				ent);

	    e->data = NULL;
	    e->free_data = NULL;

	    break;
	}
    }
}

static void entity_entityE(struct parser_context *ctx,
			   struct tagstack_entry *e)
{
    struct state_entity *parent = peek_tag(ctx->stack, 1)->data;
    struct state_entity *ent = e->data;
    int i;

    for (i = 0 ; e->attrs[i] ; i += 2) {
	if (strcmp("name", (gchar*)e->attrs[i]) == 0) {
	    g_hash_table_insert(parent->entities, g_strdup((gchar*)e->attrs[i+1]), 
				ent);

	    e->data = NULL;
	    e->free_data = NULL;

	    break;
	}
    }
}

static void state_valuesS(struct parser_context *ctx,
			  struct tagstack_entry *e)
{
/*     struct state_entity *ent = peek_tag(ctx->stack, 1)->data; */
    int i;
    enum state_value_type t = 0;

    for (i = 0 ; e->attrs[i] ; i += 2) {
	if (strcmp("type", (gchar*)e->attrs[i]) == 0) {
	    t = tokenize(token_value_type_names, (gchar*)e->attrs[i+1]);
	}
    }

    if (t != 0) {
	struct state_value *v = new_state_value(t);
	e->data = v;
	e->free_data = (free_func) free_state_value;
    } else {
	/** FIXME: report error **/
    }


}

static void state_valueE(struct parser_context *ctx,
			 struct tagstack_entry *e)
{
    struct state_entity *ent = peek_tag(ctx->stack, 1)->data;
    guchar const* n = NULL;
    int i;
    struct state_value *v = (struct state_value *) e->data;

    for (i = 0 ; e->attrs[i] ; i += 2) {
	if (strcmp("name", (gchar*)e->attrs[i]) == 0) {
	    n = e->attrs[i+1];
	}
    }

    if (n != NULL && v->type != 0) {
	char *ep;

	g_assert(v);

	switch (v->type) {
	case SV_int: 
	    *(v->val.int_val) = 
		strtol((gchar*)e->cdata, &ep, 10); /* FIXME handle error */
	    break;
	case SV_char: 
	    v->val.char_val = g_strdup((gchar*)e->cdata);
	    break;
	case SV_list: 
	    v->free_list_element = (sv_free_list_func) g_free;
	    break;
	default: 
	    abort();
	}

	g_hash_table_insert(ent->values, g_strdup((gchar*)n), 
			    v);

	e->data = NULL;
	e->free_data = NULL;
    }
}

static void list_itemE(struct parser_context *ctx,
		       struct tagstack_entry *e)
{
    struct state_value *v = peek_tag(ctx->stack, 1)->data;
    
    if (v->type == SV_list) {
	v->val.list_val = g_list_append(v->val.list_val, 
					g_strdup((gchar*)e->cdata));
    } else {
	/* FIXME: report error */
    }
}

static struct xml_tag state_tags[] = {
    { 
	"gq-state", 0, 
	gq_stateS, gq_stateE, 
	{ NULL },
    },
    { 
	"entity", 0, 
	entityS, entityE, 
	{ "gq-state", NULL },
    },
    { 
	"entity", 0, 
	entityS, entity_entityE, 
	{ "entity", NULL },
    },
    { 
	"state-value", 0, 
	state_valuesS, state_valueE, 
	{ "entity", NULL },
    },
    { 
	"list-item", 0, 
	NULL, list_itemE, 
	{ "state-value", NULL },
    },
    {
	NULL, 0,
	NULL, NULL, 
	{ NULL },
    }
};

static GHashTable *process_statefile_XML(int error_context,
					 const char *filename)
{
    xmlSAXHandler *handler = g_malloc0(sizeof(xmlSAXHandler));
    int rc;
    struct parser_comm comm;

    comm.error_context = error_context;
    comm.result = NULL;

    handler->error	= (errorSAXFunc) XMLerrorHandler;
    handler->fatalError	= (fatalErrorSAXFunc) XMLfatalErrorHandler;
    handler->warning	= (warningSAXFunc) XMLwarningHandler;

    rc = XMLparse(state_tags, handler, &comm, filename);

    g_free(handler);
    if (rc != 0) {
	/* FIXME - cleanup failed parsing attempt */
	comm.result = NULL;
    }

    return comm.result;
}

void init_state() {
    int load_context = error_new_context(_("Error loading statefile"), NULL);
    gchar *statefile = filename_state(load_context);

    if (statefile) {
	struct stat sfile;
	if (stat(statefile, &sfile) == 0) {
	    entities = process_statefile_XML(load_context, statefile);
	}
	g_free(statefile);
    }

    error_flush(load_context);
}
