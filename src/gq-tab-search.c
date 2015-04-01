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

#include "gq-tab-search.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdlib.h>
#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "common.h"
#include "configfile.h"
#include "errorchain.h"
#include "gq-constants.h"
#include "gq-server-list.h"
#include "gq-tab-browse.h"
#include "mainwin.h"
#include "util.h"
#include "formfill.h"
#include "input.h"
#include "template.h"
#include "tinput.h"
#include "filter.h"
#include "encode.h"
#include "schema.h"
#include "state.h"
#include "syntax.h"
#include "browse-export.h"

#define MAX_NUM_ATTRIBUTES		256

static void find_in_browser(GqTab *tab);
static void add_all_to_browser(GqTab *tab);
static void add_selected_to_browser(GqTab *tab);
static void export_search_selected_entry(GqTab *tab);
static void delete_search_selected(GqTab *tab);
static void query(GqTab *tab);

static int column_by_attr(struct attrs **attrlist, const char *attribute);
static int new_attr(struct attrs **attrlist, const char *attr);
static struct attrs *find_attr(struct attrs *attrlist, const char *attr);

static gint searchbase_button_clicked(GtkWidget *widget,
				      GdkEventButton *event,
				      GqTab *tab);
static void findbutton_clicked_callback(GqTab *tab);

static gboolean search_button_press_on_tree_item(GtkWidget *clist,
						 GdkEventButton *event,
						 GqTab *tab);

static void servername_changed_callback(GqTab *tab);
static int select_entry_callback(GtkWidget *clist, gint row, gint column,
				 GdkEventButton *event, GqTab *tab);
static void search_edit_entry_callback(GqTab *tab);
static void search_new_from_entry_callback(GtkWidget *w, GqTab *tab);
static void delete_search_entry(GqTab *tab);

static void search_save_snapshot(int error_context,
				 char *state_name, GqTab *tab)
{
     GList *hist;
     char *tmp;

     hist = GQ_TAB_SEARCH(tab)->history;
     if (hist) {
	  state_value_set_list(state_name, "history", hist);
     }

     state_value_set_list(state_name, "attributes", GQ_TAB_SEARCH(tab)->attrs);
     state_value_set_int(state_name, "chase", GQ_TAB_SEARCH(tab)->chase_ref);
     state_value_set_int(state_name, "max-depth", GQ_TAB_SEARCH(tab)->max_depth);
     state_value_set_int(state_name, "scope", GQ_TAB_SEARCH(tab)->scope);

     state_value_set_int(state_name, "last-options-tab",
			 GQ_TAB_SEARCH(tab)->last_options_tab);

     tmp =  gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(GQ_TAB_SEARCH(tab)->serverlist_combo)->entry), 0, -1);
     state_value_set_string(state_name, "lastserver", tmp);
     g_free(tmp);
}

static void
search_restore_selection(GQServerList* list, GqServer* server, gpointer user_data) {
	gpointer**i_tab_and_servername = user_data;
	gint *i = (gint*)i_tab_and_servername[0];
	GqTab* tab = GQ_TAB(i_tab_and_servername[1]);
	gchar const* servername = (gchar const*)i_tab_and_servername[2];

	if(!strcasecmp(server->name, servername)) {
		gtk_list_select_item(GTK_LIST(GTK_COMBO(GQ_TAB_SEARCH(tab)->serverlist_combo)->list), *i);
	}

	*i++;
}

static void search_restore_snapshot(int error_context,
				    char *state_name, GqTab *tab,
				    struct pbar_win *progress)
{
     GqServer *server;
     GList *searchhist;
     const char *lastserver;

     lastserver = state_value_get_string(state_name, "lastserver", NULL);
     if(lastserver && lastserver[0]) {
          gint i = 0;
	  gpointer i_tab_and_servername[3] = {
		  &i,
		  tab,
		  (gchar*)lastserver
	  };
	  gq_server_list_foreach(gq_server_list_get(), search_restore_selection, i_tab_and_servername);
     }

     if (config->restore_search_history) {
	  const GList *hist = state_value_get_list(state_name, "history");
	  if (hist) {
	       const GList *I;

	       searchhist = GQ_TAB_SEARCH(tab)->history;
	       for (I = hist ; I ; I = g_list_next(I)) {
		    searchhist = g_list_append(searchhist, g_strdup(I->data));
	       }
	       searchhist = g_list_insert(searchhist, "", 0);
	       gtk_combo_set_popdown_strings(GTK_COMBO(GQ_TAB_SEARCH(tab)->search_combo), searchhist);
	       searchhist = g_list_remove(searchhist, searchhist->data);
	       GQ_TAB_SEARCH(tab)->history = searchhist;
	  }

	  /* independently configurable? */

	  GQ_TAB_SEARCH(tab)->chase_ref = 
	       state_value_get_int(state_name, "chase", 1);
	  GQ_TAB_SEARCH(tab)->max_depth = 
	       state_value_get_int(state_name, "max-depth", 7);
	  GQ_TAB_SEARCH(tab)->scope = 
	       state_value_get_int(state_name, "scope", LDAP_SCOPE_SUBTREE);

	  GQ_TAB_SEARCH(tab)->attrs = free_list_of_strings(GQ_TAB_SEARCH(tab)->attrs);
	  GQ_TAB_SEARCH(tab)->attrs = 
	       copy_list_of_strings(state_value_get_list(state_name,
							 "attributes"));

	  GQ_TAB_SEARCH(tab)->last_options_tab =
	       state_value_get_int(state_name, "last-options-tab", 0);
     }
}

static GtkWidget *current_search_options_window = NULL;

static void search_options_destroyed(GtkWidget *w)
{
     current_search_options_window = NULL;  
}

struct so_windata {
     GtkWidget *win;
     GtkWidget *notebook;
     GtkWidget *chase_ref;
     GtkWidget *show_ref;
     GtkWidget *onelevel;
     GtkWidget *subtree;
     GtkWidget *max;
     GtkWidget *list;

     GqTab *tab;
     GqServer *server;
     struct server_schema *ss;
};

static void
free_so_windata(struct so_windata *so)
{
	g_return_if_fail(so);

	g_object_unref(so->server);
	g_free(so);
}

const char *lastoptions = "global.search.lastoptions";

static void so_okbutton_clicked(GtkWidget *w, struct so_windata *so)
{
     GqTab *tab = so->tab;
     GList *I;

     if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(so->chase_ref))) {
	  GQ_TAB_SEARCH(tab)->chase_ref = 1;
     } else {
	  GQ_TAB_SEARCH(tab)->chase_ref = 0;
     }

     GQ_TAB_SEARCH(tab)->max_depth =
	  gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(so->max));

     if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(so->onelevel))) {
	  GQ_TAB_SEARCH(tab)->scope = LDAP_SCOPE_ONELEVEL;
     } else {
	  GQ_TAB_SEARCH(tab)->scope = LDAP_SCOPE_SUBTREE;
     }

     GQ_TAB_SEARCH(tab)->attrs = free_list_of_strings(GQ_TAB_SEARCH(tab)->attrs);

     for( I = GTK_CLIST(so->list)->selection ; I ; I = g_list_next(I) ) {
	  int row = GPOINTER_TO_INT(I->data);
	  char *t;

	  gtk_clist_get_text(GTK_CLIST(so->list), row, 0, &t);

	  GQ_TAB_SEARCH(tab)->attrs = g_list_append(GQ_TAB_SEARCH(tab)->attrs,
						g_strdup(t));
     }

     GQ_TAB_SEARCH(tab)->last_options_tab = 
	  gtk_notebook_get_current_page(GTK_NOTEBOOK(so->notebook));

     state_value_set_int(lastoptions, "chase", GQ_TAB_SEARCH(tab)->chase_ref);
     state_value_set_int(lastoptions, "max-depth", GQ_TAB_SEARCH(tab)->max_depth);
     state_value_set_int(lastoptions, "scope", GQ_TAB_SEARCH(tab)->scope);
     state_value_set_list(lastoptions, "attributes", GQ_TAB_SEARCH(tab)->attrs);

     state_value_set_int(lastoptions, "last-options-tab", 
			 GQ_TAB_SEARCH(tab)->last_options_tab);

     gtk_widget_destroy(so->win);
}

static void so_attrs_clear_clicked(GtkWidget *w, struct so_windata *so) 
{
     gtk_clist_unselect_all( GTK_CLIST(so->list));
}

struct so_select_by_oc_cb_data
{
     struct so_windata *so;
     LDAPObjectClass *oc;
};

static void so_select_by_oc(GtkWidget *w,
			    struct so_select_by_oc_cb_data *cbd) 
{
     int i, j;
     g_assert(cbd);

