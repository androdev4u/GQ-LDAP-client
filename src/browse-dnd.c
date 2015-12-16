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

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */
#ifdef BROWSER_DND

#include <stdio.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "browse-dnd.h"
#include "debug.h"
#include "errorchain.h"
#include "gq-browser-node-dn.h"
#include "gq-server-list.h"
#include "gq-tab-browse.h"
#include "ldapops.h"
#include "ldif.h"
#include "util.h"

typedef struct _dnd_refresh {
     GQTreeWidget *tree;
     GqServer *server;
     int options;
     char *dn;
} dnd_refresh;

static GList *dnd_refresh_list = NULL;


static dnd_refresh *new_dnd_refresh_dn(GQTreeWidget *tree, 
				       GqServer *server,
				       char *dn,
				       int options)
{
     dnd_refresh *dr = g_malloc(sizeof(dnd_refresh));

     dr->tree = tree;
     dr->options = options;
     dr->server = server;

     if (dn)
	  dr->dn = g_strdup(dn);
     else 
	  dr->dn = NULL;

     return dr;
}

static dnd_refresh *new_dnd_refresh_node(GQTreeWidget *tree, 
					 GQTreeWidgetNode *node,
					 int options)
{
     GqBrowserNodeDn *entry;
     entry = GQ_BROWSER_NODE_DN(gq_tree_get_node_data (tree, node));

     if (GQ_IS_BROWSER_NODE_DN(entry)) {
	  return new_dnd_refresh_dn(tree, server_from_node(tree, node),
				    entry->dn, options);
     } else {
	  return new_dnd_refresh_dn(tree, server_from_node(tree, node),
				    NULL, options);
     }
}

static void dnd_refresh_free(dnd_refresh *dr)
{
     if (dr) {
	  if (dr->dn) g_free(dr->dn);
	  g_free(dr);
     }
}

typedef struct _motion_timer_data {
     GtkAdjustment *adj;
     gfloat change;
     int timer_id;
     int oldinterval, newinterval;
} motion_timer_data;

/* We want the CTree to scroll up or down during dragging if we get
   close to its border. We use a time for this.

   Some Notes:
   * The closer we are to the border the faster we scroll.
   * The timer gets started by a drag motion callback
 */
static gboolean drag_motion_timer(motion_timer_data *td) {
     gfloat nval = td->adj->value + td->change;
     gboolean rc = TRUE;

     if (td->oldinterval != td->newinterval) {
	  rc = FALSE; /* remove THIS timer */
	  td->timer_id = gtk_timeout_add(td->newinterval, 
					 (GtkFunction) drag_motion_timer,
					 td);
	  td->oldinterval = td->newinterval;  
     }

     if (nval < td->adj->lower) nval = td->adj->lower;
     if (nval > td->adj->upper - td->adj->page_size) nval = td->adj->upper - td->adj->page_size;

     gtk_adjustment_set_value(td->adj, nval);
     return rc;
}

/* stops/removes the scrolling timer of the ctree if the CTree get
   destroyed */
static void remove_drag_motion_timer(motion_timer_data *td) {
     gtk_timeout_remove(td->timer_id);
     g_free(td);
}

/* implements changing scroll speeds depending on distance to border */
static int find_scroll_interval(int distance) {
     static int intervalmap[] = { 10, 10, 20, 20, 25, 25, 50, 50, 
				  67, 100, 200 };

     if (distance >= (int) (sizeof(intervalmap) / sizeof(intervalmap[0])) ||
	 distance < 0) {
	  return 200;
     } 
     return intervalmap[distance];
}

/* During dragging: check if we get close to the top or bottom
   borders. If we do start scrolling using a timer */
