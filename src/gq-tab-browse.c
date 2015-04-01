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

#include "gq-tab-browse.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <errno.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include "common.h"
#include "configfile.h"

#include "gq-browser-node-dn.h"
#include "gq-browser-node-server.h"
#include "gq-browser-node-reference.h"
#include "gq-server-list.h"

#include "mainwin.h"
#include "template.h"
#include "util.h"
#include "tinput.h"
#include "errorchain.h"
#include "ldif.h"
#include "formfill.h"
#include "encode.h"
#include "input.h"
#include "debug.h"
#include "ldapops.h"
#include "search.h"
#include "state.h"
#include "progress.h"
#include "prefs.h"

#ifdef BROWSER_DND
#include "browse-dnd.h"
#endif

static gboolean button_press_on_tree_item(GtkWidget *tree,
					  GdkEventButton *event,
					  GqTab *tab);


static void tree_row_refresh(GtkMenuItem *menuitem, GqTab *tab);

void record_path(GqTab *tab, GqBrowserNode *entry,
		 GQTreeWidget *ctreeroot, GQTreeWidgetNode *node)
{
     GQTreeWidgetRow *row;
     GqBrowserNode *e;
     GType type = -1;

     if (GQ_TAB_BROWSE(tab)->cur_path) {
	  g_list_foreach(GQ_TAB_BROWSE(tab)->cur_path, (GFunc) g_free, NULL);
	  g_list_free(GQ_TAB_BROWSE(tab)->cur_path);
     }

     GQ_TAB_BROWSE(tab)->cur_path = NULL;

     for ( ; node ; node = row->parent ) {
	  row = GQ_TREE_WIDGET_ROW(node);
	  e = GQ_BROWSER_NODE(gq_tree_get_node_data (ctreeroot, node));

	  /* currently it is sufficient to keep changes in entry types only */
	  if (e && G_OBJECT_TYPE(e) != type) {
	       GString *str = g_string_new("");
	       g_string_append_printf(str, "%d:%s",
				G_OBJECT_TYPE(e),
				GQ_BROWSER_NODE_GET_CLASS(e)->get_name(e, TRUE));

	       GQ_TAB_BROWSE(tab)->cur_path =
		    g_list_insert(GQ_TAB_BROWSE(tab)->cur_path, str->str, 0);

	       g_string_free(str, FALSE);

	       type = G_OBJECT_TYPE(e);
	  }
     }
}

GQTreeWidgetNode *dn_browse_single_add(const char *dn,
				   GQTreeWidget *ctree,
				   GQTreeWidgetNode *node)
{
     char **exploded_dn = NULL;
     gchar const* label;
     GqBrowserNode *new_entry;
     GQTreeWidgetNode *new_item, *added = NULL;
     int ctx;

     ctx = error_new_context(_("Exploding DN"), GTK_WIDGET(ctree));

     if (config->show_rdn_only) {
	  /* explode DN */
	  exploded_dn = gq_ldap_explode_dn(dn, FALSE);

	  if (exploded_dn == NULL) {
	       /* problem with DN */
	       /*	  printf("problem dn: %s\n", dn); */
	       error_push(ctx, _("Cannot explode DN '%s'. Maybe problems with quoting or special characters. See RFC 2253 for details of DN syntax."), dn);

	       goto fail;
	  }
	  label = exploded_dn[0];
     } else {
	  label = dn;
     }

     new_entry = gq_browser_node_dn_new(dn);

     added = gq_tree_insert_node (ctree,
				  node,
				  NULL,
				  label,
				  new_entry,
				  g_object_unref);

     /* add dummy node */
     gq_tree_insert_dummy_node (ctree,
				added);

 fail:
     if (exploded_dn) gq_exploded_free(exploded_dn);

     error_flush(ctx);

     return added;
}

/*
 * adds a single server to the root node
 */
static void add_single_server_internal(GQTreeWidget *ctree,
				       GqServer *server)
{
     GqBrowserNode *entry;
     GQTreeWidgetNode *new_item;

     if (ctree && server) {
	  entry = gq_browser_node_server_new(server);

	  new_item = gq_tree_insert_node (ctree,
					  NULL,
					  NULL,
					  server->name,
					  entry,
					  g_object_unref);
	  gq_tree_insert_dummy_node (ctree,
				     new_item);
     }
}

