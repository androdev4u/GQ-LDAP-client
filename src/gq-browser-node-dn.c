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

#include "gq-browser-node-dn.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <errno.h>		/* errno */
#include <stdio.h>		/* FILE */
#include <stdlib.h>		/* free - MUST get rid of malloc/free */

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

#include "common.h"
#include "gq-browser-node-reference.h"
#include "gq-tab-browse.h"
#include "gq-tab-search.h"

#include "input.h"		/* new_from_entry */
#include "search.h"		/* fill_out_search */
#include "template.h"		/* struct gq_template */
#include "formfill.h"		/* formlist_from_entry */

#include "tinput.h"		/* formfill_from_template */
#include "browse-dnd.h"		/* copy_entry et al */

#include "configfile.h"		/* config */
#include "errorchain.h"
#include "util.h"
#include "encode.h"

#include "browse-export.h"

static void tree_row_search_below(GtkMenuItem *menuitem, GqTab *tab)
{
     GQTreeWidget *ctree;
     GQTreeWidgetNode *node;
     GqBrowserNode *e;
     GqServer *server;
     GqTab *search_tab;

     ctree = GQ_TAB_BROWSE(tab)->ctreeroot;
     node = GQ_TAB_BROWSE(tab)->tree_row_popped_up;
     e = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, node));

     g_assert(GQ_IS_BROWSER_NODE_DN(e));

     server = server_from_node(ctree, node);

     if (e == NULL || server == NULL)
	  return;

     search_tab = get_last_of_mode(SEARCH_MODE);
     if (!search_tab) {
	  new_modetab(&mainwin, SEARCH_MODE);
	  search_tab = get_last_of_mode(SEARCH_MODE);
     }

     if (search_tab) {
	  fill_out_search(search_tab, server, GQ_BROWSER_NODE_DN(e)->dn);
     }
}

static GQTreeWidgetNode *ref_browse_single_add(const char *uri,
					   GQTreeWidget *ctree,
					   GQTreeWidgetNode *node)
{
     GqBrowserNodeReference *new_entry;
     GQTreeWidgetNode *new_item, *added = NULL;

     new_entry = GQ_BROWSER_NODE_REFERENCE(gq_browser_node_reference_new(uri));

     added = gq_tree_insert_node (ctree,
				  node, NULL,
				  uri,
				  new_entry,
				  g_object_unref);

     gq_tree_insert_dummy_node (ctree,
				added);

     return added;
}


static void browse_new_from_entry_callback(GtkMenuItem *widget,
					   GqBrowserNodeDn *entry)
{
     if (GQ_IS_BROWSER_NODE_DN(entry)) {
	  char *dn = entry->dn;
	  int error_context =
	       error_new_context(_("Creating new entry from existing entry"),
				 GTK_WIDGET(widget));

	  GqServer *server =
	       GQ_SERVER(gtk_object_get_data(GTK_OBJECT(widget),
							 "server"));

	  new_from_entry(error_context, server, dn);

	  error_flush(error_context);
     }
}

static void browse_new_from_template_callback(GtkWidget *widget,
					      struct gq_template *template)
{
#ifdef HAVE_LDAP_STR2OBJECTCLASS
     GList *formlist;
     GqServer *server;
     struct inputform *iform;
     GqBrowserNodeDn *entry;
     int error_context;

     server = GQ_SERVER(gtk_object_get_data(GTK_OBJECT(widget),
							"server"));
     entry = GQ_BROWSER_NODE_DN(gtk_object_get_data(GTK_OBJECT(widget),
						     "entry"));
     if (!GQ_IS_BROWSER_NODE_DN(entry)) return;

     error_context =
	  error_new_context(_("Creating now entry from template"),
			    widget);

     iform = new_inputform();
     iform->dn = NULL;
     iform->server = g_object_ref(server);

     formlist = formfill_from_template(error_context, server, template);
     if(formlist) {
	  iform->formlist = formlist;
	  if (entry && entry->dn) {
	       /* don't need the RDN of the current entry */
	       char *newdn = g_malloc(strlen(entry->dn) + 2);
	       newdn[0] = ',';
	       newdn[1] = 0;
	       strcat(newdn, entry->dn);
	       iform->dn = newdn;
	  }

	  create_form_window(iform);
	  create_form_content(iform);

	  build_inputform(error_context, iform);
     } else {
	  free_inputform(iform);
     }

