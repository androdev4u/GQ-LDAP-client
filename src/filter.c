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

/* $Id: filter.c 975 2006-09-07 18:44:41Z herzi $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <stdio.h>
#if defined(HAVE_ISWSPACE)
#include <wchar.h>
#include <wctype.h>
#else
#include <ctype.h>
#endif /* HAVE_ISWSPACE */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include "common.h"
#include "gq-server-list.h"
#include "gq-tab-search.h"
#include "util.h"
#include "configfile.h"
#include "search.h"
#include "mainwin.h"
#include "filter.h"
#include "errorchain.h"
#include "debug.h"
#include "input.h"
#include "state.h"

static void copy_existing_filter(GtkCList *filter_clist);
static void name_popup(void);
static void filterlist_row_selected(GtkCList *filter_clist,
				    int row, int column,
				    GdkEventButton *event, gpointer data);
static void filterlist_row_unselected(GtkCList *filter_clist,
				      int row, int column,
				      GdkEventButton *event, gpointer data);
static void remove_from_filtermenu(struct gq_filter *filter);
static void delete_filter(GtkWidget *widget, GtkCList *filter_clist);
static void edit_filter(GtkCList *filter_clist, int is_new_filter, 
			int row, struct gq_filter *filter);

static void save_filter(GtkWidget *window);
static char *indent_filter(char *filter);
static char *unindent_filter(char *indented);
static void indent_toggled(GtkToggleButton *indent, gpointer editbox);

struct gq_filter *new_filter()
{
     struct gq_filter *filter;

     filter = g_malloc0(sizeof(struct gq_filter));

     filter->name	= g_strdup("");
     filter->ldapfilter	= g_strdup("");
     filter->servername	= g_strdup("");
     filter->basedn	= g_strdup("");

     return(filter);
}

void free_filter(struct gq_filter *filter) 
{
     g_free(filter->name);
     g_free(filter->ldapfilter);
     g_free(filter->servername);
     g_free(filter->basedn);
     g_free(filter);
}

void copy_filter(struct gq_filter *target, const struct gq_filter *source)
{
     if (target && source) {
	  g_free_and_dup(target->name, source->name);
	  g_free_and_dup(target->ldapfilter, source->ldapfilter);
	  g_free_and_dup(target->servername, source->servername);
	  g_free_and_dup(target->basedn, source->basedn);
     }
}

static struct gq_filter *check_filtername(const char *filtername)
{
     GList *filterlist;
     struct gq_filter *filter;

     filterlist = config->filters;
     while(filterlist) {
	  filter = (struct gq_filter *) filterlist->data;
	  if(!strncasecmp(filter->name, filtername, MAX_FILTERNAME_LEN - 1)) {
	       return(filter);
	  }
	  filterlist = filterlist->next;
     }

     return(NULL);
}