static void
add_single_server_count(GQServerList* list, GqServer* server, gpointer user_data) {
	gpointer**count_and_tree = user_data;
	gint    * server_cnt = (gint*)count_and_tree[0];
	GQTreeWidget* ctree = GQ_TREE_WIDGET(count_and_tree[1]);

	add_single_server_internal(ctree, server);
	(*server_cnt)++;
}

/*
 * add all configured servers to the root node
 */
static void add_all_servers(GQTreeWidget *ctree)
{
     GqServer *server;
     int server_cnt = 0;
     gpointer count_and_ctree[2] = {
	     &server_cnt,
	     ctree
     };

     gq_server_list_foreach(gq_server_list_get(), add_single_server_count, count_and_ctree);

     statusbar_msg(ngettext("One server found",
			    "%d servers found", server_cnt),
		   server_cnt);
}


/* GTK callbacks translating to the correct entry object method calls */
static void tree_row_expanded(GQTreeWidget *ctree,
			      GQTreeWidgetNode *ctree_node,
			      GqTab *tab)
{
     GqBrowserNode *entry;

     entry = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, ctree_node));

     if (!entry) return;

     if (GQ_BROWSER_NODE_GET_CLASS(entry)->expand) {
	  int ctx = error_new_context(_("Expanding subtree"),
				      GTK_WIDGET(ctree));

	  GQ_BROWSER_NODE_GET_CLASS(entry)->expand(entry, ctx, ctree, ctree_node, tab);
	  record_path(tab, entry, ctree, ctree_node);

	  error_flush(ctx);
     }
}


static void tree_row_selected(GQTreeWidget *ctree, GQTreeWidgetNode *node,
			      int column, GqTab *tab)
{
     GqBrowserNode *entry;
     struct inputform *iform;

     /* avoid recursive calls to this handler - this causes crashes!!! */
     /* This is a legitimate use of
	gtk_object_set_data/gtk_object_get_data */

     if (g_object_get_data(G_OBJECT(ctree), "in-tree_row_selected")) {
	  g_signal_stop_emission_by_name(GTK_OBJECT(ctree),
					 "tree-select-row");
	  return;
     }

     g_object_set_data(G_OBJECT(ctree), "in-tree_row_selected", GINT_TO_POINTER(1));

     entry = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, node));

     /* delete old struct inputform (if any) */
     iform = GQ_TAB_BROWSE(tab)->inputform;
     if(iform) {
	  /* but first get current hide status */
	  /* HACK: store hide status it in the browse-tab data-structure */
	  GQ_TAB_BROWSE(tab)->hidden = iform->hide_status;

	  inputform_free(iform);
	  GQ_TAB_BROWSE(tab)->inputform = NULL;
     }

     if (entry) {
	  if (GQ_BROWSER_NODE_GET_CLASS(entry)->select) {

	       /* do not update right-hand pane if update-lock is set */
	       if (! GQ_TAB_BROWSE(tab)->update_lock) {
		    int ctx = error_new_context(_("Selecting entry"),
						GTK_WIDGET(ctree));
		    GQ_BROWSER_NODE_GET_CLASS(entry)->select(entry, ctx, ctree, node, tab);
		    error_flush(ctx);
	       }
	  }
	  GQ_TAB_BROWSE(tab)->tree_row_selected = node;
     }

     g_object_set_data(G_OBJECT(ctree), "in-tree_row_selected", GINT_TO_POINTER(0));
}


static void tree_row_refresh(GtkMenuItem *menuitem, GqTab *tab)
{
     GQTreeWidget *ctree;
     GQTreeWidgetNode *node;
     GqBrowserNode *entry;

     ctree = GQ_TAB_BROWSE(tab)->ctreeroot;
     node = GQ_TAB_BROWSE(tab)->tree_row_popped_up;
     entry = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, node));

     if (!entry) return;

     if (GQ_BROWSER_NODE_GET_CLASS(entry)->refresh) {
	  int ctx = error_new_context(_("Refreshing entry"),
				      GTK_WIDGET(ctree));

	  /* delete old struct inputform (if any) */
	  struct inputform *iform = GQ_TAB_BROWSE(tab)->inputform;
	  if(iform) {
	       /* but first get current hide status */
	       /* HACK: store hide status it in the browse-tab
		  data-structure */
	       GQ_TAB_BROWSE(tab)->hidden = iform->hide_status;

	       inputform_free(iform);
	       GQ_TAB_BROWSE(tab)->inputform = NULL;
	  }

	  GQ_BROWSER_NODE_GET_CLASS(entry)->refresh(entry, ctx, ctree, node, tab);
	  error_flush(ctx);
     }
}