     error_flush(error_context);
#endif /* HAVE_LDAP_STR2OBJECTCLASS */
}

static void dump_subtree(GtkWidget *widget, GqTab *tab)
{
     GQTreeWidget *ctree;
     GQTreeWidgetNode *node;
     GqBrowserNode *e;
     GqServer *server;
     GList *to_export = NULL;
     struct dn_on_server *dos;
     int error_context;

     ctree = GQ_TAB_BROWSE(tab)->ctreeroot;
     node = GQ_TAB_BROWSE(tab)->tree_row_popped_up;
     e = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, node));

     g_assert(GQ_IS_BROWSER_NODE_DN(e));

     server = server_from_node(ctree, node);

     if (e == NULL || server == NULL)
	  return;

     error_context = error_new_context(_("Exporting entry to LDIF"),
				       tab->win->mainwin);

     dos = new_dn_on_server(GQ_BROWSER_NODE_DN(e)->dn, server);
     dos->flags = LDAP_SCOPE_SUBTREE; /* default is LDAP_SCOPE_BASE */
     to_export = g_list_append(to_export, dos);

     export_many(error_context, tab->win->mainwin, to_export);

     error_flush(error_context);
}

static void delete_browse_entry(GtkWidget *widget, GqTab *tab)
{
     GQTreeWidget *ctree;
     GQTreeWidgetNode *node;
     GqServer *server;
     GqBrowserNodeDn *entry;
     int do_delete;

     ctree = GQ_TAB_BROWSE(tab)->ctreeroot;
     node = GQ_TAB_BROWSE(tab)->selected_ctree_node;

     entry = GQ_BROWSER_NODE_DN(gq_tree_get_node_data (ctree, node));
     if (entry == NULL)
	  return;

     if ((server = server_from_node(ctree, node)) == NULL)
	  return;

     do_delete = 0;

     gtk_clist_freeze(GTK_CLIST(ctree));
     
     if (!entry->seen) {
	  gq_tree_fire_expand_callback (ctree, node);
     }

     if (entry->leaf) {
	  /* item is a leaf node */
	  do_delete = 1;
     } else {
	  /* maybe delete everything in the subtree as well?
	     should do another LDAP_SCOPE_SUBTREE search after
	     each batch of deletes, in case the server is limiting
	     the number of entries returned per search. This could
	     get hairy...

	     For now, just pop up a dialog box with a warning
	  */

	  do_delete =
	       question_popup(_("Warning"),
			      _("This entry has a subtree!\n"
				"Do you want to delete every entry under it as well?"));
     }


     if (do_delete) {
	  int ctx = error_new_context(_("Deleting entry/subtree"),
				      GTK_WIDGET(ctree));
	  if (delete_entry_full(ctx, server, entry->dn, TRUE)) {
	       GqBrowserNode *p_entry;
	       GQTreeWidgetNode *parent = gq_tree_get_parent_node (ctree,
								   node);
	       gq_tree_remove_node(ctree,
				     node);

	       /* the only thing left to do is to refresh the parent
                  node in order to get the leaf flag of that entry
                  right again */
	       p_entry = GQ_BROWSER_NODE(gq_tree_get_node_data (ctree, parent));
	       if (p_entry) {

		    if (GQ_BROWSER_NODE_GET_CLASS(p_entry)->refresh)
			 GQ_BROWSER_NODE_GET_CLASS(p_entry)->refresh(p_entry, ctx, ctree,
							parent, tab);
	       }
	  }
	  error_flush(ctx);
     }
     gtk_clist_thaw(GTK_CLIST(ctree));
}









/*
 * Destructor for GqBrowserNodeDn objects
 */
static void destroy_dn_browse_entry(GqBrowserNode *e)
{
     GqBrowserNodeDn *entry;

     if (!e) return;

     g_assert(GQ_IS_BROWSER_NODE_DN(e));
     entry = GQ_BROWSER_NODE_DN(e);

     if (entry->dn) g_free(entry->dn);
     free(entry);
}