void add_filter(GtkWidget *filternamebox)
{
     GtkWidget *focusbox, *button;
     GtkWidget *server_combo, *searchbase_combo;
     GList *filterlist;
     struct gq_filter *filter;
     GqServer *server;
     GqTab *tab;
     char *filterstring, *searchstring, *servername, *searchbase, msg[192];
     const char *filtername;

     /* find current tab */
     tab = mainwin_get_current_tab(mainwin.mainbook);

     /* ignore if it's not a search mode tab */
     if(tab->type != SEARCH_MODE)
	  return;

     server_combo = GQ_TAB_SEARCH(tab)->serverlist_combo;
     servername = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(server_combo)->entry), 0, -1);
     if( (server = gq_server_list_get_by_name(gq_server_list_get(), servername)) == NULL) {
	  g_free(servername);
	  return;
     }

     focusbox = tab->focus;
     searchstring = gtk_editable_get_chars(GTK_EDITABLE(focusbox), 0, -1);

     /* ignore if the filter is empty */
     if(searchstring[0] == '\0') {
	  g_free(searchstring);
	  return;
     }

     filterstring = make_filter(server, searchstring);
     g_free(searchstring);

     if(filternamebox == NULL) {
	  name_popup();
     }
     else {

	  /* ignore if no filtername given */
	  filtername = gtk_entry_get_text(GTK_ENTRY(filternamebox));
	  if(filtername == NULL || filtername[0] == '\0') {
	       gtk_widget_destroy(filternamebox->parent->parent);
	       return;
	  }

	  /* check if name already exists */
	  filterlist = config->filters;
	  while(filterlist) {
	       filter = (struct gq_filter *) filterlist->data;
	       if(!strncasecmp(filter->name, filtername, 
			       MAX_FILTERNAME_LEN - 1)) {
		    g_snprintf(msg, sizeof(msg), 
			     _("There is already a filter called '%s'"), filtername);
		    single_warning_popup(msg);
		    return;
	       }
	       filterlist = filterlist->next;
	  }

	  /* populate the new filter */
	  filter = new_filter();
	  g_free_and_dup(filter->name, filtername);
	  g_free_and_dup(filter->ldapfilter, filterstring);
	  free(filterstring);

	  /* check state of Remember... button, and get servername/basedn if active */
	  button = gtk_object_get_data(GTK_OBJECT(filternamebox), "saveserver");
	  if(GTK_TOGGLE_BUTTON(button)->active) {
	       searchbase_combo = GQ_TAB_SEARCH(tab)->searchbase_combo;
	       searchbase = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(searchbase_combo)->entry), 0, -1);

	       g_free(filter->servername);
	       filter->servername = servername;

	       g_free(filter->basedn);
	       filter->basedn = searchbase;
	  }

	  /* and add it in */
	  config->filters = g_list_append(config->filters, filter);


	  /* and add it to the Filters menu */
	  mainwin_update_filter_menu(&mainwin);

	  if (save_config(filternamebox)) {
	       /* destroy name popup (filternamebox | vbox | window) */
	       gtk_widget_destroy(filternamebox->parent->parent);
	  } else {
	       /* undo changes to config ! */
	       config->filters = g_list_remove(config->filters, filter);
	       free_filter(filter);
	  }
     }
}


static void name_popup(void)
{
     GtkWidget *window, *vbox1, *vbox2, *hbox0, *label, *filternamebox, *button;

     window = gtk_dialog_new();
/*      gtk_container_border_width(GTK_CONTAINER(window), CONTAINER_BORDER_WIDTH); */
     gtk_window_set_title(GTK_WINDOW(window), _("Filter name"));
     gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);

     g_signal_connect_swapped(window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               window);

     vbox1 = GTK_DIALOG(window)->vbox;
     gtk_widget_show(vbox1);

     gtk_container_border_width(GTK_CONTAINER(vbox1), CONTAINER_BORDER_WIDTH);

     label = gq_label_new(_("Filter _name"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(vbox1), label, TRUE, TRUE, 0);

     filternamebox = gtk_entry_new();
     GTK_WIDGET_SET_FLAGS(filternamebox, GTK_CAN_FOCUS);
     GTK_WIDGET_SET_FLAGS(filternamebox, GTK_CAN_DEFAULT);
     gtk_widget_show(filternamebox);
     g_signal_connect_swapped(filternamebox, "activate",
			       G_CALLBACK(add_filter),
			       GTK_OBJECT(filternamebox));
     gtk_box_pack_start(GTK_BOX(vbox1), filternamebox, TRUE, TRUE, 0);

     /* Save server and base DN */
     button = gq_check_button_new_with_label(_("Remember server and base DN"));
     gtk_object_set_data(GTK_OBJECT(filternamebox), "saveserver", button);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, TRUE, 10);

     vbox2 = GTK_DIALOG(window)->action_area;
     gtk_widget_show(vbox2);

     gtk_container_border_width(GTK_CONTAINER(vbox2), CONTAINER_BORDER_WIDTH);

     hbox0 = gtk_hbutton_box_new();
     gtk_widget_show(hbox0);
     gtk_box_pack_start(GTK_BOX(vbox2), hbox0, TRUE, TRUE, 0);

     button = gtk_button_new_from_stock(GTK_STOCK_OK);

     g_signal_connect_swapped(button, "clicked",
                               G_CALLBACK(add_filter),
			       GTK_OBJECT(filternamebox));

     gtk_box_pack_start(GTK_BOX(hbox0), button, FALSE, FALSE, 0);
     GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
     GTK_WIDGET_SET_FLAGS(button, GTK_RECEIVES_DEFAULT);
     gtk_widget_grab_default(button);
     gtk_widget_show(button);


     button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

     g_signal_connect_swapped(button, "clicked",
                               G_CALLBACK(gtk_widget_destroy),
			       GTK_OBJECT(window));

     gtk_box_pack_start(GTK_BOX(hbox0), button, FALSE, FALSE, 0);
     gtk_widget_show(button);

     gtk_widget_grab_focus(filternamebox);

     gtk_widget_show(window);

}


