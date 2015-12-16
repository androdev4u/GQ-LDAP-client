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

/* $Id: mainwin.c 975 2006-09-07 18:44:41Z herzi $ */

#include <string.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include "common.h"
#include "gq-server-list.h"
#include "gq-tab-browse.h"
#ifdef HAVE_LDAP_STR2OBJECTCLASS
#    include "gq-tab-schema.h"
#endif
#include "gq-tab-search.h"

#include "mainwin.h"
#include "configfile.h"
#include "prefs.h"
#include "util.h"
#include "template.h"
#include "filter.h"
#include "COPYING.h"
#include "debug.h"
#include "input.h"
#include "state.h"
#include "errorchain.h"
#include "progress.h"

struct mainwin_data mainwin;

static void create_about_window(GtkWindow* parent);

static void close_current_tab(struct mainwin_data *win);
static void mainwin_destroyed(GtkWidget *widget, struct mainwin_data *win);
static void switchpage_refocus(GtkNotebook *notebook, GtkNotebookPage *page,
			       int pagenum, struct mainwin_data *win);

GqTab *get_last_of_mode(int mode)
{
     if(!mainwin.lastofmode)
	  return NULL;

     return g_hash_table_lookup(mainwin.lastofmode, GINT_TO_POINTER(mode));
}


void go_to_page(GqTab *tab)
{
     gtk_notebook_set_page(GTK_NOTEBOOK(mainwin.mainbook),
			   gtk_notebook_page_num(GTK_NOTEBOOK(mainwin.mainbook),
						 tab->content));
}


void enter_last_of_mode(GqTab *tab)
{

     if(!mainwin.lastofmode)
	  mainwin.lastofmode = g_hash_table_new(g_direct_hash, g_direct_equal);

     g_hash_table_insert(mainwin.lastofmode, GINT_TO_POINTER(tab->type), tab);

}

static void
append_name(GQServerList* list, GqServer* server, gpointer data) {
	GList** serverlist = data;
	*serverlist = g_list_append(*serverlist, server->name);
}

void fill_serverlist_combo(GtkWidget *combo)
{
     GList *serverlist = NULL;
     GqServer *server;

     if(combo == NULL)
	  return;

     gq_server_list_foreach(gq_server_list_get(), append_name, &serverlist);

     if(!serverlist)
	  /* all servers were deleted -- pass an empty string to the combo */
	  serverlist = g_list_append(serverlist, "");

     gtk_combo_set_popdown_strings(GTK_COMBO(combo), serverlist);

     g_list_free(serverlist);
}

static gboolean mainwin_restore_snapshot(struct mainwin_data *win)
{
     int i, type;
     char tmp[32];
     struct pbar_win *pw = NULL;

     if (!config->restore_tabs) return FALSE;
     if (!exists_entity("mainwin.tabs")) return FALSE;

     pw = create_progress_bar_in_window(_("Restoring last GUI state"));
     update_progress(pw, _("Restoring tabs"));

     for (i = 0 ; ; i++) {
	  g_snprintf(tmp, sizeof(tmp), "mainwin.tabs.%d", i);
	  if (!exists_entity(tmp)) break;
	  type = state_value_get_int(tmp, "type", -1);
	  if (type > 0) {
	       GqTab *tab = new_modetab(win, type);
	       int error_ctx = error_new_context("", pw->win);

	       if (GQ_TAB_GET_CLASS(tab)->restore_snapshot) {
		    GQ_TAB_GET_CLASS(tab)->restore_snapshot(error_ctx, tmp, tab, pw);
	       }

	       error_flush(error_ctx);
	  }
	  update_progress(pw, NULL);
	  if (pw->cancelled) break;
     }

     if (i > 0) {
	  type = state_value_get_int("mainwin.tabs", "active", -1);
	  gtk_notebook_set_page(GTK_NOTEBOOK(win->mainbook), type);
     }

     update_progress(pw, _("Restoring tabs"));
     free_progress(pw);

     return i > 0;
}