static void dn_browse_entry_expand(GqBrowserNode *be,
				   int error_context,
				   GQTreeWidget *ctree,
				   GQTreeWidgetNode *node,
				   GqTab *tab)
{
     LDAP *ld = NULL;
     LDAPMessage *res = NULL, *e;
     GqServer *server = NULL;
     int msg, rc, num_children, update_counter, err;
     char message[1024 + 21];
     char *dummy[] = { "dummy", NULL };
     char *ref[] = { "ref", NULL };
     char *c, **refs;
     GqBrowserNodeDn *entry;

     LDAPControl ct;
     LDAPControl *ctrls[2] = { NULL, NULL } ;

     g_assert(GQ_IS_BROWSER_NODE_DN(be));
     entry = GQ_BROWSER_NODE_DN(be);

     if (!entry->seen) {
	  server = server_from_node(ctree, node);
/*  	  printf("server=%08lx host=%s dn=%s\n", (long) server, */
/*  		 server->ldaphost, */
/*  		 entry->dn); */
	  
	  gtk_clist_freeze(GTK_CLIST(ctree));

	  gq_tree_remove_children (ctree, node);

	  if( (ld = open_connection(error_context, server)) == NULL) {
	       gtk_clist_thaw(GTK_CLIST(ctree));
	       return;
	  }

#if HAVE_LDAP_CLIENT_CACHE
	  if (entry->uncache) {
	       ldap_uncache_entry(ld, entry->dn);
	       entry->uncache = FALSE;
	  }
#endif

	  statusbar_msg(_("Onelevel search on %s"), entry->dn);
	  
	  ct.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
	  ct.ldctl_value.bv_val	= NULL;
	  ct.ldctl_value.bv_len	= 0;
	  ct.ldctl_iscritical	= 1;
		    
	  ctrls[0] = &ct;
	  
	  /* check if this is a referral object */

	  rc = ldap_search_ext(ld, entry->dn,
			       LDAP_SCOPE_BASE, 
			       "(objectClass=referral)", ref, 0,
			       ctrls,		/* serverctrls */
			       NULL,		/* clientctrls */
			       NULL,		/* timeout */
			       LDAP_NO_LIMIT,	/* sizelimit */
			       &msg);

	  /* FIXME: THIS IS NOT CORRECT */
/* 	  if (rc == -1) { */
/* 	       statusbar_msg(_("Searching for '%1$s': %2$s"),  */
/* 			     entry->dn, */
/* 			     ldap_err2string(msg)); */
/* 	       close_connection(server, FALSE); */
/* 	       gtk_clist_thaw(GTK_CLIST(ctree)); */
	       
/* 	       error_flush(context); */
/* 	       return; */
	       
/* 	  } */

	  while((rc = ldap_result(ld, msg, 0,
				  NULL, &res)) == LDAP_RES_SEARCH_ENTRY) {
	       for(e = ldap_first_entry(ld, res) ; e != NULL ;
		   e = ldap_next_entry(ld, e)) {
		    char **vals = ldap_get_values(ld, e, "ref");
		    int i;
		    
		    if (vals == NULL) continue;
		    
		    for(i = 0; vals[i]; i++) {
			 entry->is_ref = TRUE; /* now we know for sure */
			 ref_browse_single_add(vals[i], ctree, node);
		    }
		    
		    if (vals)  /* redundant, but... */
			 ldap_value_free(vals);
	       }
	       if (res) ldap_msgfree(res);
	       res = NULL;
	  }
	  
	  if (res) ldap_msgfree(res);
	  res = NULL;

	  if (entry->is_ref) {
	       entry->seen = TRUE;
	       statusbar_msg(_("Showing referrals"));
	       gtk_clist_thaw(GTK_CLIST(ctree));
	       close_connection(server, FALSE);
	       return;
	  }





	  rc = ldap_search_ext(ld, entry->dn,
			       LDAP_SCOPE_ONELEVEL, 
			       "(objectClass=*)", dummy, 1,
			       ctrls,		/* serverctrls */
			       NULL,		/* clientctrls */
			       NULL,		/* timeout */
			       LDAP_NO_LIMIT,	/* sizelimit */
			       &msg);

	  /* FIXME */
/* 	  if(rc != LDAP_SUCCESS) { */
/* 	       statusbar_msg(_("Error while searching below '%1$s': %2$s"), */
/* 			     entry->dn, */
/* 			     ldap_err2string(msg)); */
/* 	       close_connection(server, FALSE); */
/* 	       gtk_clist_thaw(GTK_CLIST(ctree)); */

/* 	       error_flush(context); */
/* 	       return; */
/* 	  } */
	  
	  num_children = update_counter = 0;

	  while( (rc = ldap_result(ld, msg, 0,
				   NULL, &res)) == LDAP_RES_SEARCH_ENTRY) {
	       for(e = ldap_first_entry(ld, res) ; e != NULL ;
		   e = ldap_next_entry(ld, e)) {

		    char *dn = ldap_get_dn(ld, e);
		    dn_browse_single_add(dn, ctree, node);
		    if (dn) ldap_memfree(dn);

		    num_children++;
		    update_counter++;
		    if(update_counter >= 100) {
			 statusbar_msg(ngettext("One entry found (running)",
						"%d entries found (running)",
						num_children), num_children);

			 update_counter = 0;
		    }
	       }
	       ldap_msgfree(res);
	  }
	  /* tree sorting */
	  gtk_clist_set_sort_type(GTK_CLIST(ctree), GTK_SORT_ASCENDING);
	  gtk_clist_set_sort_column(GTK_CLIST(ctree), 0);
	  gtk_clist_set_compare_func(GTK_CLIST(ctree), (GtkCListCompareFunc)NULL);
	  gq_tree_widget_sort_node(GQ_TREE_WIDGET(ctree), node);
	  entry->leaf = (num_children == 0);

	  g_snprintf(message, sizeof(message),
		   ngettext("One entry found (finished)",
			    "%d entries found (finished)", num_children),
		   num_children);

	  ldap_get_option(ld, LDAP_OPT_ERROR_NUMBER, &rc);

	  if (rc == LDAP_SERVER_DOWN) {
	       server->server_down++;
	       gtk_clist_thaw(GTK_CLIST(ctree));
	       goto done;
	  }

	  if (res) {
	       rc = ldap_parse_result(ld, res,
				      &err, &c, NULL, &refs, NULL, 0);
	  }

	  if (rc != LDAP_SUCCESS) {	
	       /* FIXME: better error message (but what is the exact cause?)*/
	       error_push(error_context, ldap_err2string(rc));
	       push_ldap_addl_error(ld, error_context);

	       if (rc == LDAP_SERVER_DOWN) {
		    server->server_down++;
	       }
	  } else {
	       if (err == LDAP_SIZELIMIT_EXCEEDED) {
		    int l = strlen(message);
		    g_snprintf(message + l, sizeof(message) - l, 
			     " - %s", _("size limit exceeded"));
	       } else if (err == LDAP_TIMELIMIT_EXCEEDED) {
		    int l = strlen(message);
		    g_snprintf(message + l, sizeof(message) - l, 
			     " - %s", _("time limit exceeded"));
	       } else if (err != LDAP_SUCCESS) {
		    error_push(error_context, ldap_err2string(err));
		    push_ldap_addl_error(ld, error_context);
		    if (c && strlen(c)) {
			 error_push(error_context, _("Matched DN: %s"), c);
		    }
		    if (refs) {
			 int i;
			 for (i = 0 ; refs[i] ; i++) {
			      error_push(error_context,
					 _("Referral to: %s"), refs[i]);
			 }
		    }
	       }
	  }

	  statusbar_msg(message);

	  gtk_clist_thaw(GTK_CLIST(ctree));

	  entry->seen = TRUE;
     }


     /* XXX the code that sets this is #if0'ed, so this is dead code...
     if (g_hash_table_lookup(hash, "expand-all")) {
	  GQTreeWidgetNode *n;
	  gtk_clist_freeze(GTK_CLIST(ctree));
	  for (n = GTK_CTREE_ROW(node)->children ; n ;
	       n = GTK_CTREE_NODE_NEXT(n)) {
	       gq_tree_expand_node(ctree, n);
	  }
	  gtk_clist_thaw(GTK_CLIST(ctree));
     }
     */

 done:
     if (res) ldap_msgfree(res);
     if (server && ld) close_connection(server, FALSE);
}