void filter_selected(struct gq_filter *filter)
{
     GtkWidget *focusbox, *server_combo, *searchbase_combo;
     GqTab *tab;

     /* find current tab */
     tab = mainwin_get_current_tab(mainwin.mainbook);

     /* we're in luck if the current tab is a Search tab: just use this one */

     /* if the current tab isn't a Search tab, find the last used on  */
     if(tab->type != SEARCH_MODE) {
	  tab = get_last_of_mode(SEARCH_MODE);
	  go_to_page(tab);
     }

     /* no search-tab - cannot use filter */
     if (tab == NULL) {
	  /* FIXME: pop up error using an error_context */
	  return;
     }

     /* paste filter into searchterm box */
     focusbox = tab->focus;
     if(!focusbox)
	  return;
     gtk_entry_set_text(GTK_ENTRY(focusbox), filter->ldapfilter);

     /* set server combo to this filter's server */
     if(filter->servername[0]) {
	  server_combo = GQ_TAB_SEARCH(tab)->serverlist_combo;
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(server_combo)->entry),
			     filter->servername);
     }

     /* set searchbase combo to this filter's base DN */
     if(filter->basedn[0]) {
	  searchbase_combo = GQ_TAB_SEARCH(tab)->searchbase_combo;
	  gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(searchbase_combo)->entry),
			     filter->basedn);
     }

     /* uhhh, what a hack */
     g_signal_emit_by_name(focusbox, "activate");
}