     if (cbd->oc->oc_at_oids_must) {
	  for ( i = 0 ; cbd->oc->oc_at_oids_must[i] ; i++ ) {
	       char *t;
	       for(j = 0 ; 
		   gtk_clist_get_text(GTK_CLIST(cbd->so->list), j, 0, &t) ;
		   j++) {
		    if (strcasecmp(t, cbd->oc->oc_at_oids_must[i]) == 0) {
			 gtk_clist_select_row(GTK_CLIST(cbd->so->list), j, 0);
			 break;
		    }
	       }
	  }
     }
     if (cbd->oc->oc_at_oids_may) {
	  for ( i = 0 ; cbd->oc->oc_at_oids_may[i] ; i++ ) {
	       char *t;
	       for(j = 0 ; 
		   gtk_clist_get_text(GTK_CLIST(cbd->so->list), j, 0, &t) ;
		   j++) {
		    if (strcasecmp(t, cbd->oc->oc_at_oids_may[i]) == 0) {
			 gtk_clist_select_row(GTK_CLIST(cbd->so->list), j, 0);
			 break;
		    }
	       }
	  }
     }

}

/*
 * Button pressed on a tree item. Button 3 gets intercepted and puts up
 * a popup menu, all other buttons get passed along to the default handler
 *
 * the return values here are related to handling of button events only.
 */
static gboolean so_button_press_on_tree_item(GtkWidget *clist,
					     GdkEventButton *event,
					     struct so_windata *so)
{
     GtkWidget *root_menu, *menu, *menu_item, *label, *submenu;

     if (event->type == GDK_BUTTON_PRESS && event->button == 3
	 && event->window == GTK_CLIST(clist)->clist_window) {

	  root_menu = gtk_menu_item_new_with_label("Root");
	  gtk_widget_show(root_menu);
	  menu = gtk_menu_new();

	  gtk_menu_item_set_submenu(GTK_MENU_ITEM(root_menu), menu);

	  label = gtk_menu_item_new_with_label(_("Attributes"));
	  gtk_widget_set_sensitive(label, FALSE);
	  gtk_widget_show(label);

	  gtk_menu_append(GTK_MENU(menu), label);
	  gtk_menu_set_title(GTK_MENU(menu), _("Attributes"));

	  menu_item = gtk_separator_menu_item_new();
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  gtk_widget_set_sensitive(menu_item, FALSE);
	  gtk_widget_show(menu_item);
	  
/* 	  /\* The common Refresh item *\/ */
/* 	  menu_item = gtk_menu_item_new_with_label(_("Refresh")); */
/* 	  gtk_menu_append(GTK_MENU(menu), menu_item); */
/* 	  gtk_widget_show(menu_item); */

/* 	  g_signal_connect(menu_item, "activate", */
/* 			     G_CALLBACK(tree_row_refresh), */
/* 			     tab); */


	  /* Clear */
	  menu_item = gtk_menu_item_new_with_label(_("Clear"));
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  gtk_widget_show(menu_item);

	  g_signal_connect(menu_item, "activate",
			     G_CALLBACK(so_attrs_clear_clicked),
			     so);


	  /* "Select by objectClass" submenu */
	  menu_item = gtk_menu_item_new_with_label(_("Select by objectclass"));
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  submenu = gtk_menu_new();
	  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
	  gtk_widget_show(menu_item);

	  if (so->ss) {
	       GList *I;
	       int i;

	       LDAPObjectClass *oc;

	       for ( I = so->ss->oc ; I ; I = g_list_next(I) ) {
		    oc = (LDAPObjectClass *) I->data;

		    if(oc->oc_names && oc->oc_names[0]) {
			 for(i = 0; oc->oc_names[i]; i++) {
			      struct so_select_by_oc_cb_data *cbd =
				   (struct so_select_by_oc_cb_data*) g_malloc0(sizeof(struct so_select_by_oc_cb_data));
			      cbd->so = so;
			      cbd->oc = oc;

			      menu_item = 
				   gtk_menu_item_new_with_label(oc->oc_names[i]);
			      /* attach for destruction */
			      gtk_object_set_data_full(GTK_OBJECT(menu_item),
						       "cbd",
						       cbd,
						       (GtkDestroyNotify)g_free);
			      gtk_menu_append(GTK_MENU(submenu), menu_item);
			      g_signal_connect(menu_item, "activate",
						 G_CALLBACK(so_select_by_oc),
						 cbd);
			      gtk_widget_show(menu_item);
			 }
		    }
	       }
	  }

/* 	  g_signal_connect(menu_item, "activate", */
/* 			     G_CALLBACK(so_select_by_oc), */
/* 			     so); */

	  /*  *****  */

	  
	  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			 event->button, event->time);
	  
	  g_signal_stop_emission_by_name(clist, 
				       "button_press_event");
	  return(TRUE);
     }

     return(FALSE);
}

static void chase_toggled(GtkToggleButton *togglebutton,
			  struct so_windata *so)
{
     gboolean st = gtk_toggle_button_get_active(togglebutton);
     gtk_widget_set_sensitive(so->max, st);
}

static void create_search_options_window(GqTab *tab)
{
     GtkWidget *window, *vbox0, *vbox1, *frame, *radio, 
	  *radio_chase, *radio_show;
     GtkWidget *table, *label, *entry, *bbox, *button, *list, *w;
     GtkWidget *notebook;
     GSList *radio_group;
     struct so_windata *so;
     GtkTooltips *tips;

     struct server_schema *ss = NULL;

     g_assert(tab);

     if(current_search_options_window) {
	  gdk_beep(); /* Is this OK, philosophically? */
	  gtk_window_present(GTK_WINDOW(current_search_options_window));
	  return;
     }

     tips = gtk_tooltips_new();

     window = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
					  "search-options", 300, 350);
     g_assert(tab->win && tab->win->mainwin);
     gtk_window_set_modal(GTK_WINDOW(window), TRUE);
     gtk_window_set_transient_for(GTK_WINDOW(window),
				  GTK_WINDOW(tab->win->mainwin));

     gtk_widget_realize(window);

     so = g_malloc0(sizeof(struct so_windata));
     gtk_object_set_data_full(GTK_OBJECT(window), "so",
			      so, (GtkDestroyNotify) free_so_windata);

     so->tab = tab;
     so->win = window;

     g_signal_connect(window, "destroy",
			G_CALLBACK(search_options_destroyed), so);
     g_signal_connect(window, "key_press_event",
			G_CALLBACK(close_on_esc),
			window);

     current_search_options_window = window;
     gtk_window_set_title(GTK_WINDOW(window), _("Search Options"));
     gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);

     vbox0 = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox0), 
				CONTAINER_BORDER_WIDTH);
     gtk_widget_show(vbox0);
     gtk_container_add(GTK_CONTAINER(window), vbox0);

     so->notebook = notebook = gtk_notebook_new();
     gtk_widget_show(notebook);
     gtk_box_pack_start(GTK_BOX(vbox0), notebook, TRUE, TRUE, 5);

     /* General Tab */
     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_container_border_width(GTK_CONTAINER(vbox1), 
				CONTAINER_BORDER_WIDTH);

     label = gq_label_new(_("_General"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox1, label);

     /* Referrals Frame */
     frame = gtk_frame_new(_("Referrals"));
     gtk_widget_show(frame);
     gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, TRUE, 5);

     table = gtk_table_new(2, 3, FALSE);
     gtk_widget_show(table);

     gtk_container_add(GTK_CONTAINER(frame), table);

     radio_chase = gq_radio_button_new_with_label(NULL, _("C_hase"));
     so->chase_ref = radio_chase;

/*      gtk_box_pack_start(GTK_BOX(vbox2), radio, TRUE, TRUE, 0); */
     gtk_table_attach(GTK_TABLE(table), radio_chase,
		      0, 1,
		      0, 1, 
		      0, 0,
		      0, 0);
     gtk_widget_show(radio_chase);
     g_signal_connect(radio_chase, "toggled", 
			G_CALLBACK(chase_toggled),
			so);

     radio_group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio_chase));
     radio_show = gq_radio_button_new_with_label(radio_group, _("_Show"));
     so->show_ref = radio_show;
/*      gtk_box_pack_start(GTK_BOX(vbox2), radio, TRUE, TRUE, 0); */
     gtk_table_attach(GTK_TABLE(table), radio_show,
		      1, 2, 
		      0, 1, 
		      0, 0,
		      0, 0);
     gtk_widget_show(radio_show);

     /* Max. Depth label & spin button */

     label = gq_label_new(_("Max. _Depth"));
     gtk_table_attach(GTK_TABLE(table), label,
		      0, 1, 
		      1, 2, 
		      0, 0,
		      0, 0);
     gtk_widget_show(label);

     entry = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(GQ_TAB_SEARCH(tab)->max_depth,
								   0.0, 999.0,
								   1.0, 5.0,
								   1.0)),
				 1.0, 0);
     so->max = entry;

     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_chase), 
				  GQ_TAB_SEARCH(tab)->chase_ref);
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio_show),
				  !(GQ_TAB_SEARCH(tab)->chase_ref));

     gtk_table_attach(GTK_TABLE(table), entry,
		      1, 2, 
		      1, 2, 
		      GTK_SHRINK, GTK_SHRINK,
		      2, 0);
     gtk_widget_show(entry);

     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

     /* Search Scope Frame */
     frame = gtk_frame_new(_("Search scope"));
     gtk_widget_show(frame);
     gtk_box_pack_start(GTK_BOX(vbox1), frame, FALSE, TRUE, 5);

     table = gtk_table_new(2, 3, FALSE);
     gtk_widget_show(table);

     gtk_container_add(GTK_CONTAINER(frame), table);

     radio = gq_radio_button_new_with_label(NULL, _("_1 level"));
     so->onelevel = radio;

     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio),
				  GQ_TAB_SEARCH(tab)->scope == LDAP_SCOPE_ONELEVEL
				  );