static void browse_edit_from_entry(GqBrowserNode *e,
				   int error_context,
				   GQTreeWidget *ctreeroot,
				   GQTreeWidgetNode *ctreenode,
				   GqTab *tab)
{
     GList *oldlist, *newlist, *tmplist;
     GtkWidget *pane2_scrwin, *pane2_vbox;
     GqServer *server;
     struct inputform *iform;
     char *dn;
/*      int hidden = 0; */
     GqBrowserNodeDn *entry;

     g_assert(GQ_IS_BROWSER_NODE_DN(e));
     entry = GQ_BROWSER_NODE_DN(e);

     if (ctreenode == NULL)
	  return;

     if( (server = server_from_node(ctreeroot, ctreenode)) == NULL)
	  return;

     dn = entry->dn;
     record_path(tab, GQ_BROWSER_NODE(entry), ctreeroot, ctreenode);

     ctreeroot = GQ_TAB_BROWSE(tab)->ctreeroot;

     iform = new_inputform();
     GQ_TAB_BROWSE(tab)->inputform = iform;

     iform->server = g_object_ref(server);

     iform->edit = 1;

     /* pass on old "hide" status */
     iform->hide_status = GQ_TAB_BROWSE(tab)->hidden;

     tmplist = NULL;
     oldlist = formlist_from_entry(error_context, server, dn, 0);
#ifdef HAVE_LDAP_STR2OBJECTCLASS
     oldlist = add_schema_attrs(error_context, server, oldlist);
#endif

     if(oldlist) {
	  iform->oldlist = oldlist;
	  newlist = dup_formlist(oldlist);
	  iform->formlist = newlist;
	  iform->olddn = g_strdup(dn);
	  iform->dn = g_strdup(dn);

	  if (ctreeroot) {
	       iform->ctreeroot = ctreeroot;
	       iform->ctree_refresh = gq_tree_get_parent_node (ctreeroot,
							       ctreenode);
	  }

	  /* XXX should free etc first */
	  pane2_scrwin = GQ_TAB_BROWSE(tab)->pane2_scrwin;
	  gtk_container_remove(GTK_CONTAINER(pane2_scrwin),
			       GTK_BIN(pane2_scrwin)->child);

	  pane2_vbox = gtk_vbox_new(FALSE, 2);
	  iform->target_vbox = pane2_vbox;
	  GQ_TAB_BROWSE(tab)->pane2_vbox = pane2_vbox;

	  gtk_widget_show(pane2_vbox);
	  gtk_widget_set_parent_window(pane2_vbox, (mainwin.mainwin->window));
	  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(pane2_scrwin), pane2_vbox);

	  create_form_content(iform);
	  build_inputform(error_context, iform);
     } else {
	  inputform_free(iform);
	  GQ_TAB_BROWSE(tab)->inputform = NULL;
     }

}