void show_filters(void)
{
     GtkWidget *window, *vbox1, *hbox0, *hbox1, *hbox2, *new_button, *delete_button;
     GtkWidget *copy_button, *ok_button, *scrwin, *filter_clist;
     GList *filterlist;
     struct gq_filter *filter;
     int i, row;
     char *titles[] = { _("Filter name"), _("Server"),
			_("Base DN"), _("Filter") };
     char *clist_content[5];

     window = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
				      "filterlist", 670, 350);

     gtk_container_border_width(GTK_CONTAINER(window), CONTAINER_BORDER_WIDTH);
     gtk_window_set_title(GTK_WINDOW(window), _("Filters"));

     g_signal_connect_swapped(window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               window);

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_container_add(GTK_CONTAINER(window), vbox1);

     hbox0 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox0);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox0, FALSE, FALSE, 0);

     hbox1 = gtk_hbutton_box_new();
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(hbox0), hbox1, FALSE, FALSE, 0);

     new_button = gtk_button_new_from_stock(GTK_STOCK_NEW);

     GTK_WIDGET_UNSET_FLAGS(new_button, GTK_CAN_DEFAULT);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(new_button, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(new_button);
     gtk_box_pack_start(GTK_BOX(hbox1), new_button, FALSE, FALSE, 0);

     delete_button = gtk_button_new_from_stock(GTK_STOCK_DELETE);

     GTK_WIDGET_UNSET_FLAGS(delete_button, GTK_CAN_DEFAULT);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(delete_button, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(delete_button);
     gtk_box_pack_start(GTK_BOX(hbox1), delete_button, FALSE, FALSE, 5);

     copy_button = gtk_button_new_from_stock(GTK_STOCK_COPY);

     GTK_WIDGET_UNSET_FLAGS(copy_button, GTK_CAN_DEFAULT);
     GTK_WIDGET_UNSET_FLAGS(copy_button, GTK_CAN_FOCUS);
     gtk_widget_show(copy_button);
     gtk_box_pack_start(GTK_BOX(hbox1), copy_button, FALSE, FALSE, 0);

     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
                                    GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
     gtk_box_pack_start(GTK_BOX(vbox1), scrwin, TRUE, TRUE, 5);

     filter_clist = gtk_clist_new_with_titles(4, titles);
     gtk_object_set_data(GTK_OBJECT(filter_clist), "selected_row", GINT_TO_POINTER(-1));
     gtk_widget_show(filter_clist);
     gtk_container_add(GTK_CONTAINER(scrwin), filter_clist);
     gtk_clist_column_titles_passive(GTK_CLIST(filter_clist));
     g_signal_connect_swapped(filter_clist, "select_row",
			       G_CALLBACK(filterlist_row_selected), filter_clist);
     g_signal_connect_swapped(filter_clist, "unselect_row",
			       G_CALLBACK(filterlist_row_unselected), filter_clist);
     for(i = 0; i < 3; i++) {
	  gtk_clist_set_column_width(GTK_CLIST(filter_clist), i, 100);
     }

     gtk_clist_freeze(GTK_CLIST(filter_clist));
     filterlist = config->filters;
     while(filterlist) {
	  filter = (struct gq_filter *) filterlist->data;
	  clist_content[0] = filter->name;
	  clist_content[1] = filter->servername;
	  clist_content[2] = filter->basedn;
	  clist_content[3] = filter->ldapfilter;
	  clist_content[4] = "";
	  row = gtk_clist_append(GTK_CLIST(filter_clist), clist_content);
	  gtk_clist_set_row_data(GTK_CLIST(filter_clist), row, filter);

	  filterlist = filterlist->next;
     }
     gtk_clist_thaw(GTK_CLIST(filter_clist));

     hbox2 = gtk_hbutton_box_new(); /* (FALSE, 20); */
     gtk_widget_show(hbox2);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox2, FALSE, FALSE, 5);

     ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);

     gtk_widget_show(ok_button);
     gtk_box_pack_start(GTK_BOX(hbox2), ok_button, FALSE, FALSE, 0);
     GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
     GTK_WIDGET_SET_FLAGS(ok_button, GTK_RECEIVES_DEFAULT);
     gtk_widget_grab_default(ok_button);
     g_signal_connect_swapped(ok_button, "clicked",
                               G_CALLBACK(gtk_widget_destroy),
                               window);

     /* can only pass the clist to new/delete/copy buttons after it was created */
     g_signal_connect_swapped(new_button, "clicked",
			       G_CALLBACK(add_new_filter_callback), filter_clist);

     g_signal_connect(delete_button, "clicked",
			G_CALLBACK(delete_filter),
			filter_clist);

     g_signal_connect_swapped(copy_button, "clicked",
			       G_CALLBACK(copy_existing_filter),
			       filter_clist);

     gtk_widget_show(window);

     statusbar_msg(_("Filter list window opened."));
}


static void filterlist_row_selected(GtkCList *filter_clist,
				    int row, int column,
				    GdkEventButton *event, gpointer data)
{
     struct gq_filter *filter;

     if(event && event->button == 1) {
	  if(event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) {
	       /* single button click on a row */
	       gtk_object_set_data(GTK_OBJECT(filter_clist), "selected_row", GINT_TO_POINTER(row));
	  }
	  else if(event->type == GDK_2BUTTON_PRESS) {
	       /* double-click also selects the row */
	       gtk_object_set_data(GTK_OBJECT(filter_clist), "selected_row", GINT_TO_POINTER(row));

	       /* double-click on first mouse button */
	       filter = (struct gq_filter *) gtk_clist_get_row_data(filter_clist, row);
	       if(filter)
		    edit_filter(filter_clist, 0, row, filter);
	  }
     }

}


static void filterlist_row_unselected(GtkCList *filter_clist,
				      int row, int column,
				      GdkEventButton *event, gpointer data)
{

     gtk_object_set_data(GTK_OBJECT(filter_clist), "selected_row", GINT_TO_POINTER(-1));

}


void add_new_filter_callback(GtkCList *filter_clist)
{

     edit_filter(filter_clist, 1, 0, NULL);

}