static gboolean drag_motion(GtkWidget *ctreeroot,
			    GdkDragContext *drag_context,
			    gint x,
			    gint y,
			    guint time,
			    gpointer user_data)
{
     motion_timer_data *td = gtk_object_get_data(GTK_OBJECT(ctreeroot),
						 "scroll-timer-data");

     /* 

	HACK Alert

	clist_window_height is internal to CList AND doesn't count the
	highlighting border... (thus the + 2) 

	HACK Alert

     */

     int height = GTK_CLIST(ctreeroot)->clist_window_height + 2;

     if (y < 10) {
	  if (!td) {
	       td = g_malloc(sizeof(motion_timer_data));
	       td->adj = gtk_clist_get_vadjustment(GTK_CLIST(ctreeroot));
	       td->change = -td->adj->step_increment;
	       td->oldinterval = td->newinterval = find_scroll_interval(y);
	       td->timer_id = gtk_timeout_add(td->oldinterval, 
                                              (GtkFunction) drag_motion_timer,
					      td);

	       gtk_object_set_data_full(GTK_OBJECT(ctreeroot),
					"scroll-timer-data", td,
					(GtkDestroyNotify)remove_drag_motion_timer);
	  } else {
	       td->newinterval = find_scroll_interval(y);
	       td->change = -td->adj->step_increment;
	  }
     } else if (height - y < 10) {
	  if (!td) {
	       td = g_malloc(sizeof(motion_timer_data));
	       td->adj = gtk_clist_get_vadjustment(GTK_CLIST(ctreeroot));
	       td->change = td->adj->step_increment;
	       td->oldinterval = td->newinterval = find_scroll_interval(height - y);
	       td->timer_id = gtk_timeout_add(td->oldinterval, 
                                              (GtkFunction) drag_motion_timer,
					      td);

	       gtk_object_set_data_full(GTK_OBJECT(ctreeroot),
					"scroll-timer-data", td,
					(GtkDestroyNotify)remove_drag_motion_timer);
	  } else {
	       td->newinterval = find_scroll_interval(height - y);
	       td->change = td->adj->step_increment;
	  }
     } else {
	  if (td) {
	       gtk_object_remove_data(GTK_OBJECT(ctreeroot),
				      "scroll-timer-data");
	  }
     }
     
     return TRUE;
}

/* Cleanup function for the data in the drag and selection data hash */
static gboolean drag_and_select_free_elem(gpointer key,
					  gpointer value,
					  gpointer user_data)
{
     g_free(key);
     g_free(value);
     return TRUE;
}

/* Cleanup function for the drag and selection data hash */
static void drag_and_select_data_free(GHashTable *hash)
{
     g_hash_table_foreach_remove(hash, drag_and_select_free_elem, NULL);
     g_hash_table_destroy(hash);
}

static void drag_begin(GtkWidget *ctreeroot,
		       GdkDragContext *drag_context,
		       GqTab *tab)
{
     /* store data we began dragging with for drag_data_get later on */
     GQTreeWidgetNode *ctree_node;
     GqServer *server;
     GqBrowserNode *entry;
     GHashTable *seldata;
     char cbuf[20];

     ctree_node = GQ_TAB_BROWSE(tab)->tree_row_selected;
     server = server_from_node(GTK_CTREE(ctreeroot), ctree_node);
     entry = GQ_BROWSER_NODE(gq_tree_get_node_data (GTK_CTREE(ctreeroot),
							  ctree_node));

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("drag_begin start node=%08lx entry=%08lx\n",
		 (unsigned long) ctree_node,
		 (unsigned long) entry);
     }
#endif

     if (GQ_IS_BROWSER_NODE_DN(entry)) {
	  GqBrowserNodeDn *dn_entry = GQ_BROWSER_NODE_DN(entry);

	  seldata = g_hash_table_new(g_str_hash, g_str_equal);
	  g_hash_table_insert(seldata, g_strdup("dn"), g_strdup(dn_entry->dn));
	  g_hash_table_insert(seldata, g_strdup("nickname"),
			      g_strdup(server->name));
	  g_hash_table_insert(seldata, g_strdup("server"),
			      g_strdup(server->ldaphost));
	  g_snprintf(cbuf, sizeof(cbuf), "%d", server->ldapport);
	  g_hash_table_insert(seldata, g_strdup("port"), g_strdup(cbuf));

	  /* dragging is always done recusively (for now) */
	  g_hash_table_insert(seldata, g_strdup("recursively"), g_strdup("TRUE"));
	  
	  gtk_object_set_data_full(GTK_OBJECT(ctreeroot), 
				   "drag-and-selection-data", 
				   seldata,
				   (GtkDestroyNotify) drag_and_select_data_free);

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("drag_begin %08lx %08lx\n", (unsigned long) ctreeroot,
		 (unsigned long) seldata);
     }
#endif
     }
}


static int dnd_refresh_sort_func(const dnd_refresh *a, const dnd_refresh *b)
{
     if (a == NULL || a->dn == NULL) return -1;
     if (b == NULL || b->dn == NULL) return 1;

     return strlen(a->dn) - strlen(b->dn);
}