/*      gtk_box_pack_start(GTK_BOX(vbox2), radio, TRUE, TRUE, 0); */
     gtk_table_attach(GTK_TABLE(table), radio,
		      0, 1,
		      0, 1, 
		      0, 0,
		      0, 0);
     gtk_widget_show(radio);

     radio_group = gtk_radio_button_group(GTK_RADIO_BUTTON(radio));
     radio = gq_radio_button_new_with_label(radio_group, _("Sub_tree"));
     so->subtree = radio;

     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radio),
				  GQ_TAB_SEARCH(tab)->scope != LDAP_SCOPE_ONELEVEL
				  );

/*      gtk_box_pack_start(GTK_BOX(vbox2), radio, TRUE, TRUE, 0); */
     gtk_table_attach(GTK_TABLE(table), radio,
		      1, 2, 
		      0, 1, 
		      0, 0,
		      0, 0);
     gtk_widget_show(radio);

     /* well, they aren't */

/*      label = gtk_label_new(_("The search options are global search settings. They are common to all search tabs")); */
     
/*      gtk_label_set_line_wrap(GTK_LABEL(label), TRUE); */
/*      gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, FALSE, 0); */
/*      gtk_widget_show(label); */

     /* attributes Tab */
     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_container_border_width(GTK_CONTAINER(vbox1), 
				CONTAINER_BORDER_WIDTH);

     label = gq_label_new(_("_Attributes"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox1, label);

     w = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(w);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(w),
                                    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
     gtk_box_pack_start(GTK_BOX(vbox1), w, TRUE, TRUE, 0);

     so->list = list = gtk_clist_new(1);
     gtk_widget_show(list);

     g_signal_connect(list, "button_press_event",
			G_CALLBACK(so_button_press_on_tree_item),
			so);

     gtk_container_add(GTK_CONTAINER(w), list);
     gtk_clist_set_selection_mode(GTK_CLIST(list),
				  GTK_SELECTION_MULTIPLE);

     gtk_tooltips_set_tip(tips, list,
			  _("The attributes to show in the search result. Selecting NO attributs will show ALL."),
			  Q_("tooltip|")
			  );

     /* fill list with attributes from schema */
     {
	  GqServer *server;
	  char *cur_servername;
	  int opt;
	  GtkWidget *servcombo = GQ_TAB_SEARCH(tab)->serverlist_combo;

	  cur_servername = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(servcombo)->entry), 0, -1);
	  server = gq_server_list_get_by_name(gq_server_list_get(), cur_servername);
	  g_free(cur_servername);

	  if (server) {
	       int context = error_new_context(_("Creating search option window"), tab->win->mainwin);

	       so->ss = ss = get_server_schema(context, server);
	       so->server = g_object_ref(server);

	       error_flush(context);
	  }
	  if (ss) {
	       GList *I;
	       int i;
	       char *t[2] = { NULL, NULL };
	       int row;

	       LDAPAttributeType *at;
	       
	       for ( I = ss->at ; I ; I = g_list_next(I) ) {
		    at = (LDAPAttributeType *) I->data;

		    if(at->at_names && at->at_names[0]) {
			 for(i = 0; at->at_names[i]; i++) {
			      t[0] = (char*) at->at_names[i];
			      row = gtk_clist_append(GTK_CLIST(list), t);
			      if (g_list_find_custom(GQ_TAB_SEARCH(tab)->attrs,
						     t[0], 
						     (GCompareFunc) strcasecmp)) { /*UTF-8?*/
				   gtk_clist_select_row(GTK_CLIST(list), 
							row, 0);
			      }
			 }
		    }
	       }
	  }
	  opt = gtk_clist_optimal_column_width(GTK_CLIST(list), 0);
	  gtk_clist_set_column_width(GTK_CLIST(list), 0, opt);
     }

     /* Global Buttons */

     bbox = gtk_hbutton_box_new();
     gtk_widget_show(bbox);

     gtk_box_pack_end(GTK_BOX(vbox0), bbox, FALSE, FALSE, 0);

     button = gtk_button_new_from_stock(GTK_STOCK_OK);
     gtk_widget_show(button);
     g_signal_connect(button, "clicked",
			G_CALLBACK(so_okbutton_clicked),
			so);
     gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, TRUE, 10);
     GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
     gtk_widget_grab_default(button);

     button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
     gtk_widget_show(button);
     g_signal_connect_swapped(button, "clicked",
			       G_CALLBACK(gtk_widget_destroy),
			       window);

     gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, TRUE, 10);

     gtk_notebook_set_page(GTK_NOTEBOOK(notebook),
			   GQ_TAB_SEARCH(tab)->last_options_tab);

     gtk_widget_show(window);
}



static void optbutton_clicked_callback(GqTab *tab)
{
     create_search_options_window(tab);
}