static void delete_filter(GtkWidget *widget, GtkCList *filter_clist)
{
     int selected_row;
     struct gq_filter *filter;

     selected_row = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(filter_clist), "selected_row"));
     if(selected_row == -1)
	  /* nothing selected */
	  return;

     filter = gtk_clist_get_row_data(filter_clist, selected_row);
     /* delete from internal filterlist */
     config->filters = g_list_remove(config->filters, filter);

     if (save_config(widget)) {
	  /* delete from filterlist window */
	  gtk_clist_remove(filter_clist, selected_row);
	  
	  /* delete from menu */
	  remove_from_filtermenu(filter);
	  
	  free_filter(filter);
     } else {
	  /* save failed - re-insert into filter list */
	  config->filters = g_list_insert(config->filters, filter, 
					  selected_row);
     }
}


static void copy_existing_filter(GtkCList *filter_clist)
{
     int selected_row;
     struct gq_filter *filter;

     selected_row = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(filter_clist), "selected_row"));
     if(selected_row == -1)
	  /* nothing selected */
	  return;

     filter = gtk_clist_get_row_data(filter_clist, selected_row);
     if(filter)
	  edit_filter(filter_clist, 1, selected_row, filter);

}


static void remove_from_filtermenu(struct gq_filter *filter)
{
     GtkWidget *menuitem;
     GList *menuitems;
     struct gq_filter *found_filter;

     menuitems = gtk_container_children(GTK_CONTAINER(mainwin.filtermenu));

     while(menuitems) {
	  menuitem = menuitems->data;
	  found_filter = gtk_object_get_data(GTK_OBJECT(menuitem), "filter");
	  if(filter == found_filter) {
	       gtk_container_remove(GTK_CONTAINER(mainwin.filtermenu), menuitem);
	       break;
	  }

	  menuitems = menuitems->next;
     }

}