static void mainwin_save_snapshot(struct mainwin_data *win)
{
     GqTab *tab = NULL;
     int i;
     char tmp[32];

     rm_value("mainwin.tabs");

     if (!config->restore_tabs) return;

     for( i = 0 ; (tab = mainwin_get_tab_nth(win, i)) != NULL ; i++) {
	  g_snprintf(tmp, sizeof(tmp), "mainwin.tabs.%d", i);
	  state_value_set_int(tmp, "type", tab->type);
	  if (GQ_TAB_GET_CLASS(tab)->save_snapshot) {
	       int error_ctx =
		    error_new_context(_("Saving main window snapshot"), NULL);
	       GQ_TAB_GET_CLASS(tab)->save_snapshot(error_ctx, tmp, tab);
	       error_flush(error_ctx);
	  }
     }

     state_value_set_int("mainwin.tabs", "active",
			 gtk_notebook_get_current_page(GTK_NOTEBOOK(win->mainbook)));
}


/* gtk2 checked (multiple destroy callbacks safety), confidence 0.7:
   cleanup_all_tabs semantics? */
static void mainwin_destroyed(GtkWidget *widget, struct mainwin_data *win)
{
     mainwin_save_snapshot(win);
     cleanup(win);
     gtk_main_quit();
}

void cleanup(struct mainwin_data *win)
{
     cleanup_all_tabs(win);
}

static gboolean ctrl_b_hack(GtkWidget *widget, GdkEventKey *event, gpointer obj)
{
     if(event && event->type == GDK_KEY_PRESS &&
	event->state & GDK_CONTROL_MASK && event->keyval == GDK_b) {
	  g_signal_emit_by_name(obj, "activate");
	  g_signal_stop_emission_by_name(widget, "key_press_event");
	  return(TRUE);
     }

     return(FALSE);
}


static gboolean ctrl_w_hack(GtkWidget *widget, GdkEventKey *event, gpointer obj)
{
     if(event && event->type == GDK_KEY_PRESS &&
	event->state & GDK_CONTROL_MASK && event->keyval == GDK_w) {
	  g_signal_emit_by_name(obj, "activate");
	  g_signal_stop_emission_by_name(widget, "key_press_event");
	  return(TRUE);
     }

     return(FALSE);
}

static void new_modetab_search(struct mainwin_data *win) 
{
     new_modetab(win, SEARCH_MODE);
}

static void new_modetab_browse(struct mainwin_data *win) 
{
     new_modetab(win, BROWSE_MODE);
}

static void new_modetab_schema(struct mainwin_data *win) 
{
     new_modetab(win, SCHEMA_MODE);
}


static GList *log_list = NULL;
static int log_list_len = 0;

void clear_message_history()
{
     if (log_list) {
	  g_list_foreach(log_list, (GFunc) g_free, NULL);
	  g_list_free(log_list);
	  log_list = NULL;
     }

     if (mainwin.ml_text) {
	  GtkTextIter start;
	  GtkTextIter end;
	  
	  gtk_text_buffer_get_start_iter(mainwin.ml_buffer, &start);
	  gtk_text_buffer_get_end_iter(mainwin.ml_buffer, &end);

	  gtk_text_buffer_delete(mainwin.ml_buffer, &start, &end);
     }
}


void message_log_append(const char *buf)
{
     log_list = g_list_append(log_list, g_strdup(buf));
     log_list_len++;

     if (mainwin.ml_text) {
	  GtkTextIter iter;
	  gtk_text_buffer_get_end_iter(mainwin.ml_buffer, &iter);
	  gtk_text_buffer_insert(mainwin.ml_buffer, &iter,
				 buf, strlen(buf));
	  gtk_text_buffer_insert(mainwin.ml_buffer, &iter, "\n", 1);

	  gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(mainwin.ml_text),
				       gtk_text_buffer_create_mark(mainwin.ml_buffer,
								   NULL,
								   &iter,
								   FALSE),
				       0.0,
				       FALSE,
				       0.0, 0.0);
     }
     while (log_list_len > MESSAGE_LOG_MAX) {
	  g_free(log_list->data);
	  log_list = g_list_remove(log_list, log_list->data);
	  log_list_len--;
     }
}