GqTab *new_searchmode()
{
     GtkWidget *main_clist, *searchmode_vbox, *hbox1, *scrwin;
     GtkWidget *searchcombo, *servcombo, *searchbase_combo;
     GtkWidget *findbutton, *optbutton;
     GList *searchhist;
     GqTabSearch *modeinfo;
     GqTab *tab = g_object_new(GQ_TYPE_TAB_SEARCH, NULL);
     const GList *last_attr;

     tab->type = SEARCH_MODE;

     modeinfo = GQ_TAB_SEARCH(tab);

     modeinfo->scope     = state_value_get_int(lastoptions, "scope",
					       LDAP_SCOPE_SUBTREE);
     modeinfo->chase_ref = state_value_get_int(lastoptions, "chase", 1);
     modeinfo->max_depth = state_value_get_int(lastoptions, "max-depth", 7);
     modeinfo->last_options_tab =
	  state_value_get_int(lastoptions, "last-options-tab", 0);

     /* copy attribute list */
     last_attr = state_value_get_list(lastoptions, "attributes");

     modeinfo->attrs = free_list_of_strings(modeinfo->attrs);
     modeinfo->attrs = copy_list_of_strings(last_attr);

     /* setup widgets */

     searchmode_vbox = gtk_vbox_new(FALSE, 0);
     hbox1 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(searchmode_vbox), hbox1, FALSE, FALSE, 3);

     /* Initially the list is empty. It might be changed to the data
	read from the persistant storage later on*/
     searchhist = modeinfo->history;

     /* searchterm combo box */
     GQ_TAB_SEARCH(tab)->search_combo = searchcombo = gtk_combo_new();
     gtk_combo_disable_activate(GTK_COMBO(searchcombo));
     if(searchhist)
	  gtk_combo_set_popdown_strings(GTK_COMBO(searchcombo), searchhist);
     gtk_widget_show(searchcombo);
     GTK_WIDGET_SET_FLAGS(GTK_COMBO(searchcombo)->entry, GTK_CAN_FOCUS);
     GTK_WIDGET_SET_FLAGS(GTK_COMBO(searchcombo)->entry, GTK_RECEIVES_DEFAULT);
     gtk_box_pack_start(GTK_BOX(hbox1), searchcombo,
			TRUE, TRUE, SEARCHBOX_PADDING);
     g_signal_connect_swapped(GTK_COMBO(searchcombo)->entry, "activate",
			       G_CALLBACK(findbutton_clicked_callback),
			       tab);
     tab->focus = GTK_COMBO(searchcombo)->entry;

     /* LDAP server combo box */
     servcombo = gtk_combo_new();
     fill_serverlist_combo(servcombo);
     gtk_box_pack_start(GTK_BOX(hbox1), servcombo,
			FALSE, TRUE, SEARCHBOX_PADDING);
     gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(servcombo)->entry), FALSE);
     g_signal_connect_swapped(GTK_COMBO(servcombo)->entry, "changed",
			       G_CALLBACK(servername_changed_callback),
			       tab);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(GTK_COMBO(servcombo)->entry), GTK_CAN_FOCUS);
     GTK_WIDGET_UNSET_FLAGS(GTK_BUTTON(GTK_COMBO(servcombo)->button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(servcombo);
     modeinfo->serverlist_combo = servcombo;

     /* search base combo box */
     searchbase_combo = gtk_combo_new();

     if(gq_server_list_n_servers(gq_server_list_get())) {
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(searchbase_combo)->entry),
			     gq_server_list_get_server(gq_server_list_get(), 0)->basedn);
     }

     gtk_box_pack_start(GTK_BOX(hbox1), searchbase_combo,
			FALSE, TRUE, SEARCHBOX_PADDING);
     g_signal_connect(GTK_COMBO(searchbase_combo)->button, "button_press_event",
			G_CALLBACK(searchbase_button_clicked), tab);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_ENTRY(GTK_COMBO(searchbase_combo)->entry), GTK_CAN_FOCUS);
     GTK_WIDGET_UNSET_FLAGS(GTK_BUTTON(GTK_COMBO(searchbase_combo)->button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(searchbase_combo);
     modeinfo->searchbase_combo = searchbase_combo;

     /* find button */
     findbutton = gq_button_new_with_label(_("_Find"));
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(findbutton, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(findbutton);
     gtk_box_pack_start(GTK_BOX(hbox1), findbutton, 
			FALSE, TRUE, SEARCHBOX_PADDING);
     gtk_container_border_width(GTK_CONTAINER (findbutton), 0);
     g_signal_connect_swapped(findbutton, "clicked",
			       G_CALLBACK(findbutton_clicked_callback),
			       tab);

     /* Options button */
     optbutton = gq_button_new_with_label(_("_Options"));
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(optbutton, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(optbutton);
     gtk_box_pack_start(GTK_BOX(hbox1), optbutton,
			FALSE, TRUE, SEARCHBOX_PADDING);
     gtk_container_border_width(GTK_CONTAINER (optbutton), 0);
     g_signal_connect_swapped(optbutton, "clicked",
			       G_CALLBACK(optbutton_clicked_callback),
			       tab);


     /* dummy clist, gets replaced on first search */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
     main_clist = gtk_clist_new(1);
     gtk_clist_set_selection_mode(GTK_CLIST(main_clist),
				  GTK_SELECTION_EXTENDED);
     gtk_clist_set_column_title(GTK_CLIST(main_clist), 0, "");
     gtk_clist_column_titles_show(GTK_CLIST(main_clist));
     gtk_widget_show(main_clist);
     gtk_clist_column_titles_passive(GTK_CLIST(main_clist));
     modeinfo->main_clist = main_clist;

     gtk_container_add(GTK_CONTAINER(scrwin), main_clist);
     gtk_box_pack_start(GTK_BOX(searchmode_vbox), scrwin, TRUE, TRUE, 0);

     gtk_widget_show(searchmode_vbox);

     g_signal_connect_swapped(searchmode_vbox, "destroy",
			      G_CALLBACK(g_object_unref), tab);

     tab->content = searchmode_vbox;
     gtk_object_set_data(GTK_OBJECT(tab->content), "tab", tab);
     return tab;
}


static void servername_changed_callback(GqTab *tab)
{
     GList *searchbase_list;
     GtkWidget *servcombo, *searchbase_combo;
     GqServer *cur_server;
     char *cur_servername;

     servcombo = GQ_TAB_SEARCH(tab)->serverlist_combo;
     cur_servername = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(servcombo)->entry), 0, -1);
     cur_server = gq_server_list_get_by_name(gq_server_list_get(), cur_servername);
     g_free(cur_servername);

     /* make sure searchbase gets refreshed next time the searchbase combo button is
	pressed. Just insert the server's default base DN for now */
     GQ_TAB_SEARCH(tab)->populated_searchbase = 0;
     searchbase_combo = GQ_TAB_SEARCH(tab)->searchbase_combo;
     if (cur_server) {
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(searchbase_combo)->entry), cur_server->basedn);
	  searchbase_list = g_list_append(NULL, cur_server->basedn);
	  gtk_combo_set_popdown_strings(GTK_COMBO(searchbase_combo), searchbase_list);
	  g_list_free(searchbase_list);
     }

}


static gint searchbase_button_clicked(GtkWidget *widget, 
				      GdkEventButton *event, GqTab *tab)
{
     GList *searchbase_list;
     GList *suffixes_list, *temp;
     GtkWidget *servcombo, *searchbase_combo;
     GqServer *server;
     int found_default_searchbase;
     char *cur_servername;
     int error_context;

     found_default_searchbase = 0;

     if (!GQ_TAB_SEARCH(tab)->populated_searchbase && event->button == 1) {
	  servcombo = GQ_TAB_SEARCH(tab)->serverlist_combo;
	  cur_servername = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(servcombo)->entry), 0, -1);
	  server = gq_server_list_get_by_name(gq_server_list_get(), cur_servername);
	  g_free(cur_servername);
	  if (!server)
	       return(FALSE);

	  error_context = error_new_context(_("Looking for search-bases"),
					    widget);

	  searchbase_list = NULL;
 	  suffixes_list = get_suffixes(error_context, server);
    
	  temp = suffixes_list;
	  while (temp) {
	       if (g_strcasecmp(server->basedn, temp->data) == 0)
		    found_default_searchbase = 1;
	       searchbase_list = g_list_append(searchbase_list, temp->data);
      	       temp = g_list_next(temp);
	  }

	  if (found_default_searchbase == 0)
	       searchbase_list = g_list_prepend(searchbase_list, server->basedn);

	  searchbase_combo = GQ_TAB_SEARCH(tab)->searchbase_combo;
	  gtk_combo_set_popdown_strings(GTK_COMBO(searchbase_combo), searchbase_list);

	  temp = suffixes_list;
	  while (temp) {
	       g_free(temp->data);
	       temp = g_list_next(temp);
	  }
	  if (suffixes_list)
	       g_list_free(suffixes_list);
	  g_list_free(searchbase_list);

	  GQ_TAB_SEARCH(tab)->populated_searchbase = 1;

	  error_flush(error_context);
     }

     return FALSE;
}


void findbutton_clicked_callback(GqTab *tab)
{
     GtkWidget *focusbox;

     set_busycursor();
     query(tab);
     set_normalcursor();

     focusbox = tab->focus;
     gtk_widget_grab_focus(focusbox);
     gtk_editable_select_region(GTK_EDITABLE(focusbox), 0, -1);

}

static int column_by_attr(struct attrs **attrlist, const char *attribute)
{
     struct attrs *attr;

     attr = find_attr(*attrlist, attribute);

     if(!attr)
	  return(new_attr(attrlist, attribute));
     else
	  return(attr->column);
}


static int new_attr(struct attrs **attrlist, const char *attr)
{
     struct attrs *n_attr, *alist;
     int cnt;

     n_attr = g_malloc(sizeof(struct attrs));
     n_attr->name = g_strdup(attr);
     n_attr->next = NULL;

     cnt = 0;
     if(*attrlist) {
	  cnt++;
	  alist = *attrlist;
	  while(alist->next) {
	       cnt++;
	       alist = alist->next;
	  }
	  alist->next = n_attr;
     }
     else
	  *attrlist = n_attr;

     n_attr->column = cnt;

     return(cnt);
}


static struct attrs *find_attr(struct attrs *attrlist, const char *attribute)
{

     while(attrlist) {
	  if(!strcasecmp(attrlist->name, attribute))
	       return(attrlist);
	  attrlist = attrlist->next;
     }

     return(NULL);
}


static void
free_attrlist(struct attrs *attrlist)
{
	g_free(attrlist->name);
	attrlist->name = NULL;

	// FIXME: Try to do this iteratively
	if(G_LIKELY(attrlist->next)) {
		free_attrlist(attrlist->next);
		attrlist->next = NULL;
	}

	g_free(attrlist);
}


char *make_filter(GqServer *server, char *querystring)
{
     char *filter = NULL;
     int l = strlen(querystring);

     if(querystring[0] == '(') {  /* UTF-8 OK */
	  filter = g_strdup(querystring);
     }
     else if(g_utf8_strchr(querystring, -1, '=')) {
	  l += 3;
	  filter = g_malloc(l);
	  g_snprintf(filter, l, "(%s)", querystring);
     }
     else {
	  int sl = strlen(server->searchattr);
	  l += sl + 10;
	  filter = g_malloc(l + 1);

	  switch(config->search_argument) {
	  case SEARCHARG_BEGINS_WITH:
	       g_snprintf(filter, l, "(%s=%s*)", server->searchattr, querystring);
	       break;
	  case SEARCHARG_ENDS_WITH:
	       g_snprintf(filter, l, "(%s=*%s)", server->searchattr, querystring);
	       break;
	  case SEARCHARG_CONTAINS:
	       g_snprintf(filter, l, "(%s=*%s*)", server->searchattr, querystring);
	       break;
	  case SEARCHARG_EQUALS:
	       g_snprintf(filter, l, "(%s=%s)", server->searchattr, querystring);
	       break;
	  default:
	       filter[0] = 0;
	       break;
	  };
     }

     return(filter);
}