static void edit_filter(GtkCList *filter_clist, int is_new_filter, 
			int row, struct gq_filter *filter)
{
     GtkWidget *scrolled;
     GtkWidget *window, *vbox1, *hbox1, *hbox2, *label, *entry;
     GtkWidget *table1, *indent, *editbox, *save, *cancel;
     char *indented;

     window = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
				      "editfilter", 400, 350); 

     if(!is_new_filter)
	  gtk_object_set_data(GTK_OBJECT(window), "filter", filter);
     gtk_object_set_data(GTK_OBJECT(window), "filter_clist", filter_clist);
     gtk_object_set_data(GTK_OBJECT(window), "filter_row", GINT_TO_POINTER(row));

     gtk_container_border_width(GTK_CONTAINER(window), CONTAINER_BORDER_WIDTH);
     gtk_window_set_title(GTK_WINDOW(window), _("Edit filter"));

     g_signal_connect_swapped(window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               window);

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_container_add(GTK_CONTAINER(window), vbox1);

     table1 = gtk_table_new(3, 2, FALSE);
     gtk_widget_show(table1);
     gtk_box_pack_start(GTK_BOX(vbox1), table1, FALSE, FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(table1), 0);
     gtk_table_set_row_spacings(GTK_TABLE(table1), 5);
     gtk_table_set_col_spacings(GTK_TABLE(table1), 13);

     /* Filter name */
     label = gq_label_new(_("Filter _name"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 0, 1,
		      0, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(window), "filtername_entry", entry);
     gtk_widget_show(entry);
     if(filter && !is_new_filter)
	  gtk_entry_set_text(GTK_ENTRY(entry), filter->name);
     gtk_table_attach(GTK_TABLE(table1), entry, 1, 2, 0, 1,
		      GTK_EXPAND | GTK_FILL, 0, 0, 0);

     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

     /* Server */
     label = gq_label_new(_("_Server"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 1, 2,
		      0, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(window), "server_entry", entry);
     gtk_widget_show(entry);
     if(filter)
	  gtk_entry_set_text(GTK_ENTRY(entry), filter->servername);
     gtk_table_attach(GTK_TABLE(table1), entry, 1, 2, 1, 2,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

     /* Base DN */
     label = gq_label_new(_("_Base DN"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 2, 3,
		      0, GTK_EXPAND | GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     gtk_object_set_data(GTK_OBJECT(window), "basedn_entry", entry);
     gtk_widget_show(entry);
     if(filter)
	  gtk_entry_set_text(GTK_ENTRY(entry), filter->basedn);
     gtk_table_attach(GTK_TABLE(table1), entry, 1, 2, 2, 3,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     hbox1 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);

     indent = gq_toggle_button_new_with_label(_("_Indent"));
     gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(indent), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(indent, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(indent);
     gtk_box_pack_start(GTK_BOX(hbox1), indent, FALSE, FALSE, 0);

     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

     scrolled = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), 
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
     gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
					 GTK_SHADOW_IN);

     editbox = gtk_text_view_new();
     gtk_container_add(GTK_CONTAINER(scrolled), editbox); 

     gtk_object_set_data(GTK_OBJECT(window), "editbox", editbox);
     gtk_text_view_set_editable(GTK_TEXT_VIEW(editbox), TRUE);
     gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(editbox), GTK_WRAP_CHAR);

     /* populate editbox */
     if(filter) {
	  indented = indent_filter(filter->ldapfilter);

	  gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(editbox)),
				   indented, strlen(indented));  /* FIXME: UTF-8 ?? */

	  g_free(indented);
     }

     gtk_box_pack_start(GTK_BOX(vbox1), scrolled, TRUE, TRUE, 3);
     gtk_widget_show(editbox);
     gtk_widget_show(scrolled);

     g_signal_connect(indent, "toggled",
			G_CALLBACK(indent_toggled), editbox);


     hbox2 = gtk_hbutton_box_new(); /* box_new(FALSE, 0); */
     gtk_widget_show(hbox2);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox2, FALSE, FALSE, 0);

     save = gtk_button_new_from_stock(GTK_STOCK_SAVE);

     g_signal_connect_swapped(save, "clicked",
                               G_CALLBACK(save_filter), GTK_OBJECT(window));
     gtk_box_pack_start(GTK_BOX(hbox2), save, FALSE, FALSE, 0);
     GTK_WIDGET_UNSET_FLAGS(save, GTK_CAN_FOCUS);
     GTK_WIDGET_SET_FLAGS(save, GTK_CAN_DEFAULT);
     gtk_widget_grab_default(save);
     gtk_widget_show(save);

     cancel = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

     GTK_WIDGET_UNSET_FLAGS(cancel, GTK_CAN_FOCUS);
     gtk_box_pack_end(GTK_BOX(hbox2), cancel, FALSE, FALSE, 0);
     g_signal_connect_swapped(cancel, "clicked",
                               G_CALLBACK(statusbar_msg),
                               _("Editing filter cancelled."));
     g_signal_connect_swapped(cancel, "clicked",
                               G_CALLBACK(gtk_widget_destroy),
                               window);
     gtk_widget_show(cancel);

     gtk_widget_grab_focus(editbox);

     gtk_widget_show(window);

     if (filter) {
	  statusbar_msg(_("Edit filter window opened for filter '%s'."), 
			filter->name);
     } else {
	  statusbar_msg(_("Edit filter window opened for new filter."));
     }
}