/* server & ref */
void tree_row_close_connection(GtkMenuItem *menuitem, GqTab *tab)
{
     GQTreeWidget *ctree;
     GQTreeWidgetNode *node;
     GqServer *server;

     ctree = GQ_TAB_BROWSE(tab)->ctreeroot;
     node = GQ_TAB_BROWSE(tab)->tree_row_popped_up;
     server = server_from_node(ctree, node);
     close_connection(server, TRUE);

     statusbar_msg(_("Closed connection to server %s"), server->name);
}



static void browse_save_snapshot(int error_context, 
				 char *state_name, GqTab *tab)
{
     char *tmp;
     state_value_set_list(state_name, "open-path", GQ_TAB_BROWSE(tab)->cur_path);

     if (GQ_TAB_BROWSE(tab)->mainpane)
	  state_value_set_int(state_name, "gutter-pos", 
			      gtk_paned_get_position(GTK_PANED(GQ_TAB_BROWSE(tab)->mainpane)));
     /* the state of the show empty attributes toggle button */


     tmp = g_malloc(strlen(state_name) + 10);
     strcpy(tmp, state_name);
     strcat(tmp, ".input");
     
     if (GQ_TAB_BROWSE(tab)->inputform) {
	  save_input_snapshot(error_context, GQ_TAB_BROWSE(tab)->inputform, tmp);
     } else {
	  rm_value(tmp);
     }

     g_free(tmp);
}

static int cmp_name(GqBrowserNode *entry, const char *name)
{
     gchar *c;
     int rc; 

     if (entry == NULL) return -1;
     c = GQ_BROWSER_NODE_GET_CLASS(entry)->get_name(entry, TRUE);
     rc = strcasecmp(name, c);
     g_free(c);
     return rc;
}

static void browse_restore_snapshot(int context,
				    char *state_name, GqTab *tab,
				    struct pbar_win *progress)
{
     GQTreeWidget *ctree = GQ_TAB_BROWSE(tab)->ctreeroot;
     int gutter = state_value_get_int(state_name, "gutter-pos", -1);
     const GList *path = state_value_get_list(state_name, "open-path");
     char *tmp;

     GqServer *server = NULL;

     GQTreeWidgetNode *node = gq_tree_get_root_node (ctree);

     if (path) {
	  const GList *I;
	  for (I = path ; I ; I = g_list_next(I)) {
	       const char *s = I->data;
	       const char *c = g_utf8_strchr(s, -1, ':');
	       char *ep;
	       GType type = strtoul(s, &ep, 10);

	       if (progress->cancelled) break;

	       if (progress) {
		    update_progress(progress, _("Opening %s"), c + 1);
	       }

	       if (c == ep) {
		    if(type == GQ_TYPE_BROWSER_NODE_DN) {
			 node = show_dn(context, ctree, node, c + 1, FALSE);
		    }
		    else if(type == GQ_TYPE_BROWSER_NODE_REFERENCE) {
			 gq_tree_expand_node (ctree, node);
			 node =
			      gq_tree_widget_find_by_row_data_custom(GQ_TREE_WIDGET(ctree),
								node,
								(gpointer)(c + 1),
								(GCompareFunc) cmp_name);
		    }
		    else if(type == GQ_TYPE_BROWSER_NODE_SERVER) {
			 server = gq_server_list_get_by_name(gq_server_list_get(), c + 1);
			 if (server != NULL) {
			      node = tree_node_from_server_dn(ctree, server,
							      "");
			 } else {
			      /* FIXME - popup error? */
			      node = NULL;
			 }
		    }
	       }
	       if (node == NULL) break;
	  }
	  if (node) gq_tree_select_node(ctree, node);
     }

     if (!progress->cancelled) {
	  if (gutter > 0) {
	       gtk_paned_set_position(GTK_PANED(GQ_TAB_BROWSE(tab)->mainpane),
				      gutter);
	  }

	  tmp = g_malloc(strlen(state_name) + 10);
	  strcpy(tmp, state_name);
	  strcat(tmp, ".input");

	  if (GQ_TAB_BROWSE(tab)->inputform) {
	       restore_input_snapshot(context, GQ_TAB_BROWSE(tab)->inputform, tmp);
	  }

	  g_free(tmp);
     }
}

