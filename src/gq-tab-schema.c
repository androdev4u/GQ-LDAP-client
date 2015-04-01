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

#include "gq-tab-schema.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_LDAP_STR2OBJECTCLASS

#include <lber.h>
#include <ldap.h>
#include <ldap_schema.h>

#include <string.h>


#define GTK_ENABLE_BROKEN  /* for the tree widget - should be replaced */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gtk/gtktree.h>
#include <gtk/gtktreeitem.h>

#include "gq-server-list.h"
#include "mainwin.h"
#include "configfile.h"
#include "common.h"
#include "util.h"
#include "schema.h"
#include "debug.h"
#include "errorchain.h"


static void attach_schema_item(GtkWidget *tree, const char *type,
			       void *schemaobject, char *itemname);
static gboolean schema_button_tree(GtkWidget *tree_item, GdkEventButton *event,
				   GtkWidget *root);


static void add_single_schema_server(GqTab *tab,
				     GqServer *server);
static void add_schema_servers(GqTab *tab);
static void attach_server_schema(GtkWidget *item, GqTab *tab);
static void schema_refresh_server(GtkWidget *widget, GtkWidget *item);

static void make_detail_notebook(GqTab *tab);
static void popup_detail_callback(GtkWidget *dummy, GtkWidget *widget);
static void make_oc_detail(GtkWidget *target_oc_vbox);
static void fill_oc_detail_rightpane(GtkWidget *treeroot, GtkWidget *tree_item,
				     GqTab *tab);
static void fill_oc_detail(GtkWidget *target_oc_vbox,
			   GqServer *server,
			   LDAPObjectClass *oc);

static void make_at_detail(GtkWidget *target_at_vbox);
static void fill_at_detail_rightpane(GtkWidget *treeroot,
				     GtkWidget *tree_item, GqTab *tab);
static void fill_at_detail(int error_context, GtkWidget *target_vbox,
			   GqServer *server,
			   LDAPAttributeType *at);

static void make_mr_detail(GtkWidget *target_mr_vbox);
static void fill_mr_detail_rightpane(GtkWidget *treeroot, GtkWidget *tree_item,
				     GqTab *tab);
static void fill_mr_detail(GtkWidget *target_vbox, GqServer *server,
			   LDAPMatchingRule *mr);

static void make_s_detail(GtkWidget *target_vbox);
static void fill_s_detail_rightpane(GtkWidget *treeroot, GtkWidget *tree_item,
				    GqTab *tab);
static void fill_s_detail(GtkWidget *target_vbox, GqServer *server,
			  LDAPSyntax *s);

GqTab *new_schemamode()
{
     GtkWidget *schemamode_vbox, *rightpane_vbox, *spacer;
     GtkWidget *mainpane, *treeroot;
     GtkWidget *leftpane_scrwin, *rightpane_scrwin;
     GqTabSchema *modeinfo;

     GqTab *tab = g_object_new(GQ_TYPE_TAB_SCHEMA, NULL);
     tab->type = SCHEMA_MODE;

     modeinfo = GQ_TAB_SCHEMA(tab);

     schemamode_vbox = gtk_vbox_new(FALSE, 0);

     spacer = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(spacer);
     gtk_box_pack_start(GTK_BOX(schemamode_vbox), spacer, FALSE, FALSE, 3);

     mainpane = gtk_hpaned_new();
     gtk_container_border_width(GTK_CONTAINER(mainpane), 2);
     gtk_widget_show(mainpane);
     gtk_box_pack_start(GTK_BOX(schemamode_vbox), mainpane, TRUE, TRUE, 0);

     treeroot = gtk_tree_new();
     modeinfo->treeroot = treeroot;
     gtk_widget_show(treeroot);
     add_schema_servers(tab);

     leftpane_scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(leftpane_scrwin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
     gtk_widget_show(leftpane_scrwin);
     gtk_paned_set_position(GTK_PANED(mainpane), 300);

     gtk_paned_add1(GTK_PANED(mainpane), leftpane_scrwin);
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(leftpane_scrwin),
                                           treeroot);

     rightpane_scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(rightpane_scrwin),
                                    GTK_POLICY_AUTOMATIC,
                                    GTK_POLICY_AUTOMATIC);
     gtk_widget_show(rightpane_scrwin);

     rightpane_vbox = gtk_vbox_new(FALSE, 5);
     gtk_widget_show(rightpane_vbox);
     modeinfo->rightpane_vbox = rightpane_vbox;
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(rightpane_scrwin),
                                           rightpane_vbox);
     gtk_paned_add2(GTK_PANED(mainpane), rightpane_scrwin);

     gtk_object_set_data(GTK_OBJECT(schemamode_vbox), "focus", rightpane_scrwin);
     gtk_widget_show(schemamode_vbox);


     g_signal_connect_swapped(schemamode_vbox, "destroy",
			      G_CALLBACK(g_object_unref), tab);

     tab->content = schemamode_vbox;
     gtk_object_set_data(GTK_OBJECT(tab->content), "tab", tab);
     return tab;
}

static void
add_schema_server_and_count(GQServerList* list, GqServer* server, gpointer user_data) {
	gpointer**tab_and_count = user_data;
	GqTab* tab = GQ_TAB(tab_and_count[0]);
	gint* count = (gint*)tab_and_count[1];

	add_single_schema_server(tab, server);
	*count++;
}

static void add_schema_servers(GqTab *tab)
{
     int server_cnt = 0;
     gpointer tab_and_count[2] = {
	     tab,
	     &server_cnt
     };

     gq_server_list_foreach(gq_server_list_get(), add_schema_server_and_count, tab_and_count);

     statusbar_msg(ngettext("%d server found",
			    "%d servers found",
			    server_cnt),
		   server_cnt);
}


static void add_single_schema_server(GqTab *tab,
				     GqServer *server)
{
     GtkWidget *new_item, *new_subtree;

     if (server) {
	  new_item = gtk_tree_item_new_with_label(server->name);
	  GTK_WIDGET_UNSET_FLAGS(new_item, GTK_CAN_FOCUS);
	  g_signal_connect(new_item, "button_press_event",
			     G_CALLBACK(schema_button_tree),
			     GQ_TAB_SCHEMA(tab)->treeroot);
	  g_signal_connect(new_item, "button_release_event",
			     G_CALLBACK(schema_button_tree),
			     GQ_TAB_SCHEMA(tab)->treeroot);
	  gtk_object_set_data(GTK_OBJECT(new_item), "tab", tab);
	  gtk_object_set_data_full(GTK_OBJECT(new_item), "server",
				   g_object_ref(server), g_object_unref);

	  gtk_tree_append(GTK_TREE(GQ_TAB_SCHEMA(tab)->treeroot), new_item);
	  new_subtree = gtk_tree_new();
	  gtk_widget_show(new_subtree);
	  gtk_tree_item_set_subtree(GTK_TREE_ITEM(new_item), new_subtree);
	  gtk_widget_show(new_item);
	  g_signal_connect(new_item, "expand",
			     G_CALLBACK(attach_server_schema), tab);

     }
}


