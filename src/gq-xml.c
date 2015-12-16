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

/* $Id: gq-xml.c 975 2006-09-07 18:44:41Z herzi $ */

#include <string.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>

#include "configfile.h"
#include "gq-server-list.h"
#include "gq-xml.h"
#include "xmlparse.h"
#include "xmlutil.h"
#include "util.h"
#include "ldif.h"
#include "syntax.h"
#include "filter.h"
#include "template.h"
#include "errorchain.h"
#include "common.h"
#include "encode.h"		/* gq_codeset */


static void gq_configS(struct parser_context *ctx,
		       struct tagstack_entry *e)
{
    e->data = new_config();
    e->free_data = (free_func) free_config;
}

static void gq_configE(struct parser_context *ctx,
		       struct tagstack_entry *e)
{
    if (ctx->user_data) {
	struct parser_comm *comm =  (struct parser_comm *)ctx->user_data;

	if (comm->result) {
	    free_config(comm->result);
	}
	comm->result = e->data;

	e->data = NULL;
	e->free_data = NULL;
    } else {
	/* the free callback will clean up */
    }
}

static long longCDATA(struct parser_context *ctx,
		      struct tagstack_entry *e) 
{
    char *ep;
    long l;

    l = strtol((gchar*)e->cdata, &ep, 10);
    if ((ep && *ep) || l < 0) {
	XMLhandleError(ctx,
		       _("Non-negative integer CDATA expected ('%s')"),
		       e->tag);
	return -1;
    }
    return l;
}

static int booleanCDATA(struct parser_context *ctx,
			struct tagstack_entry *e) 
{
    int b;

    if (strcasecmp("true", (gchar*)e->cdata) == 0) {
	b = 1;
    } else if (strcasecmp("false", (gchar*)e->cdata) == 0) {
	b = 0;
    } else {
	XMLhandleError(ctx,
		       _("Boolean CDATA ('true' or 'false') expected ('%s')"),
		       e->tag);
	return -1;
    }
    return b;
}

static void config_versionE(struct parser_context *ctx,
			    struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;
    long l = longCDATA(ctx, e);
    if (l >= 0) c->config_version = l;
}

static void asked_config_versionE(struct parser_context *ctx,
				  struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;
    long l = longCDATA(ctx, e);
    if (l >= 0) c->asked_version = l;
}


static void last_askedE(struct parser_context *ctx,
			struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data; 

    long l = longCDATA(ctx, e);
    if (l >= 0) c->last_asked = l;
}

static void confirm_modE(struct parser_context *ctx,
			 struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->confirm_mod = b;
}


static void show_dnE(struct parser_context *ctx,
		     struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->showdn = b;
}

static void show_ocE(struct parser_context *ctx,
		     struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->showoc = b;
}

static void show_rdn_onlyE(struct parser_context *ctx,
			   struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->show_rdn_only = b;
}


static void sort_search_modeE(struct parser_context *ctx,
			      struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->sort_search = b;
}

static void sort_browse_modeE(struct parser_context *ctx,
			      struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->sort_browse = b;
}

static void browse_use_user_friendlyE(struct parser_context *ctx,
				      struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->browse_use_user_friendly = b;
}

static void restore_window_sizesE(struct parser_context *ctx,
				  struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->restore_window_sizes = b;
}

static void restore_window_positionsE(struct parser_context *ctx,
				      struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->restore_window_positions = b;
}

static void restore_search_historyE(struct parser_context *ctx,
				    struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->restore_search_history = b;
}

static void restore_tabsE(struct parser_context *ctx,
			  struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->restore_tabs = b;
}

static void never_leak_credentialsE(struct parser_context *ctx,
				    struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;
    
    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->never_leak_credentials = b;
}

static void do_not_use_ldap_confE(struct parser_context *ctx,
				  struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;
    
    int b = booleanCDATA(ctx, e);
    if (b >= 0) c->do_not_use_ldap_conf = b;
}

static void ldif_formatE(struct parser_context *ctx,
			 struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    int t = tokenize(token_ldifformat, (gchar*)e->cdata);
    c->ldifformat = t;
}

static void search_argumentE(struct parser_context *ctx,
			     struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    /* compatibility with older versions of gq -- accept
       token number as well */

    gunichar ch = g_utf8_get_char((gchar*)e->cdata);
    if (g_unichar_isdigit(ch)) {
	long l = longCDATA(ctx, e);
	if (l >= 0) {
	    c->search_argument = l;
	} else {
	    XMLhandleError(ctx, _("Invalid token for '%s'"), e->tag);
	}
    } else {
	int t = tokenize(token_searchargument, (gchar*)e->cdata);
	c->search_argument = t;
    }
}