void set_update_lock(GqTab *tab)
{
     g_assert(tab);
     GQ_TAB_BROWSE(tab)->update_lock++;
}

void release_update_lock(GqTab *tab)
{
     g_assert(tab);
     GQ_TAB_BROWSE(tab)->update_lock--;
}

GqTab *new_browsemode()
{
     GQTreeWidget *ctreeroot;
     GtkWidget *browsemode_vbox, *spacer;
     GtkWidget *mainpane, *pane2_vbox, *pane1_scrwin, *pane2_scrwin;
     GqTabBrowse *modeinfo;
     GqTab *tab = g_object_new(GQ_TYPE_TAB_BROWSE, NULL);
     tab->type = BROWSE_MODE;

     modeinfo = GQ_TAB_BROWSE(tab);

     browsemode_vbox = gtk_vbox_new(FALSE, 0);

     spacer = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(spacer);
     gtk_box_pack_start(GTK_BOX(browsemode_vbox), spacer, FALSE, FALSE, 3);

     mainpane = gtk_hpaned_new();
     gtk_container_border_width(GTK_CONTAINER(mainpane), 2);
     gtk_widget_show(mainpane);
     gtk_box_pack_start(GTK_BOX(browsemode_vbox), mainpane, TRUE, TRUE, 0);
     GQ_TAB_BROWSE(tab)->mainpane = mainpane;

     ctreeroot = gq_tree_widget_new ();
     modeinfo->ctreeroot = ctreeroot;

     gq_tree_widget_set_selection_mode(GQ_TREE_WIDGET(ctreeroot), GTK_SELECTION_BROWSE);
     gq_tree_widget_set_column_auto_resize(GQ_TREE_WIDGET(ctreeroot), 0, TRUE);
     if (config->sort_browse) {
	  gtk_clist_set_auto_sort(GTK_CLIST(ctreeroot), TRUE);
     }

     gq_tree_widget_set_select_callback(GQ_TREE_WIDGET(ctreeroot),
					G_CALLBACK(tree_row_selected),
					tab);
     gq_tree_widget_set_expand_callback(GQ_TREE_WIDGET(ctreeroot),
					G_CALLBACK(tree_row_expanded),
					tab);
     g_signal_connect(ctreeroot, "button_press_event",
			G_CALLBACK(button_press_on_tree_item), tab);

#ifdef BROWSER_DND
     browse_dnd_setup(GTK_WIDGET(ctreeroot), tab);
#endif /* BROWSER_DND */

     add_all_servers(GQ_TREE_WIDGET(ctreeroot));
     gtk_widget_show(GTK_WIDGET (ctreeroot));

     pane1_scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pane1_scrwin),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
     gtk_widget_show(pane1_scrwin);

     gtk_paned_set_position(GTK_PANED(mainpane), 300);
     gtk_paned_pack1(GTK_PANED(mainpane), pane1_scrwin, FALSE, FALSE);
     gtk_container_add(GTK_CONTAINER(pane1_scrwin), GTK_WIDGET(ctreeroot));

     // FIXME: remove this scrolled window, we only keep it for API compatibility
     pane2_scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(pane2_scrwin),
				    GTK_POLICY_NEVER,
				    GTK_POLICY_NEVER);
     gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(pane2_scrwin),
					 GTK_SHADOW_NONE);
     gtk_widget_show(pane2_scrwin);
     modeinfo->pane2_scrwin = pane2_scrwin;

     pane2_vbox = gtk_vbox_new(FALSE, 5);
     gtk_widget_show(pane2_vbox);
     modeinfo->pane2_vbox = pane2_vbox;

     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(pane2_scrwin), pane2_vbox);

     gtk_paned_pack2(GTK_PANED(mainpane), pane2_scrwin, TRUE, FALSE);

     gtk_widget_show(browsemode_vbox);

     /* prepare for proper cleanup */
     g_signal_connect_swapped(browsemode_vbox, "destroy",
			      G_CALLBACK(g_object_unref), tab);

     tab->content = browsemode_vbox;
     gtk_object_set_data(GTK_OBJECT(tab->content), "tab", tab);
     return tab;
}