static void message_log_destroyed(GtkWidget *window,
				  struct mainwin_data *win)
{
     win->ml_window = NULL;
     win->ml_text   = NULL;
     win->ml_buffer = NULL;
}

static void clear_clicked(void)
{
     clear_message_history();
}

static void message_log(struct mainwin_data *win) 
{
     GtkWidget *window, *vbox0, *scrwin, *text, *bbox, *button;
     GtkTextBuffer *buffer;
     GtkTextIter iter;
     GList *I;

     g_assert(win);

     if (win->ml_window) {
	  gdk_beep(); /* Is this OK, philosophically? */
	  gtk_window_present(GTK_WINDOW(win->ml_window));
	  return;
     }

     window = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
				      "statusbar-log", 500, 350);
     win->ml_window = window;

     gtk_widget_realize(window);

     g_signal_connect(window, "destroy",
			G_CALLBACK(message_log_destroyed), win);

     g_signal_connect(window, "key_press_event",
			G_CALLBACK(close_on_esc),
			window);

/*      current_search_options_window = window; */
     gtk_window_set_title(GTK_WINDOW(window), _("Message Log"));
     gtk_window_set_policy(GTK_WINDOW(window), TRUE, TRUE, FALSE);

     vbox0 = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox0), 
				CONTAINER_BORDER_WIDTH);
     gtk_widget_show(vbox0);
     gtk_container_add(GTK_CONTAINER(window), vbox0);

     /* scrolled window to hold the log */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
     gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrwin),
					 GTK_SHADOW_IN);
     gtk_box_pack_start(GTK_BOX(vbox0), scrwin, TRUE, TRUE, 0);

     text = gtk_text_view_new();
     buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
     win->ml_buffer = buffer;

     gtk_text_view_set_editable(GTK_TEXT_VIEW(text), FALSE);
     gtk_text_buffer_get_end_iter(buffer, &iter);

     win->ml_text = text;

     gtk_widget_show(text);
     gtk_container_add(GTK_CONTAINER(scrwin), text); 

     for (I = log_list ; I ; I = g_list_next(I) ) {
	  gtk_text_buffer_insert(buffer, &iter,
				 I->data, strlen(I->data));
	  gtk_text_buffer_insert(buffer, &iter, "\n", 1);
     }

     gtk_text_buffer_get_end_iter(buffer, &iter);
     gtk_text_view_scroll_to_mark(GTK_TEXT_VIEW(text),
				  gtk_text_buffer_create_mark(buffer, NULL,
							      &iter, FALSE),
				  0.0,
				  FALSE,
				  0.0, 0.0);

     bbox = gtk_hbutton_box_new();
     gtk_widget_show(bbox);

     gtk_box_pack_end(GTK_BOX(vbox0), bbox, FALSE, FALSE, 3);

     button = gtk_button_new_from_stock(GTK_STOCK_CLEAR);
     gtk_widget_show(button);
     g_signal_connect(button, "clicked",
			G_CALLBACK(clear_clicked),
			win);
     gtk_box_pack_start(GTK_BOX(bbox), button, FALSE, TRUE, 10);

     button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
     gtk_widget_show(button);
     g_signal_connect_swapped(button, "clicked",
			       G_CALLBACK(gtk_widget_destroy),
			       window);
     gtk_box_pack_end(GTK_BOX(bbox), button, FALSE, TRUE, 10);

     GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
     gtk_widget_grab_default(button);

     gtk_widget_show(window);
}

/* Callback function called when a tab gets removed from the
   notebook. */
static void remove_tab(GtkContainer *notebook,
		       GtkWidget *content,
		       struct mainwin_data *win)
{
     int thismode;
     GqTab *tab = NULL;
     int i;

     tab = gtk_object_get_data(GTK_OBJECT(content), "tab");
     if (tab) {
	  thismode = tab->type;
	  g_hash_table_insert(win->lastofmode, GINT_TO_POINTER(thismode), NULL);

	  /* try to find another tab with the same mode so we can put that
	     one into lastofmode... */
	  for( i = 0 ; (tab = mainwin_get_tab_nth(win, i)) != NULL ; i++) {
	       if (tab->type == thismode) {
		    /* found one! */
		    enter_last_of_mode(tab);
		    break;
	       }
	  }
     }

