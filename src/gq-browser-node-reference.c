/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 Peter Stamfest <peter@stamfest.at>

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

#include "gq-browser-node-reference.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <errno.h>
#include <string.h>
#include <stdlib.h>		/* free */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include "common.h"
#include "gq-browser-node-dn.h"
#include "gq-browser-node-server.h"

#include "gq-server-list.h"
#include "gq-tab-browse.h"
#include "configfile.h"		/* free_ldapserver */
#include "prefs.h"		/* create_edit_server_window */

#include "util.h"
#include "errorchain.h"
#include "encode.h"
#include "browse-export.h"

/**************************************************************************/



static void
free_ref_browse_entry(GqBrowserNode *e)
{
	GqBrowserNodeReference *entry;

	g_return_if_fail(GQ_IS_BROWSER_NODE_REFERENCE(e));

	entry = GQ_BROWSER_NODE_REFERENCE(e);

	g_free(entry->uri);
	if(entry->server) {
		g_object_unref(entry->server);
		entry->server = NULL;
	}

	g_free(entry);
}

/*
 * a ref browse entry was selected in the tree widget.
 *
 * put up some info.
 */
static void ref_browse_entry_selected(GqBrowserNode *be,
				      int error_context,
				      GQTreeWidget *ctree,
				      GQTreeWidgetNode *node,
				      GqTab *tab)
{
     GtkWidget *pane2_scrwin, *pane2_vbox, *label, *e;
     GtkWidget *table;
     int row = 0;
     GqBrowserNodeReference *entry;

     g_assert(GQ_IS_BROWSER_NODE_REFERENCE(be));
     entry = GQ_BROWSER_NODE_REFERENCE(be);

     record_path(tab, GQ_BROWSER_NODE(entry), ctree, node);

     pane2_scrwin = GQ_TAB_BROWSE(tab)->pane2_scrwin;
     pane2_vbox = GQ_TAB_BROWSE(tab)->pane2_vbox;

     /*  	  gtk_widget_destroy(pane2_vbox); */
     /* remove the viewport of the scrolled window. This should
	_really_ destroy the widgets below it. The pane2_scrwin
	is a GtkBin Object and thus has only one child, use this
	to obtain the viewport */

     gtk_container_remove(GTK_CONTAINER(pane2_scrwin),
			  GTK_BIN(pane2_scrwin)->child);

     pane2_vbox = gtk_vbox_new(FALSE, 2);
     GQ_TAB_BROWSE(tab)->pane2_vbox = pane2_vbox;

     gtk_widget_show(pane2_vbox);
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(pane2_scrwin),
					   pane2_vbox);

     table = gtk_table_new(5, 2, FALSE);
     gtk_container_border_width(GTK_CONTAINER(table), 5);
     gtk_widget_show(table);
     gtk_box_pack_start(GTK_BOX(pane2_vbox), table, FALSE, FALSE, 5);

     /* URI */
     label = gtk_label_new(_("Referral URI"));
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table),
		      label,
		      0, 1, row, row+1,
		      GTK_SHRINK,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     e = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(e), entry->uri);
     gtk_widget_set_sensitive(e, FALSE);
     gtk_widget_show(e);
     gtk_table_attach(GTK_TABLE(table),
		      e,
		      1, 2, row, row+1,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);
     row++;
}