static void save_filter(GtkWidget *window)
{
     GtkWidget *entry, *text, *filter_clist;
     struct gq_filter *filter;
     int row, is_a_new_filter;
     char *clist_content[5];
     const char *filtername, *servername, *basedn;
     char *filtertext, *flattened = NULL;
     int error_context;
     gboolean save_ok = FALSE;

     error_context = error_new_context(_("Saving filter"), window);

     filter = (struct gq_filter *) gtk_object_get_data(GTK_OBJECT(window), "filter");

     /* make sure we can find the filter in the list -- might have been deleted
	while it was being edited, which would cause a segfault here */
     if(filter) {
	  if (g_list_find(config->filters, filter) == NULL) 
	       filter = NULL;
     }

     if(filter == NULL) {
	  filter = new_filter();
	  is_a_new_filter = 1;
     } else {
	  is_a_new_filter = 0;
     }

     entry = gtk_object_get_data(GTK_OBJECT(window), "filtername_entry");
     filtername = gtk_entry_get_text(GTK_ENTRY(entry));

     entry = gtk_object_get_data(GTK_OBJECT(window), "server_entry");
     servername = gtk_entry_get_text(GTK_ENTRY(entry));

     entry = gtk_object_get_data(GTK_OBJECT(window), "basedn_entry");
     basedn = gtk_entry_get_text(GTK_ENTRY(entry));

     text = gtk_object_get_data(GTK_OBJECT(window), "editbox");

     {
	  GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
	  GtkTextIter s, e;
	  
	  gtk_text_buffer_get_start_iter(b, &s);
	  gtk_text_buffer_get_end_iter(b, &e);
	  filtertext = gtk_text_buffer_get_text(b, &s, &e, TRUE);
     }

     filter_clist = gtk_object_get_data(GTK_OBJECT(window), "filter_clist");
     row = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(window), "filter_row"));

     if(!filtername || !filtername[0]) {
	  error_push(error_context,
		     _("You must fill in a name for the filter"));
	  goto done;
     }

     g_assert(filter);
     g_assert(servername);
     g_assert(basedn);
     
     if(!filter || !servername || !basedn ) {
	  /* shouldn't happen */
	  error_push(error_context, _("Unknown error"));
	  goto done;
     }

     flattened = unindent_filter(filtertext);

     if(is_a_new_filter) {
	  if(check_filtername(filtername)) {
	       error_push(error_context, 
			  _("There is already a filter called '%s'"),
			  filtername);
	       goto done;
	  }

	  /* populate internal struct */
	  g_free_and_dup(filter->name, filtername);
	  g_free_and_dup(filter->servername, servername);
	  g_free_and_dup(filter->basedn, basedn);
	  g_free_and_dup(filter->ldapfilter, flattened);

	  /* add the new filter to the internal filter list */
	  config->filters = g_list_append(config->filters, filter);

	  if (save_config_ext(error_context)) {
	       /* there's no filter_clist if we got here through the menu */
	       if(filter_clist) {
		    /* add to filter clist window */
		    clist_content[0] = filter->name;
		    clist_content[1] = filter->servername;
		    clist_content[2] = filter->basedn;
		    clist_content[3] = filter->ldapfilter;
		    clist_content[4] = "";
		    gtk_clist_freeze(GTK_CLIST(filter_clist));
		    row = gtk_clist_append(GTK_CLIST(filter_clist), clist_content);
		    gtk_clist_set_row_data(GTK_CLIST(filter_clist), row, filter);
		    gtk_clist_thaw(GTK_CLIST(filter_clist));
	       }
	       
	       /* add to Filters menu */
	       mainwin_update_filter_menu(&mainwin);

	       save_ok = TRUE;
	       filter = NULL;		/* set to NULL to not free it
					   in the end */
	  } else {
	       /* save was not OK - undo changes */
	       config->filters = g_list_remove(config->filters, filter);
	       goto done;
	  }
     }
     else {
	  struct gq_filter *old_backup = NULL;

	  /* ! is_a_new_filter */
	  if(strcasecmp(filter->name, filtername)) {
	       /* existing filter was renamed */
	       if(check_filtername(filtername)) {
		    error_push(error_context, 
			       _("There is already a filter called '%s'"),
			       filtername);
		    goto done;
	       }
	  }
	  old_backup = new_filter();
	  copy_filter(old_backup, filter);
	  
	  if(strcasecmp(filter->servername, servername)) {
	       g_free_and_dup(filter->servername, servername);
	  }
	  
	  if(strcasecmp(filter->basedn, basedn)) {
	       g_free_and_dup(filter->basedn, basedn);
	  }
	  
	  if(strcasecmp(filter->ldapfilter, flattened)) {
	       g_free_and_dup(filter->ldapfilter, flattened);
	  }
	  
	  /* change filtername in internal struct, and in the filter clist window */
	  g_free_and_dup(filter->name, filtername);
	  
	  if (!save_config_ext(error_context)) {
	       /* undo changes */
	       copy_filter(filter, old_backup);
	  } else {
	       gtk_clist_set_text(GTK_CLIST(filter_clist), row, 1,
				  servername);
	       gtk_clist_set_text(GTK_CLIST(filter_clist), row, 2,
				  basedn);
	       gtk_clist_set_text(GTK_CLIST(filter_clist), row, 3,
				  flattened);

	       /* udpate filter menu in main window */
	       mainwin_update_filter_menu(&mainwin);

	       gtk_clist_set_text(GTK_CLIST(filter_clist), row, 0, filtername);
	       save_ok = TRUE;
	       filter = NULL;		/* set to NULL to not
					   free it */
	  }
	  free_filter(old_backup);
     }

 done:
     g_free(filtertext);
     g_free(flattened);

     if (is_a_new_filter && filter) free_filter(filter);
	  
     error_flush(error_context);

     if (save_ok) {
	  gtk_widget_destroy(window);
     } else {
	  /* changes already undone */
     }
}