static void do_refresh(int error_context, GqTab *tab) {
     GList *l;
     dnd_refresh *at_end = NULL;

     /* do any queued refresh operations */
     
     /* sort refresh operations to work top-down. This way at the end
        everything will be visible */
     dnd_refresh_list = g_list_sort(dnd_refresh_list,
				    (GCompareFunc) dnd_refresh_sort_func);

     set_update_lock(tab);

     for (l = g_list_first(dnd_refresh_list) ; l ; l = g_list_next(l)) {
	  dnd_refresh *dr = l->data;
	  if (dr->dn == NULL) continue;

	  refresh_subtree_with_options(error_context,
				       dr->tree, 
				       tree_node_from_server_dn(dr->tree, 
								dr->server,
								dr->dn),
				       dr->options); 

	  if (dr->options & SELECT_AT_END) {
	       at_end = dr;
	  }
     }

     release_update_lock(tab);

     if (at_end) {
	  GQTreeWidgetNode *node = tree_node_from_server_dn(at_end->tree, 
							at_end->server,
							at_end->dn);
/*  	  show_dn(at_end->tree, */


/*  		  , at_end->dn); */
	  if (node) 
	       gq_tree_select_node(at_end->tree, node);
     }
     
     for (l = g_list_first(dnd_refresh_list) ; l ; l = g_list_next(l)) {
	  dnd_refresh_free(l->data);
     }
     g_list_free(dnd_refresh_list);
     dnd_refresh_list = NULL;
}


static void drag_end(GtkWidget *ctreeroot,
		     GdkDragContext *drag_context,
		     GqTab *tab) 
{
     int ctx;
#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("drag_end %08lx\n", (unsigned long) drag_context);
     }
#endif
     /* kill any motion timer ... */
     /* NOTE: This would not work for inter-widget drag/drop as the
        motion-related widget would be different from the one for
        which drag_end gets called */

     gtk_object_remove_data(GTK_OBJECT(ctreeroot),
			    "scroll-timer-data"); 

     gtk_object_remove_data(GTK_OBJECT(ctreeroot), 
			    "drag-and-selection-data");

     ctx = error_new_context(_("Refreshing entry after dragging"), ctreeroot);
     do_refresh(ctx, tab);
     error_flush(ctx);
}


static void drag_selection_data_prepare(gpointer key,
					gpointer value,
					GByteArray *buf)
{
     guchar nul = '\0';
     g_byte_array_append(buf, key, strlen(key));
     g_byte_array_append(buf, (guchar*)"=", 1);
     g_byte_array_append(buf, value, strlen(value));
     g_byte_array_append(buf, &nul, 1);
}

static void drag_data_get(GtkWidget *ctreeroot,
			  GdkDragContext *drag_context,
			  GtkSelectionData *data,
			  guint info,
			  guint time,
			  GqTab *tab)   /* don't really need tab here */
{
     GHashTable *selhash = gtk_object_get_data(GTK_OBJECT(ctreeroot),
					       "drag-and-selection-data");
     GByteArray *buf;
     guchar nul = '\0';

     if (!selhash) {
	  statusbar_msg(_("Could not find data to drag - internal error"));
	  return;
     }
     buf = g_byte_array_new();

     g_hash_table_foreach(selhash, (GHFunc) drag_selection_data_prepare, buf);
     g_byte_array_append(buf, &nul, 1);

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("drag_data_get ctx=%08lx data=%08lx\n", (unsigned long) drag_context,
		 (unsigned long) data);
     }
#endif

     gtk_selection_data_set(data,
			    gdk_atom_intern("gq-browse-ctree", FALSE),
			    8, /* character data */
			    buf->data,
			    buf->len);

     g_byte_array_free(buf, TRUE);

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("data->data=%08lx data=%08lxx\n",
		 (unsigned long)(data->data), (unsigned long) data);
     }
#endif
}


static void move_progress(const char *from_dn,
			  const char *to_dn,
			  const char *new_dn) 
{
     statusbar_msg(_("Created %s"), new_dn);
}