static void attach_server_schema(GtkWidget *item, GqTab *tab)
{
     LDAPObjectClass *oc;
     LDAPAttributeType *at;
     LDAPMatchingRule *mr;
     LDAPSyntax *s;
     GList *tmp;
     GtkWidget *new_item, *server_subtree, *new_subtree;
     GqServer *server;
     struct server_schema *ss;
     int i;
     int attach_context = error_new_context(_("Expanding server schema entry"),
					    item);

     g_signal_handlers_disconnect_by_func(item,
				   G_CALLBACK(attach_server_schema), tab);

     if( (server = gtk_object_get_data(GTK_OBJECT(item), "server")) == NULL)
	  return;

     set_busycursor();

     ss = get_server_schema(attach_context, server);
     if(ss) {

	  server_subtree = GTK_TREE_ITEM_SUBTREE(GTK_TREE_ITEM(item));
	  if(ss->oc) {
	       new_item = gtk_tree_item_new_with_label("objectClasses");
	       GTK_WIDGET_UNSET_FLAGS(new_item, GTK_CAN_FOCUS);
	       g_signal_connect(new_item, "button_press_event",
				  G_CALLBACK(schema_button_tree),
				  GQ_TAB_SCHEMA(tab)->treeroot);
	       g_signal_connect(new_item, "button_release_event",
				  G_CALLBACK(schema_button_tree),
				  GQ_TAB_SCHEMA(tab)->treeroot);
	       gtk_widget_show(new_item);
	       gtk_tree_append(GTK_TREE(server_subtree), new_item);

	       new_subtree = gtk_tree_new();
	       GTK_WIDGET_UNSET_FLAGS(new_subtree, GTK_CAN_FOCUS);
	       gtk_object_set_data_full(GTK_OBJECT(new_subtree), "server",
					g_object_ref(server), g_object_unref);

	       gtk_widget_show(new_subtree);
	       gtk_tree_item_set_subtree(GTK_TREE_ITEM(new_item), new_subtree);
	       g_signal_connect(new_subtree, "select-child",
				  G_CALLBACK(fill_oc_detail_rightpane), tab);

	       tmp = ss->oc;
	       while(tmp) {
		    oc = (LDAPObjectClass *) tmp->data;
		    if(oc->oc_names && oc->oc_names[0]) {
			 for(i = 0; oc->oc_names[i]; i++) {
			      attach_schema_item(new_subtree, "oc", oc, oc->oc_names[i]);
			 }
		    } else {
			 attach_schema_item(new_subtree, "oc", oc, oc->oc_oid);
		    }
		    tmp = tmp->next;
	       }
	  }

	  if(ss->at) {
	       new_item = gtk_tree_item_new_with_label("attributeTypes");
	       GTK_WIDGET_UNSET_FLAGS(new_item, GTK_CAN_FOCUS);
	       g_signal_connect(new_item, "button_press_event",
				  G_CALLBACK(schema_button_tree),
				  GQ_TAB_SCHEMA(tab)->treeroot);
	       g_signal_connect(new_item, "button_release_event",
				  G_CALLBACK(schema_button_tree),
				  GQ_TAB_SCHEMA(tab)->treeroot);
	       gtk_tree_append(GTK_TREE(server_subtree), new_item);
	       gtk_widget_show(new_item);

	       new_subtree = gtk_tree_new();
	       GTK_WIDGET_UNSET_FLAGS(new_subtree, GTK_CAN_FOCUS);
	       gtk_object_set_data_full(GTK_OBJECT(new_subtree), "server",
					g_object_ref(server), g_object_unref);

	       gtk_widget_show(new_subtree);
	       gtk_tree_item_set_subtree(GTK_TREE_ITEM(new_item), new_subtree);
	       g_signal_connect(new_subtree, "select-child",
				  G_CALLBACK(fill_at_detail_rightpane), tab);

	       tmp = ss->at;
	       while(tmp) {
		    at = (LDAPAttributeType *) tmp->data;
		    if(at->at_names && at->at_names[0]) {
			 for(i = 0; at->at_names[i]; i++) {
			      attach_schema_item(new_subtree, "at", at, at->at_names[i]);
			 }
		    } else {
			 attach_schema_item(new_subtree, "at", at, at->at_oid);
		    }
		    tmp = tmp->next;
	       }
	  }

	  if(ss->mr) {
	       new_item = gtk_tree_item_new_with_label("matchingRules");
	       GTK_WIDGET_UNSET_FLAGS(new_item, GTK_CAN_FOCUS);
	       g_signal_connect(new_item, "button_press_event",
				  G_CALLBACK(schema_button_tree),
				  GQ_TAB_SCHEMA(tab)->treeroot);
	       g_signal_connect(new_item, "button_release_event",
				  G_CALLBACK(schema_button_tree),
				  GQ_TAB_SCHEMA(tab)->treeroot);
	       gtk_tree_append(GTK_TREE(server_subtree), new_item);
	       gtk_widget_show(new_item);

	       new_subtree = gtk_tree_new();
	       GTK_WIDGET_UNSET_FLAGS(new_subtree, GTK_CAN_FOCUS);
	       gtk_object_set_data_full(GTK_OBJECT(new_subtree), "server",
					g_object_ref(server), g_object_unref);

	       gtk_widget_show(new_subtree);
	       gtk_tree_item_set_subtree(GTK_TREE_ITEM(new_item), new_subtree);
	       g_signal_connect(new_subtree, "select-child",
				  G_CALLBACK(fill_mr_detail_rightpane), tab);

	       tmp = ss->mr;
	       while(tmp) {
		    mr = (LDAPMatchingRule *) tmp->data;
		    if(mr->mr_names && mr->mr_names[0]) {
			 for(i = 0; mr->mr_names[i]; i++) {
			      attach_schema_item(new_subtree, "mr", mr, mr->mr_names[i]);
			 }
		    } else {
			 attach_schema_item(new_subtree, "mr", mr, mr->mr_oid);
		    }
		    tmp = tmp->next;
	       }
	  }

	  if(ss->s) {
	       new_item = gtk_tree_item_new_with_label("ldapSyntaxes");
	       GTK_WIDGET_UNSET_FLAGS(new_item, GTK_CAN_FOCUS);
	       g_signal_connect(new_item, "button_press_event",
				  G_CALLBACK(schema_button_tree),
				  GQ_TAB_SCHEMA(tab)->treeroot);
	       g_signal_connect(new_item, "button_release_event",
				  G_CALLBACK(schema_button_tree),
				  GQ_TAB_SCHEMA(tab)->treeroot);
	       gtk_tree_append(GTK_TREE(server_subtree), new_item);
	       gtk_widget_show(new_item);

	       new_subtree = gtk_tree_new();
	       GTK_WIDGET_UNSET_FLAGS(new_subtree, GTK_CAN_FOCUS);
	       gtk_object_set_data_full(GTK_OBJECT(new_subtree), "server",
					g_object_ref(server), g_object_unref);

	       gtk_widget_show(new_subtree);
	       gtk_tree_item_set_subtree(GTK_TREE_ITEM(new_item), new_subtree);
	       g_signal_connect(new_subtree, "select-child",
				  G_CALLBACK(fill_s_detail_rightpane), tab);

	       tmp = ss->s;
	       while(tmp) {
		    s = (LDAPSyntax *) tmp->data;
		    attach_schema_item(new_subtree, "s", s, s->syn_oid);
		    tmp = tmp->next;
	       }
	  }

     }

     set_normalcursor();

     error_flush(attach_context);
}


