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

#include "gq-browser-node-server.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <errno.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include "gq-browser-node-dn.h"
#include "gq-tab-browse.h"
#include "prefs.h"			/* create_edit_server_window */
#include "util.h"			/* get_suffixes */

#include "browse-export.h"
#include "errorchain.h"

#ifdef BROWSER_DND
#include "browse-dnd.h"
#endif

/*
 *  Really add a single suffix to the tree
 */
static void add_suffix(GqBrowserNodeServer *entry,
		       GQTreeWidget *ctreeroot, GQTreeWidgetNode *node,
		       char *suffix)
{
     GQTreeWidgetNode *new_item;
     GqBrowserNode *new_entry = gq_browser_node_dn_new(suffix);

     new_item = gq_tree_insert_node (ctreeroot,
				     node, NULL,
				     suffix,
				     new_entry,
				     g_object_unref);

     gq_tree_insert_dummy_node (ctreeroot,
				new_item);
}

static void
server_browse_entry_destroy(GqBrowserNode *entry)
{
	GqBrowserNodeServer* self;
	g_return_if_fail(GQ_IS_BROWSER_NODE_SERVER(entry));

	self = GQ_BROWSER_NODE_SERVER(entry);
	g_object_unref(self->server);
	self->server = NULL;

	g_free(entry);
}

static void server_browse_entry_expand(GqBrowserNode *e,
				       int error_context,
				       GQTreeWidget *ctree, GQTreeWidgetNode *node,
				       GqTab *tab)
{
     GList *suffixes = NULL, *next;
     GqBrowserNodeServer *entry;

     g_assert(GQ_IS_BROWSER_NODE_SERVER(e));
     entry = GQ_BROWSER_NODE_SERVER(e);

     if (!entry->once_expanded) {
/*	  printf("expanding %s\n", entry->server->name); */

	  gq_tree_remove_children (ctree, node);
	  entry->once_expanded = 1;

	  suffixes = get_suffixes(error_context, entry->server);

	  gtk_clist_freeze(GTK_CLIST(ctree));

	  for (next = suffixes ; next ; next = g_list_next(next) ) {
	       add_suffix(entry, ctree, node, next->data);
	       g_free(next->data);
	       next->data = NULL;
	  }

	  gtk_clist_thaw(GTK_CLIST(ctree));

	  g_list_free(suffixes);
     }
}

/*
 * a server was selected in the tree widget.
 *
 * put up some server info.
 */