     if (gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 0) == NULL) {
	  gtk_widget_destroy(win->mainwin);
     }
}


void mainwin_update_filter_menu(struct mainwin_data *win)
{
     GList *menuitems = gtk_container_children(GTK_CONTAINER(win->filtermenu));
     GList *I;

     /* Filters | list of filters */

     if (menuitems) {
	  for ( I = g_list_first(menuitems) ; I ; I = g_list_next(I) ) {
	       GtkWidget *item = GTK_WIDGET(I->data);
	       gpointer data = gtk_object_get_data(GTK_OBJECT(item), "filter");

	       if (data) {
		    gtk_widget_destroy(item);
	       }
	  }

	  g_list_free(menuitems);
     }

     for ( I = g_list_first(config->filters) ; I ; I = g_list_next(I) ) {
	  struct gq_filter *filter;
	  GtkWidget *menuitem;

	  filter = (struct gq_filter *) I->data;
	  menuitem = gtk_menu_item_new_with_label(filter->name);

	  gtk_object_set_data(GTK_OBJECT(menuitem), "filter", filter);
	  g_signal_connect_swapped(menuitem, "activate",
				    G_CALLBACK(filter_selected),
				    filter);

	  gtk_container_add(GTK_CONTAINER(win->filtermenu), menuitem);
	  gtk_widget_show(menuitem);
     }
}