static void schema_serverE(struct parser_context *ctx,
			   struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(c->schemaserver, (gchar*)e->cdata);
}

static void ldapserverS(struct parser_context *ctx,
			struct tagstack_entry *e)
{
    GqServer *server = gq_server_new();
    e->data = server;
    e->free_data = (free_func)g_object_unref;
}

static void ldapserverE(struct parser_context *ctx,
			struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;
    GqServer *server = e->data;

    if (strcasecmp(server->pwencoding, "Base64") == 0 && server->bindpw[0]) {
	GByteArray *o = g_byte_array_new();
	b64_decode(o, server->bindpw, strlen(server->bindpw));

	server->bindpw = g_malloc(o->len + 1);
	strncpy(server->bindpw, (gchar*)o->data, o->len); /* UTF-8 OK */
	server->bindpw[o->len] = 0;
    } else if (server->bindpw[0] && server->pwencoding[0]) {
	XMLhandleError(ctx, _("Unsupported password encoding"));
    }

    canonicalize_ldapserver(server);
    gq_server_list_add(gq_server_list_get(), server);

    e->data = NULL;
    e->free_data = NULL;
}

static void ldapserver_nameE(struct parser_context *ctx,
			     struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(server->name, (gchar*)e->cdata);
}

static void ldapserver_ldaphostE(struct parser_context *ctx,
				 struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(server->ldaphost, (gchar*)e->cdata);
}

static void ldapserver_ldapportE(struct parser_context *ctx,
				 struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;

    long l = longCDATA(ctx, e);
    if (l >= 0) server->ldapport = l;
}


static void ldapserver_basednE(struct parser_context *ctx,
			       struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(server->basedn, (gchar*)e->cdata);
}


static void ldapserver_binddnE(struct parser_context *ctx,
			       struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(server->binddn, (gchar*)e->cdata);
}

static void ldapserver_bindpwE(struct parser_context *ctx,
			       struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(server->bindpw, (gchar*)e->cdata);
}


static void ldapserver_pw_encodingE(struct parser_context *ctx,
				    struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(server->pwencoding, (gchar*)e->cdata);
}


static void ldapserver_bindtypeE(struct parser_context *ctx,
				 struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;

    int t = tokenize(token_bindtype, (gchar*)e->cdata);
    server->bindtype = t;
}

static void ldapserver_search_attributeE(struct parser_context *ctx,
					 struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(server->searchattr, (gchar*)e->cdata);
}

static void ldapserver_ask_pwE(struct parser_context *ctx,
			       struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) server->ask_pw = b;
}

static void ldapserver_hide_internalE(struct parser_context *ctx,
				      struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) server->hide_internal = b;
}


static void ldapserver_show_refE(struct parser_context *ctx,
				 struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) server->show_ref = b;
}

static void ldapserver_enable_tlsE(struct parser_context *ctx,
				   struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) server->enabletls = b;
}

static void ldapserver_cache_connectionE(struct parser_context *ctx,
					 struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;

    int b = booleanCDATA(ctx, e);
    if (b >= 0) server->cacheconn = b;
}

static void ldapserver_local_cache_timeoutE(struct parser_context *ctx,
					    struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;

    long l = longCDATA(ctx, e);
    if (l >= 0) server->local_cache_timeout = l;
}

static void ldapserver_maxentriesE(struct parser_context *ctx,
				   struct tagstack_entry *e)
{
    GqServer *server = peek_tag(ctx->stack, 1)->data;

    long l = longCDATA(ctx, e);
    if (l >= 0) server->maxentries = l;
}




static void filterS(struct parser_context *ctx,
		    struct tagstack_entry *e)
{
    e->data = new_filter();
    e->free_data = (free_func) free_filter;
}



static void filterE(struct parser_context *ctx,
		    struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;

    c->filters = g_list_append(c->filters, e->data);
    e->data = NULL;
    e->free_data = NULL;
}


static void filter_nameE(struct parser_context *ctx,
			 struct tagstack_entry *e)
{
    struct gq_filter *filter = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(filter->name, (gchar*)e->cdata);
}

static void filter_ldapfilterE(struct parser_context *ctx,
			       struct tagstack_entry *e)
{
    struct gq_filter *filter = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(filter->ldapfilter, (gchar*)e->cdata);
}