static void server_browse_entry_selected(GqBrowserNode *be,
					 int error_context,
					 GQTreeWidget *ctree,
					 GQTreeWidgetNode *node,
					 GqTab *tab)
{
     GtkWidget *pane2_scrwin, *pane2_vbox, *label, *e;
     GtkWidget *table;
     char *server_name;
     int row = 0;
     char buf[128];
     LDAP *ld;
     GqBrowserNodeServer *entry;

     g_assert(GQ_IS_BROWSER_NODE_SERVER(be));
     entry = GQ_BROWSER_NODE_SERVER(be);

     ld = open_connection(error_context, entry->server);
     if (!ld) return;

     server_name = entry->server->name; /* dn_by_node(node); */
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

     /* Nickname */
     label = gtk_label_new(_("Nickname"));
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table),
		      label,
		      0, 1, row, row+1,
		      GTK_SHRINK,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     e = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(e), server_name);
     gtk_widget_set_sensitive(e, FALSE);
     gtk_widget_show(e);
     gtk_table_attach(GTK_TABLE(table),
		      e,
		      1, 2, row, row+1,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);
     row++;

     /* Host name */
     label = gtk_label_new(_("Hostname"));
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table),
		      label,
		      0, 1, row, row+1,
		      GTK_SHRINK,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     e = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(e), entry->server->ldaphost);
     gtk_widget_set_sensitive(e, FALSE);
     gtk_widget_show(e);
     gtk_table_attach(GTK_TABLE(table),
		      e,
		      1, 2, row, row+1,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);
     row++;

     /* Port */
     label = gtk_label_new(_("Port"));
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table),
		      label,
		      0, 1, row, row+1,
		      GTK_SHRINK,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     g_snprintf(buf, sizeof(buf), "%d", entry->server->ldapport);
     e = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(e), buf);
     gtk_widget_set_sensitive(e, FALSE);
     gtk_widget_show(e);
     gtk_table_attach(GTK_TABLE(table),
		      e,
		      1, 2, row, row+1,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);
     row++;

     /* Connection caching */
     label = gtk_label_new(_("Connection caching"));
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table),
		      label,
		      0, 1, row, row+1,
		      GTK_SHRINK,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     g_snprintf(buf, sizeof(buf), "%s", 
	      entry->server->cacheconn ? _("on") : _("off"));
     e = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(e), buf);
     gtk_widget_set_sensitive(e, FALSE);
     gtk_widget_show(e);
     gtk_table_attach(GTK_TABLE(table),
		      e,
		      1, 2, row, row+1,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     row++;

     /* TLS */
     label = gtk_label_new(_("TLS"));
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table),
		      label,
		      0, 1, row, row+1,
		      GTK_SHRINK,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     g_snprintf(buf, sizeof(buf), "%s", 
	      entry->server->enabletls ? _("on") : _("off"));
     e = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(e), buf);
     gtk_widget_set_sensitive(e, FALSE);
     gtk_widget_show(e);
     gtk_table_attach(GTK_TABLE(table),
		      e,
		      1, 2, row, row+1,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     row++;

#if HAVE_LDAP_CLIENT_CACHE
     label = gtk_label_new(_("Client-side caching"));
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table),
		      label,
		      0, 1, row, row+1,
		      GTK_SHRINK,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     g_snprintf(buf, sizeof(buf), "%s", 
	      (entry->server->local_cache_timeout >= 0) ? _("on") : _("off"));
     e = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(e), buf);
     gtk_widget_set_sensitive(e, FALSE);
     gtk_widget_show(e);
     gtk_table_attach(GTK_TABLE(table),
		      e,
		      1, 2, row, row+1,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     row++;
#endif

     /* Connections so far */
     label = gtk_label_new(_("Connections so far"));
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table),
		      label,
		      0, 1, row, row+1,
		      GTK_SHRINK,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     g_snprintf(buf, sizeof(buf), "%d", 
	      entry->server->incarnation);
     e = gtk_entry_new();
     gtk_entry_set_text(GTK_ENTRY(e), buf);
     gtk_widget_set_sensitive(e, FALSE);
     gtk_widget_show(e);
     gtk_table_attach(GTK_TABLE(table),
		      e,
		      1, 2, row, row+1,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      GTK_EXPAND | GTK_SHRINK | GTK_FILL,
		      0, 0);

     row++;

     if (ld) {
	  int intdata;
	  int rc;
	  /*  	       void *optdata; */
	  char *rootDSEattr[] = {
	       "vendorName",    _("Vendor Name"),	/* RFC 3045 */
	       "vendorVersion", _("Vendor Version"),	/* RFC 3045 */
	       "altServer", _("Alternative Server(s)"), /* RFC 2251 */
	       "supportedLDAPVersion", _("Supported LDAP Version"), /* RFC 2251 */
	       "supportedSASLMechanisms", _("Supported SASL Mechanisms"), /* RFC 2251 */
	       NULL
	  };
	  LDAPMessage *res, *ee; 
	  BerElement *berptr;
	  char *attr;
	  char **vals;
	  int i, msg;

	  rc = ldap_get_option(ld, LDAP_OPT_PROTOCOL_VERSION, &intdata);

	  /* LDAP protocol version */
	  label = gtk_label_new(_("LDAP protocol version"));
	  gtk_widget_show(label);
	  gtk_table_attach(GTK_TABLE(table),
			   label,
			   0, 1, row, row+1,
			   GTK_SHRINK,
			   GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			   0, 0);
	       
	  g_snprintf(buf, sizeof(buf), "%d", intdata);
	  e = gtk_entry_new();
	  gtk_entry_set_text(GTK_ENTRY(e), buf);
	  gtk_widget_set_sensitive(e, FALSE);
	  gtk_widget_show(e);
	  gtk_table_attach(GTK_TABLE(table),
			   e,
			   1, 2, row, row+1,
			   GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			   GTK_EXPAND | GTK_SHRINK | GTK_FILL,
			   0, 0);
	       
	  row++;

	  /* read some Information from the root DSE */
	  for (i = 0 ; rootDSEattr[i] && ld != NULL ; i += 2) {
	       char *attrs[2] = { rootDSEattr[i], NULL };

	       msg = ldap_search_ext_s(ld, "", 
				       LDAP_SCOPE_BASE,	/* scope */
				       "(objectClass=*)", /* filter */
				       attrs,		/* attrs */
				       0,		/* attrsonly */
				       NULL,		/* serverctrls */
				       NULL,		/* clientctrls */
				       NULL,		/* timeout */
				       LDAP_NO_LIMIT,	/* sizelimit */
				       &res);

	       if(msg == LDAP_NOT_SUPPORTED) {
		    msg = ldap_search_s(ld, "", LDAP_SCOPE_BASE,
					"(objectClass=*)",
					attrs, 0, &res);
	       }

	       if(msg != LDAP_SUCCESS) {
		    if (msg == LDAP_SERVER_DOWN) {
			 close_connection(entry->server, FALSE);
			 ld = open_connection(error_context, entry->server);
		    }
		    statusbar_msg("%s", ldap_err2string(msg));
	       } else {
		    if(res == NULL) continue;
		    ee = ldap_first_entry(ld, res);

		    if (ee == NULL) {
			 ldap_msgfree(res);
			 continue;
		    }
		    attr = ldap_first_attribute(ld, res, &berptr);

		    if (attr == NULL) {
			 ldap_msgfree(res);
#ifndef HAVE_OPENLDAP_12
			 if(berptr) ber_free(berptr, 0);
#endif
			 continue;
		    }
		    vals = ldap_get_values(ld, res, attr);
		    if (vals) {
			 int j;
			 for (j = 0 ; vals[j] ; j++) ;

			 label = gtk_label_new(rootDSEattr[i + 1]);
			 gtk_widget_show(label);
			 gtk_table_attach(GTK_TABLE(table),
					  label,
					  0, 1, row, row+j,
					  GTK_SHRINK,
					  GTK_EXPAND | GTK_SHRINK | GTK_FILL,
					  0, 0);
			 
			 for (j = 0 ; vals[j] ; j++) {
			      g_snprintf(buf, sizeof(buf), "%s", vals[j]);
			      e = gtk_entry_new();
			      gtk_entry_set_text(GTK_ENTRY(e), buf);
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

			 
			 ldap_value_free(vals);
		    }
		    
		    ldap_memfree(attr);
#ifndef HAVE_OPENLDAP_12
		    if(berptr) ber_free(berptr, 0);
#endif
		    ldap_msgfree(res);
	       }
	  }
	  close_connection(entry->server, FALSE);
     }

     /*  	  gtk_box_pack_start(GTK_BOX(pane2_vbox), label, FALSE, FALSE, 0); */
}