void create_mainwin(struct mainwin_data *win)
{
     GtkWidget *outer_vbox, *main_vbox, *menubar, *menuitem, *submenu;
     GtkWidget *File, *menuFile, *New, *Close, *ShowM, *Quit;
     GtkWidget *Search, *Browse, *Schema;
     GtkWidget *menuHelp, *Help, *About;
     GtkWidget *Filters, *menuFilters;
     GtkWidget *handlebox;
     GtkAccelGroup *accel_group;

     g_assert(win != NULL);

     win->mainwin = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
					    "mainwin", 770, 478);

     gtk_container_border_width(GTK_CONTAINER(win->mainwin), 0);
     g_signal_connect(win->mainwin, "destroy",
			G_CALLBACK(mainwin_destroyed),
			win);
     gtk_window_set_title(GTK_WINDOW(win->mainwin), _("GQ"));
     gtk_window_set_policy(GTK_WINDOW(win->mainwin), FALSE, TRUE, FALSE);



     outer_vbox = gtk_vbox_new(FALSE, 2);
     gtk_container_border_width(GTK_CONTAINER(outer_vbox), 0);
     gtk_widget_show(outer_vbox);
     gtk_container_add(GTK_CONTAINER(win->mainwin), outer_vbox);

     accel_group = gtk_accel_group_new();

     gtk_window_add_accel_group(GTK_WINDOW(win->mainwin), accel_group);

     handlebox = gtk_handle_box_new();
     gtk_widget_show(handlebox);
     gtk_box_pack_start(GTK_BOX(outer_vbox), handlebox, FALSE, TRUE, 0);

     menubar = gtk_menu_bar_new();
     gtk_widget_show(menubar);
     gtk_container_add(GTK_CONTAINER(handlebox), menubar);

     /* File menu */
     File = gq_menu_item_new_with_label(_("_File"));
     gtk_widget_show(File);
     gtk_container_add(GTK_CONTAINER(menubar), File);

     menuFile = gtk_menu_new();
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(File), menuFile);

     /* File | New */
     New = gq_menu_item_new_with_label(_("_New tab"));
     gtk_widget_show(New);
     gtk_container_add(GTK_CONTAINER(menuFile), New);
     submenu = gtk_menu_new();
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(New), submenu);

     /* File | New | Search */
     Search = gq_menu_item_new_with_label(_("_Search"));
     gtk_widget_show(Search);
     gtk_menu_append(GTK_MENU(submenu), Search);
     g_signal_connect_swapped(Search, "activate",
			       G_CALLBACK(new_modetab_search),
			       win);
     gtk_widget_add_accelerator(Search, "activate", accel_group, 'S',
				GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

     /* File | New | Browse */
     Browse = gq_menu_item_new_with_label(_("_Browse"));
     gtk_widget_show(Browse);
     gtk_menu_append(GTK_MENU(submenu), Browse);
     g_signal_connect_swapped(Browse, "activate",
			       G_CALLBACK(new_modetab_browse),
			       win);
     gtk_widget_add_accelerator(Browse, "activate", accel_group, 'B',
				GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     /* ctrl-b is used by text widgets, so the searchterm textbox that
	always has focus in search mode blocks the above accelerator...*/
     g_signal_connect(win->mainwin, "key_press_event",
			G_CALLBACK(ctrl_b_hack),
			Browse);

     /* File | New | Schema */
     Schema = gq_menu_item_new_with_label(_("S_chema"));
     gtk_widget_show(Schema);
     gtk_menu_append(GTK_MENU(submenu), Schema);
     gtk_widget_add_accelerator(Schema, "activate", accel_group, 'Z',
				GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
#ifdef HAVE_LDAP_STR2OBJECTCLASS
     g_signal_connect_swapped(Schema, "activate",
			       G_CALLBACK(new_modetab_schema),
			       win);
#else
     gtk_widget_set_sensitive(Schema, FALSE);
#endif

     /* File | Preferences */
     menuitem = gq_menu_item_new_with_label(_("_Preferences"));
     gtk_widget_show(menuitem);
     gtk_container_add(GTK_CONTAINER(menuFile), menuitem);
     g_signal_connect_swapped(menuitem, "activate",
			       G_CALLBACK(create_prefs_window),
			       win);
     gtk_widget_add_accelerator(menuitem, "activate", accel_group, 'P',
				GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);

     /* File | Close */
     Close = gq_menu_item_new_with_label(_("_Close tab"));
     gtk_widget_show(Close);
     gtk_container_add(GTK_CONTAINER(menuFile), Close);
     g_signal_connect_swapped(Close, "activate",
			       G_CALLBACK(close_current_tab),
			       win);
     gtk_widget_add_accelerator(Close, "activate", accel_group, 'W',
				GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
     /* :-( */
     g_signal_connect(win->mainwin, "key_press_event",
			G_CALLBACK(ctrl_w_hack),
			Close);

     /* File | Show Message */

     ShowM = gq_menu_item_new_with_label(_("Show _Message Log"));
     gtk_widget_show(ShowM);
     gtk_container_add(GTK_CONTAINER(menuFile), ShowM);
     g_signal_connect_swapped(ShowM, "activate",
			       G_CALLBACK(message_log),
			       win);

     /* File | Quit */
     Quit = gq_menu_item_new_with_label(_("_Quit"));
     gtk_widget_show(Quit);
     gtk_container_add(GTK_CONTAINER(menuFile), Quit);
     g_signal_connect_swapped(Quit, "activate",
			       G_CALLBACK(gtk_widget_destroy),
			       win->mainwin);
     gtk_widget_add_accelerator(Quit, "activate", accel_group, 'Q',
				GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);


     /* Filters menu */
     Filters = gq_menu_item_new_with_label(_("F_ilters"));
     gtk_widget_show(Filters);
     gtk_container_add(GTK_CONTAINER(menubar), Filters);

     menuFilters = gtk_menu_new();
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(Filters), menuFilters);
     win->filtermenu = menuFilters;

     /* Filters | New */
     New = gq_menu_item_new_with_label(_("_New filter"));
     gtk_widget_show(New);
     gtk_container_add(GTK_CONTAINER(menuFilters), New);
     submenu = gtk_menu_new();
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(New), submenu);

     /* Filters | New | From Search tab */
     menuitem = gq_menu_item_new_with_label(_("From _Search tab"));
     gtk_widget_show(menuitem);
     gtk_menu_append(GTK_MENU(submenu), menuitem);
     g_signal_connect_swapped(menuitem, "activate",
			       G_CALLBACK(add_filter),
			       NULL);

     /* Filters | New | Filter Editor */
     menuitem = gq_menu_item_new_with_label(_("Filter _editor"));
     gtk_widget_show(menuitem);
     gtk_menu_append(GTK_MENU(submenu), menuitem);
     g_signal_connect_swapped(menuitem, "activate",
			       G_CALLBACK(add_new_filter_callback),
			       NULL);

     /* Filters | Edit Filters */
     menuitem = gq_menu_item_new_with_label(_("_Edit Filters"));
     gtk_widget_show(menuitem);
     gtk_container_add(GTK_CONTAINER(menuFilters), menuitem);
     g_signal_connect_swapped(menuitem, "activate",
			       G_CALLBACK(show_filters),
			       NULL);

     /* Filters separator */
     menuitem = gtk_menu_item_new();
     gtk_widget_show(menuitem);
     gtk_container_add(GTK_CONTAINER(menuFilters), menuitem);

     mainwin_update_filter_menu(win);

     /* Help menu */
     Help = gq_menu_item_new_with_label(_("_Help"));
     gtk_widget_show(Help);
     gtk_container_add(GTK_CONTAINER(menubar), Help);

     menuHelp = gtk_menu_new();
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(Help), menuHelp);

     /* Help | About */
     About = gq_menu_item_new_with_label(_("_About"));
     gtk_widget_show(About);
     gtk_container_add(GTK_CONTAINER(menuHelp), About);
     g_signal_connect_swapped(About, "activate",
			      G_CALLBACK(create_about_window), win->mainwin);

     main_vbox = gtk_vbox_new(FALSE, 2);
     gtk_container_border_width(GTK_CONTAINER(main_vbox), 4);
     gtk_widget_show(main_vbox);
     gtk_box_pack_start(GTK_BOX(outer_vbox), main_vbox, TRUE, TRUE, 1);

     win->mainbook = gtk_notebook_new();
     gtk_widget_show(win->mainbook);
/*      GTK_WIDGET_UNSET_FLAGS(GTK_NOTEBOOK(mainbook), GTK_CAN_FOCUS); */
     gtk_box_pack_start(GTK_BOX(main_vbox), win->mainbook, TRUE, TRUE, 0);

     win->statusbar = gtk_statusbar_new();
     gtk_widget_show(win->statusbar);

     gtk_box_pack_end(GTK_BOX(outer_vbox), win->statusbar, FALSE, FALSE, 0);
     gtk_widget_set_sensitive(win->statusbar, TRUE);
     
     g_signal_connect(win->mainbook, "switch-page",
			G_CALLBACK(switchpage_refocus), win);
     g_signal_connect(win->mainbook, "remove",
			G_CALLBACK(remove_tab), win);

     gtk_widget_realize(win->mainwin);

     if (! mainwin_restore_snapshot(win)) {
	  new_modetab(win, SEARCH_MODE);
	  new_modetab(win, BROWSE_MODE | 32768);
	  new_modetab(win, SCHEMA_MODE | 32768);
     }

     gtk_widget_show(win->mainwin);
}