static void filter_servernameE(struct parser_context *ctx,
			       struct tagstack_entry *e)
{
    struct gq_filter *filter = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(filter->servername, (gchar*)e->cdata);
}

static void filter_basednE(struct parser_context *ctx,
			   struct tagstack_entry *e)
{
    struct gq_filter *filter = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(filter->basedn, (gchar*)e->cdata);
}

static void templateS(struct parser_context *ctx,
		      struct tagstack_entry *e)
{
    e->data = new_template();
    e->free_data = (free_func) free_template;
}

static void templateE(struct parser_context *ctx,
		      struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 1)->data;
    c->templates = g_list_append(c->templates, e->data);
    e->free_data = NULL;
}



static void template_nameE(struct parser_context *ctx,
			   struct tagstack_entry *e)
{
    struct gq_template *t = peek_tag(ctx->stack, 1)->data;
    g_free_and_dup(t->name, (gchar*)e->cdata);
}


static void template_objectclassE(struct parser_context *ctx,
				  struct tagstack_entry *e)
{
    struct gq_template *t = peek_tag(ctx->stack, 1)->data;

    t->objectclasses = g_list_append(t->objectclasses, strdup((gchar*)e->cdata));
}

struct ddt {
    char *attr;
    long dt;
};

static void free_ddt(struct ddt *d) {
    if (d) {
	if (d->attr) free(d->attr);
	d->attr = NULL;
	free(d);
    }
}

static void display_typeS(struct parser_context *ctx,
			  struct tagstack_entry *e)
{
    e->data = calloc(1, sizeof(struct ddt));
    e->free_data = (free_func) free_ddt;
}

static void display_typeE(struct parser_context *ctx,
			  struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 3)->data;
    struct ddt *d = e->data;

    if (d->attr) {
	char *t;
	for (t = d->attr ; *t ; t++) *t = tolower(*t);
	
	if (get_dt_handler(d->dt)) {
	    struct attr_settings *as = 
		(struct attr_settings *) g_hash_table_lookup(c->attrs,
							     d->attr);
	    if (!as) {
		as = new_attr_settings();
		as->name = g_strdup(d->attr);
		g_hash_table_insert(c->attrs, g_strdup(as->name),
				    as);
	    }

	    as->defaultDT = d->dt;
	}
    }
}

static void ldapserver_dt_attributeE(struct parser_context *ctx,
				     struct tagstack_entry *e)
{
    struct ddt *d = peek_tag(ctx->stack, 1)->data;

    if (d->attr) free(d->attr);
    d->attr = strdup((gchar*)e->cdata);
}

static void ldapserver_dt_defaultE(struct parser_context *ctx,
				   struct tagstack_entry *e)
{
    struct ddt *d = peek_tag(ctx->stack, 1)->data;

    long l = longCDATA(ctx, e);
    if (l >= 0) d->dt = l;
}

static void ldap_attributeS(struct parser_context *ctx,
			    struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 3)->data;
    int i;

    for (i = 0 ; e->attrs[i] ; i += 2) {
	if (strcasecmp((gchar*)e->attrs[i], "name") == 0) {
	    char *tmp, *t;
	    struct attr_settings *as;

	    g_assert(e->attrs[i + 1]);

	    tmp = g_strdup((gchar*)e->attrs[i + 1]);
	    for (t = tmp ; *t ; t++) *t = tolower(*t);

	    as = (struct attr_settings *) g_hash_table_lookup(c->attrs, tmp);

	    if (!as) {
		as = new_attr_settings();
		as->name = g_strdup(tmp);
		g_hash_table_insert(c->attrs, g_strdup(as->name),
				    as);
	    }

	    g_free(tmp);

	    e->data = as;
	    e->free_data = NULL;
	    break;
	}
    }

    if (!e->data) {
	/* raise error: missing attribute "name" */
	XMLhandleError(ctx, _("Missing attribute 'name'"));
    }
}


static void ldap_attributeE(struct parser_context *ctx,
			    struct tagstack_entry *e)
{
    struct gq_config *c = peek_tag(ctx->stack, 3)->data;
    struct attr_settings *as;
    gpointer okey;

    g_assert(e->data);
    as = (struct attr_settings *) e->data;

    if (is_default_attr_settings(as)) {
	/* corresponds to default settings anyway - get rid of this
	   unneeded object */

	g_hash_table_lookup_extended(c->attrs, 
				     as->name, 
				     &okey, NULL);
	g_hash_table_remove(c->attrs, as->name);
	g_free(okey);

	free_attr_settings(as);
    }
}


