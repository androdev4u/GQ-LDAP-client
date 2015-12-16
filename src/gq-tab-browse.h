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

#ifndef GQ_BROWSE_H_INCLUDED
#define GQ_BROWSE_H_INCLUDED

#include "common.h"
#include "gq-browser-node.h"
#include "gq-tab.h"   /* GqTab */

G_BEGIN_DECLS

typedef struct _GqTabBrowse GqTabBrowse;
typedef GqTabClass          GqTabBrowseClass;

#define GQ_TYPE_TAB_BROWSE         (gq_tab_browse_get_type())
#define GQ_TAB_BROWSE(i)           (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_TAB_BROWSE, GqTabBrowse))

GType gq_tab_browse_get_type(void);

struct _GqTabBrowse {
	GqTab base_instance;

     GList *cur_path;

     GQTreeWidget *ctreeroot;
     GtkWidget *ctree_refresh;        /* XXX try to get rid of this */
     GtkWidget *pane2_scrwin;
     GtkWidget *pane2_vbox;
     GtkWidget *mainpane;
     GQTreeWidgetNode *tree_row_selected;
     struct inputform *inputform;

     /* stuff that only gets used temporary, i.e. hacks */
     GQTreeWidgetNode *tree_row_popped_up;
     GQTreeWidgetNode *selected_ctree_node;

     /* lock used to suppress flickering during d'n'd */
     int update_lock;

     /* used to store old hide-button state - Hack */
     int hidden;
};




void record_path(GqTab *tab, GqBrowserNode *entry,
		 GQTreeWidget *ctreeroot, GQTreeWidgetNode *node);



/**************************************************************************/

/* get the server object for the current entry */
GqServer *server_from_node(GQTreeWidget *ctreeroot,
				    GQTreeWidgetNode *node);

/* void cleanup_browse_mode(GqTab *tab); */
GqTab *new_browsemode();

void refresh_subtree(int error_context,
		     GQTreeWidget *ctree,
		     GQTreeWidgetNode *node);

void refresh_subtree_new_dn(int error_context,
			    GQTreeWidget *ctree,
			    GQTreeWidgetNode *node,
			    const char *newdn,
			    int options);

void refresh_subtree_with_options(int error_context,
				  GQTreeWidget *ctree,
				  GQTreeWidgetNode *node,
				  int options);

GQTreeWidgetNode *show_server_dn(int error_context,
			     GQTreeWidget *tree,
			     GqServer *server, const char *dn,
			     gboolean select_node);
GQTreeWidgetNode *show_dn(int error_context,
		      GQTreeWidget *tree, GQTreeWidgetNode *node, const char *dn,
		      gboolean select_node);

GQTreeWidgetNode *node_from_dn(GQTreeWidget *ctreeroot,
			   GQTreeWidgetNode *top,
			   char *dn);

/* void add_single_server(GqTab *tab, GqServer *server); */

GQTreeWidgetNode *dn_browse_single_add(const char *dn,
				   GQTreeWidget *ctree,
				   GQTreeWidgetNode *node);


/* options to the refresh_subtree_with_options functions */
/* NOTE: browse-dnd.h defines additional options starting at 256 */
#define REFRESH_FORCE_EXPAND	1
#define REFRESH_FORCE_UNEXPAND	2

GQTreeWidgetNode *tree_node_from_server_dn(GQTreeWidget *ctree,
				       GqServer *server,
				       const char *dn);

void update_browse_serverlist(GqTab *tab);
void tree_row_close_connection(GtkMenuItem *menuitem, GqTab *tab);

void set_update_lock(GqTab *tab);
void release_update_lock(GqTab *tab);

G_END_DECLS

#endif