/* more of a macro really, this makes inserting duplicates easier */
static void attach_schema_item(GtkWidget *tree, const char *type, 
			       void *schemaobject, char *itemname)
{
     GtkWidget *new_item;

     new_item = gtk_tree_item_new_with_label(itemname);
     GTK_WIDGET_UNSET_FLAGS(new_item, GTK_CAN_FOCUS);
     g_signal_connect(new_item, "button_press_event",
			G_CALLBACK(schema_button_tree),
			tree);
     g_signal_connect(new_item, "button_release_event",
			G_CALLBACK(schema_button_tree),
			tree);
     gtk_object_set_data(GTK_OBJECT(new_item), type, schemaobject);
     gtk_widget_show(new_item);
     /* need gtk_tree_insert_sorted() here :-( */
     gtk_tree_append(GTK_TREE(tree), new_item);

}


static gboolean schema_button_tree(GtkWidget *tree_item, GdkEventButton *event,
				   GtkWidget *root)
{
     GtkWidget *root_menu, *menu, *menu_item;

     if(event->type == GDK_BUTTON_PRESS && event->button == 3) {
	  g_signal_stop_emission_by_name(tree_item, "button_press_event");

	  root_menu = gtk_menu_item_new_with_label("Root");
	  gtk_widget_show(root_menu);
	  menu = gtk_menu_new();
	  gtk_menu_item_set_submenu(GTK_MENU_ITEM(root_menu), menu);

	  /* Refresh */
	  menu_item = gtk_menu_item_new_with_label(_("Refresh"));
	  if(gtk_object_get_data(GTK_OBJECT(tree_item), "server") == NULL)
	       gtk_widget_set_sensitive(menu_item, FALSE);
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  gtk_widget_show(menu_item);
	  g_signal_connect(menu_item, "activate",
			     G_CALLBACK(schema_refresh_server), tree_item);

	  /* Open in new window */
	  menu_item = gtk_menu_item_new_with_label(_("Open in new window"));
	  /* only leaf nodes can have a detail popup */
	  if(GTK_TREE_ITEM(tree_item)->subtree)
	       gtk_widget_set_sensitive(menu_item, FALSE);
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  gtk_widget_show(menu_item);
	  g_signal_connect(menu_item, "activate",
			     G_CALLBACK(popup_detail_callback), tree_item);


	  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			 event->button, event->time);

	  return(TRUE);
     } else if(event->type == GDK_BUTTON_RELEASE && event->button == 2) {
	  popup_detail_callback(NULL, tree_item);
	  return(TRUE);
	  /* hack, hack, hack */
     } else if(event->type == GDK_BUTTON_RELEASE && event->button == 1) {
	  gtk_tree_select_child(GTK_TREE(root), tree_item);
	  return(TRUE);
     }

     return(FALSE);
}


static void schema_refresh_server(GtkWidget *widget, GtkWidget *item)
{
     GList *sel;
     GtkTree *tree;
     GtkWidget *new_subtree;
     GqServer *server;
     GqTab *tab;

     if( (server = gtk_object_get_data(GTK_OBJECT(item), "server")) == NULL)
	  return;

     close_connection(server, TRUE);

     tree = (GtkTree *) GTK_TREE_ITEM(item)->subtree;
     if(tree) {

	  /* this is a workaround -- lots of GTK warnings if I don't do this :-( */
	  sel = GTK_TREE_SELECTION_OLD(tree);
	  while(sel) {
	       if(sel->data)
		    gtk_tree_unselect_child(GTK_TREE(tree), sel->data);
	       sel = sel->next;
	  }

	  gtk_tree_item_remove_subtree(GTK_TREE_ITEM(item));

	  new_subtree = gtk_tree_new();
	  gtk_tree_item_set_subtree(GTK_TREE_ITEM(item), new_subtree);
	  tab = gtk_object_get_data(GTK_OBJECT(item), "tab");
	  g_signal_connect(item, "expand",
			     G_CALLBACK(attach_server_schema), tab);
     }
}