GqTab *mainwin_get_tab_nth(struct mainwin_data *win, int n)
{
     GtkWidget *content = 
	  gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->mainbook), n);
     if (content == NULL) return NULL;

     return gtk_object_get_data(GTK_OBJECT(content), "tab");
}

GqTab *mainwin_get_current_tab(GtkWidget *notebook)
{
     int tabnum = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook));
     GtkWidget *content = gtk_notebook_get_nth_page(GTK_NOTEBOOK(notebook), 
						    tabnum);
     return gtk_object_get_data(GTK_OBJECT(content), "tab");
}

GqTab *new_modetab(struct mainwin_data *win, int mode)
{
     GtkWidget *label, *focusbox;
     GqTab *tab;
     int focus;

     /* hack, hack */
     focus = !(mode & 32768);
     mode &= 32767;

     switch(mode) {
     case SEARCH_MODE:
	  label = gq_label_new(_("_Search"));
	  tab = new_searchmode();
	  break;
     case BROWSE_MODE:
	  label = gq_label_new(_("_Browse"));
	  tab = new_browsemode();
	  break;
#ifdef HAVE_LDAP_STR2OBJECTCLASS
     case SCHEMA_MODE:
	  label = gq_label_new(_("S_chema"));
	  tab = new_schemamode();
	  break;
#endif
     default:
	  return NULL;
     }

     gtk_object_set_data(GTK_OBJECT(tab->content), "tab", tab);

     tab->win = win;

     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(win->mainbook), 
			      tab->content,
			      label);

     if(focus) {
	  enter_last_of_mode(tab);

	  gtk_notebook_set_page(GTK_NOTEBOOK(win->mainbook), -1);

	  focusbox = tab->focus;
	  if(focusbox)
	       gtk_widget_grab_focus(focusbox);
     }
     return tab;
}