static GHashTable *drag_selection_data_unpack(const char *data, int len)
{
     GHashTable *selhash = NULL;
     const char *d = data, *eq;
     char *key, *value;
     int l, n;

     if (data == NULL) return NULL;

     selhash = g_hash_table_new(g_str_hash, g_str_equal);

     for ( d = data ; (l = strlen(d)) ; d = d + l + 1 ) {
	  if ((eq = strchr(d, '=')) != NULL) {
	       n = eq - d;

	       key = g_malloc(n + 1);
	       strncpy(key, d, n);
	       key[n] = 0;
	       
	       n = l - 1 - n;
	       eq++;
	       
	       value = g_malloc(n + 1);
	       strncpy(value, eq, n);
	       value[n] = 0;

#ifdef DEBUG
	       if (debug & GQ_DEBUG_BROWSER_DND) {
		    printf("key=%s value=%s\n", key, value);
	       }
#endif
	       g_hash_table_insert(selhash, key, value);
	  } 
     }
     return selhash;
}


static void do_move_after_reception(GtkWidget *ctreeroot,
				    GHashTable *selhash,
				    GQTreeWidgetNode *target_ctree_node,
				    GqBrowserNodeDn *target_entry,
				    GqServer *target_server,
				    int flags)
{
     GQTreeWidgetNode *ctree_node;
     char *dn = NULL;
     GqServer *source_server;
     char *newdn = NULL;
     int context;

     context = error_new_context(_("Moving entry"), ctreeroot);
#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("do_move_after_reception selhash=%08lx server=%s dn=%s\n",
		 (unsigned long) selhash,
		 (const char*)g_hash_table_lookup(selhash, "nickname"), dn);
     }
#endif

     source_server = gq_server_list_get_by_name(gq_server_list_get(),
						g_hash_table_lookup(selhash, "nickname"));
     if (source_server == NULL) {
	  statusbar_msg(_("Could not find source server by its nickname ('%s')!"),
			(const char*)g_hash_table_lookup(selhash, "nickname"));
	  return;
     }

     dn = g_hash_table_lookup(selhash, "dn");
     
     /* Avoid infinite recursion when copying/dragging an entry on top
        of itself. */
     /* NOTE: THIS WILL NOT WORK IF MOVING ENTRIES BETWEEN DIFFERENT
        "servers" THAT ARE ACTUALLY POINTING TO THE SAME LDAP
        DIRECTORY!!!! - INFINITE RECURSION ALERT! - FIXME */
     /* Note: Pointer comparison, as we only store information about
	servers once.  */
     

     if ((source_server == target_server) &&
	 ( strcasecmp(dn, target_entry->dn) == 0)) {
          error_push(context, _("Cannot move/copy entry onto itself!"));
	  
	  goto done;
     }
     
     if ((flags & MOVE_RECURSIVELY) &&
	 (source_server == target_server) &&
	 is_ancestor(target_entry->dn, dn)) {
          error_push(context,
		     _("Cannot recursively move/copy entry onto or below itself!"));
	  
	  goto done;
     }
     
     if (GQ_IS_BROWSER_NODE_DN(target_entry)) {
	  newdn = move_entry(dn, source_server, 
			     target_entry->dn, target_server,
			     flags, move_progress, context);

	  /* register that we have to refresh the target node */
	  dnd_refresh_list =
	       g_list_append(dnd_refresh_list,
			     new_dnd_refresh_dn(GTK_CTREE(ctreeroot),
						target_server, 
						target_entry->dn,
						REFRESH_FORCE_EXPAND)
			     );

	  /* arrange for selecting the final node */

	  dnd_refresh_list =
	       g_list_append(dnd_refresh_list,
			     new_dnd_refresh_dn(GTK_CTREE(ctreeroot),
						target_server, 
						newdn,
						SELECT_AT_END)
			     );

/*  	  refresh_subtree_with_options(GTK_CTREE(ctreeroot), target_ctree_node, */
/*  				       REFRESH_FORCE_EXPAND); */
	  
	  if (newdn) {
	       ctree_node = node_from_dn(GTK_CTREE(ctreeroot),
					 target_ctree_node, newdn);
	       if (ctree_node)
		    gq_tree_select_node(GQ_TREE_WIDGET(ctreeroot),
					ctree_node);
	       

#ifdef DEBUG
	       if (debug & GQ_DEBUG_BROWSER_DND) {
		    printf("moved %s below %s, became %s\n",
			   dn,
			   target_entry->dn,
			   newdn);
	       }
#endif
	       g_free(newdn);
	  }
     }
 done:
     error_flush(context);
}