static void dt_defaultE(struct parser_context *ctx,
			struct tagstack_entry *e)
{
    struct attr_settings *as =
	(struct attr_settings *) peek_tag(ctx->stack, 1)->data;

    long l = longCDATA(ctx, e);

    g_assert(as);

    if (l >= 0) as->defaultDT = l;
}

static void user_friendlyE(struct parser_context *ctx,
			   struct tagstack_entry *e)
{
    struct attr_settings *as =
	(struct attr_settings *) peek_tag(ctx->stack, 1)->data;

    g_assert(as);
    
    if (e->cdata && strlen((gchar*)e->cdata) > 0) {
	as->user_friendly = g_strdup((gchar*)e->cdata);
    }
}


static struct xml_tag config_tags[] = {
    { 
	"gq-config", 0, 
	gq_configS, gq_configE, 
	{ NULL },
    },
    { 
	"config-version", 0, 
	NULL, config_versionE, 
	{ "gq-config", NULL },
    },
    { 
	"asked-config-version", 0, 
	NULL, asked_config_versionE, 
	{ "gq-config", NULL },
    },
    { 
	"last-asked", 0, 
	NULL, last_askedE, 
	{ "gq-config", NULL },
    },
    { 
	"confirm-mod", 0, 
	NULL, confirm_modE, 
	{ "gq-config", NULL },
    },
    { 
	"search-argument", 0, 
	NULL, search_argumentE, 
	{ "gq-config", NULL },
    },
    { 
	"show-dn", 0, 
	NULL, show_dnE, 
	{ "gq-config", NULL },
    },
    { 
	"show-oc", 0, 
	NULL, show_ocE, 
	{ "gq-config", NULL },
    },
    { 
	"show-rdn-only", 0, 
	NULL, show_rdn_onlyE, 
	{ "gq-config", NULL },
    },
    { 
	"sort-search-mode", 0, 
	NULL, sort_search_modeE, 
	{ "gq-config", NULL },
    },
    { 
	"sort-browse-mode", 0, 
	NULL, sort_browse_modeE, 
	{ "gq-config", NULL },
    },
    { 
	"browse-use-user-friendly", 0, 
	NULL, browse_use_user_friendlyE, 
	{ "gq-config", NULL },
    },
    { 
	"restore-window-sizes", 0, 
	NULL, restore_window_sizesE, 
	{ "gq-config", NULL },
    },
    { 
	"restore-window-positions", 0, 
	NULL, restore_window_positionsE, 
	{ "gq-config", NULL },
    },
    { 
	"restore-search-history", 0, 
	NULL, restore_search_historyE, 
	{ "gq-config", NULL },
    },
    { 
	"restore-tabs", 0, 
	NULL, restore_tabsE, 
	{ "gq-config", NULL },
    },
    { 
	"never-leak-credentials", 0, 
	NULL, never_leak_credentialsE, 
	{ "gq-config", NULL },
    },
    { 
	"do-not-use-ldap-conf", 0, 
	NULL, do_not_use_ldap_confE, 
	{ "gq-config", NULL },
    },
    { 
	"ldif-format", 0, 
	NULL, ldif_formatE, 
	{ "gq-config", NULL },
    },
    { 
	"schema-server", 0, 
	NULL, schema_serverE, 
	{ "gq-config", NULL },
    },