static void server_browse_entry_refresh(GqBrowserNode *e,
					int error_context,
					GQTreeWidget *ctree,
					GQTreeWidgetNode *node,
					GqTab *tab)
{
     GqBrowserNodeServer *entry;

     g_assert(GQ_IS_BROWSER_NODE_SERVER(e));
     entry = GQ_BROWSER_NODE_SERVER(e);

     entry->once_expanded = 0;

     gtk_clist_freeze(GTK_CLIST(ctree));

     server_browse_entry_selected(e, error_context, ctree, node, tab);

     gq_tree_fire_expand_callback (ctree, node);

/*       server_browse_entry_expand(entry, ctree, node, tab); */

     gtk_clist_thaw(GTK_CLIST(ctree));

}


static char* server_browse_entry_get_name(GqBrowserNode *entry,
					  gboolean long_form)
{
     g_assert(GQ_IS_BROWSER_NODE_SERVER(entry));

     return g_strdup(GQ_BROWSER_NODE_SERVER(entry)->server->name);
}

struct edit_server_cb_data {
     GqServer *server;
     GqTab *tab;
};

static void free_edit_server_cb_data(struct edit_server_cb_data *cbd)
{
	g_object_unref(cbd->server);
	cbd->server = NULL;
	g_free(cbd);
}