/* handles data receiving and does the actual move */
static void drag_data_received(GtkWidget *ctreeroot,
			       GdkDragContext *drag_context,
			       gint x,
			       gint y,
			       GtkSelectionData *data,
			       guint info,
			       guint time,
			       gpointer user_data)
{
     GQTreeWidgetNode *ctree_node;
     GqServer *target_server;
     GHashTable *selhash = NULL;
     GqBrowserNodeDn *target_entry;
     int answer;

     if (data == NULL) {  /* PSt: I've seen this happen */
	  statusbar_msg(_("No selection data available"));
	  return;
     }

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("drag_data_received ctx=%08lx seldata=%08lx\n",
		 (unsigned long) drag_context, (unsigned long) data);
     }
#endif

     ctree_node = gq_tree_get_node_at (GTK_CTREE(ctreeroot), x, y);
     if (ctree_node == NULL) return;

     /* NOTE: type has not yet been checked !!! */
     target_entry = GQ_BROWSER_NODE_DN(
	  gq_tree_get_node_data (GTK_CTREE(ctreeroot),
				      GTK_CTREE_NODE(ctree_node)));

     if (! GQ_IS_BROWSER_NODE_DN(target_entry)) return;
     target_server = server_from_node(GTK_CTREE(ctreeroot),
				      GTK_CTREE_NODE(ctree_node));

     /* For now we allow recursive moves, as we _believe_ it
	works even with a limited number of answers from
	servers... */

     answer =
	  question_popup(_("Do you really want to move this entry?"),
			 _("Do you really want to move this entry recursively?\n"
			   "Note that you should only do this if you have\n"
			   "access to ALL attributes of the object to move\n"
			   "(the same holds for objects below it) as the\n"
			   "original object(s) WILL BE REMOVED!\n\n"
			   "USE AT YOUR OWN RISK!"));

     if (answer) {
	  selhash = drag_selection_data_unpack((gchar*)data->data,
					       data->length);

	  if (selhash) {
	       do_move_after_reception(ctreeroot, selhash, ctree_node,
				       target_entry, target_server, 
				       MOVE_CROSS_SERVER | MOVE_RECURSIVELY |
				       MOVE_DELETE_MOVED);
	       
	       drag_and_select_data_free(selhash);


	  }
     }
#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("dragged to %d/%d\n", x, y);
     }
#endif
}

/*

  do sender side "deletion" after a move - IMHO gtk is buggy in having
  trouble to communicate to the sending side if the dropping actually
  worked... However, I do not know if the dnd protocols actually work
  like this...

  Luckily, we do EVERYTHING in the drag_data_received handler (only
  possible because we only do in-widget dnd)

*/

static void drag_data_delete(GtkWidget *widget,
			     GdkDragContext *drag_context,
			     GqTab *tab)
{
     GQTreeWidgetNode *ctree_node;
     char *s = NULL, *dn = NULL;
     GqServer *server;

     GHashTable *selhash = gtk_object_get_data(GTK_OBJECT(widget),
					       "drag-and-selection-data");

     s = g_hash_table_lookup(selhash, "nickname");
     dn = g_hash_table_lookup(selhash, "dn");

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("drag_data_delete ctx=%08lx suggested=%d action=%d server=%s dn=%s\n",
		 (unsigned long) drag_context,
		 drag_context->suggested_action,
		 drag_context->action,
		 s, dn
		 );
     }
#endif

     server = gq_server_list_get_by_name(gq_server_list_get(), s);
     if (!server) {
	  return;
     }
     ctree_node = tree_node_from_server_dn(GQ_TREE_WIDGET(widget),
					   server, dn);

     if (ctree_node) {
	  GQTreeWidgetNode *parent = gq_tree_get_parent_node (GQ_TREE_WIDGET (widget),
							      ctree_node);
	  if (parent) {
	       /* register that we have to refresh the parent node */

	       dnd_refresh_list =
		    g_list_append(dnd_refresh_list,
				  new_dnd_refresh_node(GTK_CTREE(widget),
						       parent, 0)
				  );
/*	       refresh_subtree_with_options(GTK_CTREE(widget), parent, 0); */
	  }
     }
#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("drag_data_delete done\n");
     }
#endif
     gtk_object_remove_data(GTK_OBJECT(widget),
			    "drag-and-selection-data");
}