static void ref_browse_entry_expand(GqBrowserNode *be,
				    int error_context,
				    GQTreeWidget *ctree,
				    GQTreeWidgetNode *node,
				    GqTab *tab)
{
     GqBrowserNodeReference *entry;
     g_assert(GQ_IS_BROWSER_NODE_REFERENCE(be));
     entry = GQ_BROWSER_NODE_REFERENCE(be);

     if (!entry->expanded) {
	  LDAPURLDesc *desc = NULL;

	  gq_tree_remove_children (ctree, node);

	  if (ldap_url_parse(entry->uri, &desc) == 0) {
	       GqServer *parent = NULL, *newserver = NULL;
	       char *dummy[] = { "dummy", NULL };
	       GQTreeWidgetNode *new_item, *added = NULL;
	       GqBrowserNode *new_entry;


	       /* find parent server */
	       GQTreeWidgetRow *row = NULL;
	       GQTreeWidgetNode *n;
	       GqBrowserNode *e;

	       n = GQ_TREE_WIDGET_ROW(node)->parent;
	       for ( ; n ; n = row->parent ) {
		    row = GQ_TREE_WIDGET_ROW(n);
		    e = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, n));

		    if(!GQ_IS_BROWSER_NODE_DN(e)) {
			    parent = gq_browser_node_get_server(e);
			    break;
		    }
	       }

	       if (!parent) {
		    return;
	       }

	       newserver = get_referral_server(error_context,
					       parent, entry->uri);

	       newserver->quiet = 1;
	       canonicalize_ldapserver(newserver);

	       transient_add_server(newserver);

	       entry->server = g_object_ref(newserver);

	       entry->expanded = TRUE;

	       gtk_clist_freeze(GTK_CLIST(ctree));

	       new_entry = gq_browser_node_dn_new(desc->lud_dn);

	       added = gq_tree_insert_node (ctree,
					    node, NULL,
					    desc->lud_dn,
					    new_entry,
					    g_object_unref);

	       gq_tree_widget_node_set_row_data_full(ctree,
						added,
						new_entry,
						g_object_unref);

	       gq_tree_insert_dummy_node (ctree,
					  added);

	       gtk_clist_thaw(GTK_CLIST(ctree));

	       ldap_free_urldesc(desc);
	  }
     }
}

static char* ref_browse_entry_get_name(GqBrowserNode *entry, gboolean long_form)
{
     char *g;

     g_assert(GQ_IS_BROWSER_NODE_REFERENCE(entry));

     g = g_strdup(GQ_BROWSER_NODE_REFERENCE(entry)->uri);

     return g;
}

static void ref_browse_entry_refresh(GqBrowserNode *entry,
				     int error_context, 
				     GQTreeWidget *ctree,
				     GQTreeWidgetNode *node,
				     GqTab *tab)
{
     g_assert(GQ_IS_BROWSER_NODE_REFERENCE(entry));

     GQ_BROWSER_NODE_REFERENCE(entry)->expanded = 0;

     gtk_clist_freeze(GTK_CLIST(ctree));

     ref_browse_entry_selected(entry, error_context, ctree, node, tab);

     gq_tree_fire_expand_callback (ctree, node);

/*       server_browse_entry_expand(entry, ctree, node, tab); */

     gtk_clist_thaw(GTK_CLIST(ctree));

}

static void add_to_permanent_servers(GtkWidget *menu_item,
				     GqServer *server)
{
     /* no assertion, it could happen... */
     if (is_transient_server(server)) {
	  int ctx = error_new_context(_("Adding server permanently"),
				      menu_item);

	  if (gq_server_list_get_by_name(gq_server_list_get(), server->name) == NULL) {
	       GQServerList* list = gq_server_list_get();
	       GqServer *s = gq_server_new();
	       copy_ldapserver(s, server);
	       gq_server_list_add(list, s);
	       if (save_config_ext(ctx)) {
		    update_serverlist(&mainwin);
	       } else {
		    /* save failed - undo changes */
		    gq_server_list_remove(list, s);
	       }
	  } else {
	       error_push(ctx,
			  _("Another server with the name '%s' already exists."),
			  server->name);
	  }
	  /* popup error, if any */
	  error_flush(ctx);
     }
}


struct edit_server_cb_data {
     GqServer *server;
     GqTab *tab;
};

static void
free_edit_server_cb_data(struct edit_server_cb_data *cbd)
{
	g_object_unref(cbd->server);
	cbd->server = NULL;
	g_free(cbd);
}

static void edit_server_activated(struct edit_server_cb_data *cbd)
{
     create_edit_server_window(cbd->server, cbd->tab->win->mainwin);
}