    /* ldapserver tags */
    { 
	"ldapserver", 0, 
	ldapserverS, ldapserverE, 
	{ "gq-config", NULL },
    },
    { 
	"name", 0, 
	NULL, ldapserver_nameE, 
	{ "ldapserver", NULL },
    },
    { 
	"ldaphost", 0, 
	NULL, ldapserver_ldaphostE, 
	{ "ldapserver", NULL },
    },
    { 
	"ldapport", 0, 
	NULL, ldapserver_ldapportE, 
	{ "ldapserver", NULL },
    },
    { 
	"basedn", 0, 
	NULL, ldapserver_basednE, 
	{ "ldapserver", NULL },
    },
    { 
	"binddn", 0, 
	NULL, ldapserver_binddnE, 
	{ "ldapserver", NULL },
    },
    { 
	"bindpw", 0, 
	NULL, ldapserver_bindpwE, 
	{ "ldapserver", NULL },
    },
    { 
	"pw-encoding", 0, 
	NULL, ldapserver_pw_encodingE, 
	{ "ldapserver", NULL },
    },
    { 
	"bindtype", 0, 
	NULL, ldapserver_bindtypeE, 
	{ "ldapserver", NULL },
    },
    { 
	"search-attribute", 0, 
	NULL, ldapserver_search_attributeE, 
	{ "ldapserver", NULL },
    },
    { 
	"ask-pw", 0, 
	NULL, ldapserver_ask_pwE, 
	{ "ldapserver", NULL },
    },
    { 
	"hide-internal", 0, 
	NULL, ldapserver_hide_internalE, 
	{ "ldapserver", NULL },
    },
    { 
	"show-ref", 0, 
	NULL, ldapserver_show_refE, 
	{ "ldapserver", NULL },
    },
    { 
	"enable-tls", 0, 
	NULL, ldapserver_enable_tlsE, 
	{ "ldapserver", NULL },
    },
    { 
	"cache-connection", 0, 
	NULL, ldapserver_cache_connectionE, 
	{ "ldapserver", NULL },
    },
    { 
	"local-cache-timeout", 0, 
	NULL, ldapserver_local_cache_timeoutE, 
	{ "ldapserver", NULL },
    },
    { 
	"maxentries", 0, 
	NULL, ldapserver_maxentriesE, 
	{ "ldapserver", NULL },
    },

    /* templates */
    { 
	"template", 0, 
	templateS, templateE, 
	{ "gq-config", NULL },
    },
    { 
	"name", 0, 
	NULL, template_nameE, 
	{ "template", NULL },
    },
    { 
	"objectclass", 0, 
	NULL, template_objectclassE, 
	{ "template", NULL },
    },

    /* filter */
    { 
	"filter", 0, 
	filterS, filterE, 
	{ "gq-config", NULL },
    },
    { 
	"name", 0, 
	NULL, filter_nameE, 
	{ "filter", NULL },
    },
    { 
	"ldapfilter", 0, 
	NULL, filter_ldapfilterE, 
	{ "filter", NULL },
    },
    { 
	"servername", 0, 
	NULL, filter_servernameE, 
	{ "filter", NULL },
    },
    { 
	"basedn", 0, 
	NULL, filter_basednE, 
	{ "filter", NULL },
    },

    /* defaults */

    { 
	"defaults", 0, 
	NULL, NULL, 
	{ "gq-config", NULL },
    },

    /* ldap-attributes */
    { 
	"ldap-attributes", 0, 
	NULL, NULL, 
	{ "defaults", NULL },
    },

    /* ldap-attribute */
    { 
	"ldap-attribute", 0, 
	ldap_attributeS, ldap_attributeE, 
	{ "ldap-attributes", "defaults", NULL },
    },

    { 
	"dt-default", 0, 
	NULL, dt_defaultE, 
	{ "ldap-attribute", NULL },
    },

    { 
	"user-friendly", 0, 
	NULL, user_friendlyE, 
	{ "ldap-attribute", NULL },
    },


#ifndef NO_DEPRECATED
    /* display-types */

    { 
	"display-types", 0, 
	NULL, NULL, 
	{ "defaults", NULL },
    },

    /* display-type */

    { 
	"display-type", 0, 
	display_typeS, display_typeE, 
	{ "display-types", "defaults", "gq-config", NULL },
    },
    { 
	"dt-attribute", 0, 
	NULL, ldapserver_dt_attributeE, 
	{ "display-type", NULL },
    },
    { 
	"dt-default", 0, 
	NULL, ldapserver_dt_defaultE, 
	{ "display-type", NULL },
    },

#endif


    {
	NULL, 0,
	NULL, NULL,
	{ NULL },
    },

};