static void make_detail_notebook(GqTab *tab)
{
     GtkWidget *rightpane_notebook, *rightpane_vbox;
     GtkWidget *oc_vbox, *at_vbox, *mr_vbox, *s_vbox;
     GtkWidget *dummy_vbox, *label;

     oc_vbox = gtk_vbox_new(FALSE, 0);
     at_vbox = gtk_vbox_new(FALSE, 0);
     mr_vbox = gtk_vbox_new(FALSE, 0);
     s_vbox = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(oc_vbox);
     gtk_widget_show(at_vbox);
     gtk_widget_show(mr_vbox);
     gtk_widget_show(s_vbox);
     GQ_TAB_SCHEMA(tab)->oc_vbox = oc_vbox;
     GQ_TAB_SCHEMA(tab)->at_vbox = at_vbox;
     GQ_TAB_SCHEMA(tab)->mr_vbox = mr_vbox;
     GQ_TAB_SCHEMA(tab)->s_vbox = s_vbox;

     rightpane_vbox = GQ_TAB_SCHEMA(tab)->rightpane_vbox;
     rightpane_notebook = gtk_notebook_new();
     gtk_box_pack_start(GTK_BOX(rightpane_vbox), rightpane_notebook, TRUE, TRUE, 0);
     GTK_WIDGET_UNSET_FLAGS(GTK_NOTEBOOK(rightpane_notebook), GTK_CAN_FOCUS);
     gtk_widget_show(rightpane_notebook);
     GQ_TAB_SCHEMA(tab)->rightpane_notebook = rightpane_notebook;

     /* Objectclasses tab */
     dummy_vbox = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(dummy_vbox);
     gtk_box_pack_start(GTK_BOX(dummy_vbox), oc_vbox, TRUE, TRUE, 0);
     make_oc_detail(oc_vbox);
     label = gtk_label_new(_("Objectclasses"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(rightpane_notebook), dummy_vbox, label);

     /* Attribute types tab */
     dummy_vbox = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(dummy_vbox);
     gtk_box_pack_start(GTK_BOX(dummy_vbox), at_vbox, TRUE, TRUE, 0);
     make_at_detail(at_vbox);
     label = gtk_label_new(_("Attribute types"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(rightpane_notebook), dummy_vbox, label);

     /* Matching rules tab */
     dummy_vbox = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(dummy_vbox);
     gtk_box_pack_start(GTK_BOX(dummy_vbox), mr_vbox, TRUE, TRUE, 0);
     make_mr_detail(mr_vbox);
     label = gtk_label_new(_("Matching rules"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(rightpane_notebook), dummy_vbox, label);

     /* Syntaxes tab */
     dummy_vbox = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(dummy_vbox);
     gtk_box_pack_start(GTK_BOX(dummy_vbox), s_vbox, TRUE, TRUE, 0);
     make_s_detail(s_vbox);
     label = gtk_label_new(_("Syntaxes"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(rightpane_notebook), dummy_vbox, label);

}


static void popup_detail_callback(GtkWidget *dummy, GtkWidget *widget)
{
     LDAPObjectClass *oc;
     LDAPAttributeType *at;
     LDAPMatchingRule *mr;
     LDAPSyntax *s;
     GqServer *server;

     if( (server = gtk_object_get_data(GTK_OBJECT(widget), "server")) == NULL
	  && ( (server = gtk_object_get_data(GTK_OBJECT(widget->parent), "server")) == NULL))
	  return;

     oc = gtk_object_get_data(GTK_OBJECT(widget), "oc");
     at = gtk_object_get_data(GTK_OBJECT(widget), "at");
     mr = gtk_object_get_data(GTK_OBJECT(widget), "mr");
     s = gtk_object_get_data(GTK_OBJECT(widget), "s");

     if(!oc && !at && !mr && !s)
	  /* how did we get here? */
	  return;

     if(oc)
	  popup_detail(SCHEMA_TYPE_OC, server, oc);
     else if(at)
	  popup_detail(SCHEMA_TYPE_AT, server, at);
     else if(mr)
	  popup_detail(SCHEMA_TYPE_MR, server, mr);
     else if(s)
	  popup_detail(SCHEMA_TYPE_S, server, s);

}


void popup_detail(enum schema_detail_type type,
		  GqServer *server, void *detail)
{
     GtkWidget *window, *vbox;
     int error_context;

     window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
     gtk_window_set_title(GTK_WINDOW(window), _("GQ"));
     gtk_window_set_default_size(GTK_WINDOW(window), 446, 340);
     g_signal_connect_swapped(window, "key_press_event",
			       G_CALLBACK(close_on_esc), window);
     vbox = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox);
     gtk_container_add(GTK_CONTAINER(window), vbox);

     error_context = error_new_context(_("Showing schema details"),
				       window);

     switch(type) {
     case SCHEMA_TYPE_OC:
	  gtk_window_set_title(GTK_WINDOW(window), _("GQ: objectclass"));
	  make_oc_detail(vbox);
	  fill_oc_detail(vbox, server, detail);
	  break;
     case SCHEMA_TYPE_AT:
	  gtk_window_set_title(GTK_WINDOW(window), _("GQ: attribute type"));
	  make_at_detail(vbox);
	  fill_at_detail(error_context, vbox, server, detail);
	  break;
     case SCHEMA_TYPE_MR:
	  gtk_window_set_title(GTK_WINDOW(window), _("GQ: matching rule"));
	  make_mr_detail(vbox);
	  fill_mr_detail(vbox, server, detail);
	  break;
     case SCHEMA_TYPE_S:
	  gtk_window_set_title(GTK_WINDOW(window), _("GQ: syntax"));
	  make_s_detail(vbox);
	  fill_s_detail(vbox, server, detail);
	  break;
     }

     gtk_widget_show(window);

     error_flush(error_context);
}


static void make_oc_detail(GtkWidget *target_oc_vbox)
{
     GtkWidget *label, *entry, *combo, *check, *clist;
     GtkWidget *hbox1, *hbox2, *vbox1, *table1, *scrwin, *paned;
     char *rtitle[] = { _("Required attributes"), 0 };
     char *atitle[] = { _("Allowed attributes"), 0 };

     /* homogeneous hbox to split pane down the middle */
     hbox1 = gtk_hbox_new(TRUE, 0);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(target_oc_vbox), hbox1, TRUE, TRUE, 10);

     /* left half */
     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_box_pack_start(GTK_BOX(hbox1), vbox1, TRUE, TRUE, 10);

     table1 = gtk_table_new(11, 1, FALSE);
     gtk_table_set_row_spacings(GTK_TABLE(table1), 2);
     gtk_widget_show(table1);
     gtk_box_pack_start(GTK_BOX(vbox1), table1, FALSE, FALSE, 0);

     /* Name */
     label = gtk_label_new(_("Name"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 0, 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     combo = gtk_combo_new();
     gtk_object_set_data(GTK_OBJECT(target_oc_vbox), "name", combo);
     gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo)->entry), FALSE);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(GTK_COMBO(combo)->entry), GTK_CAN_FOCUS);
     gtk_widget_show(combo);
     gtk_table_attach(GTK_TABLE(table1), combo, 0, 1, 1, 2,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 1, 10);

     /* Description */
     label = gtk_label_new(_("Description"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 2, 3,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_oc_vbox), "description", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 3, 4,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 3, 10);

     /* OID */
     label = gtk_label_new(_("OID"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 4, 5,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_oc_vbox), "oid", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 5, 6,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 5, 10);

     /* Superior */
     label = gtk_label_new(_("Superior"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 6, 7,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     combo = gtk_combo_new();
     gtk_object_set_data(GTK_OBJECT(target_oc_vbox), "superior", combo);
     gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo)->entry), FALSE);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(GTK_COMBO(combo)->entry), GTK_CAN_FOCUS);
     gtk_widget_show(combo);
     gtk_table_attach(GTK_TABLE(table1), combo, 0, 1, 7, 8,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 7, 10);

     /* Kind */
     label = gtk_label_new(_("Kind"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 8, 9,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_oc_vbox), "kind", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 9, 10,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 9, 10);

     /* Obsolete */
     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox2);
     check = gtk_check_button_new();
     gtk_object_set_data(GTK_OBJECT(target_oc_vbox), "obsolete", check);
     gtk_widget_set_sensitive(check, FALSE);
     GTK_WIDGET_UNSET_FLAGS(check, GTK_CAN_FOCUS);
     gtk_widget_show(check);
     gtk_box_pack_start(GTK_BOX(hbox2), check, FALSE, FALSE, 0);
     label = gtk_label_new(_("Obsolete"));
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
     gtk_table_attach(GTK_TABLE(table1), hbox2, 0, 1, 10, 11,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);


     /* right half */

     paned = gtk_vpaned_new();

     gtk_widget_show(paned);
     gtk_box_pack_start(GTK_BOX(hbox1), paned, TRUE, TRUE, 10);

     /* Required attributes */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC, 
				    GTK_POLICY_AUTOMATIC);

     /* just about right for four lines in the top clist */
     gtk_widget_set_usize(scrwin, -1, 93);

     clist = gtk_clist_new_with_titles(1, rtitle);  
     gtk_widget_show(clist);
     GTK_CLIST(clist)->button_actions[1] = GTK_BUTTON_SELECTS;
     g_signal_connect(clist, "select_row",
			G_CALLBACK(select_at_from_clist), NULL);
     gtk_object_set_data(GTK_OBJECT(target_oc_vbox), "required", clist);
     gtk_clist_column_titles_passive(GTK_CLIST(clist));
     GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);
     gtk_container_add(GTK_CONTAINER(scrwin), clist);
     gtk_paned_pack1(GTK_PANED(paned), scrwin, FALSE, FALSE);

     /* Allowed attributes */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
     clist = gtk_clist_new_with_titles(1, atitle);
     gtk_widget_show(clist);
     GTK_CLIST(clist)->button_actions[1] = GTK_BUTTON_SELECTS;
     g_signal_connect(clist, "select_row",
			G_CALLBACK(select_at_from_clist), NULL);
     gtk_object_set_data(GTK_OBJECT(target_oc_vbox), "allowed", clist);
     gtk_clist_column_titles_passive(GTK_CLIST(clist));
     GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);
     gtk_container_add(GTK_CONTAINER(scrwin), clist);
     gtk_paned_pack2(GTK_PANED(paned), scrwin, TRUE, FALSE);

     /* and again, for good measure */
     gtk_paned_set_position(GTK_PANED(paned), 93);

}


static void fill_oc_detail_rightpane(GtkWidget *treeroot, GtkWidget *tree_item, GqTab *tab)
{
     GtkWidget *rightpane_notebook, *oc_vbox;
     LDAPObjectClass *oc;
     GqServer *server;

     if( (server = gtk_object_get_data(GTK_OBJECT(treeroot), "server")) == NULL)
	  return;

     if( (oc = gtk_object_get_data(GTK_OBJECT(tree_item), "oc")) == NULL)
	  return;

     if(GQ_TAB_SCHEMA(tab)->cur_oc == oc)
	  return;

     GQ_TAB_SCHEMA(tab)->cur_oc = oc;

     oc_vbox = GQ_TAB_SCHEMA(tab)->oc_vbox;
     if(oc_vbox == NULL) {
	  make_detail_notebook(tab);
	  oc_vbox = GQ_TAB_SCHEMA(tab)->oc_vbox;
     }

     fill_oc_detail(oc_vbox, server, oc);

     /* switch to objectClasses tab */
     rightpane_notebook = GQ_TAB_SCHEMA(tab)->rightpane_notebook;
     gtk_notebook_set_page(GTK_NOTEBOOK(rightpane_notebook), 0);

}