GqServer *server_from_node(GQTreeWidget *ctreeroot,
				    GQTreeWidgetNode *node)
{
     GQTreeWidgetRow *row = NULL;
     GqBrowserNode *entry;

     for ( ; node ; node = row->parent ) {
	  row = GQ_TREE_WIDGET_ROW(node);
	  entry = GQ_BROWSER_NODE(gq_tree_get_node_data (ctreeroot,
							       node));
	  if (GQ_IS_BROWSER_NODE_SERVER(entry)) {
	       return GQ_BROWSER_NODE_SERVER(entry)->server;
	  }
	  if (GQ_IS_BROWSER_NODE_REFERENCE(entry)) {
	       return GQ_BROWSER_NODE_REFERENCE(entry)->server;
	  }
     }
     return NULL;
}

static int dn_row_compare_func(GqBrowserNodeDn *row_data, char *dn)
{
     if (row_data == NULL || !GQ_IS_BROWSER_NODE_DN(row_data)) return 1;

     return strcasecmp(dn, row_data->dn);
}

/*
 * returns this DN's GQTreeWidgetNode
 */

GQTreeWidgetNode *node_from_dn(GQTreeWidget *ctreeroot,
			   GQTreeWidgetNode *top,
			   char *dn)
{
     return gq_tree_widget_find_by_row_data_custom(ctreeroot,
					      top, dn,
					      (GCompareFunc) dn_row_compare_func);
}

struct server_dn {
     const GqServer *server;
     const char *dn;
     GqServer *currserver;
     GQTreeWidgetNode *found;
};

static void tree_node_from_server_dn_check_func(GQTreeWidget *ctree,
						GQTreeWidgetNode *node,
						struct server_dn *sd)
{
     GqBrowserNode *e;

     e = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, node));

     if (e == NULL) return;
     if (GQ_IS_BROWSER_NODE_SERVER(e)) {
	  sd->currserver = GQ_BROWSER_NODE_SERVER(e)->server;
	  if (strlen(sd->dn) == 0 && sd->currserver == sd->server) {
	       sd->found = node;
	  }
	  return;
     }
     if (GQ_IS_BROWSER_NODE_DN(e)) {
	  if (strcasecmp(sd->dn, GQ_BROWSER_NODE_DN(e)->dn) == 0 &&
	      sd->currserver && sd->currserver == sd->server) {
	       sd->found = node;
	  }
	  return;
     }
}

/* NOTE: used by server_node_from_server() as well */

GQTreeWidgetNode *tree_node_from_server_dn(GQTreeWidget *ctree,
				       GqServer *server,
				       const char *dn)
{
     GQTreeWidgetNode *thenode;

     struct server_dn *sd = g_malloc(sizeof(struct server_dn));
     sd->server = g_object_ref(server);
     sd->dn = dn;
     sd->currserver = NULL;
     sd->found = NULL;

     gq_tree_widget_pre_recursive(ctree,
			     NULL, /* root */
			     (GQTreeWidgetFunc) tree_node_from_server_dn_check_func,
			     sd);

     thenode = sd->found;
     g_object_unref(server);

     g_free(sd);
     return thenode;
}


static GQTreeWidgetNode *server_node_from_server(GQTreeWidget *ctree,
					     GqServer *server)
{

     return tree_node_from_server_dn(ctree, server, "");
}