static int dot_gq_upgrade_hack(int error_context, const char *filename)
{
    FILE *fp = fopen(filename, "r");
    int changed = 0;

    if (fp) {
	/* nasty heuristics to find out if .gq is "old" (written with
	   a gq that does not yet use libxml) or "new". 

	   The way this is done is by checking for an "encoding" in
	   the first line of .gq (should be: in the xml declaration,
	   actually). This works because the encoding is only there in
	   "new" versions of .gq.

	   The situation does not allow to do it differently: We
	   cannot just check for the config-version, because for
	   this to work the parser would need to run.

	   A better way would be to check the encoding when the parser
	   sees the xml decl. But libxml seems not to allow for this.
	*/

	char firstline[255];
	fgets(firstline, sizeof(firstline) - 1, fp);
	rewind(fp);
	
	if (strstr(firstline, "encoding=") != NULL ) {
	    /* NEW version - everything is OK */
	    fclose(fp);
	    return 0;
	} else {
	    char *cfgbuf;
	    GString *newbuf;
	    int i;
	    struct stat sfile;

	    /* OLD version */

	    /* more nastiness... since this config file was written by a
	       pre-UTF-8 version of GQ, it's not going to have <>&'" escaped
	       properly... fudge this now: silently rewrite the config
	       file on disk, then reopen it for the XML parser */
	    changed = 0;
	    stat(filename, &sfile);
	    cfgbuf = g_malloc(sfile.st_size + 1);
	    fread(cfgbuf, 1, sfile.st_size, fp);

	    cfgbuf[sfile.st_size] = 0;
	    newbuf = g_string_sized_new(sfile.st_size + 128);

	    /* take care of the XML declaration in the first line... */

	    g_string_sprintf(newbuf, 
			     "<?xml version=\"1.0\" encoding=\"%s\" standalone=\"yes\"?>",
			     gq_codeset);

	    /* skip line 1 in the input buffer */
	    for(i = 0 ; cfgbuf[i] && cfgbuf[i] != '\n' ; i++) ;
	    for( ; cfgbuf[i] ; i++) {
		switch(cfgbuf[i]) {
		case '\\':
		    /* < and > used to be escaped with a \ */
		    if(cfgbuf[i + 1] == '<') {
			g_string_append(newbuf, "&lt;");
			changed++;
			i++;
		    }
		    else if(cfgbuf[i + 1] == '>') {
			g_string_append(newbuf, "&gt;");
			changed++;
			i++;
		    }
		    else
			g_string_append_c(newbuf, cfgbuf[i]);
		    break;

		case '&':
		    g_string_append(newbuf, "&amp;");
		    changed++;
		    break;
		default:
		    g_string_append_c(newbuf, cfgbuf[i]);
		}
	    }
	    fclose(fp);

	    if(changed) {
		int fd;
		int rc;
		int wrote;

		int l = strlen(filename) + 20;
		char *tmp = g_malloc(l);
		g_snprintf(tmp, l, "%s.tmp", filename);

		unlink(tmp); /* provide very minor security */
		fd = open(tmp, O_CREAT | O_WRONLY, 
			  sfile.st_mode & (S_IRWXU | S_IRWXG | S_IRWXO)
			  );

		if (fd < 0) {
		    error_push(error_context,
			       _("Could not open temporary configuration file '%1$s': %2$s"),
			       tmp, strerror(errno));
		    return -1;
		} 

		wrote = write(fd, newbuf->str, newbuf->len);
		if (wrote == (int) newbuf->len) {
		    close(fd);
		    rename(tmp, filename);
		    return 1;
		}
		close(fd);
		error_push(error_context, 
			   _("Could not write silently upgraded configuration file"));
		return -1; /* fwrite error */
	    }
	    
	    g_free(cfgbuf);
	    g_string_free(newbuf, TRUE);
	}
	return 1; /* silent upgrade done */
    }
    if (errno != ENOENT) {
	error_push(error_context,
		   _("Could not open configuration file '%1$s': %2$s"),
		   filename, strerror(errno));
    }
    return -1; /* failed (fopen) */
}

struct gq_config *process_rcfile_XML(int error_context, const char *filename)
{
    xmlSAXHandler *handler = g_malloc0(sizeof(xmlSAXHandler));
    int rc;
    struct parser_comm comm;

    comm.error_context = error_context;
    comm.result = NULL;

    handler->error	= (errorSAXFunc) XMLerrorHandler;
    handler->fatalError	= (fatalErrorSAXFunc) XMLfatalErrorHandler;
    handler->warning	= (warningSAXFunc) XMLwarningHandler;

    dot_gq_upgrade_hack(error_context, filename);
    rc = XMLparse(config_tags, handler, &comm, filename);

    g_free(handler);
    if (rc != 0) {
	if (comm.result) {
	    free_config(comm.result);
	}
	comm.result = NULL;
    }

    return comm.result;
}

#ifdef TEST_MAIN

int main_test(int argc, char **argv)
{
    struct gq_config *config = NULL;
    int rc = 0;
    int load_context, write_context;

    init_syntaxes();

    load_context = error_new_context(_("Error loading configfile"));

    /*     rc = XMLparse(config_tags, NULL, &config, argv[1]); */
    config = process_rcfile_XML(load_context, argv[1]);

    if (config) {
	write_context = error_new_context(_("Error writing configfile"));
	save_config_internal(write_context, config, "tmp-conf");
    }

    return rc;
}

#endif