static void fill_oc_detail(GtkWidget *target_oc_vbox,
			   GqServer *server,
			   LDAPObjectClass *oc)
{
     GList *list;
     GtkWidget *entry, *combo, *check, *clist;
     int i;
     const char *kind;
     char *dummy[2];

     /* left half */

     /* Name */
     combo = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_oc_vbox), "name");
     list = ar2glist(oc->oc_names);
     if(list) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(combo), list);
	  g_list_free(list);
     }

     /* Description */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_oc_vbox), "description");
     gtk_entry_set_text(GTK_ENTRY(entry), oc->oc_desc ? oc->oc_desc : "");

     /* OID */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_oc_vbox), "oid");
     gtk_entry_set_text(GTK_ENTRY(entry), oc->oc_oid ? oc->oc_oid : "");

     /* Superior OIDs */
     combo = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_oc_vbox), "superior");
     list = ar2glist(oc->oc_sup_oids);
     if(list) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(combo), list);
	  g_list_free(list);
     }

     /* Kind - peter: deliberaty no I18N (questionable) */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_oc_vbox), "kind");
     switch(oc->oc_kind) {
     case LDAP_SCHEMA_ABSTRACT:
	  kind = "Abstract";
	  break;
     case LDAP_SCHEMA_STRUCTURAL:
	  kind = "Structural";
	  break;
     case LDAP_SCHEMA_AUXILIARY:
	  kind = "Auxiliary";
	  break;
     default:
	  kind = "Unknown";
	  break;
     }
     gtk_entry_set_text(GTK_ENTRY(entry), kind);

     /* Obsolete */
     check = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_oc_vbox), "obsolete");
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), oc->oc_obsolete);

     /* right half */

     /* Required attributes */
     clist = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_oc_vbox), "required");
     gtk_object_set_data_full(GTK_OBJECT(clist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_clist_freeze(GTK_CLIST(clist));
     gtk_clist_clear(GTK_CLIST(clist));
     i = 0;
     dummy[1] = NULL;
     while(oc->oc_at_oids_must && oc->oc_at_oids_must[i]) {
	  dummy[0] = oc->oc_at_oids_must[i];
	  gtk_clist_insert(GTK_CLIST(clist), i, dummy);
	  i++;
     }
     gtk_clist_thaw(GTK_CLIST(clist));

     /* Allowed attributes */
     clist = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_oc_vbox), "allowed");
     gtk_object_set_data_full(GTK_OBJECT(clist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_clist_freeze(GTK_CLIST(clist));
     gtk_clist_clear(GTK_CLIST(clist));
     i = 0;
     while(oc->oc_at_oids_may && oc->oc_at_oids_may[i]) {
	  dummy[0] = oc->oc_at_oids_may[i];
	  gtk_clist_insert(GTK_CLIST(clist), i, dummy);
	  i++;
     }
     gtk_clist_thaw(GTK_CLIST(clist));

}


static void make_at_detail(GtkWidget *target_vbox)
{
     GtkWidget *hbox1, *hbox2, *vbox1, *vbox2, *table1, *table2, *scrwin;
     GtkWidget *label, *entry, *combo, *check, *clist;
     char *otitle[] = { _("Used in objectclasses"), 0 };

     /* homogeneous hbox to split the pane down the middle */
     hbox1 = gtk_hbox_new(TRUE, 0);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(target_vbox), hbox1, TRUE, TRUE, 10);

     /* left half */

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_box_pack_start(GTK_BOX(hbox1), vbox1, TRUE, TRUE, 10);

     table1 = gtk_table_new(14, 1, FALSE);
     gtk_table_set_row_spacings(GTK_TABLE(table1), 1);
     gtk_widget_show(table1);
     gtk_box_pack_start(GTK_BOX(vbox1), table1, FALSE, FALSE, 0);

     /* Name */
     label = gtk_label_new(_("Name"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 0, 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     combo = gtk_combo_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "name", combo);
     gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo)->entry), FALSE);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(GTK_COMBO(combo)->entry), GTK_CAN_FOCUS);
     gtk_widget_show(combo);
     gtk_table_attach(GTK_TABLE(table1), combo, 0, 1, 1, 2,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 1, 10);

     /* Description */
     label = gtk_label_new(_("Description"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 2, 3,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "description", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 3, 4,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 3, 10);

     /* OID */
     label = gtk_label_new(_("OID"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 4, 5,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "oid", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 5, 6,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 5, 10);

     /* Superior */
     label = gtk_label_new(_("Superior"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 6, 7,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "superior", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 7, 8,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 7, 10);

     /* Usage */
     label = gtk_label_new(_("Usage"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 8, 9,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "usage", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 9, 10,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 9, 10);

     /* Obsolete */
     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox2);
     check = gtk_check_button_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "obsolete", check);
     gtk_widget_set_sensitive(check, FALSE);
     GTK_WIDGET_UNSET_FLAGS(check, GTK_CAN_FOCUS);
     gtk_widget_show(check);
     gtk_box_pack_start(GTK_BOX(hbox2), check, FALSE, FALSE, 0);
     label = gtk_label_new(_("Obsolete"));
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
     gtk_table_attach(GTK_TABLE(table1), hbox2, 0, 1, 10, 11,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     /* Single value */
     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox2);
     check = gtk_check_button_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "singlevalue", check);
     gtk_widget_set_sensitive(check, FALSE);
     GTK_WIDGET_UNSET_FLAGS(check, GTK_CAN_FOCUS);
     gtk_widget_show(check);
     gtk_box_pack_start(GTK_BOX(hbox2), check, FALSE, FALSE, 0);
     label = gtk_label_new(_("Single value"));
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
     gtk_table_attach(GTK_TABLE(table1), hbox2, 0, 1, 11, 12,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     /* Collective */
     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox2);
     check = gtk_check_button_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "collective", check);
     gtk_widget_set_sensitive(check, FALSE);
     GTK_WIDGET_UNSET_FLAGS(check, GTK_CAN_FOCUS);
     gtk_widget_show(check);
     gtk_box_pack_start(GTK_BOX(hbox2), check, FALSE, FALSE, 0);
     label = gtk_label_new(_("Collective"));
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
     gtk_table_attach(GTK_TABLE(table1), hbox2, 0, 1, 12, 13,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     /* No user modification */

     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox2);
     check = gtk_check_button_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "nousermod", check);
     gtk_widget_set_sensitive(check, FALSE);
     GTK_WIDGET_UNSET_FLAGS(check, GTK_CAN_FOCUS);
     gtk_widget_show(check);
     gtk_box_pack_start(GTK_BOX(hbox2), check, FALSE, FALSE, 0);
     label = gtk_label_new(_("No user modification"));
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
     gtk_table_attach(GTK_TABLE(table1), hbox2, 0, 1, 13, 14,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);


     /* right half */

     vbox2 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox2);
     gtk_box_pack_start(GTK_BOX(hbox1), vbox2, TRUE, TRUE, 10);

     table2 = gtk_table_new(9, 1, FALSE);
     gtk_table_set_row_spacings(GTK_TABLE(table2), 1);
     gtk_widget_show(table2);
     gtk_box_pack_start(GTK_BOX(vbox2), table2, FALSE, FALSE, 0);

     /* Equality */
     label = gtk_label_new(_("Equality"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 0, 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "equality", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table2), entry, 0, 1, 1, 2,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table2), 1, 10);

     /* Ordering */
     label = gtk_label_new(_("Ordering"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 2, 3,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "ordering", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table2), entry, 0, 1, 3, 4,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table2), 3, 10);

     /* Substrings */
     label = gtk_label_new(_("Substrings"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 4, 5,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "substrings", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table2), entry, 0, 1, 5, 6,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table2), 5, 10);

     /* Syntax */
     label = gtk_label_new(_("Syntax { length }"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, 6, 7,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "syntax", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table2), entry, 0, 1, 7, 8,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table2), 7, 10);

     /* Used in objectclasses */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

     clist = gtk_clist_new_with_titles(1, otitle);
     gtk_widget_show(clist);
     GTK_CLIST(clist)->button_actions[1] = GTK_BUTTON_SELECTS;
     g_signal_connect(clist, "select_row",
			G_CALLBACK(select_oc_from_clist), NULL);
     gtk_object_set_data(GTK_OBJECT(target_vbox), "usedoc", clist);
     gtk_clist_column_titles_passive(GTK_CLIST(clist));
     GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);
     gtk_container_add(GTK_CONTAINER(scrwin), clist);
     gtk_box_pack_start(GTK_BOX(vbox2), scrwin, TRUE, TRUE, 0);

}