void refresh_subtree_new_dn(int error_context, 
			    GQTreeWidget *ctree, 
			    GQTreeWidgetNode *node,
			    const char *newdn, 
			    int options)
{
     GQTreeWidgetNode *new_node;
     GqBrowserNodeDn *e =
	  GQ_BROWSER_NODE_DN(gq_tree_get_node_data (ctree, node));

     if (GQ_IS_BROWSER_NODE_DN(e)) {
	  GQTreeWidgetNode *parent, *sibling;
	  char **exploded_dn;
	  char *label;
	  gboolean is_expanded;
	  int n_expand = 2;

	  /* mark the entry to be uncached before we check for it again */
	  e->uncache = TRUE;
	  gtk_clist_freeze(GTK_CLIST(ctree));

	  is_expanded = gq_tree_is_node_expanded (ctree, node);

	  if (newdn) {
	       parent = GQ_TREE_WIDGET_ROW(node)->parent;
	       sibling = GQ_TREE_WIDGET_ROW(node)->sibling;

	       /* disconnecting entry from row doesn't work without calling
		  the destroy notify function - thus copy the entry */
	       e = GQ_BROWSER_NODE_DN(gq_browser_node_dn_new(newdn ? newdn : e->dn));

	       gq_tree_widget_unselect(ctree, node);

	       exploded_dn = gq_ldap_explode_dn(e->dn, FALSE);
	       if (exploded_dn == NULL) {
		    /* parsing problem */
	       }

	       label = exploded_dn[0];

	       /* add a new entry - alternatively we could just set
                  the new info (new rdn) for the existing node, but we
                  would nevertheless have to remove child nodes and
                  mark the node as unvisited. Therefore we just start
                  with a completely fresh tree */
	       new_node = gq_tree_insert_node (ctree,
					       parent, sibling,
					       label,
					       e,
					       g_object_unref);

	       /* add dummy node to have something to expand */
	       gq_tree_insert_dummy_node (ctree,
					  new_node);

	       /* select the newly added node */
	       gq_tree_select_node(ctree, new_node);

	       /* delete old node */
	       gq_tree_remove_node(ctree, node);
	       node = new_node;

	       gq_exploded_free(exploded_dn);
	  } else {
	       /* ! newdn */
	       e->seen = FALSE;

	       /* make the node unexpanded */
	       if (is_expanded)
		    gq_tree_toggle_expansion(ctree, node);
	  }
	  /* toggle expansion at least twice to fire the expand
	     callback (NOTE: do we need this anymore?) */
	  
	  if (options & REFRESH_FORCE_EXPAND) {
	       n_expand = 1;
	  } else if (options & REFRESH_FORCE_UNEXPAND) {
	       n_expand = 0;
	  } else if (is_expanded) {
	       n_expand = 1;
	  } else {
	       n_expand = 0;
	  }
	  
	  while (n_expand-- > 0) {
	       gq_tree_toggle_expansion(ctree, node);
	  }

	  show_server_dn(error_context, 
			 ctree, server_from_node(ctree, node), e->dn, TRUE);


	  gtk_clist_thaw(GTK_CLIST(ctree));
     }
}

void refresh_subtree(int error_context, 
		     GQTreeWidget *ctree, 
		     GQTreeWidgetNode *node)
{
     refresh_subtree_new_dn(error_context, ctree, node, NULL, 0);
}

void refresh_subtree_with_options(int error_context, 
				  GQTreeWidget *ctree,
				  GQTreeWidgetNode *node,
				  int options)
{
     refresh_subtree_new_dn(error_context, ctree, node, NULL, options);
}

GQTreeWidgetNode *show_server_dn(int context, 
			     GQTreeWidget *tree, 
			     GqServer *server, const char *dn,
			     gboolean select_node)
{
     GQTreeWidgetNode *node = tree_node_from_server_dn(tree, server, "");
     if (node) {
	  gq_tree_expand_node(tree, node);
	  return show_dn(context, tree, node, dn, select_node);
     }
     return NULL;
}