static void edit_server_activated(struct edit_server_cb_data *cbd)
{
     create_edit_server_window(cbd->server, cbd->tab->win->mainwin);
}

static void dump_server(GtkWidget *widget, GqTab *tab)
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

     g_assert(GQ_IS_BROWSER_NODE_SERVER(e));

     server = server_from_node(ctree, node);

     if (e == NULL || server == NULL)
	  return;

     error_context = error_new_context(_("Exporting server to LDIF"),
				       tab->win->mainwin);

     bases = get_suffixes(error_context, GQ_BROWSER_NODE_SERVER(e)->server);

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

static void server_browse_entry_popup(GqBrowserNode *e,
				      GtkWidget *menu,
				      GQTreeWidget *ctreeroot,
				      GQTreeWidgetNode *ctree_node,
				      GqTab *tab)
{
     GtkWidget *menu_item;
     GqServer *server;
     struct edit_server_cb_data *cbd;
     GqBrowserNodeServer *entry;

     g_assert(GQ_IS_BROWSER_NODE_SERVER(e));
     entry = GQ_BROWSER_NODE_SERVER(e);

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

     /* Export to LDIF */
     menu_item = gtk_menu_item_new_with_label(_("Export to LDIF"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(dump_server),
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
gq_browser_node_server_new(GqServer *server)
{
	return g_object_new(GQ_TYPE_BROWSER_NODE_SERVER, "server", server, NULL);
}

/* GType */
G_DEFINE_TYPE(GqBrowserNodeServer, gq_browser_node_server, GQ_TYPE_BROWSER_NODE);

enum {
	PROP_0,
	PROP_SERVER
};

static void
gq_browser_node_server_init(GqBrowserNodeServer* self) {}

static void
server_node_get_property(GObject* object, guint pid, GValue* value, GParamSpec* pspec)
{
	switch(pid) {
	case PROP_SERVER:
		g_value_set_object(value, GQ_BROWSER_NODE_SERVER(object));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, pid, pspec);
	}
}

static void
server_node_set_property(GObject* object, guint pid, GValue const* value, GParamSpec* pspec)
{
	GqBrowserNodeServer* self = GQ_BROWSER_NODE_SERVER(object);
	switch(pid) {
	case PROP_SERVER:
		if(self->server != g_value_get_object(value)) {
			if(self->server) {
				g_object_unref(self->server);
				self->server = NULL;
			}

			if(g_value_get_object(value)) {
				self->server = g_object_ref(g_value_get_object(value));
			}

			g_object_notify(object, "server");
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID(object, pid, pspec);
	}
}

static GqServer*
server_node_get_server(GqBrowserNode* node)
{
	return GQ_BROWSER_NODE_SERVER(node)->server;
}

static void
gq_browser_node_server_class_init(GqBrowserNodeServerClass* self_class)
{
	GObjectClass*     object_class = G_OBJECT_CLASS(self_class);
	GqBrowserNodeClass* node_class = GQ_BROWSER_NODE_CLASS(self_class);

	object_class->get_property = server_node_get_property;
	object_class->set_property = server_node_set_property;
	g_object_class_install_property(object_class,
					PROP_SERVER,
					g_param_spec_object("server",
							    _("The Server"),
							    _("The Server connected to this Node"),
							    GQ_TYPE_SERVER,
							    G_PARAM_READWRITE));

	node_class->destroy    = server_browse_entry_destroy;
	node_class->expand     = server_browse_entry_expand;
	node_class->select     = server_browse_entry_selected;
	node_class->refresh    = server_browse_entry_refresh;
	node_class->get_name   = server_browse_entry_get_name;
	node_class->popup      = server_browse_entry_popup;
	node_class->get_server = server_node_get_server;
};