void copy_entry(GtkWidget *widget, GqTab *tab)
{
     GQTreeWidget *ctree;
     int have_sel;
     GqServer *server;
     GqBrowserNodeDn *entry;
     GQTreeWidgetNode *ctree_node;
     GHashTable *seldata;
     char cbuf[20];

     ctree = GQ_TAB_BROWSE(tab)->ctreeroot;

     ctree_node = GQ_TAB_BROWSE(tab)->tree_row_popped_up;
     entry = GQ_BROWSER_NODE_DN(gq_tree_get_node_data (ctree, ctree_node));
     if (entry == NULL) return;
     if (!GQ_IS_BROWSER_NODE_DN(entry)) return;

     server = server_from_node(ctree, ctree_node);

     seldata = g_hash_table_new(g_str_hash, g_str_equal);
     g_hash_table_insert(seldata, g_strdup("dn"), g_strdup(entry->dn));
     g_hash_table_insert(seldata, g_strdup("nickname"), g_strdup(server->name));
     g_hash_table_insert(seldata, g_strdup("server"), g_strdup(server->ldaphost));
     g_snprintf(cbuf, sizeof(cbuf), "%d", server->ldapport);
     g_hash_table_insert(seldata, g_strdup("port"), g_strdup(cbuf));

     gtk_object_set_data_full(GTK_OBJECT(ctree), 
			      "drag-and-selection-data", 
			      seldata,
			      (GtkDestroyNotify) drag_and_select_data_free);

     have_sel = gtk_selection_owner_set(GTK_WIDGET(ctree),
					GDK_SELECTION_PRIMARY,
					GDK_CURRENT_TIME);

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("copy_entry %08lx %d\n", (unsigned long) ctree, have_sel);
     }
#endif

}


void copy_entry_all(GtkWidget *widget, GqTab *tab)
{
     GHashTable *selhash = NULL;
     GtkWidget *ctreeroot;

     copy_entry(widget, tab);

     ctreeroot = GTK_WIDGET(GQ_TAB_BROWSE(tab)->ctreeroot);

     selhash = gtk_object_get_data(GTK_OBJECT(ctreeroot), 
				   "drag-and-selection-data");

     g_hash_table_insert(selhash, g_strdup("recursively"), g_strdup("TRUE"));

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("copy_entry_all %08lx\n", (unsigned long) widget);
     }
#endif

}


static void get_selection_string(GtkWidget *ctree,
				 GtkSelectionData *data,
				 GHashTable *selhash)
{
     GqServer *server =
	  gq_server_list_get_by_name(gq_server_list_get(),
				     g_hash_table_lookup(selhash, "nickname"));
     char *dn = g_hash_table_lookup(selhash, "dn");
     LDAPControl c, *ctrls[2] = { NULL, NULL } ;
     int scope;
     LDAP *ld;
     int rc;

     LDAPMessage *e, *res = NULL;
     GString *out = NULL;
     gboolean ok = FALSE;

     char *attrs[] = {
	  LDAP_ALL_USER_ATTRIBUTES,
	  "ref",
	  NULL
     };

     int ctx = error_new_context(_("Getting selection string"), ctree);

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("get_selection_string\n");
     }