GQTreeWidgetNode *show_dn(int error_context,
		      GQTreeWidget *tree, GQTreeWidgetNode *node, const char *dn, 
		      gboolean select_node)
{
     char **dnparts;
     int i;
     GString *s;
     GQTreeWidgetNode *found = NULL;
     char *attrs[] = { LDAP_NO_ATTRS, NULL };

     if (!dn) return NULL;
     if (!tree) return NULL;
     
     s = g_string_new("");
     dnparts = gq_ldap_explode_dn(dn, 0);
     
     for(i = 0 ; dnparts[i] ; i++) {
     }

     for(i-- ; i >= 0 ; i--) {
	  if (*dnparts[i] == 0) continue;  /* skip empty DN elements (ie always the last one???) - ah, forget to think about it. */
	  g_string_insert(s, 0, dnparts[i]);

/*  	  printf("try %s at %08lx\n", s->str, node); */
	  if (node) gq_tree_expand_node(tree, node);

	  found = node_from_dn(tree, node, s->str);

	  if (found) {
	       node = found;
	  } else if (node) {
	       /* check if the object with this dn actually exists. If
                  it does, we add it to the tree by hand, as we
                  probably cannot see it due to a size limit */

	       GqServer *server = server_from_node(tree, node);
	       LDAP *ld;

	       if ((ld = open_connection(error_context, server)) != NULL) {
		    LDAPMessage *res, *e;
		    LDAPControl c;
		    LDAPControl *ctrls[2] = { NULL, NULL } ;
		    int rc;

		    c.ldctl_oid			= LDAP_CONTROL_MANAGEDSAIT;
		    c.ldctl_value.bv_val	= NULL;
		    c.ldctl_value.bv_len	= 0;
		    c.ldctl_iscritical		= 1;
		    
		    ctrls[0] = &c;

		    rc = ldap_search_ext_s(ld, s->str,
					   LDAP_SCOPE_BASE, 
					   "(objectClass=*)",
					   attrs,
					   0, 
					   ctrls,	/* serverctrls */
					   NULL,	/* clientctrls */
					   NULL,	/* timeout */
					   LDAP_NO_LIMIT,	/* sizelimit */
					   &res);

		    if(rc == LDAP_NOT_SUPPORTED) {
			 rc = ldap_search_s(ld, s->str, LDAP_SCOPE_BASE,
					    "(objectClass=*)",
					    attrs, 0, &res);
		    }

		    if (rc == LDAP_SUCCESS) {
			 e = ldap_first_entry(ld, res);
			 if (e) {
			      /* have it!! */
			      char *dn2 = ldap_get_dn(ld, e);
			      found = dn_browse_single_add(dn2, tree, node);
			      if (dn2) ldap_memfree(dn2);

			      node = found;
			 }
		    } else {
			 /* FIXME report error */
		    }

		    if (res) ldap_msgfree(res);
		    res = NULL;

		    close_connection(server, FALSE);
	       } else {
		    /* ERROR - cannot open connection to server,
		       either it was just restarted or the server is
		       down. In order to not prolong the time for
		       deeply nested DNs to finally fail just break
		       out of this DN parts loop.
		    */
		    break;
	       }
	  }

/*	  else break; */

	  g_string_insert(s, 0, ",");
     }

     gq_exploded_free(dnparts);
     g_string_free(s, TRUE);

     if (found && select_node) {
	  gq_tree_select_node(tree, found);
	  gq_tree_widget_scroll_to(tree, node,
				0,
				0.5, 0);
     }

     if (found) return found;
     return NULL;
}

/*
 * Button pressed on a tree item. Button 3 gets intercepted and puts up
 * a popup menu, all other buttons get passed along to the default handler
 *
 * the return values here are related to handling of button events only.
 */
static gboolean button_press_on_tree_item(GtkWidget *tree,
					  GdkEventButton *event,
					  GqTab *tab)
{
     GtkWidget *ctreeroot;
     GtkWidget *root_menu, *menu, *menu_item, *label;
     GQTreeWidgetNode *ctree_node;
     GqBrowserNodeDn *entry;

     if (event->type == GDK_BUTTON_PRESS && event->button == 3
	 && event->window == GTK_CLIST(tree)->clist_window) {
	  char *name;
	  ctree_node = gq_tree_get_node_at (GQ_TREE_WIDGET(tree), event->x, event->y);

	  if (ctree_node == NULL)
	       return TRUE;

	  ctreeroot = GTK_WIDGET(GQ_TAB_BROWSE(tab)->ctreeroot);

	  entry = GQ_BROWSER_NODE_DN(gq_tree_get_node_data (GQ_TREE_WIDGET(ctreeroot), GQ_TREE_WIDGET_NODE(ctree_node)));

	  if (entry == NULL) return TRUE;
	  if (GQ_BROWSER_NODE_GET_CLASS(entry)->popup) {
	       /* The get_name method already does UTF-8 decoding */
	       name = GQ_BROWSER_NODE_GET_CLASS(entry)->get_name(GQ_BROWSER_NODE(entry), FALSE);

	       GQ_TAB_BROWSE(tab)->tree_row_popped_up = ctree_node;

	       /* hack, hack. Need to pass both the ctree_node and tab... */
	       GQ_TAB_BROWSE(tab)->selected_ctree_node = ctree_node;

	       root_menu = gtk_menu_item_new_with_label("Root");
	       gtk_widget_show(root_menu);
	       menu = gtk_menu_new();

	       gtk_menu_item_set_submenu(GTK_MENU_ITEM(root_menu), menu);

	       label = gtk_menu_item_new_with_label(name);
	       gtk_widget_set_sensitive(label, FALSE);
	       gtk_widget_show(label);

	       gtk_menu_append(GTK_MENU(menu), label);
	       gtk_menu_set_title(GTK_MENU(menu), name);

	       menu_item = gtk_separator_menu_item_new();

	       gtk_menu_append(GTK_MENU(menu), menu_item);
	       gtk_widget_set_sensitive(menu_item, FALSE);
	       gtk_widget_show(menu_item);

	       if (name) g_free(name);

	       /* The common Refresh item */
	       menu_item = gtk_menu_item_new_with_label(_("Refresh"));
	       gtk_menu_append(GTK_MENU(menu), menu_item);
	       gtk_widget_show(menu_item);

	       g_signal_connect(menu_item, "activate",
				  G_CALLBACK(tree_row_refresh),
				  tab);

	       GQ_BROWSER_NODE_GET_CLASS(entry)->popup(GQ_BROWSER_NODE(entry), menu,
					  GQ_TREE_WIDGET (ctreeroot), ctree_node, tab);

	       gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			      event->button, event->time);

	       g_signal_stop_emission_by_name(ctreeroot,
					    "button_press_event");
	       return(TRUE);
	  }
     }
     return(FALSE);
}