static void add_to_search_history(GqTab *tab)
{
     gchar *searchterm;
     GList *list, *last, *item;

     list = GQ_TAB_SEARCH(tab)->history;
     searchterm = gtk_editable_get_chars(GTK_EDITABLE(tab->focus), 0, -1);
     if(list && !strcmp(list->data, searchterm)) {
	  g_free(searchterm);
     }
     else {
	  /* if this searchterm is already in history, delete it first */
	  item = g_list_find_custom(list, searchterm, (GCompareFunc) strcasecmp);
	  if(item) {
	       g_free(item->data);
	       list = g_list_remove(list, item->data);
	  }

	  list = g_list_insert(list, searchterm, 0);
	  if(g_list_length(list) > MAX_SEARCH_HISTORY_LENGTH) {
	       last = g_list_last(list);
	       g_free(last->data);
	       list = g_list_remove(list, last->data);
	  }
	  GQ_TAB_SEARCH(tab)->history = list;
	  gtk_combo_set_popdown_strings(GTK_COMBO(tab->focus->parent), list);
     }

}

static int fill_one_row(int query_context,
			GqServer *server,
			LDAP *ld, LDAPMessage *e,
			GtkWidget *clist,
			GString **tolist,
			int *columns_done,
			struct attrs *attrlist,
			GqTab *tab)
{
     BerElement *berptr;
     int i;
     gchar *cl[MAX_NUM_ATTRIBUTES];
     int cur_col;
     int row;
     char *dn, *attr, **vals;
     struct dn_on_server *set;

     /* not every attribute necessarily comes back for
      * every entry, so clear this every time */
     for(i = 0; i < MAX_NUM_ATTRIBUTES; i++) {
	  cl[i] = NULL;
	  g_string_truncate(tolist[i], 0);
     }
     
     dn = ldap_get_dn(ld, e);
     /* store for later reference */
     set = new_dn_on_server(dn, server);

     if(config->showdn) {
	  g_string_append(tolist[0], dn);
	  cl[0] = tolist[0]->str;
     }
#if defined(HAVE_LDAP_MEMFREE)
     ldap_memfree(dn);
#else
     free(dn);
#endif
     
     for(attr = ldap_first_attribute(ld, e, &berptr); attr != NULL;
	 attr = ldap_next_attribute(ld, e, berptr)) {
	  if (!show_in_search(query_context, server, attr)) {
	       ldap_memfree(attr);
	       continue;
	  }
	  
	  /* This should now work for ;binary as well */
	  cur_col = column_by_attr(&attrlist, attr);
	  if(cur_col == MAX_NUM_ATTRIBUTES) {
	       ldap_memfree(attr);
	       break;
	  }

	  if(!columns_done[cur_col]) {
	       char *c = attr_strip(attr);
	       gtk_clist_set_column_title(GTK_CLIST(clist), cur_col, c);
	       /* setting the width somehow causes my gtk2 to not show
		  the title correctly - BUG */
/* 	       gtk_clist_set_column_width(GTK_CLIST(clist), cur_col, 120); */
	       gtk_clist_set_column_resizeable(GTK_CLIST(clist), cur_col,
					       TRUE);
	       if (c) g_free(c);
	       columns_done[cur_col] = 1;
	  }
	  
	  vals = ldap_get_values(ld, e, attr);
	  if(vals) {
	       for(i = 0; vals[i] != NULL; i++) {
		    if(i == 0) {
			 g_string_assign(tolist[cur_col], vals[i]);
		    } else {
			 g_string_append(tolist[cur_col], " ");
			 g_string_append(tolist[cur_col], vals[i]);
		    }
	       }
	       ldap_value_free(vals);
	       if (g_utf8_validate(tolist[cur_col]->str, -1, NULL)) {
		    cl[cur_col] = tolist[cur_col]->str;
	       } else {
		    cl[cur_col] = "";
	       }
	  }
	  ldap_memfree(attr);
     }
#ifndef HAVE_OPENLDAP12
     if(berptr)
	  ber_free(berptr, 0);
#endif


     for(i = MAX_NUM_ATTRIBUTES ; i >= 0 ; i--) {
	  if (cl[i]) {
	       for ( ; i >= 0 ; i-- ) {
		    if (!cl[i]) {
			 cl[i] = "";
		    }
	       }
	       break;
	  }
     }
     
     /* insert row into result window */
     row = gtk_clist_append(GTK_CLIST(clist), cl);
     gtk_clist_set_row_data_full(GTK_CLIST(clist), row, set, 
				 (GtkDestroyNotify) free_dn_on_server);

     gtk_clist_column_titles_show(GTK_CLIST(clist));

     return row;
}


struct chasing {
     GqServer *server;
     char *base;
};

static struct chasing *new_chasing(GqServer *server,
				   const char *base)
{
	struct chasing *n = g_malloc0(sizeof(struct chasing));
	n->base = g_strdup(base);
	if(server) {
		n->server = g_object_ref(server);
	} else {
		n->server = NULL;
	}
	return n;
}

static void
free_chasing(struct chasing *ch)
{
	if(ch->server) {
		g_object_unref(ch->server);
		ch->server = NULL;
	}
	g_free(ch->base);
	g_free(ch);
}


static void add_referral(int error_context, 
			 GqServer *server, 
			 const char *referral, GList **nextlevel) 
{
     LDAPURLDesc *desc = NULL;

     if (ldap_url_parse(referral, &desc) == 0) {
/* 	  GString *new_uri = g_string_sized_new(strlen(referral)); */
	  GqServer *newserver;

	  newserver = get_referral_server(error_context, server, referral);

/* 	  g_string_sprintf(new_uri, "%s://%s:%d/",  */
/* 			   desc->lud_scheme, */
/* 			   desc->lud_host, */
/* 			   desc->lud_port); */

/* 	  newserver = new_ldapserver(); */

/* 	  /\* some sensible settings for the "usual" case: */
/* 	     Anonymous bind. Also show referrals *\/ */
/* 	  newserver->ask_pw   = 0; */
/* 	  newserver->show_ref = 1; */

/* #warning "ADD CONFIG FOR EXTENDED REFERENCE CHASING" */
/* 	  /\* check: do we have this server around already??? *\/ */
/* 	  s = server_by_canon_name(new_uri->str, TRUE); */

/* 	  if (!s) { */
/* 	       s = server; */
/* 	  } */
					     
/* 	  if (s) { */
/* 	       copy_ldapserver(newserver, s); */
/* 	       statusbar_msg(_("Initialized temporary server-definition '%1$s' from existing server '%2$s'"), new_uri->str, server->name); */
/* 	  } else { */
/* 	       statusbar_msg(_("Created temporary server-definition '%1$s' with no pre-set values."), new_uri->str); */
/* 	  } */

/* 	  g_free_and_dup(newserver->name,     new_uri->str); */
/* 	  g_free_and_dup(newserver->ldaphost, new_uri->str); */
/* 	  g_free_and_dup(newserver->basedn,   desc->lud_dn); */

	  newserver->quiet = 1;

	  canonicalize_ldapserver(newserver);

	  transient_add_server(newserver);

	  *nextlevel = g_list_append(*nextlevel, 
				     new_chasing(newserver, desc->lud_dn));

	  ldap_free_urldesc(desc);
/* 	  g_string_free(new_uri, TRUE); */
     }
}

struct list_click_info {
     int last_col;
     int last_type;
};


/* static gint result_compare_func(GtkCList *clist, */
/* 				gconstpointer ptr1, */
/* 				gconstpointer ptr2) */
/* { */

/*      gtk_clist_get_sort_column(clist); */


/* } */




static void click_column(GtkCList *clist,
			 gint column,
			 struct list_click_info *lci)
{
     gtk_clist_set_sort_column(clist, column);
     if (lci->last_col != column) {
	  lci->last_type = GTK_SORT_ASCENDING;
     } else {
	  lci->last_type = (lci->last_type == GTK_SORT_ASCENDING) ? GTK_SORT_DESCENDING : GTK_SORT_ASCENDING;
     }
     lci->last_col = column;

     gtk_clist_set_sort_type(clist, lci->last_type);
     gtk_clist_sort(clist);

}