static void dump_ref(GtkWidget *widget, GqTab *tab)
{
     GQTreeWidget *ctree;
     GQTreeWidgetNode *node;
     GqBrowserNode *e;
     GqServer *server;
     GList *bases = NULL;
     GList *to_export = NULL, *I;
     struct dn_on_server *dos;
     int error_context;

     ctree = GQ_TAB_BROWSE(tab)->ctreeroot;
     node = GQ_TAB_BROWSE(tab)->tree_row_popped_up;
     e = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, node));

     g_assert(GQ_IS_BROWSER_NODE_REFERENCE(e));

     server = server_from_node(ctree, node);

     if (e == NULL || server == NULL)
	  return;

     error_context = error_new_context(_("Exporting referred-to server/DN to LDIF"),
				       tab->win->mainwin);

     bases = get_suffixes(error_context, GQ_BROWSER_NODE_REFERENCE(e)->server);

     /* turn suffixes list into a list of dn_on_server objects
	(impedance match) */
     for (I = g_list_first(bases) ; I ; I = g_list_next(I) ) {
	  dos = new_dn_on_server(I->data, server);
	  dos->flags = LDAP_SCOPE_SUBTREE; /* default is LDAP_SCOPE_BASE */
	  to_export = g_list_append(to_export, dos);
	  g_free(I->data);
	  I->data = NULL;
     }
     g_list_free(bases);

     export_many(error_context, tab->win->mainwin, to_export);

     error_flush(error_context);

}


static void ref_browse_entry_popup(GqBrowserNode *entry,
				   GtkWidget *menu,
				   GQTreeWidget *ctreeroot,
				   GQTreeWidgetNode *ctree_node,
				   GqTab *tab)
{
     GtkWidget *menu_item;
     GqServer *server;
     struct edit_server_cb_data *cbd;

     g_assert(GQ_IS_BROWSER_NODE_REFERENCE(entry));

     server = server_from_node(ctreeroot, ctree_node);

     /* Edit Server settings */
     menu_item = gtk_menu_item_new_with_label(_("Edit Server"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     gtk_widget_show(menu_item);

     cbd = (struct edit_server_cb_data *)
	  g_malloc0(sizeof(struct edit_server_cb_data));
     cbd->server = g_object_ref(server);
     cbd->tab = tab;

     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(edit_server_activated),
			       cbd);

     /* explicitly attach cbd to assure call to destructor */
     gtk_object_set_data_full(GTK_OBJECT(menu_item), "cbd",
			      cbd, (GtkDestroyNotify)free_edit_server_cb_data);

     gtk_widget_show(menu_item);

     if (server == NULL) {
	  gtk_widget_set_sensitive(menu_item, FALSE);
     }

     /* Add to permanent list of servers */
     menu_item = gtk_menu_item_new_with_label(_("Add to permanent list of servers"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     gtk_widget_show(menu_item);

     g_signal_connect(menu_item, "activate",
			G_CALLBACK(add_to_permanent_servers),
			server);

     gtk_widget_show(menu_item);

     if (server == NULL || !is_transient_server(server)) {
	  gtk_widget_set_sensitive(menu_item, FALSE);
     }
     
     /* Export to LDIF */
     menu_item = gtk_menu_item_new_with_label(_("Export to LDIF"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(dump_ref),
			tab);
     gtk_widget_show(menu_item);

     /* Close connection */
     menu_item = gtk_menu_item_new_with_label(_("Close Connection"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(tree_row_close_connection),
			tab);
     gtk_widget_show(menu_item);

     if (server == NULL) {
	  gtk_widget_set_sensitive(menu_item, FALSE);
     }
}

GqBrowserNode*
gq_browser_node_reference_new(const char *uri)
{
     GqBrowserNodeReference *e = g_object_new(GQ_TYPE_BROWSER_NODE_REFERENCE, NULL);

     e->uri = g_strdup(uri);

     return GQ_BROWSER_NODE(e);
}

/* GType */
G_DEFINE_TYPE(GqBrowserNodeReference, gq_browser_node_reference, GQ_TYPE_BROWSER_NODE);

static void
gq_browser_node_reference_init(GqBrowserNodeReference* self) {}

static GqServer*
reference_node_get_server(GqBrowserNode* node)
{
	return GQ_BROWSER_NODE_REFERENCE(node)->server;
}

static void
gq_browser_node_reference_class_init(GqBrowserNodeReferenceClass* self_class)
{
	GqBrowserNodeClass* node_class = GQ_BROWSER_NODE_CLASS(self_class);
	node_class->destroy    = free_ref_browse_entry;
	node_class->expand     = ref_browse_entry_expand;
	node_class->select     = ref_browse_entry_selected;
	node_class->refresh    = ref_browse_entry_refresh;
	node_class->get_name   = ref_browse_entry_get_name;
	node_class->popup      = ref_browse_entry_popup;
	node_class->get_server = reference_node_get_server;
};