static char *indent_filter(char *filter)
{
     unsigned int i, f, s, indent, oslstack[30];
     int osl;
     char c, *indented;

     indent = 0;
     osl = 0;
     f = s = 0;
     indented = g_malloc((strlen(filter) * FILTER_TABSIZE) + 1);
     indented[0] = '\0';
     while( (c = filter[f++]) ) {
	  if(osl < 0) {
	       /* unmatched parens -- just dump everything unindented from now on */
	       indented[s++] = c;
	  }
	  else {
	       switch(c) {
	       case '\\':
		    /* escaping backslash -- print it + the escaped char */
		    indented[s++] = c;
		    indented[s++] = filter[f++];
		    break;
	       case '&':
	       case '|':
	       case '!':
		    indent++;
		    indented[s++] = c;
		    /* last open not on the same line anymore */
		    oslstack[osl] = 0;
		    indented[s++] = '\n';
		    for(i = 0; i < FILTER_TABSIZE * indent; i++)
			 indented[s++] = ' ';
		    break;
	       case '(':
		    /* next open will be on the same line */
		    oslstack[++osl] = 1;
		    indented[s++] = c;
		    break;
	       case ')':
		    if(oslstack[osl--] == 0) {
			 /* matching open was not on the same line */
			 indent--;
			 indented[s++] = '\n';
			 for(i = 0; i < FILTER_TABSIZE * indent; i++)
			      indented[s++] = ' ';
		    }
		    indented[s++] = c;
		    /* peek ahead -- don't set up the next line + indent if it's another close */
		    if(filter[f] != ')') {
			 indented[s++] = '\n';
			 for(i = 0; i < FILTER_TABSIZE * indent; i++)
			      indented[s++] = ' ';
		    }
		    break;
	       default:
		    indented[s++] = c;
		    break;
	       }
	  }
     }
     indented[s] = '\0';

     return(indented);
}


static char *unindent_filter(char *indented)
{
     unsigned int s, f, allow_sp, allow_sp_next;
     unsigned char c;
     gchar* unindented;

     unindented = g_malloc(strlen(indented) + 1);
     unindented[0] = '\0';
     f = s = 0;
     allow_sp = allow_sp_next = 0;
     while( (c = indented[s++]) )
     {
	  /* allow spaces between '=' and ')' */
	  if(c == '=')
	       allow_sp_next = 1;
	  else if(c == ')')
	       allow_sp_next = 0;

	  switch(c) {
	  case '\\':
	       unindented[f++] = c;
	       if(indented[s])
		    unindented[f++] = indented[s++];
	       break;
	  case ' ':
	       if(allow_sp)
		    unindented[f++] = c;
	       break;
	  default:
	       /* caught spaces before -- this is testing for \n etc */
#if defined(HAVE_ISWSPACE)
	       if(!iswspace(btowc(c)))
#else
               if(!isspace(c))
#endif /* HAVE_ISWSPACE */
		    unindented[f++] = c;
	       break;
	  }

	  allow_sp = allow_sp_next;
     }
     unindented[f] = '\0';

     return unindented;
}


static void indent_toggled(GtkToggleButton *indent, gpointer editbox)
{
     char *current, *newtext;

     GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(editbox));
     GtkTextIter s, e;

     gtk_text_buffer_get_start_iter(b, &s);
     gtk_text_buffer_get_end_iter(b, &e);
     current = gtk_text_buffer_get_text(b, &s, &e, TRUE);

     if(!current) return;

     if(gtk_toggle_button_get_active(indent)) {
	  /* indent */
	  newtext = indent_filter(current);
     } else {
	  /* unindent */
	  newtext = unindent_filter(current);
     }

     gtk_text_buffer_set_text(b, newtext, strlen(newtext));  /* FIXME: UTF-8 ?? */

     g_free(newtext);
     g_free(current);

}

/*
   Local Variables:
   c-basic-offset: 5
   End:
*/
