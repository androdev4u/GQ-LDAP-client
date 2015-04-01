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

/* $Id: gq-tab-schema.h 955 2006-08-31 19:15:21Z herzi $ */

#ifndef GQ_SCHEMABROWSE_H_INCLUDED
#define GQ_SCHEMABROWSE_H_INCLUDED

#include <ldap_schema.h>

#include "common.h"
#include "gq-tab.h"  /* GqTab */

G_BEGIN_DECLS

typedef struct _GqTabSchema GqTabSchema;
typedef GqTabClass          GqTabSchemaClass;

#define GQ_TYPE_TAB_SCHEMA         (gq_tab_schema_get_type())
#define GQ_TAB_SCHEMA(i)           (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_TAB_SCHEMA, GqTabSchema))

GType gq_tab_schema_get_type(void);

struct _GqTabSchema {
	GqTab base_instance;

	GtkWidget *treeroot;
	GtkWidget *rightpane_vbox;
	GtkWidget *rightpane_notebook;
	GtkWidget *oc_vbox;
	GtkWidget *at_vbox;
	GtkWidget *mr_vbox;
	GtkWidget *s_vbox;
	LDAPObjectClass *cur_oc;
	LDAPAttributeType *cur_at;
	LDAPMatchingRule *cur_mr;
	LDAPSyntax *cur_s;
};

enum schema_detail_type {
     SCHEMA_TYPE_OC,
     SCHEMA_TYPE_AT,
     SCHEMA_TYPE_MR,
     SCHEMA_TYPE_S
};


GqTab *new_schemamode();
void popup_detail(enum schema_detail_type type, GqServer *server, void *detail);

void select_oc_from_clist(GtkWidget *clist, gint row, gint column,
			  GdkEventButton *event, gpointer data);
void select_at_from_clist(GtkWidget *clist, gint row, gint column,
			  GdkEventButton *event, gpointer data);
void select_mr_from_clist(GtkWidget *clist, gint row, gint column,
			  GdkEventButton *event, gpointer data);

void cleanup_schema_mode(void);

void update_schema_serverlist(GqTab *tab);

G_END_DECLS

#endif