static void switchpage_refocus(GtkNotebook *notebook, GtkNotebookPage *page,
			       int pagenum, struct mainwin_data *win)
{
     GtkWidget *focusbox;
     GqTab *tab;

     tab = mainwin_get_tab_nth(win, pagenum);
     if(!tab)
	  return;

     /* retrieve mode, store this pane as the last one used for this mode */
     enter_last_of_mode(tab);

     focusbox = tab->focus;
     if(focusbox) {
	  gtk_widget_grab_focus(focusbox);
	  gtk_editable_select_region(GTK_EDITABLE(focusbox), 0, -1);
     }
}


void cleanup_all_tabs(struct mainwin_data *win)
{
     /* don't waste time refocusing on disappearing tabs */
     g_signal_handlers_disconnect_by_func(win->mainbook,
				   G_CALLBACK(switchpage_refocus), win);
}


static void close_current_tab(struct mainwin_data *win)
{
     int tabnum;
     GtkWidget *content;

     tabnum = gtk_notebook_get_current_page(GTK_NOTEBOOK(win->mainbook));
     content = gtk_notebook_get_nth_page(GTK_NOTEBOOK(win->mainbook), tabnum);
     /* for whatever reason: gtk_notebook_remove_page does not call
	the remove signal on the notebook. I consider this to be a GTK
	bug */

/*      gtk_notebook_remove_page(GTK_NOTEBOOK(win->mainbook), tabnum); */

     gtk_widget_destroy(content);
}

void update_serverlist(struct mainwin_data *win)
{
     GqTab *tab;
     int i;

     for( i = 0 ; (tab = mainwin_get_tab_nth(win, i)) != NULL ; i++) {
	  switch(tab->type) {
	  case SEARCH_MODE:
	       fill_serverlist_combo(GQ_TAB_SEARCH(tab)->serverlist_combo);
	       break;
	  case BROWSE_MODE:
	       update_browse_serverlist(tab);
	       break;
	  case SCHEMA_MODE:
	       update_schema_serverlist(tab);
	       break;
	  }
     }
}

static void
create_about_window(GtkWindow* parent) {
	static gchar const* authors[] = {
		"Bert Vermeulen",
		"Peter Stamfest",
		"David Malcom",
		"Sven Herzberg (current maintainer)",
		NULL
	};
	GdkPixbuf* logo = gdk_pixbuf_new_from_file(PACKAGE_PREFIX "/share/pixmaps/gq/gq.xpm", NULL);
	gtk_show_about_dialog(parent,
			      // "artists", NULL,
			      "authors", authors,
			      "comments", _("The gentleman's LDAP client"),
			      "copyright", _("Copyright (C) 1998-2003 Bert Vermeulen\n"
					     "Copyright (C) 2002-2003 Peter Stamfest\n"
					     "Copyright (C) 2006 Sven Herzberg"),
			      // documenters
			      "license", license,
			      "logo", logo,
			      "name", _("GQ LDAP Client"),
			      "translator-credits", _("translator-credits"),
			      "version", VERSION,
			      "website", "http://www.gq-project.org/",
			      "website-label", _("GQ Website"),
			      NULL);
	if(logo) {
		g_object_unref(logo);
	}
}

/*
   Local Variables:
   c-basic-offset: 5
   End:
*/