static void query(GqTab *tab)
{
     LDAP *ld;
     LDAPMessage *res, *e;
     GtkWidget *main_clist, *new_main_clist, *scrwin, *focusbox;
     GtkWidget *servcombo, *searchbase_combo;
     GqServer *server;
     gchar *cur_servername, *cur_searchbase, *enc_searchbase, *querystring;
     GString *tolist[MAX_NUM_ATTRIBUTES];
     char *filter, *searchterm;
     int msg, rc, i, row, l;
     int oc_col, columns_done[MAX_NUM_ATTRIBUTES];
     int want_oc = 1;
     struct attrs *attrlist;
     LDAPControl c;
     LDAPControl *ctrls[2] = { NULL, NULL } ;
     char *base;
     struct list_click_info *lci;
     const char **attrs = NULL;

     GList *thislevel = NULL, *nextlevel = NULL;
     int level = 0;
     struct chasing *ch = NULL;
     int query_context;

     if(GQ_TAB_SEARCH(tab)->search_lock)
	  return;

     GQ_TAB_SEARCH(tab)->search_lock = 1;

     query_context = error_new_context(_("Searching"), tab->win->mainwin);

     focusbox = tab->focus;
     searchterm = gtk_editable_get_chars(GTK_EDITABLE(focusbox), 0, -1);
     querystring = encoded_string(searchterm);
     g_free(searchterm);

     if(querystring[0] == 0) {
	  error_push(query_context, _("Please enter a valid search filter"));
	  goto done;
     }

     for(i = 0; i < MAX_NUM_ATTRIBUTES; i++)
	  columns_done[i] = 0;

     servcombo = GQ_TAB_SEARCH(tab)->serverlist_combo;
     cur_servername =
	  gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(servcombo)->entry),
				 0, -1);
     server = gq_server_list_get_by_name(gq_server_list_get(), cur_servername);
     if(!server) {
	  error_push(query_context, 
		     _("Oops! Server '%s' not found!"), cur_servername);
	  g_free(cur_servername);
	  goto done;
     }
     g_free(cur_servername);

     filter = make_filter(server, querystring);
     free(querystring);

     statusbar_msg(_("Searching for %s"), filter);

     searchbase_combo = GQ_TAB_SEARCH(tab)->searchbase_combo;
     cur_searchbase = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(searchbase_combo)->entry), 0, -1);

     enc_searchbase = encoded_string(cur_searchbase);
     g_free(cur_searchbase);

     /* setup GUI - build new clist */
     new_main_clist = gtk_clist_new(MAX_NUM_ATTRIBUTES);
     gtk_clist_set_selection_mode(GTK_CLIST(new_main_clist),
				  GTK_SELECTION_EXTENDED);

     GTK_CLIST(new_main_clist)->button_actions[2] = GTK_BUTTON_SELECTS;
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CLIST(new_main_clist), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(new_main_clist);
     gtk_clist_column_titles_show(GTK_CLIST(new_main_clist));
     gtk_clist_set_row_height(GTK_CLIST(new_main_clist), 0);

     g_signal_connect(new_main_clist, "select_row",
                        G_CALLBACK(select_entry_callback),
                        tab);
/*       g_signal_connect(new_main_clist, "unselect_row", */
/*                          G_CALLBACK(unselect_entry_callback), */
/*                          tab); */
     g_signal_connect(new_main_clist, "button_press_event",
			G_CALLBACK(search_button_press_on_tree_item),
			tab);

     lci = g_malloc0(sizeof(struct list_click_info));
     lci->last_col = -1;

     gtk_object_set_data_full(GTK_OBJECT(new_main_clist), "lci", lci, g_free);
     g_signal_connect(new_main_clist,
			"click-column",
			G_CALLBACK(click_column),
			lci);

     main_clist = GQ_TAB_SEARCH(tab)->main_clist;
     gtk_clist_clear(GTK_CLIST(main_clist));
     scrwin = main_clist->parent;
     gtk_widget_destroy(main_clist);
     GQ_TAB_SEARCH(tab)->main_clist = new_main_clist;

     gtk_container_add(GTK_CONTAINER(scrwin), new_main_clist);

     /* prepare attrs list for searches */
     l = g_list_length(GQ_TAB_SEARCH(tab)->attrs);
     
     want_oc = 1;
     if (l) {
	  /* fill attrs with pointers belonging to the
	     list. The attrs[i] MUST NOT be free'd */
	  const GList *I;
	  
	  want_oc = 0;
	  attrs = g_malloc0(sizeof(const char *) * (l + 1));
	  for ( i = 0, I = GQ_TAB_SEARCH(tab)->attrs ; 
		I ; 
		i++, I = g_list_next(I)) {
	       attrs[i] = I->data;
	       if (strcasecmp(attrs[i], "objectclass") == 0) {
		    want_oc = 1;
	       }
	  }
     }

     attrlist = NULL;

     /* reserve columns 0 & 1 for DN and objectClass, respectively */
     if(config->showdn) {
	  column_by_attr(&attrlist, "DN");
	  gtk_clist_set_column_title(GTK_CLIST(new_main_clist), 0, "DN");
	  gtk_clist_set_column_width(GTK_CLIST(new_main_clist), 0, 260);
	  gtk_clist_set_column_resizeable(GTK_CLIST(new_main_clist), 0, TRUE);
	  columns_done[0] = 1;
     }

     if (want_oc) {
	  oc_col = column_by_attr(&attrlist, "objectClass");
	  gtk_clist_set_column_title(GTK_CLIST(new_main_clist), oc_col,
				     "objectClass");
	  gtk_clist_set_column_width(GTK_CLIST(new_main_clist), oc_col, 120);
	  columns_done[oc_col] = 1;

	  gtk_clist_set_column_resizeable(GTK_CLIST(new_main_clist),
					  oc_col, TRUE);

/*	  gtk_clist_set_column_visibility(GTK_CLIST(new_main_clist), */
/*					  oc_col, 0); */
     }

     for(i = 0; i < MAX_NUM_ATTRIBUTES; i++) {
	  tolist[i] = g_string_new("");
     }

     /* prepare ManageDSAit in case we should show referrals */
     c.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     c.ldctl_value.bv_val	= NULL;
     c.ldctl_value.bv_len	= 0;
     c.ldctl_iscritical	= 1;

     ctrls[0] = &c;

     thislevel = NULL;
     nextlevel = NULL;
     level = 0;

     ch = new_chasing(server, enc_searchbase);
     thislevel = g_list_append(thislevel, ch);

     if (enc_searchbase) free(enc_searchbase);

     row = 0;

     gtk_clist_freeze(GTK_CLIST(new_main_clist));

     /* do the searching */
     set_busycursor();

     while (thislevel || nextlevel) {
	  if (thislevel == NULL) {
	       level++;
	       thislevel = nextlevel;
	       nextlevel = NULL;
	  }
	  if (level > GQ_TAB_SEARCH(tab)->max_depth) {
	       statusbar_msg(_("Reached maximum recursion depth"));

	       g_list_foreach(thislevel, (GFunc) free_chasing, NULL);
	       g_list_free(thislevel);
	       break;
	  }

	  ch = thislevel->data;
	  thislevel = g_list_remove(thislevel, ch);

	  server = ch->server;
	  base = ch->base;

	  if( (ld = open_connection(query_context, server)) != NULL) {
	       statusbar_msg(_("Searching on server '%1$s' below '%2$s'"), 
			     server->name, base);
	       rc = ldap_search_ext(ld, base, 
				    GQ_TAB_SEARCH(tab)->scope,
				    filter,
				    (char **)attrs,	/* attrs & API bug*/
				    0,			/* attrsonly */
				    GQ_TAB_SEARCH(tab)->chase_ref ? NULL : ctrls,
/* 				    server->show_ref ? ctrls : NULL, */
				    /* serverctrls */
				    NULL,		/* clientctrls */
				    NULL,		/* timeout */
				    LDAP_NO_LIMIT,	/* sizelimit */
				    &msg);

	       if(rc == -1) {
		    server->server_down++;
		    error_push(query_context,
			       _("Error searching below '%1$s': %2$s"), 
			       enc_searchbase, ldap_err2string(rc));
/* 		    close_connection(server, FALSE); */
/* 		    GQ_TAB_SEARCH(tab)->search_lock = 0; */
/* 		    return; */
		    goto cont;
	       }
	  
	       for (rc = 1 ; rc ; ) {
		    int code = ldap_result(ld, msg, 0, NULL, &res);
		    if (code == -1) {
			 /* error */
			 error_push(query_context,
				    _("Unspecified error searching below '%1$s'"),
				    enc_searchbase);
			 break;
		    }
		    for( e = ldap_first_message(ld, res) ; e != NULL ;
			 e = ldap_next_message(ld, e) ) {
			 switch (ldap_msgtype(e)) {
			 case LDAP_RES_SEARCH_ENTRY:
			      fill_one_row(query_context,
					   server, ld, e, new_main_clist,
					   tolist,
					   columns_done,
					   attrlist,
					   tab);
			      row++;

			      rc = 1;
			      break; /* OK */
			 case LDAP_RES_SEARCH_REFERENCE: {
			      char **refs = NULL;
			      rc = ldap_parse_reference(ld, e, &refs, NULL, 0);

			      if (rc == LDAP_SUCCESS) {
				   for (i = 0 ; refs[i] ; i++) {
					add_referral(query_context,
						     server,
						     refs[i], &nextlevel);
				   }
				   rc = 1;
			      }
			      ldap_value_free(refs);
			      break;
			 }
			 case LDAP_RES_SEARCH_RESULT:
			      rc = 0;
			      break;
			 default:
			      rc = 0;
			      break;
			 }
		    }
		    if (res) ldap_msgfree(res);
	       }
	  cont:
	       close_connection(server, FALSE);
	  } else {
	       /* ld == NULL */
	  }

	  free_chasing(ch);
     }

     set_normalcursor();

     if (attrs) g_free(attrs);
     free(filter);

     for(i = 0; i < MAX_NUM_ATTRIBUTES; i++) {
	  g_string_free(tolist[i], TRUE);
     }