static void fill_at_detail_rightpane(GtkWidget *treeroot, GtkWidget *tree_item,
				     GqTab *tab)
{
     GtkWidget *rightpane_notebook, *at_vbox;
     LDAPAttributeType *at;
     GqServer *server;
     int error_context; 

     if( (server = gtk_object_get_data(GTK_OBJECT(treeroot), "server")) == NULL)
	  return;

     if( (at = gtk_object_get_data(GTK_OBJECT(tree_item), "at")) == NULL)
	  return;

     if(GQ_TAB_SCHEMA(tab)->cur_at == at)
	  return;

     GQ_TAB_SCHEMA(tab)->cur_at = at;

     at_vbox = GQ_TAB_SCHEMA(tab)->at_vbox;
     if(at_vbox == NULL) {
	  make_detail_notebook(tab);
	  at_vbox = GQ_TAB_SCHEMA(tab)->at_vbox;
     }

     error_context = error_new_context(_("Attribute details"), at_vbox);

     fill_at_detail(error_context, at_vbox, server, at);

     /* switch to Attribute types tab */
     rightpane_notebook = GQ_TAB_SCHEMA(tab)->rightpane_notebook;
     gtk_notebook_set_page(GTK_NOTEBOOK(rightpane_notebook), 1);

     error_flush(error_context);

}


static void fill_at_detail(int error_context, GtkWidget *target_vbox,
			   GqServer *server, LDAPAttributeType *at)
{
     GList *list;
     GString *syntax;
     GtkWidget *combo, *entry, *check, *clist;
     int i;
     const char *usage;
     char *dummy[2];

     /* left half */

     /* Name */
     combo = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "name");
     list = ar2glist(at->at_names);
     if(list) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(combo), list);
	  g_list_free(list);
     }

     /* Description */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "description");
     gtk_entry_set_text(GTK_ENTRY(entry), at->at_desc ? at->at_desc : "");

     /* OID */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "oid");
     gtk_entry_set_text(GTK_ENTRY(entry), at->at_oid ? at->at_oid : "");

     /* Superior */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "superior");
     gtk_entry_set_text(GTK_ENTRY(entry), at->at_sup_oid ? at->at_sup_oid : "");

     /* Usage - peter: deliberatly no I18N - questionable */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "usage");
     switch(at->at_usage) {
     case LDAP_SCHEMA_USER_APPLICATIONS:
	  usage = "User applications";
	  break;
     case LDAP_SCHEMA_DIRECTORY_OPERATION:
	  usage = "Directory operation";
	  break;
     case LDAP_SCHEMA_DISTRIBUTED_OPERATION:
	  usage = "Distributed operation";
	  break;
     case LDAP_SCHEMA_DSA_OPERATION:
	  usage = "DSA operation";
	  break;
     default:
	  usage = "Unknown";
	  break;
     }
     gtk_entry_set_text(GTK_ENTRY(entry), usage);

     /* Obsolete */
     check = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "obsolete");
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), at->at_obsolete);

     /* Single value */
     check = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "singlevalue");
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), at->at_single_value);

     /* Collective */
     check = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "collective");
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), at->at_collective);

     /* No user modification */
     check = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "nousermod");
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), at->at_no_user_mod);

     /* right half */

     /* Equality */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "equality");
     gtk_entry_set_text(GTK_ENTRY(entry), at->at_equality_oid ? at->at_equality_oid : "");

     /* Ordering */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "ordering");
     gtk_entry_set_text(GTK_ENTRY(entry), at->at_ordering_oid ? at->at_ordering_oid : "");

     /* Substrings */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "substrings");
     gtk_entry_set_text(GTK_ENTRY(entry), at->at_substr_oid ? at->at_substr_oid : "");

     /* Syntax */
     syntax = g_string_new(at->at_syntax_oid ? at->at_syntax_oid : "");
     if(at->at_syntax_len)
	  g_string_sprintfa(syntax, "{%d}", at->at_syntax_len);
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "syntax");
     gtk_entry_set_text(GTK_ENTRY(entry), syntax->str);

     clist = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "usedoc");
     gtk_object_set_data_full(GTK_OBJECT(clist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_clist_freeze(GTK_CLIST(clist));
     gtk_clist_clear(GTK_CLIST(clist));

     if(at->at_names && at->at_names[0]) {
	  list = find_oc_by_at(error_context, server, at->at_names[0]);
	  if(list) {
	       i = 0;
	       dummy[1] = NULL;
	       while(list) {
		    dummy[0] = list->data;
		    gtk_clist_insert(GTK_CLIST(clist), i, dummy);
		    list = list->next;
		    i++;
	       }
	       g_list_free(list);
	  }
     }
     gtk_clist_thaw(GTK_CLIST(clist));

}


static void make_mr_detail(GtkWidget *target_vbox)
{
     GtkWidget *hbox1, *vbox1, *table1, *hbox2, *vbox2, *scrwin;
     GtkWidget *label, *combo, *entry, *check, *clist;
     char *atitle[] = { _("Used in attribute types"), 0 };

     /* homogeneous hbox to split the pane down the middle */
     hbox1 = gtk_hbox_new(TRUE, 0);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(target_vbox), hbox1, TRUE, TRUE, 10);

     /* left half */

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_box_pack_start(GTK_BOX(hbox1), vbox1, TRUE, TRUE, 10);

     table1 = gtk_table_new(9, 1, FALSE);
     gtk_table_set_row_spacings(GTK_TABLE(table1), 1);
     gtk_widget_show(table1);
     gtk_box_pack_start(GTK_BOX(vbox1), table1, FALSE, FALSE, 0);

     /* Name */
     label = gtk_label_new(_("Name"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 0, 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     combo = gtk_combo_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "name", combo);
     gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(combo)->entry), FALSE);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(GTK_COMBO(combo)->entry), GTK_CAN_FOCUS);
     gtk_widget_show(combo);
     gtk_table_attach(GTK_TABLE(table1), combo, 0, 1, 1, 2,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 1, 10);

     /* Description */
     label = gtk_label_new(_("Description"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 2, 3,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "description", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 3, 4,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 3, 10);

     /* OID */
     label = gtk_label_new(_("OID"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 4, 5,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "oid", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 5, 6,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 5, 10);

     /* Syntax */
     label = gtk_label_new(_("Syntax"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 6, 7,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "syntax", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 7, 8,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 7, 10);

     /* Obsolete */
     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox2);
     check = gtk_check_button_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "obsolete", check);
     gtk_widget_set_sensitive(check, FALSE);
     GTK_WIDGET_UNSET_FLAGS(check, GTK_CAN_FOCUS);
     gtk_widget_show(check);
     gtk_box_pack_start(GTK_BOX(hbox2), check, FALSE, FALSE, 0);
     label = gtk_label_new(_("Obsolete"));
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(hbox2), label, FALSE, FALSE, 0);
     gtk_table_attach(GTK_TABLE(table1), hbox2, 0, 1, 8, 9,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     /* right half */

     vbox2 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox2);
     gtk_box_pack_start(GTK_BOX(hbox1), vbox2, TRUE, TRUE, 10);

     /* Used in attribute types */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

     clist = gtk_clist_new_with_titles(1, atitle);
     gtk_widget_show(clist);
     GTK_CLIST(clist)->button_actions[1] = GTK_BUTTON_SELECTS;
     g_signal_connect(clist, "select_row",
			G_CALLBACK(select_at_from_clist), NULL);
     gtk_object_set_data(GTK_OBJECT(target_vbox), "usedin", clist);
     gtk_clist_column_titles_passive(GTK_CLIST(clist));
     GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);
     gtk_container_add(GTK_CONTAINER(scrwin), clist);
     gtk_box_pack_start(GTK_BOX(vbox2), scrwin, TRUE, TRUE, 0);

}