#endif

     if (server == NULL) {
	  error_push(ctx,
		     _("Cannot find server by its nickname ('%s')"), 
		     (char*) g_hash_table_lookup(selhash, "nickname"));
	  goto done;
     }
     /* retrieve from server ... */

     if( (ld = open_connection(ctx, server)) == NULL) goto fail;

     /* use ManageDSAit: we want to move/copy refs intact */
     c.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     c.ldctl_value.bv_val	= NULL;
     c.ldctl_value.bv_len	= 0;
     c.ldctl_iscritical	= 1;
     
     ctrls[0] = &c;

     scope = g_hash_table_lookup(selhash, "recursively") ? 
	  LDAP_SCOPE_SUBTREE : LDAP_SCOPE_BASE;

     rc = ldap_search_ext_s(ld, 
			    dn,			/* base */
			    scope,		/* search scope */
			    "(objectClass=*)",	/* filter */
			    attrs,		/* attrs */
			    0,			/* attrsonly */
			    ctrls,		/* serverctrls */
			    NULL,		/* clientctrls */
			    NULL,		/* timeout */
			    LDAP_NO_LIMIT,	/* sizelimit */
			    &res);

     if(rc == LDAP_NOT_SUPPORTED) {
	  rc = ldap_search_s(ld, dn, scope, "(objectClass=*)", attrs, 0, &res);
     }

     if (rc == LDAP_SUCCESS) {
	  out = g_string_new("");

	  for( e = ldap_first_entry(ld, res) ; e != NULL ;
	       e = ldap_next_entry(ld, e) ) {
	       ldif_entry_out(out, ld, e, ctx);
	  }

	  gtk_selection_data_set(data,
				 GDK_SELECTION_TYPE_STRING,
				 8, /* character data */
				 (guchar*)out->str,
				 out->len
				 );
	  ok = TRUE;
     } else if (rc == LDAP_SERVER_DOWN) {
	  server->server_down++;
	  error_push(ctx,
		     _("Error searching below '%1$s': %2$s"),
		     dn, ldap_err2string(rc));
	  push_ldap_addl_error(ld, ctx);
     } else {
	  error_push(ctx,
		     _("Error searching below '%1$s': %2$s"),
		     dn, ldap_err2string(rc));
	  push_ldap_addl_error(ld, ctx);
     }

     if (!ok) {
	  gtk_selection_data_set(data,
				 GDK_SELECTION_TYPE_STRING,
				 8, /* character data */
				 NULL,
				 0
				 );
     }

 fail:
     if (out) g_string_free(out, TRUE);
     if (res) ldap_msgfree(res);
     if (ld) close_connection(server, FALSE);

 done:
     error_flush(ctx);
     return;
}


static void get_selection_gq(GtkWidget *ctree,
			     GtkSelectionData *data,
			     GHashTable *selhash)
{
     GByteArray *buf = g_byte_array_new();
     guchar nul = '\0';

     g_hash_table_foreach(selhash, (GHFunc) drag_selection_data_prepare, buf);
     g_byte_array_append(buf, &nul, 1);

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("get_selection_gq data=%08lx\n", (unsigned long) data);
     }
#endif

     gtk_selection_data_set(data,
			    gdk_atom_intern("gq-browse-ctree", FALSE),
			    8, /* character data */
			    buf->data,
			    buf->len);

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("data->data=%08lx data=%08lxx\n",
		 (unsigned long)(data->data), (unsigned long) data);
     }
#endif

     g_byte_array_free(buf, TRUE);
}


static void get_selection(GtkWidget *ctree,
			  GtkSelectionData *data,
			  guint info,
			  guint time,
			  GqTab *tab)  /* XXX don't need to pass tab here */
{
     GHashTable *selhash = gtk_object_get_data(GTK_OBJECT(ctree), 
					       "drag-and-selection-data");
     if (selhash) {
	  if (data->target == gdk_atom_intern("gq-browse-ctree", FALSE)) {
	       get_selection_gq(ctree, data, selhash);
	  }
	  if (data->target == GDK_SELECTION_TYPE_STRING) {
	       get_selection_string(ctree, data, selhash);
	  }
     }

}


void paste_entry(GtkWidget *widget, GqTab *tab)
{
     GtkWidget *ctree;
     int ctx;

     ctree = GTK_WIDGET(GQ_TAB_BROWSE(tab)->ctreeroot);

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("paste_entry %08lx\n", (unsigned long) ctree);
     }
#endif
     gtk_selection_convert(ctree,
			   GDK_SELECTION_PRIMARY,
			   gdk_atom_intern("gq-browse-ctree", FALSE),
			   GDK_CURRENT_TIME);


     ctx = error_new_context(_("Refreshing entry after pasting"), ctree);
     do_refresh(ctx, tab);
     error_flush(ctx);
}

/* gets fired after the gtk_selection_convert triggered through
   "paste" finished. Similar to when dragging stops. */