static void dn_browse_entry_refresh(GqBrowserNode *entry,
				    int error_context,
				    GQTreeWidget *ctree,
				    GQTreeWidgetNode *node,
				    GqTab *tab)
{
     g_assert(GQ_IS_BROWSER_NODE_DN(entry));

     refresh_subtree(error_context, ctree, node);
     GQ_BROWSER_NODE_GET_CLASS(entry)->select(entry,
						  error_context,
						  ctree, node, tab);
}

static char* dn_browse_entry_get_name(GqBrowserNode *entry,
				      gboolean long_form)
{
     char **exploded_dn;
     char *g;

     g_assert(GQ_IS_BROWSER_NODE_DN(entry));

     if (long_form) {
	  return g_strdup(GQ_BROWSER_NODE_DN(entry)->dn);
     } else {
	  exploded_dn = gq_ldap_explode_dn(GQ_BROWSER_NODE_DN(entry)->dn, FALSE);

	  g = g_strdup(exploded_dn[0]);

	  gq_exploded_free(exploded_dn);

	  return g;
     }
}

static void dn_browse_entry_popup(GqBrowserNode *entry, GtkWidget *menu,
				  GQTreeWidget *ctreeroot,
				  GQTreeWidgetNode *ctree_node,
				  GqTab *tab)
{
     GtkWidget *menu_item, *submenu;
     GqServer *server;
     int is_dn;
#ifdef HAVE_LDAP_STR2OBJECTCLASS
     GList *templatelist;
     struct gq_template *template;
#endif

     g_assert(GQ_IS_BROWSER_NODE_DN(entry));

     is_dn = GQ_IS_BROWSER_NODE_DN(entry);
     
     if ((server = server_from_node(ctreeroot, ctree_node)) == NULL)
	  return;
     
     /* New submenu */
     menu_item = gtk_menu_item_new_with_label(_("New"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     submenu = gtk_menu_new();
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
     gtk_widget_show(menu_item);

#ifdef HAVE_LDAP_STR2OBJECTCLASS
     templatelist = config->templates;
     while(templatelist) {
	  template = (struct gq_template *) templatelist->data;
	  menu_item = gtk_menu_item_new_with_label(template->name);
	  gtk_object_set_data_full(GTK_OBJECT(menu_item), "server",
				   g_object_ref(server), g_object_unref);

	  gtk_object_set_data(GTK_OBJECT(menu_item), "entry", entry);
	  gtk_menu_append(GTK_MENU(submenu), menu_item);
	  g_signal_connect(menu_item, "activate",
			     G_CALLBACK(browse_new_from_template_callback),
			     template);
	  gtk_widget_show(menu_item);

	  templatelist = templatelist->next;
     }
#endif

     menu_item = gtk_menu_item_new_with_label(_("Use current entry"));
     gtk_object_set_data(GTK_OBJECT(menu_item), "server", server);
     gtk_menu_append(GTK_MENU(submenu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(browse_new_from_entry_callback),
			entry);
     gtk_widget_show(menu_item);

     /* Export to LDIF */
     menu_item = gtk_menu_item_new_with_label(_("Export to LDIF"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(dump_subtree),
			tab);
     gtk_widget_show(menu_item);

     menu_item = gtk_menu_item_new();
     gtk_menu_append(GTK_MENU(menu), menu_item);
     gtk_widget_show(menu_item);

     /* Search below */
     menu_item = gtk_menu_item_new_with_label(_("Search below"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(tree_row_search_below),
			tab);
     gtk_widget_show(menu_item);

     menu_item = gtk_menu_item_new();
     gtk_menu_append(GTK_MENU(menu), menu_item);
     gtk_widget_show(menu_item);


#ifdef BROWSER_DND
     /* Copy */
     menu_item = gtk_menu_item_new_with_label(_("Copy"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(copy_entry),
			tab);

     if (!is_dn) {
	  gtk_widget_set_sensitive(menu_item, FALSE);
     }

     gtk_widget_show(menu_item);

     /* Copy all */
     menu_item = gtk_menu_item_new_with_label(_("Copy all"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(copy_entry_all),
			tab);

     if (!is_dn) {
	  gtk_widget_set_sensitive(menu_item, FALSE);
     }
     gtk_widget_show(menu_item);

     /* Paste */
     menu_item = gtk_menu_item_new_with_label(_("Paste"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(paste_entry),
			tab);

     if (!is_dn) {
	  gtk_widget_set_sensitive(menu_item, FALSE);
     }
     gtk_widget_show(menu_item);

     menu_item = gtk_menu_item_new();
     gtk_menu_append(GTK_MENU(menu), menu_item);
     gtk_widget_show(menu_item);
#endif
     /* Delete */
     menu_item = gtk_menu_item_new_with_label(_("Delete"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(delete_browse_entry),
			tab);

     if (!is_dn) {
	  gtk_widget_set_sensitive(menu_item, FALSE);
     }

     gtk_widget_show(menu_item);

}

/*
 * Constructor for GqBrowserNodeDn objects taking the dn
 */
GqBrowserNode*
gq_browser_node_dn_new(const char *dn)
{
	GqBrowserNodeDn *e = g_object_new(GQ_TYPE_BROWSER_NODE_DN, NULL);

	e->dn = g_strdup(dn);

	return GQ_BROWSER_NODE(e);
}

/* GType */
G_DEFINE_TYPE(GqBrowserNodeDn, gq_browser_node_dn, GQ_TYPE_BROWSER_NODE);

static void
gq_browser_node_dn_init(GqBrowserNodeDn* self) {}

static void
gq_browser_node_dn_class_init(GqBrowserNodeDnClass* self_class) {
	GqBrowserNodeClass* node_class = GQ_BROWSER_NODE_CLASS(self_class);
	node_class->destroy  = destroy_dn_browse_entry;
	node_class->expand   = dn_browse_entry_expand;
	node_class->select   = browse_edit_from_entry;
	node_class->refresh  = dn_browse_entry_refresh;
	node_class->get_name = dn_browse_entry_get_name;
	node_class->popup    = dn_browse_entry_popup;
}