static void fill_mr_detail_rightpane(GtkWidget *treeroot, 
				     GtkWidget *tree_item, GqTab *tab)
{
     GtkWidget *rightpane_notebook, *mr_vbox;
     LDAPMatchingRule *mr;
     GqServer *server;

     if( (server = gtk_object_get_data(GTK_OBJECT(treeroot), "server")) == NULL)
	  return;

     if( (mr = gtk_object_get_data(GTK_OBJECT(tree_item), "mr")) == NULL)
	  return;

     if(GQ_TAB_SCHEMA(tab)->cur_mr == mr)
	  return;

     GQ_TAB_SCHEMA(tab)->cur_mr = mr;

     mr_vbox = GQ_TAB_SCHEMA(tab)->mr_vbox;
     if(mr_vbox == NULL) {
	  make_detail_notebook(tab);
	  mr_vbox = GQ_TAB_SCHEMA(tab)->mr_vbox;
     }

     fill_mr_detail(mr_vbox, server, mr);

     /* switch to Matching rules tab */
     rightpane_notebook = GQ_TAB_SCHEMA(tab)->rightpane_notebook;
     gtk_notebook_set_page(GTK_NOTEBOOK(rightpane_notebook), 2);

}


static void fill_mr_detail(GtkWidget *target_vbox, GqServer *server,
			   LDAPMatchingRule *mr)
{
     GList *list;
     GtkWidget *combo, *entry, *check, *clist;
     int i;
     char *dummy[2];

     /* left half */

     /* Name */
     combo = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "name");
     list = ar2glist(mr->mr_names);
     if(list) {
	  gtk_combo_set_popdown_strings(GTK_COMBO(combo), list);
	  g_list_free(list);
     }

     /* Description */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "description");
     gtk_entry_set_text(GTK_ENTRY(entry), mr->mr_desc ? mr->mr_desc : "");

     /* OID */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "oid");
     gtk_entry_set_text(GTK_ENTRY(entry), mr->mr_oid ? mr->mr_oid : "");

     /* Syntax */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "syntax");
     gtk_entry_set_text(GTK_ENTRY(entry), mr->mr_syntax_oid ? mr->mr_syntax_oid : "");

     /* Obsolete */
     check = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "obsolete");
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(check), mr->mr_obsolete);

     /* right half */

     clist = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "usedin");
     gtk_object_set_data_full(GTK_OBJECT(clist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_clist_freeze(GTK_CLIST(clist));
     gtk_clist_clear(GTK_CLIST(clist));

     list = find_at_by_mr_oid(server, mr->mr_oid);
     if(list) {
	  i = 0;
	  dummy[1] = NULL;
	  while(list) {
	       dummy[0] = list->data;
	       gtk_clist_insert(GTK_CLIST(clist), i, dummy);
	       list = list->next;
	       i++;
	  }
	  g_list_free(list);
     }

     gtk_clist_thaw(GTK_CLIST(clist));

}


static void make_s_detail(GtkWidget *target_vbox)
{
     GtkWidget *hbox1, *vbox1, *table1, *scrwin, *paned;
     GtkWidget *label, *entry, *clist;
     char *atitle[] = { _("Used in attribute types"), 0 };
     char *mtitle[] = { _("Used in matching rules"), 0 };

     /* homogeneous hbox to split the pane down the middle */
     hbox1 = gtk_hbox_new(TRUE, 0);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(target_vbox), hbox1, TRUE, TRUE, 10);

     /* left half */

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_box_pack_start(GTK_BOX(hbox1), vbox1, TRUE, TRUE, 10);

     table1 = gtk_table_new(5, 1, FALSE);
     gtk_table_set_row_spacings(GTK_TABLE(table1), 1);
     gtk_widget_show(table1);
     gtk_box_pack_start(GTK_BOX(vbox1), table1, FALSE, FALSE, 0);

     /* OID */
     label = gtk_label_new(_("OID"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 0, 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "oid", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 1, 2,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 1, 10);

     /* Description */
     label = gtk_label_new(_("Description"));
     gtk_misc_set_alignment(GTK_MISC(label), .0, .0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 2, 3,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(target_vbox), "description", entry);
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(entry), GTK_CAN_FOCUS);
     gtk_widget_show(entry);
     gtk_table_attach(GTK_TABLE(table1), entry, 0, 1, 3, 4,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_table_set_row_spacing(GTK_TABLE(table1), 3, 10);


     /* right half */

     paned = gtk_vpaned_new();

     gtk_paned_set_position(GTK_PANED(paned), 157);
     gtk_widget_show(paned);
     gtk_box_pack_start(GTK_BOX(hbox1), paned, TRUE, TRUE, 10);

     /* Used in attribute types */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

     /* just about right for four lines in the top clist */
     gtk_widget_set_usize(scrwin, -1, 157);


     clist = gtk_clist_new_with_titles(1, atitle);
     gtk_widget_show(clist);
     GTK_CLIST(clist)->button_actions[1] = GTK_BUTTON_SELECTS;
     g_signal_connect(clist, "select_row",
			G_CALLBACK(select_at_from_clist), NULL);
     gtk_object_set_data(GTK_OBJECT(target_vbox), "usedat", clist);
     gtk_clist_column_titles_passive(GTK_CLIST(clist));
     GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);
     gtk_container_add(GTK_CONTAINER(scrwin), clist);
     gtk_paned_pack1(GTK_PANED(paned), scrwin, FALSE, FALSE);

     /* Used in matching rules */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

     clist = gtk_clist_new_with_titles(1, mtitle);
     gtk_widget_show(clist);
     GTK_CLIST(clist)->button_actions[1] = GTK_BUTTON_SELECTS;
     g_signal_connect(clist, "select_row",
			G_CALLBACK(select_mr_from_clist), NULL);
     gtk_object_set_data(GTK_OBJECT(target_vbox), "usedmr", clist);
     gtk_clist_column_titles_passive(GTK_CLIST(clist));
     GTK_WIDGET_UNSET_FLAGS(clist, GTK_CAN_FOCUS);
     gtk_container_add(GTK_CONTAINER(scrwin), clist);
     gtk_paned_pack2(GTK_PANED(paned), scrwin, FALSE, FALSE);

}