static void selection_received(GtkWidget *ctree,
			       GtkSelectionData *data,
			       guint time,
			       GqTab *tab) 
{
     GQTreeWidgetNode *ctree_node;
     GqBrowserNodeDn *target_entry;
     GqServer *target_server;
     GQTreeWidget *ctreeroot;
     GHashTable *selhash = NULL;

     ctreeroot = GQ_TAB_BROWSE(tab)->ctreeroot;

#ifdef DEBUG
     if (debug & GQ_DEBUG_BROWSER_DND) {
	  printf("selection_received seldata=%08lx\n",
		 (unsigned long) data);
     }
#endif

     ctree_node = GQ_TAB_BROWSE(tab)->tree_row_popped_up;
     target_entry =
	  GQ_BROWSER_NODE_DN(gq_tree_get_node_data (ctreeroot,
						     ctree_node));
     if (target_entry == NULL) return;
     if (!GQ_IS_BROWSER_NODE_DN(target_entry)) return;

     target_server = server_from_node(ctreeroot, ctree_node);

     /* For now we allow recursive moves, as we _believe_ it
	works even with a limited number of answers from
	servers... */

     selhash = drag_selection_data_unpack((gchar*)data->data,
					  data->length);

     if (selhash) {
	  int flags = MOVE_CROSS_SERVER;
	  char *msg = NULL;
	  int answer;


	  if (g_hash_table_lookup(selhash, "recursively") != NULL) {
	       flags |= MOVE_RECURSIVELY;
	  }

	  if (flags & MOVE_RECURSIVELY) {
	       msg = _("Do you really want to paste this entry recursively?\n"
		       "Note that you might not be able to really copy everything\n"
		       "in case you do not have access to ALL attributes of the\n"
		       "object(s) to paste (the same holds for objects below it).\n"
		       "USE AT YOUR OWN RISK!");
	  } else {
	       msg = _("Do you really want to paste this entry?\n"
		       "Note that you might not be able to really paste\n"
		       "the entire object if you do not have\n"
		       "access to ALL attributes of the object.\n");
	  }
	  
	  answer = 
	       question_popup(_("Do you really want to move this entry?"),
			      msg);
	  
	  if (answer) {
	       do_move_after_reception(GTK_WIDGET(ctreeroot),
				       selhash, ctree_node,
				       target_entry, target_server,
				       flags);
	  }
     }
}


void browse_dnd_setup(GtkWidget *ctreeroot, GqTab *tab) 
{
     GtkTargetEntry *target_entry;

     /* signal for copying entries out of the tree */
     gtk_selection_add_target(GTK_WIDGET(ctreeroot),
			      GDK_SELECTION_PRIMARY,
			      GDK_SELECTION_TYPE_STRING,
			      1);
     gtk_selection_add_target(GTK_WIDGET(ctreeroot),
			      GDK_SELECTION_PRIMARY,
			      gdk_atom_intern("gq-browse-ctree", FALSE),
			      1);

     g_signal_connect(ctreeroot, 
			"selection-get",
			G_CALLBACK(get_selection),
			tab);
     g_signal_connect(ctreeroot, 
			"selection-received",
			G_CALLBACK(selection_received),
			tab);

     target_entry = (GtkTargetEntry*) g_malloc(sizeof(GtkTargetEntry));
     target_entry->target = "gq-browse-ctree";
     target_entry->flags = 0; /* GTK_TARGET_SAME_WIDGET; */
     target_entry->info = 0;

     gtk_drag_dest_set(ctreeroot, GTK_DEST_DEFAULT_ALL,
		       target_entry, 1,
		       GDK_ACTION_MOVE
		       );
     gtk_drag_source_set(ctreeroot, 
			 GDK_BUTTON1_MASK | GDK_BUTTON2_MASK,
			 target_entry, 1,
			 GDK_ACTION_MOVE
			 );

     gtk_clist_set_button_actions(GTK_CLIST(ctreeroot), 1, 
				  GTK_BUTTON_SELECTS | GTK_BUTTON_DRAGS | GTK_BUTTON_EXPANDS);

     g_signal_connect(ctreeroot, "drag-data-received",
			G_CALLBACK(drag_data_received),
			NULL);

     g_signal_connect(ctreeroot, "drag-data-get",
			 G_CALLBACK(drag_data_get),
			tab);
     
     g_signal_connect(ctreeroot, "drag-data-delete",
			 G_CALLBACK(drag_data_delete),
			 tab);

     g_signal_connect(ctreeroot, "drag-motion",
			G_CALLBACK(drag_motion),
			NULL);

     g_signal_connect(ctreeroot, "drag-begin",
			G_CALLBACK(drag_begin),
			tab);


     /* HACK alert */
     g_signal_connect(ctreeroot, "drag-end",
			G_CALLBACK(drag_end),
			tab);

/*       gtk_ctree_set_drag_compare_func(GTK_CTREE(ctreeroot), */
/*				     compare_drag_func); */

     g_free(target_entry);
     target_entry = NULL;

}

#endif /* BROWSER_DND */

/*
   Local Variables:
   c-basic-offset: 5
   End:
 */