/*      gtk_clist_thaw(GTK_CLIST(new_main_clist)); */


     if(row > 0) {
/*	  statusbar_msg("%s", ldap_err2string(msg)); */
/*      else { */
	  statusbar_msg(ngettext("One entry found", "%d entries found", row),
			row);
     }

/*      gtk_clist_freeze(GTK_CLIST(new_main_clist)); */
     gtk_clist_column_titles_active(GTK_CLIST(new_main_clist));

     for (i = 0 ; gtk_clist_get_column_widget(GTK_CLIST(new_main_clist), i) ;
	  i++ ) {
	  int opt = gtk_clist_optimal_column_width(GTK_CLIST(new_main_clist), i);
	  if (opt < 40) {
	       opt = 40;
	  }
	  if (opt > 150) {
	       opt = 150;
	  }
	  gtk_clist_set_column_width(GTK_CLIST(new_main_clist), i, opt);
     }

     gtk_clist_thaw(GTK_CLIST(new_main_clist));

     free_attrlist(attrlist);
     add_to_search_history(tab);

 done:
     error_flush(query_context);
     GQ_TAB_SEARCH(tab)->search_lock = 0;
}

static void results_popup_menu(GqTab *tab, GdkEventButton *event,
			       struct dn_on_server *set)
{
     GtkWidget *root_menu, *menu, *menu_item, *label;
     GtkWidget *submenu;
     int transient = is_transient_server(set->server);
     char **exploded_dn = NULL, *name;
     GList *selection;
     int have_sel;

     /* this is a hack to pass the selected set under the menu to the callbacks.
	Each callback MUST clear this after use! */
     GQ_TAB_SEARCH(tab)->set = set;

     root_menu = gtk_menu_item_new_with_label("Root");
     gtk_widget_show(root_menu);

