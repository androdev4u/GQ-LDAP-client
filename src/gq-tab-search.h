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

/* $Id: gq-tab-search.h 958 2006-09-02 20:27:07Z herzi $ */

#ifndef GQ_SEARCH_H_INCLUDED
#define GQ_SEARCH_H_INCLUDED

#include "common.h"
#include "mainwin.h"

G_BEGIN_DECLS

typedef struct _GqTabSearch GqTabSearch;
typedef GqTabClass          GqTabSearchClass;

#define GQ_TYPE_TAB_SEARCH         (gq_tab_search_get_type())
#define GQ_TAB_SEARCH(i)           (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_TAB_SEARCH, GqTabSearch))
#define GQ_TAB_SEARCH_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_TAB_SEARCH, GqTabSearchClass))
#define GQ_IS_TAB_SEARCH(i)        (G_TYPE_CHECK_INSTANCE_TYPE((i), GQ_TYPE_TAB_SEARCH))
#define GQ_IS_TAB_SEARCH_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE((c), GQ_TYPE_TAB_SEARCH))
#define GQ_TAB_SEARCH_GET_CLASS(i) (G_TYPE_INSTANCE_GET_CLASS((i), GQ_TYPE_TAB_SEARCH, GqTabSearchClass))

GType gq_tab_search_get_type(void);

typedef enum {
	SEARCHARG_BEGINS_WITH,
	SEARCHARG_ENDS_WITH,
	SEARCHARG_CONTAINS,
	SEARCHARG_EQUALS
} GqSearchType;

struct _GqTabSearch {
	GqTab base_instance;

	GtkWidget *search_combo;
	GtkWidget *serverlist_combo;
	GtkWidget *searchbase_combo;
	GtkWidget *main_clist;
	int populated_searchbase;
	int search_lock;

	/* set gets used to pass the current result for some
	callbacks. There was no simple other way except to hack */
	struct dn_on_server *set;
	GList *history;

	int last_options_tab;

	int scope;
	int chase_ref;
	int max_depth;
	GList *attrs;
};

struct attrs {
     gchar *name;
     int column;
     struct attrs *next;
};

#define SEARCHBOX_PADDING 2

GqTab *new_searchmode();

char *make_filter(GqServer *server, char *querystring);

void fill_out_search(GqTab *tab,
		     GqServer *server,
		     const char *search_base_dn);

G_END_DECLS

#endif /* GQ_SEARCH_H_INCLUDED */