typedef struct {
     GList *list;
} cb_server_list;

static void get_current_servers_list_check_func(GQTreeWidget *ctree,
						GQTreeWidgetNode *node,
						cb_server_list *csl)
{
     GqBrowserNode *e;
     e = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, node));

     if (e == NULL) return;
     if (GQ_IS_BROWSER_NODE_SERVER(e)) {
	  GqServer *thisserver = GQ_BROWSER_NODE_SERVER(e)->server;

	  csl->list = g_list_append(csl->list, g_object_ref(thisserver));
     }
}

static void
add_missing_server(GQServerList* list, GqServer* server, gpointer user_data) {
	GQTreeWidget* tree = GQ_TREE_WIDGET(user_data);

	if(!server_node_from_server(tree, server)) {
		add_single_server_internal(tree, server);
	}
}

void update_browse_serverlist(GqTab *tab)
{
     GQTreeWidget *ctree;
     GqServer *server;
     cb_server_list csl;
     GList *l, *I;

     ctree = GQ_TAB_BROWSE(tab)->ctreeroot;
     /* walk the list of ldapservers, add any not yet in the show list */

     gq_server_list_foreach(gq_server_list_get(), add_missing_server, ctree);

     csl.list = NULL;
     gq_tree_widget_pre_recursive_to_depth(ctree,
				      NULL, /* root node */
				      1,    /* max depth */
				      (GQTreeWidgetFunc) get_current_servers_list_check_func,
				      &csl);

     /* walk list of shown servers, remove any no longer in config */
     for (l = csl.list ; l ; l=l->next) {
	  GqServer *thisserver = l->data;
	  GQTreeWidgetNode *node = server_node_from_server(ctree, thisserver);
	  int found = gq_server_list_contains(gq_server_list_get(), thisserver);

	  /* deleted servers */
	  if (!found) {
	       if (node) gq_tree_remove_node(ctree, node);
	  } else {
	       /* renamed servers ? */
	       char *currtext = gq_tree_get_node_text (ctree,node);
	       if (currtext && strcmp(currtext, thisserver->name) != 0) {
		    gq_tree_set_node_text (ctree, node, thisserver->name);
	       }
	  }
     }

     if (csl.list) {
	  g_list_foreach(csl.list, (GFunc)g_object_unref, NULL);
	  g_list_free(csl.list);
     }
}

/* GType */
G_DEFINE_TYPE(GqTabBrowse, gq_tab_browse, GQ_TYPE_TAB);

static void
gq_tab_browse_init(GqTabBrowse* self) {}

static void
tab_dispose(GObject* object)
{
	GqTabBrowse* self = GQ_TAB_BROWSE(object);

	if(self->inputform) {
		inputform_free(self->inputform);
		self = NULL;
	}

	G_OBJECT_CLASS(gq_tab_browse_parent_class)->dispose(object);
}

static void
tab_finalize(GObject* object)
{
	GqTabBrowse* self = GQ_TAB_BROWSE(object);

	while(self->cur_path) {
		g_free(self->cur_path->data);
		self->cur_path = g_list_delete_link(self->cur_path, self->cur_path);
	}

	G_OBJECT_CLASS(gq_tab_browse_parent_class)->finalize(object);
}

static void
gq_tab_browse_class_init(GqTabBrowseClass* self_class)
{
	GObjectClass* object_class = G_OBJECT_CLASS(self_class);
	GqTabClass* tab_class = GQ_TAB_CLASS(self_class);

	object_class->dispose  = tab_dispose;
	object_class->finalize = tab_finalize;

	tab_class->save_snapshot    = browse_save_snapshot;
	tab_class->restore_snapshot = browse_restore_snapshot;
}