     menu = gtk_menu_new();
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(root_menu), menu);

     exploded_dn = gq_ldap_explode_dn(set->dn, FALSE);

     if (exploded_dn) {
	  name = exploded_dn[0];
     } else {
	  name = set->dn;
     }

     label = gtk_menu_item_new_with_label(name);
     gtk_widget_set_sensitive(label, FALSE);
     gtk_widget_show(label);
     
     gtk_menu_append(GTK_MENU(menu), label);
     gtk_menu_set_title(GTK_MENU(menu), name);

     if (exploded_dn) gq_exploded_free(exploded_dn);

     menu_item = gtk_separator_menu_item_new(); 
     gtk_menu_append(GTK_MENU(menu), menu_item);
     gtk_widget_set_sensitive(menu_item, FALSE);
     gtk_widget_show(menu_item);

     /* Selection submenu */
     menu_item = gtk_menu_item_new_with_label(_("Selection"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     submenu = gtk_menu_new();
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
     gtk_widget_show(menu_item);

     /* Check if several entries it should be sensitive */
     selection = GTK_CLIST(GQ_TAB_SEARCH(tab)->main_clist)->selection;
     have_sel = (selection && g_list_length(selection) > 0);

     /* Select All */
     menu_item = gtk_menu_item_new_with_label(_("Select All"));
     gtk_menu_append(GTK_MENU(submenu), menu_item);
     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(gtk_clist_select_all),
			       GQ_TAB_SEARCH(tab)->main_clist);
     gtk_widget_show(menu_item);
			      
     /* Unselect All */
     menu_item = gtk_menu_item_new_with_label(_("Unselect All"));
     gtk_menu_append(GTK_MENU(submenu), menu_item);
     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(gtk_clist_unselect_all),
			       GQ_TAB_SEARCH(tab)->main_clist);
     gtk_widget_show(menu_item);
     gtk_widget_set_sensitive(menu_item, have_sel);

     /* separator */
     menu_item = gtk_menu_item_new();
     gtk_menu_append(GTK_MENU(submenu), menu_item);
     gtk_widget_show(menu_item);

     /* Export to LDIF*/
     menu_item = gtk_menu_item_new_with_label(_("Export to LDIF"));
     gtk_menu_append(GTK_MENU(submenu), menu_item);
     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(export_search_selected_entry),
			       tab);
     gtk_widget_show(menu_item);
     gtk_widget_set_sensitive(menu_item, have_sel);

     /* Add to Browser*/
     menu_item = gtk_menu_item_new_with_label(_("Add to Browser"));
     gtk_menu_append(GTK_MENU(submenu), menu_item);
     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(add_selected_to_browser),
			       tab);
     gtk_widget_show(menu_item);
     gtk_widget_set_sensitive(menu_item, have_sel);

     /* separator */
     menu_item = gtk_menu_item_new();
     gtk_menu_append(GTK_MENU(submenu), menu_item);
     gtk_widget_show(menu_item);

     /* Delete */
     menu_item = gtk_menu_item_new_with_label(_("Delete"));
     gtk_menu_append(GTK_MENU(submenu), menu_item);
     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(delete_search_selected),
			       tab);
     gtk_widget_show(menu_item);
     gtk_widget_set_sensitive(menu_item, have_sel);

     /*** End of Selected submenu ***/

     /* Edit */
     menu_item = gtk_menu_item_new_with_label(_("Edit"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(search_edit_entry_callback),
			       tab);
     gtk_widget_show(menu_item);

     /* Use as template */
     menu_item = gtk_menu_item_new_with_label(_("Use as template"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect(menu_item, "activate",
			G_CALLBACK(search_new_from_entry_callback),
			tab);
     gtk_widget_show(menu_item);

     /* Find in Browser */
     menu_item = gtk_menu_item_new_with_label(_("Find in browser"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(find_in_browser),
			       tab);
     gtk_widget_show(menu_item);
     gtk_widget_set_sensitive(GTK_WIDGET(menu_item), !transient);

     /* Add all to Browser */
     menu_item = gtk_menu_item_new_with_label(_("Add all to browser"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(add_all_to_browser),
			       tab);
     gtk_widget_show(menu_item);
     gtk_widget_set_sensitive(GTK_WIDGET(menu_item), !transient);

     /* separator */
     menu_item = gtk_menu_item_new();
     gtk_menu_append(GTK_MENU(menu), menu_item);
     gtk_widget_show(menu_item);

     /* Delete */
     menu_item = gtk_menu_item_new_with_label(_("Delete"));
     gtk_menu_append(GTK_MENU(menu), menu_item);
     g_signal_connect_swapped(menu_item, "activate",
			       G_CALLBACK(delete_search_entry),
			       tab);
     gtk_widget_show(menu_item);



     gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
		    event->button, event->time);

}


void find_in_browser(GqTab *tab)
{
     GQTreeWidget *ctree;
     struct dn_on_server *set;
     GqTab *browsetab;

     set = GQ_TAB_SEARCH(tab)->set;
     GQ_TAB_SEARCH(tab)->set = NULL;
     if(set == NULL || set->dn == NULL || strlen(set->dn) == 0)
	  return;

     /* find last used browser... */

     browsetab = get_last_of_mode(BROWSE_MODE);
     if(browsetab) {
	  int ctx;
	  ctree = GQ_TAB_BROWSE(browsetab)->ctreeroot;
	  ctx = error_new_context(_("Finding entry in browser"),
				  GTK_WIDGET(ctree));
	  show_server_dn(ctx, ctree, set->server, set->dn, TRUE);
	  go_to_page(browsetab);
	  error_flush(ctx);
     } else {
	  single_warning_popup(_("No browser available"));
     }
}


void add_all_to_browser(GqTab *tab)
{
     GQTreeWidget *ctree;
     struct dn_on_server *cur_resultset;
     GqTab *browsetab;
     int i;
     GtkWidget *clist = GQ_TAB_SEARCH(tab)->main_clist;

     /* find last used browser... */
     
     browsetab = get_last_of_mode(BROWSE_MODE);
     if(browsetab) {
	  int ctx;
	  ctree = GQ_TAB_BROWSE(browsetab)->ctreeroot;
	  ctx = error_new_context(_("Adding all to browser"), 
				  GTK_WIDGET(ctree));

	  gtk_clist_freeze(GTK_CLIST(ctree));
	  for(i = 0 ;
	      (cur_resultset = gtk_clist_get_row_data(GTK_CLIST(clist), i)) != NULL ;
	      i++) {
	       show_server_dn(ctx, ctree, 
			      cur_resultset->server, cur_resultset->dn, FALSE);
	  }
	  gtk_clist_thaw(GTK_CLIST(ctree));
	  go_to_page(browsetab);

	  error_flush(ctx);
     } else {
	  single_warning_popup(_("No browser available"));
     }
}

static void add_selected_to_browser(GqTab *tab)
{
     GQTreeWidget *ctree;
     struct dn_on_server *cur_resultset;
     GqTab *browsetab;
     GtkWidget *clist = GQ_TAB_SEARCH(tab)->main_clist;
     GList *sel, *I;

     /* find last used browser... */
     
     browsetab = get_last_of_mode(BROWSE_MODE);
     if(browsetab) {
	  int ctx;
	  ctree = GQ_TAB_BROWSE(browsetab)->ctreeroot;
	  ctx = error_new_context(_("Adding selected entries to browser"), GTK_WIDGET(ctree));

	  gtk_clist_freeze(GTK_CLIST(ctree));

	  sel = GTK_CLIST(clist)->selection;

	  for (I = sel ; I ; I = g_list_next(I)) {
	       cur_resultset = gtk_clist_get_row_data(GTK_CLIST(clist), GPOINTER_TO_INT(I->data));
	       show_server_dn(ctx, 
			      ctree, 
			      cur_resultset->server, cur_resultset->dn, FALSE);
	  }
	  gtk_clist_thaw(GTK_CLIST(ctree));
	  go_to_page(browsetab);

	  error_flush(ctx);
     } else {
	  single_warning_popup(_("No browser available"));
     }
}



static void export_search_selected_entry(GqTab *tab)
{
     struct dn_on_server *cur_resultset;
     GqTab *browsetab;
     GtkWidget *clist = GQ_TAB_SEARCH(tab)->main_clist;
     GList *sel, *I;

     /* find last used browser... */
     
     browsetab = get_last_of_mode(BROWSE_MODE);
     if(browsetab) {
	  GList *to_export = NULL;
	  struct dn_on_server *dos;
	  int error_context;

	  sel = GTK_CLIST(clist)->selection;

	  for (I = sel ; I ; I = g_list_next(I)) {
	       cur_resultset = gtk_clist_get_row_data(GTK_CLIST(clist), GPOINTER_TO_INT(I->data));

	       dos = new_dn_on_server(cur_resultset->dn,
				      cur_resultset->server);
	       to_export = g_list_append(to_export, dos);
	  }
	  error_context = error_new_context(_("Exporting selected entries to LDIF"),
					    tab->win->mainwin);

	  export_many(error_context, tab->win->mainwin, to_export);

	  error_flush(error_context);
     } else {
	  single_warning_popup(_("No browser available"));
     }
}

static void delete_search_selected(GqTab *tab)
{
     struct dn_on_server *set;
     GtkWidget *clist = GQ_TAB_SEARCH(tab)->main_clist;
     GList *sel, *I;

     sel = GTK_CLIST(clist)->selection;
     if (g_list_length(sel) > 0) {
	  int answer = 
	       question_popup(_("Do you really want to delete the selected entries?"),
			      _("Do you really want to delete the selected entries?")
			      );

	  /* FIXME: sort by ldapserver and keep connection open across
	     deletions  */
	  if (answer) {
	       GList *deld = NULL;
	       int ctx = error_new_context(_("Deleting selected entries"), 
					   GTK_WIDGET(clist));

	       for (I = g_list_last(sel) ; I ; I = g_list_previous(I)) {
		    set = gtk_clist_get_row_data(GTK_CLIST(clist),
						 GPOINTER_TO_INT(I->data));
		    if (delete_entry(ctx, set->server, set->dn)) {
			 deld = g_list_append(deld, I->data);
		    }
	       }
	       gtk_clist_freeze(GTK_CLIST(clist));
	       for (I = g_list_first(deld) ; I ; I = g_list_next(I)) {
		    gtk_clist_remove(GTK_CLIST(clist),
				     GPOINTER_TO_INT(I->data));
	       }
	       g_list_free(deld);
	       gtk_clist_thaw(GTK_CLIST(clist));

	       error_flush(ctx);
	  }
     }
}

static void search_new_from_entry_callback(GtkWidget *w, GqTab *tab)
{
     struct dn_on_server *set;
     int error_context;

     set = GQ_TAB_SEARCH(tab)->set;
     GQ_TAB_SEARCH(tab)->set = NULL;
     if(set == NULL || set->dn == NULL)
	  return;
     
     error_context = error_new_context(_("Creating new entry from search result"), w);

     new_from_entry(error_context, set->server, set->dn);

     error_flush(error_context);
}

static void search_edit_entry_callback(GqTab *tab)
{
     struct dn_on_server *set;

     set = GQ_TAB_SEARCH(tab)->set;
     GQ_TAB_SEARCH(tab)->set = NULL;
     if(set == NULL || set->dn == NULL)
	  return;

     edit_entry(set->server, set->dn);
}

static void delete_search_entry(GqTab *tab)
{
     struct dn_on_server *set;
     int ctx;

     set = GQ_TAB_SEARCH(tab)->set;
     GQ_TAB_SEARCH(tab)->set = NULL;
     if(set == NULL || set->dn == NULL)
	  return;

     ctx = error_new_context(_("Deleting entry"), 
			     GQ_TAB_SEARCH(tab)->main_clist);
     
     delete_entry(ctx, set->server, set->dn);

     error_flush(ctx);
}

static int select_entry_callback(GtkWidget *clist, gint row, gint column,
				 GdkEventButton *event, GqTab *tab)
{
     struct dn_on_server *set;

     if(event) {
	  set = gtk_clist_get_row_data(GTK_CLIST(clist), row);

	  if(event->button == 1 && event->type == GDK_2BUTTON_PRESS) {
	       GQ_TAB_SEARCH(tab)->set = set;
	       search_edit_entry_callback(tab);
	  }
     }

     return(TRUE);
}


static gboolean search_button_press_on_tree_item(GtkWidget *clist,
						 GdkEventButton *event,
						 GqTab *tab)
{
     struct dn_on_server *set;
     int rc, row, column;

     if (event->type == GDK_BUTTON_PRESS && event->button == 3
	 && event->window == GTK_CLIST(clist)->clist_window) {
	  rc = gtk_clist_get_selection_info(GTK_CLIST(clist),
					    event->x, event->y, &row, &column);
	  if (rc == 0)
	       return TRUE;

	  set = gtk_clist_get_row_data(GTK_CLIST(clist), row);
	  results_popup_menu(tab, event, set);

	  g_signal_stop_emission_by_name(clist,
				       "button_press_event");
     }

     return FALSE;
}

void fill_out_search(GqTab *tab,
		     GqServer *server,
		     const char *search_base_dn)
{
     GtkCombo *server_combo, *searchbase_combo;

     if (!search_base_dn) {
	  search_base_dn = server->basedn;
     }

     server_combo = (GtkCombo *) GQ_TAB_SEARCH(tab)->serverlist_combo;
     searchbase_combo = (GtkCombo *) GQ_TAB_SEARCH(tab)->searchbase_combo;

     gtk_entry_set_text(GTK_ENTRY(server_combo->entry), server->name);
     gtk_entry_set_text(GTK_ENTRY(searchbase_combo->entry), search_base_dn);

     gtk_editable_set_position(GTK_EDITABLE(server_combo->entry), 0);
     gtk_editable_set_position(GTK_EDITABLE(searchbase_combo->entry), 0);

     /* make tab visible */
     go_to_page(tab);
}

/* GType */
G_DEFINE_TYPE(GqTabSearch, gq_tab_search, GQ_TYPE_TAB);

static void
gq_tab_search_init(GqTabSearch* self) {}

static void
tab_dispose(GObject* object)
{
	GqTabSearch* self = GQ_TAB_SEARCH(object);

	if(self->main_clist) {
		gtk_clist_clear(GTK_CLIST(self->main_clist));
		gtk_widget_destroy(self->main_clist);
		self->main_clist = NULL;
	}

	G_OBJECT_CLASS(gq_tab_search_parent_class)->dispose(object);
}

static void
tab_finalize(GObject* object)
{
	GqTabSearch* self = GQ_TAB_SEARCH(object);

	while(self->history) {
		g_free(self->history->data);
		self->history = g_list_delete_link(self->history, self->history);
	}

	G_OBJECT_CLASS(gq_tab_search_parent_class)->finalize(object);
}


static void
gq_tab_search_class_init(GqTabSearchClass* self_class)
{
	GObjectClass* object_class = G_OBJECT_CLASS(self_class);
	GqTabClass* tab_class = GQ_TAB_CLASS(self_class);

	object_class->dispose  = tab_dispose;
	object_class->finalize = tab_finalize;

	tab_class->save_snapshot    = search_save_snapshot;
	tab_class->restore_snapshot = search_restore_snapshot;
}