static void fill_s_detail_rightpane(GtkWidget *treeroot, GtkWidget *tree_item,
				    GqTab *tab)
{
     GtkWidget *rightpane_notebook, *s_vbox;
     LDAPSyntax *s;
     GqServer *server;

     if( (server = gtk_object_get_data(GTK_OBJECT(treeroot), "server")) == NULL)
	  return;

     if( (s = gtk_object_get_data(GTK_OBJECT(tree_item), "s")) == NULL)
	  return;

     if(GQ_TAB_SCHEMA(tab)->cur_s == s)
	  return;

     GQ_TAB_SCHEMA(tab)->cur_s = s;

     s_vbox = GQ_TAB_SCHEMA(tab)->s_vbox;
     if(s_vbox == NULL) {
	  make_detail_notebook(tab);
	  s_vbox = GQ_TAB_SCHEMA(tab)->s_vbox;
     }

     fill_s_detail(s_vbox, server, s);

     /* switch to Syntaxes tab */
     rightpane_notebook = GQ_TAB_SCHEMA(tab)->rightpane_notebook;
     gtk_notebook_set_page(GTK_NOTEBOOK(rightpane_notebook), 3);

}


static void fill_s_detail(GtkWidget *target_vbox, GqServer *server,
			  LDAPSyntax *s)
{
     GList *list;
     GtkWidget *entry, *clist;
     int i;
     char *dummy[2];

     /* OID */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "oid");
     gtk_entry_set_text(GTK_ENTRY(entry), s->syn_oid ? s->syn_oid : "");

     /* Description */
     entry = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "description");
     gtk_entry_set_text(GTK_ENTRY(entry), s->syn_desc ? s->syn_desc : "");

     /* Used in attribute types */
     clist = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "usedat");
     gtk_object_set_data_full(GTK_OBJECT(clist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_clist_freeze(GTK_CLIST(clist));
     gtk_clist_clear(GTK_CLIST(clist));

     list = find_at_by_s_oid(server, s->syn_oid);
     i = 0;
     dummy[1] = NULL;
     if(list) {
	  while(list) {
	       dummy[0] = list->data;
	       gtk_clist_insert(GTK_CLIST(clist), i, dummy);
	       i++;
	       list = list->next;
	  }
	  g_list_free(list);
     }
     gtk_clist_thaw(GTK_CLIST(clist));

     /* Used in matching rules */
     clist = (GtkWidget *) gtk_object_get_data(GTK_OBJECT(target_vbox), "usedmr");
     gtk_object_set_data_full(GTK_OBJECT(clist), "server",
			      g_object_ref(server), g_object_unref);

     gtk_clist_freeze(GTK_CLIST(clist));
     gtk_clist_clear(GTK_CLIST(clist));

     list = find_mr_by_s_oid(server, s->syn_oid);
     i = 0;
     dummy[1] = NULL;
     if(list) {
	  while(list) {
	       dummy[0] = list->data;
	       gtk_clist_insert(GTK_CLIST(clist), i, dummy);
	       i++;
	       list = list->next;
	  }
	  g_list_free(list);
     }
     gtk_clist_thaw(GTK_CLIST(clist));

}


void select_oc_from_clist(GtkWidget *clist, gint row, gint column,
			  GdkEventButton *event, gpointer data)
{
     GList *list;
     LDAPObjectClass *oc;
     GqServer *server;
     char *ocname;

     oc = NULL;

     /* double click or single middle button click */
     if( (event->type == GDK_BUTTON_RELEASE && event->button == 2) ||
	 (event->type == GDK_2BUTTON_PRESS && event->button == 1)) {

	  if( (server = gtk_object_get_data(GTK_OBJECT(clist), "server")) == NULL)
	       return;

	  if(server->ss == NULL || server->ss->oc == NULL)
	       return;

	  gtk_clist_get_text(GTK_CLIST(clist), row, column, &ocname);
	  list = server->ss->oc;
	  while(list) {
	       oc = list->data;
	       if(oc && oc->oc_names && oc->oc_names[0] &&
		  !strcasecmp(oc->oc_names[0], ocname))
		    break;
	       list = list->next;
	  }

	  if(list && oc)
	       popup_detail(SCHEMA_TYPE_OC, server, oc);

     }

}


void select_at_from_clist(GtkWidget *clist, gint row, gint column,
			  GdkEventButton *event, gpointer data)
{
     GList *list;
     LDAPAttributeType *at;
     GqServer *server;
     char *attrname;

     at = NULL;

     /* double click or single middle button click */
     if( (event->type == GDK_BUTTON_RELEASE && event->button == 2) ||
	 (event->type == GDK_2BUTTON_PRESS && event->button == 1)) {
	  gint i;

	  if( (server = gtk_object_get_data(GTK_OBJECT(clist), "server")) == NULL) {
	       return;
	  }

	  if(server->ss == NULL || server->ss->at == NULL) {
	       return;
	  }

	  gtk_clist_get_text(GTK_CLIST(clist), row, column, &attrname);
	  for(list = server->ss->at; list; list = list->next) {
	       at = list->data;
	       //g_return_val_if_fail(at && at->at_names); // FIXME: we should require this
	       for(i=0; at && at->at_names && at->at_names[i]; i++) {
	            if(!strcasecmp(at->at_names[i], attrname))
		         break; // the for loop
	       }
	       if(at && at->at_names && at->at_names[i]) { // at_names[i] == NULL after the for loop
		       break; // the while loop
	       }
	  }

	  if(list && at)
	       popup_detail(SCHEMA_TYPE_AT, server, at);

     }

}


void select_mr_from_clist(GtkWidget *clist, gint row, gint column,
			  GdkEventButton *event, gpointer data)
{
     GList *list;
     LDAPMatchingRule *mr;
     GqServer *server;
     char *mrname;

     mr = NULL;

     /* double click or single middle button click */
     if( (event->type == GDK_BUTTON_RELEASE && event->button == 2) ||
	 (event->type == GDK_2BUTTON_PRESS && event->button == 1)) {

	  if( (server = gtk_object_get_data(GTK_OBJECT(clist), "server")) == NULL)
	       return;

	  if(server->ss == NULL || server->ss->mr == NULL)
	       return;

	  gtk_clist_get_text(GTK_CLIST(clist), row, column, &mrname);
	  list = server->ss->mr;
	  while(list) {
	       mr = list->data;
	       if(mr && mr->mr_names && mr->mr_names[0] &&
		  !strcasecmp(mr->mr_names[0], mrname))
		    break;
	       list = list->next;
	  }

	  if(list && mr)
	       popup_detail(SCHEMA_TYPE_MR, server, mr);

     }

}

#endif /* HAVE_LDAP_STR2OBJECTCLASS */

void update_schema_serverlist(GqTab *tab)
{
}

/* GType */
G_DEFINE_TYPE(GqTabSchema, gq_tab_schema, GQ_TYPE_TAB);

static void
gq_tab_schema_init(GqTabSchema* self) {}

static void
gq_tab_schema_class_init(GqTabSchemaClass* self_class) {}

