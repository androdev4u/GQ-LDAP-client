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

/* $Id: input.c 975 2006-09-07 18:44:41Z herzi $ */

#include <string.h>
#include <stdio.h>		/* perror() */
#include <stdlib.h>		/* free() */
#include <ctype.h>		/* tolower() */

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include "mainwin.h"
#include "common.h"
#include "configfile.h"
#include "gq-tab-browse.h"
#include "gq-tab-schema.h"
#include "util.h"
#include "errorchain.h"
#include "formfill.h"
#include "input.h"
#include "tinput.h"
#include "encode.h"
#include "ldif.h" /* for b64_decode */
#include "syntax.h"
#include "schema.h"
#include "state.h"

void refresh_inputform(struct inputform *form);

static void add_entry_from_formlist(struct inputform *iform);
static void add_entry_from_formlist_and_select(struct inputform *iform);
static int add_entry_from_formlist_no_close(int add_context, 
					    struct inputform *iform);
static void hide_empty_attributes(GtkToggleToolButton *button, struct inputform *iform);

static void create_new_attr(GtkButton *button, struct inputform *iform);

static void change_displaytype(GtkWidget* button,
			       struct inputform *iform,
			       int wanted_dt);


struct inputform *new_inputform()
{
     struct inputform *n = g_malloc0(sizeof(struct inputform));
     return n;
}

/* free the entry related struct inputform and data it refers to */
/* gtk2 checked (multiple destroy callbacks safety), confidence 0.95 */
void free_inputform(struct inputform *iform)
{
     g_assert(iform);
     if (iform) {
	  g_free(iform->olddn);
	  g_free(iform->dn);

	  if(iform->oldlist) {
	       free_formlist(iform->oldlist);
	       iform->oldlist = NULL;
	  }

	  if(iform->formlist) {
	       free_formlist(iform->formlist);
	       iform->formlist = NULL;
	  }

	  if (iform->server) {
	       g_object_unref(iform->server);
	       iform->server = NULL;
	  }

	  g_free(iform);
     }
}

void save_input_snapshot(int error_context,
			 struct inputform *iform, const char *state_name)
{
     int hide;
     GtkWidget *w;

     g_assert(iform);
     g_assert(state_name);

     hide = iform->hide_status;
     state_value_set_int(state_name, "hide-empty-attributes", hide);

     w = iform->scwin;
     if (w) {
	  GtkAdjustment *adj;
	  int percent;

	  /* saving and restoring the "upper" value works around a
	     problem due to the fact that when restoring the inputfrom
	     in the usual case (during mainwin init) the widgets are
	     not yet shown and the upper value of the adjustments are
	     not set yet (= still zero). In order to be able to
	     restore a value it must be between the lower and upper
	     values. If both are zero this cannot work. */
	  adj = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(w));
	  percent = adj->value / adj->upper * 100.0;
	  state_value_set_int(state_name, "scrolled-window-x", percent);
/* 	  state_value_set_int(state_name, "scrolled-window-x-upper", adj->upper); */

	  adj = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(w));
	  percent = adj->value / adj->upper * 100.0;
	  state_value_set_int(state_name, "scrolled-window-y", percent);
/* 	  state_value_set_int(state_name, "scrolled-window-y-upper", adj->upper); */
     }
}

struct snapshot_info {
     struct inputform *iform;
     double x, y;
};

static void vp_pos(GtkWidget *w, struct snapshot_info *si)
{
     GtkAdjustment *adj;
     GtkWidget *vp = GTK_BIN(si->iform->scwin)->child;

     adj = gtk_viewport_get_hadjustment(GTK_VIEWPORT(vp));
     gtk_adjustment_set_value(adj, si->x * adj->upper);

     adj = gtk_viewport_get_vadjustment(GTK_VIEWPORT(vp));
     gtk_adjustment_set_value(adj, si->y * adj->upper);

     if (w) {
	  g_signal_handlers_disconnect_by_func(w, 
					G_CALLBACK(vp_pos), si);
     }

     g_free(si);
}


void restore_input_snapshot(int error_context,
			    struct inputform *iform, const char *state_name)
{
     int hide;
     GtkWidget *w;

     g_assert(iform);
     g_assert(state_name);

     hide = state_value_get_int(state_name, "hide-empty-attributes", 0);

     if (iform->hide_attr_button) {
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(iform->hide_attr_button), hide);
     }
     w = iform->scwin;
     if (w) {
	  GtkAdjustment *adj;
	  int x, y;
	  struct snapshot_info *si;

	  w = GTK_BIN(w)->child;
	  adj = gtk_viewport_get_hadjustment(GTK_VIEWPORT(w));
	  x = state_value_get_int(state_name, "scrolled-window-x", 0);

	  adj = gtk_viewport_get_vadjustment(GTK_VIEWPORT(w));
	  y = state_value_get_int(state_name, "scrolled-window-y", 0);

	  si = g_malloc0(sizeof(struct snapshot_info));
	  si->iform = iform;
	  si->x = x / 100.0;
	  si->y = y / 100.0;

	  g_signal_connect(w, "realize", 
			     G_CALLBACK(vp_pos), si);
     }
}

void create_form_window(struct inputform *iform)
{
     GtkWidget *edit_window, *vbox;

     edit_window = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
					   "inputform", 500, 450);
     gtk_window_set_title(GTK_WINDOW(edit_window), _("New entry"));

     gtk_widget_show(edit_window);
     g_signal_connect_swapped(edit_window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               edit_window);

     g_signal_connect_swapped(edit_window, "destroy",
                               G_CALLBACK(inputform_free),
                               iform);

     vbox = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox), 5);
     gtk_container_add(GTK_CONTAINER(edit_window), vbox);
     gtk_widget_show(vbox);

     iform->parent_window = edit_window;
     iform->target_vbox = vbox;

}

static void linebutton_clicked(GtkToolButton *button, struct inputform *iform)
{
     change_displaytype(GTK_WIDGET(button), iform, DISPLAYTYPE_ENTRY);
}

static void textareabutton_clicked(GtkToolButton *button, struct inputform *iform)
{
     change_displaytype(GTK_WIDGET(button), iform, DISPLAYTYPE_TEXT);
}

GdkWindow *get_any_gdk_window(GtkWidget *w)
{
     while (w) {
	  if (GTK_WIDGET(w)->window) {
	       return GTK_WIDGET(w)->window;
	  }
	  w = GTK_WIDGET(w)->parent;
     }
     g_error("No GDK window in widget hierarchy");
     return NULL;
}

void create_form_content(struct inputform *iform)
{
	GtkWidget  * toolbar_w = NULL;
	GtkToolbar * toolbar = NULL;
	GtkToolItem* tool_item = NULL;
     GdkPixmap *icon;
     GdkBitmap *icon_mask;
     GtkWidget *vbox2, *hbox2;
     GtkWidget *button, *linebutton, *textareabutton;
     GtkWidget *newattrbutton, *hideattrbutton;
     GtkWidget *scwin, *table;
     GtkWidget *pixmap;
     int detail_context;
     GtkTooltips *tips;

     detail_context = error_new_context(_("Error getting entry"),
					iform->parent_window);
     tips = gtk_tooltips_new();

	toolbar_w = gtk_toolbar_new();
	toolbar = GTK_TOOLBAR(toolbar_w);
	gtk_toolbar_set_style(toolbar, GTK_TOOLBAR_ICONS);
	gtk_widget_show(toolbar_w);
	gtk_box_pack_start(GTK_BOX(iform->target_vbox), toolbar_w, FALSE, FALSE, 0);

	/* line item */
	pixmap = gtk_image_new_from_file(PACKAGE_PREFIX "/share/pixmaps/gq/entry.png");
	gtk_widget_show(pixmap);
	tool_item = gtk_tool_button_new(pixmap, "Single Line");
	gtk_tool_item_set_tooltip(tool_item, tips,
			          _("Turn into one-line entry field"),
			          Q_("tooltip|Changes the display type of the current "
				  "attribute into 'Entry', thus makes the input "
				  "field a one-line text box."));
	g_signal_connect(tool_item, "clicked",
			 G_CALLBACK(linebutton_clicked), iform);
	gtk_widget_show(GTK_WIDGET(tool_item));
	gtk_toolbar_insert(toolbar, tool_item, -1);

	/* textarea item */
	pixmap = gtk_image_new_from_file(PACKAGE_PREFIX "/share/pixmaps/gq/textview.png");
	gtk_widget_show(pixmap);
	tool_item = gtk_tool_button_new(pixmap, "Multiline");
	gtk_tool_item_set_tooltip(tool_item, tips,
			          _("Turn into multi-line entry field"),
				  Q_("tooltip|Changes the display type of the current "
				     "attribute into 'Multi-line text', thus makes "
				     "the input field a multi-line text box."));
	g_signal_connect(tool_item, "clicked",
			 G_CALLBACK(textareabutton_clicked), iform);
	gtk_widget_show(GTK_WIDGET(tool_item));
	gtk_toolbar_insert(toolbar, tool_item, -1);

	/* new attribute item */
	pixmap = gtk_image_new_from_file(PACKAGE_PREFIX "/share/pixmaps/gq/new.xpm");
	gtk_widget_show(pixmap);
	tool_item = gtk_tool_button_new(pixmap, "New Attribute");
	gtk_tool_item_set_tooltip(tool_item, tips,
			  _("Adds an attribute to an object of class "
			    "'extensibleObject'"),
			  Q_("tooltip|Adds an attribute to an object of "
			     "class 'extensibleObject'"));
	g_signal_connect(tool_item, "clicked",
			 G_CALLBACK(create_new_attr), iform);
	gtk_widget_show(GTK_WIDGET(tool_item));
	gtk_toolbar_insert(toolbar, tool_item, -1);

	/* hide empty attributes item */
	pixmap = gtk_image_new_from_file(PACKAGE_PREFIX "/share/pixmaps/gq/hide.xpm");
	gtk_widget_show(pixmap);
	tool_item = gtk_toggle_tool_button_new();
	gtk_tool_button_set_label(GTK_TOOL_BUTTON(tool_item), "Hide Empty");
	gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(tool_item), pixmap);
	gtk_tool_item_set_tooltip(tool_item, tips,
				  _("Hide/show empty attributes"),
				  Q_("tooltip|Hides or shows all attributes without "
				     "values. "
				     "This is a good way to see immediately what "
				     "attributes the object really has."));
	g_signal_connect(tool_item, "clicked",
			 G_CALLBACK(hide_empty_attributes), iform);
	gtk_widget_show(GTK_WIDGET(tool_item));
	gtk_toolbar_insert(toolbar, tool_item, -1);

	iform->hide_attr_button = GTK_WIDGET(tool_item);

     /* scrolled window with vbox2 inside */
     scwin = gtk_scrolled_window_new(NULL, NULL);
     iform->scwin = scwin;
     gtk_container_border_width(GTK_CONTAINER(scwin), 0);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scwin),
                                    GTK_POLICY_NEVER,
                                    GTK_POLICY_ALWAYS);
     gtk_widget_show(scwin);

     gtk_box_pack_start(GTK_BOX(iform->target_vbox), scwin, TRUE, TRUE, 0);
     vbox2 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox2);
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scwin), vbox2);

     /* table inside vbox2, will self-expand */
     table = gtk_table_new(3, 2, FALSE);
     gtk_container_border_width(GTK_CONTAINER(table), 5);
     gtk_table_set_row_spacings(GTK_TABLE(table), 0);
     gtk_table_set_col_spacings(GTK_TABLE(table), 10);
     gtk_widget_show(table);
     gtk_box_pack_start(GTK_BOX(vbox2), table, FALSE, TRUE, 0);
     iform->table = table;

     hbox2 = gtk_hbutton_box_new();
     gtk_container_set_border_width(GTK_CONTAINER(hbox2), 5);
     gtk_widget_show(hbox2);
     gtk_box_pack_end(GTK_BOX(iform->target_vbox), hbox2, FALSE, TRUE, 0);

     if(iform->edit) {
	  button = gtk_button_new_from_stock(GTK_STOCK_APPLY);
          gtk_widget_show(button);
          gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
          g_signal_connect_swapped(button, "clicked",
                                    G_CALLBACK(mod_entry_from_formlist),
                                    iform);
          GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
          GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
          gtk_widget_grab_default(button);

	  button = gq_button_new_with_label(_("Add as _new"));
	  gtk_widget_set_sensitive(button, FALSE);
          gtk_widget_show(button);
          gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
          g_signal_connect_swapped(button, "clicked",
                                    G_CALLBACK(add_entry_from_formlist_and_select),
                                    iform);
          GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS);
	  iform->add_as_new_button = button;

	  button = gtk_button_new_from_stock(GTK_STOCK_REFRESH);
/*  	  gtk_widget_set_sensitive(button, FALSE); */
/*            GTK_WIDGET_UNSET_FLAGS(button, GTK_CAN_FOCUS); */
          gtk_widget_show(button);
          gtk_box_pack_end(GTK_BOX(hbox2), button, FALSE, FALSE, 0);

          g_signal_connect_swapped(button, "clicked",
                                    G_CALLBACK(refresh_inputform),
                                    iform);



	  if(iform->close_window) {
	       button = gtk_button_new_from_stock(GTK_STOCK_CLOSE);
	       gtk_widget_show(button);
	       gtk_box_pack_end(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
	       
	       g_signal_connect_swapped(button, "clicked",
					 G_CALLBACK(destroy_editwindow),
					 iform);
	  }


	  /* this will be used as the callback to set on "activate" on
	     inputfields (i.e. hitting return in an entry box). */
	  iform->activate = mod_entry_from_formlist;

     }
     else {
	  button = gtk_button_new_from_stock(GTK_STOCK_OK);
          gtk_widget_show(button);
          gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
          g_signal_connect_swapped(button, "clicked",
                                    G_CALLBACK(add_entry_from_formlist),
                                    iform);
          GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
          gtk_widget_grab_default(button);

	  button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
          gtk_widget_show(button);
          gtk_box_pack_end(GTK_BOX(hbox2), button, FALSE, FALSE, 0);
          g_signal_connect_swapped(button, "clicked",
                                    G_CALLBACK(destroy_editwindow),
                                    iform);

	  iform->activate = add_entry_from_formlist;

     }

     statusbar_msg_clear();
     error_flush(detail_context);
}


static void change_dt(GtkWidget *menu_item, gpointer data)
{
     GQTypeDisplayClass* new_h = g_type_class_ref(GPOINTER_TO_INT(data));
     struct formfill *form = gtk_object_get_data(GTK_OBJECT(menu_item),
						 "formfill");
     int error_context = error_new_context(_("Changeing display type"),
					   menu_item);

     if (form && new_h) {
	  GByteArray *d;
	  GList *l, *newlist = NULL;
	  GQTypeDisplayClass *old_h = g_type_class_ref(form->dt_handler);

/*	  printf("change displaytype of %s to %s\n",  */
/*		 form->attrname, new_h->name); */

	  /* walk the list of widgets */
	  for ( l = form->widgetList ; l ; l = l->next) {
	       GtkWidget *w = (GtkWidget *) l->data;
	       if (!w) continue;

	       /* retrieve "old" data */
	       d = old_h->get_data(form, w);
	       gtk_widget_destroy(w);

	       /* create new widget */
	       form->dt_handler = GPOINTER_TO_INT(data);
	       w = new_h->get_widget(error_context, form, d, NULL, NULL);

	       if (form->flags & FLAG_NO_USER_MOD) {
		    gtk_widget_set_sensitive(w, 0);
	       }

	       gtk_widget_show(w);

	       gtk_object_set_data(GTK_OBJECT(w),
				   "formfill", form);

	       gtk_box_pack_start(GTK_BOX(form->vbox), w, TRUE, TRUE, 0);

	       /* insert into browser */
	       newlist = g_list_append(newlist, w);
	  }
	  g_list_free(form->widgetList);
	  form->widgetList = newlist;

	  g_type_class_unref(old_h);
     }

     error_flush(error_context);
     g_type_class_unref(new_h);
}


static void schema_show_attr(GtkMenuItem *menuitem, struct formfill *form)
{
     struct server_schema *ss = NULL;
     int error_context;

     if (form->server == NULL) return;

     error_context = error_new_context(_("Showing schema information"),
				       GTK_WIDGET(menuitem));

     ss = get_schema(error_context, form->server);

     popup_detail(SCHEMA_TYPE_AT, form->server, 
		  find_canonical_at_by_at(ss, form->attrname));

     error_flush(error_context);
}

static void remove_dt_default_for_attr(GtkMenuItem *menuitem,
				       struct formfill *form)
{
     gpointer okey;
     gpointer val;
     struct attr_settings *as = lookup_attr_settings(form->attrname);
     int old_ddt;
     int do_free = 0;

     if (!as) return;

     old_ddt = as->defaultDT;

     as->defaultDT = -1;
     if (is_default_attr_settings(as)) {
	  do_free = 1;
     }

     if (do_free) {
	  if (g_hash_table_lookup_extended(config->attrs,
					   as->name,
					   &okey, &val)) {
	       g_hash_table_remove(config->attrs, okey);
	  }
     }

     if (save_config(GTK_WIDGET(menuitem))) {
	  if (do_free) {
	       if (okey) g_free(okey);
	       if (val)  free_attr_settings(val);
	  }
     } else {
	  as->defaultDT = old_ddt;

	  if (do_free) {
	       /* save failed, undo changes - re-insert into hash */
	       g_hash_table_insert(config->attrs, 
				   okey, val);
	  }
     }
}

static void make_dt_default_for_attr(GtkMenuItem *menuitem, 
				     struct formfill *form)
{
     gpointer nkey = NULL;

     struct attr_settings *as = lookup_attr_settings(form->attrname);
     int old_ddt = -1;
     int is_new = 0;

     if (!as) {
	  char *t;
	  as = new_attr_settings();
	  as->name = g_strdup(form->attrname);
	  for (t = as->name ; *t ; t++) *t = tolower(*t);
	  is_new = 1;

	  g_hash_table_insert(config->attrs, 
			      nkey = g_strdup(as->name),
			      as);
     }

     old_ddt = as->defaultDT;
     as->defaultDT = get_dt_from_handler(form->dt_handler);

     if (!save_config(GTK_WIDGET(menuitem))) {
	  /* save failed, undo changes */
	  if (is_new) {
	       g_hash_table_remove(config->attrs, nkey);
	       free_attr_settings(as);
	       g_free(nkey);
	  } else {
	       as->defaultDT = old_ddt;
	  }
     }
}

/* quick and dirty callback data */
struct iform_form {
     struct inputform *iform;
     struct formfill  *form;
};

static struct iform_form *new_iform_form() {
     struct iform_form *i = g_malloc0(sizeof(struct iform_form));
     return i;
}

static void free_iform_form(struct iform_form *iff) {
     g_free(iff);
}

static void remove_user_friendly_for_attr(GtkMenuItem *menuitem,
					  struct iform_form *iff)
{
     gpointer okey;
     gpointer val;
     struct attr_settings *as = lookup_attr_settings(iff->form->attrname);
     char *old_uf = NULL;
     int do_free = 0;

     if (!as) return;

     old_uf = as->user_friendly;
     as->user_friendly = NULL;

     if (is_default_attr_settings(as)) {
	  do_free = 1;
     }

     if (do_free) {
	  if (g_hash_table_lookup_extended(config->attrs,
					   as->name,
					   &okey, &val)) {
	       g_hash_table_remove(config->attrs, okey);
	  }
     }

     if (save_config(GTK_WIDGET(menuitem))) {
	  if (do_free) {
	       g_free(okey);
	       g_free(old_uf);
	       if (val) free_attr_settings(val);
	  }
	  refresh_inputform(iff->iform);
     } else {
	  as->user_friendly = old_uf;

	  if (do_free) {
	       /* save failed, undo changes - re-insert into hash */
	       g_hash_table_insert(config->attrs, 
				   okey, val);
	  }
     }
}

static void make_user_friendly_for_attr(GtkMenuItem *menuitem, 
					struct iform_form *iff)
{
     gchar *ret = NULL;
     struct attr_settings *as = lookup_attr_settings(iff->form->attrname);
     GString *msg = g_string_sized_new(150);

     if (as) ret = as->user_friendly;

     g_string_printf(msg, _("User friendly name for LDAP attribute '%s'"),
		     iff->form->attrname);


     if (query_popup(msg->str, &ret, FALSE, GTK_WIDGET(menuitem))) {
	  gpointer nkey = NULL;
	  
	  char *old_uf = NULL;
	  int is_new = 0;
	  
	  if (!as) {
	       char *t;

	       is_new = 1;

	       as = new_attr_settings();
	       as->name = g_strdup(iff->form->attrname);
	       for (t = as->name ; *t ; t++) *t = tolower(*t);
	       
	       g_hash_table_insert(config->attrs, 
				   nkey = g_strdup(as->name),
				   as);
	  }
	  
	  old_uf = as->user_friendly;
	  if ((ret && strlen(ret) == 0)  ||
	      (strcmp(ret, iff->form->attrname) == 0)) {
	       g_free(ret);
	       ret = NULL;
	  }

	  as->user_friendly = ret;
	  
	  if (!save_config(GTK_WIDGET(menuitem))) {
	       /* save failed, undo changes */
	       if (is_new) {
		    g_hash_table_remove(config->attrs, nkey);
		    free_attr_settings(as);
		    g_free(nkey);
	       } else {
		    as->user_friendly = old_uf;
	       }
	  } else {
	       g_free(old_uf);
	       ret = NULL;
	       refresh_inputform(iff->iform);
	  }
     }
     g_free(ret);
     g_string_free(msg, TRUE);
}


static gboolean widget_button_press(GtkWidget *widget,
				    GdkEventButton *event,
				    struct iform_form *iff)
{
     if (event->type == GDK_BUTTON_PRESS && event->button == 3) {
	  GtkWidget *menu_item, *menu, *label, *submenu;
	  GtkWidget *root_menu;
	  GList *dt_list;
	  char buf[40];
	  struct attr_settings *as;

	  root_menu = gtk_menu_item_new_with_label("Root");
	  gtk_widget_show(root_menu);
	  menu = gtk_menu_new();
	  gtk_menu_item_set_submenu(GTK_MENU_ITEM(root_menu), menu);

	  g_snprintf(buf, sizeof(buf), _("Attribute %s"), iff->form->attrname);

	  label = gtk_menu_item_new_with_label(buf);
	  gtk_widget_set_sensitive(label, FALSE);
	  gtk_widget_show(label);

	  gtk_menu_append(GTK_MENU(menu), label);
	  gtk_menu_set_title(GTK_MENU(menu), buf);

	  menu_item = gtk_separator_menu_item_new();

	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  gtk_widget_set_sensitive(menu_item, FALSE);
	  gtk_widget_show(menu_item);

	  menu_item = gtk_menu_item_new_with_label(_("Schema information"));
/*	  gtk_object_set_data(GTK_OBJECT(menu_item), "formfill", form); */
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  g_signal_connect(menu_item, "activate",
			     G_CALLBACK(schema_show_attr),
			     iff->form);
	  gtk_widget_show(menu_item);

	  /* Change display type submenu */
	  menu_item = gtk_menu_item_new_with_label(_("Change display type"));
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  submenu = gtk_menu_new();
	  gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
	  gtk_widget_show(menu_item);

	  label = gtk_menu_item_new_with_label(_("Select display type"));
	  gtk_widget_set_sensitive(label, FALSE);
	  gtk_widget_show(label);
	  gtk_menu_append(GTK_MENU(submenu), label);

	  menu_item = gtk_separator_menu_item_new();

	  gtk_menu_append(GTK_MENU(submenu), menu_item);
	  gtk_widget_set_sensitive(menu_item, FALSE);
	  gtk_widget_show(menu_item);

	  for(dt_list = get_selectable_displaytypes(); dt_list; dt_list = dt_list->next) {
	       GQTypeDisplayClass *h = g_type_class_ref(GPOINTER_TO_INT(dt_list->data));

	       if(!h) {
		       g_warning("Couldn't get class for %s", g_type_name(GPOINTER_TO_INT(dt_list->data)));
		       continue;
	       }

	       menu_item = gtk_menu_item_new_with_label(h->name);
	       gtk_object_set_data(GTK_OBJECT(menu_item), "formfill",
				   iff->form);
/*	       gtk_object_set_data(GTK_OBJECT(menu_item), "entry", entry); */
	       gtk_menu_append(GTK_MENU(submenu), menu_item);
	       g_signal_connect(menu_item, "activate",
				  G_CALLBACK(change_dt),
				  dt_list->data);
	       gtk_widget_show(menu_item);
	       g_type_class_unref(h);
	  }

	  as = lookup_attr_settings(iff->form->attrname);

	  menu_item = gtk_menu_item_new_with_label(_("Make current display type the default for this attribute"));
/*	  gtk_object_set_data(GTK_OBJECT(menu_item), "formfill", form); */
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  g_signal_connect(menu_item, "activate",
			     G_CALLBACK(make_dt_default_for_attr),
			     iff->form);
	  gtk_widget_show(menu_item);

	  menu_item = gtk_menu_item_new_with_label(_("Remove user-defined default display type setting"));
/*	  gtk_object_set_data(GTK_OBJECT(menu_item), "formfill", form); */
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  g_signal_connect(menu_item, "activate",
			     G_CALLBACK(remove_dt_default_for_attr),
			     iff->form);
	  gtk_widget_show(menu_item);

	  gtk_widget_set_sensitive(menu_item,
				   as != NULL && as->defaultDT != -1);

	  menu_item = gtk_separator_menu_item_new();
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  gtk_widget_show(menu_item);

	  menu_item = gtk_menu_item_new_with_label(_("Set user-friendly name"));
	  gtk_menu_append(GTK_MENU(menu), menu_item);

	  g_signal_connect(menu_item, "activate",
			     G_CALLBACK(make_user_friendly_for_attr),
			     iff);
	  gtk_widget_show(menu_item);

	  menu_item = gtk_menu_item_new_with_label(_("Clear user-friendly name"));
	  gtk_menu_append(GTK_MENU(menu), menu_item);
	  g_signal_connect(menu_item, "activate",
			     G_CALLBACK(remove_user_friendly_for_attr),
			     iff);
	  gtk_widget_show(menu_item);

	  gtk_widget_set_sensitive(menu_item,
				   as != NULL && as->user_friendly);

	  gtk_menu_popup(GTK_MENU(menu), NULL, NULL, NULL, NULL,
			 event->button, event->time);

	  return(TRUE);
     }
     return FALSE;
}


static void dn_changed(GtkEditable *editable, struct inputform *iform)
{
     char *val;

     if(iform->add_as_new_button) {
	  val = gtk_editable_get_chars(editable, 0, -1);
	  gtk_widget_set_sensitive(iform->add_as_new_button, strcmp(iform->dn, val) != 0);
	  g_free(val);
     }

}


void build_or_update_inputform(int error_context,
			       struct inputform *iform, gboolean build)
{
     GList *values, *formlist, *widgetList;
     GdkColor notcol = { 0, 0xffff, 0x0000, 0x0000 };
     GdkColor mustcol = { 0, 0x5c00, 0x3800, 0xffff };
     GdkColor delcol = { 0, 0xd600, 0xa000, 0x0000 };
     GdkColor extensible_col = { 0, 0xffff, 0x0000, 0xffff };

     GtkWidget *arrowbutton, *vbox;
     GtkWidget *widget = NULL, *align;
     struct formfill *ff;
     int row, row_arrow, vcnt, currcnt;
     GByteArray *gb;

     gdk_colormap_alloc_color(gdk_colormap_get_system(), &notcol, FALSE, TRUE);
     gdk_colormap_alloc_color(gdk_colormap_get_system(), &mustcol, FALSE,TRUE);
     gdk_colormap_alloc_color(gdk_colormap_get_system(), &delcol, FALSE, TRUE);
     gdk_colormap_alloc_color(gdk_colormap_get_system(), &extensible_col,
			      FALSE, TRUE);

     if (build) {
	  /* DN input box */
	  GtkWidget *label = gtk_label_new(_("Distinguished Name (DN)"));
	  gtk_misc_set_alignment(GTK_MISC(label), LABEL_JUSTIFICATION, .5);

	  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &mustcol);
	  gtk_widget_show(label);
	  gtk_table_attach(GTK_TABLE(iform->table), label, 0, 1, 0, 1,
			   GTK_FILL, GTK_FILL|GTK_EXPAND, 0, 0);

	  iform->dn_widget = gtk_entry_new();
	  gtk_widget_show(iform->dn_widget);

	  gtk_table_attach(GTK_TABLE(iform->table), iform->dn_widget, 1, 2, 0, 1,
			   GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 0, 0);
	  if(iform->activate)
	       g_signal_connect_swapped(iform->dn_widget, "activate",
					 G_CALLBACK(iform->activate), iform);
	  if(iform->dn) {
	       gtk_entry_set_text(GTK_ENTRY(iform->dn_widget), iform->dn);
	  }

	  g_signal_connect(iform->dn_widget, "changed",
			     G_CALLBACK(dn_changed),
			     iform);
     }

     row = 1;
     /* remember that form->num_inputfields and g_list_length(values) may not
      * be the same as a result of multiple input widgets */
     for(formlist = iform->formlist; formlist; formlist = formlist->next) {
	  ff = (struct formfill *) formlist->data;

	  if (build || ff->label == NULL) {
	       GQTypeDisplayClass* klass = g_type_class_ref(ff->dt_handler);
	       /* attribute name */

	       if (config->browse_use_user_friendly) {
		    ff->label = gtk_label_new(human_readable_attrname(ff->attrname));
	       } else {
		    ff->label = gtk_label_new(ff->attrname);
	       }

	       gtk_misc_set_alignment(GTK_MISC(ff->label),
				      LABEL_JUSTIFICATION, .5);
	       gtk_widget_show(ff->label);

	       ff->event_box = gtk_event_box_new();
	       gtk_container_add(GTK_CONTAINER(ff->event_box), ff->label);

	       if (klass && klass->selectable) {
		    struct iform_form *iff = new_iform_form();
		    iff->iform = iform;
		    iff->form  = ff;

		    g_signal_connect(ff->event_box,
				       "button_press_event",
				       G_CALLBACK(widget_button_press),
				       iff);
		    gtk_object_set_data_full(GTK_OBJECT(ff->event_box),
					     "cb-data", iff,
					     (GtkDestroyNotify)free_iform_form);
	       }

	       gtk_widget_show(ff->event_box);
	       gtk_table_attach(GTK_TABLE(iform->table), ff->event_box,
				0, 1, row, row + 1,
				GTK_FILL,
				GTK_FILL,
				0, 0);

	       row_arrow = row;

	       ff->vbox = vbox = gtk_vbox_new(FALSE, 1);
	       gtk_widget_show(vbox);

	       gtk_table_attach(GTK_TABLE(iform->table), vbox,
				1, 2, row, row + 1,
				GTK_FILL|GTK_EXPAND,
				GTK_FILL|GTK_EXPAND, 0, 0);
	       g_type_class_unref(klass);
	  }

	  gtk_widget_restore_default_style(ff->label);

	  if(ff->flags & FLAG_NOT_IN_SCHEMA) {
	       gtk_widget_modify_fg(ff->label, GTK_STATE_NORMAL, &notcol);
	  }
	  if(ff->flags & FLAG_MUST_IN_SCHEMA) {
	       gtk_widget_modify_fg(ff->label, GTK_STATE_NORMAL, &mustcol);
	  }
	  if(ff->flags & FLAG_DEL_ME) {
	       gtk_widget_modify_fg(ff->label, GTK_STATE_NORMAL, &delcol);
	  }
	  if(ff->flags & FLAG_EXTENSIBLE_OBJECT_ATTR) {
	       gtk_widget_modify_fg(ff->label, GTK_STATE_NORMAL,
				    &extensible_col);
	  }

	  currcnt = ff->widgetList ? g_list_length(ff->widgetList) : 0;
	  values = ff->values;
	  widgetList = ff->widgetList;

	  for(vcnt = 0; vcnt < ff->num_inputfields; vcnt++) {
	       GQTypeDisplayClass* klass = g_type_class_ref(ff->dt_handler);
	       gb = NULL;
	       if (values) {
		    gb = (GByteArray*) values->data;
	       }

/*  if (gb) { char *z = g_malloc(gb->len+1); */
/*    strncpy(z, gb->data, gb->len); */
/*    z[gb->len] = 0; */
/*    printf("attr=%s data=%s\n", ff->attrname, z); */
/*    g_free(z); */
/*  } */
	       if (klass && klass->get_widget) {
		    if (vcnt >= currcnt) {
			 widget =
			      klass->get_widget(error_context,
							 ff, gb,
							 iform->activate,
							 iform);

			 gtk_widget_show(widget);

			 gtk_object_set_data(GTK_OBJECT(widget),
					     "formfill", ff);

			 gtk_box_pack_start(GTK_BOX(ff->vbox), widget, TRUE, TRUE, 0);
			 if(ff == iform->focusform)
			      gtk_widget_grab_focus(widget);

			 ff->widgetList = g_list_append(ff->widgetList, 
							  widget);

			 if (ff->flags & FLAG_NO_USER_MOD) {
			      gtk_widget_set_sensitive(widget, 0);
			 }
		    } else {
			 widget = widgetList ? widgetList->data : NULL;
		    }
	       }

	       if (values) values = values->next;
	       if (widgetList) widgetList = widgetList->next;

	       g_type_class_unref(klass);
	  }
	  
	  row_arrow = row++;

	  if ((build || ff->morebutton == NULL) &&
	      ((ff->flags & FLAG_SINGLE_VALUE) == 0)) {
	       align = gtk_alignment_new(0.5, 1, 0, 0);
	       gtk_widget_show(align);

	       ff->morebutton = arrowbutton = gq_new_arrowbutton(iform);
	       gtk_object_set_data(GTK_OBJECT(arrowbutton), "formfill", ff);
	       gtk_container_add(GTK_CONTAINER(align), arrowbutton);

	       gtk_table_attach(GTK_TABLE(iform->table), align,
				2, 3, row_arrow, row_arrow + 1,
				GTK_SHRINK, GTK_FILL, 0, 0);

	       if (ff->flags & FLAG_NO_USER_MOD) {
		    gtk_widget_set_sensitive(ff->morebutton, 0);
	       }
	  }
     }

     /* restore the hide-status from a previous LDAP object... */
     set_hide_empty_attributes(iform->hide_status, iform);
}


void build_inputform(int error_context, struct inputform *iform)
{

     build_or_update_inputform(error_context, iform, TRUE);

}


void refresh_inputform(struct inputform *iform)
{
     GList *oldlist, *newlist, *children;
     double frac_x = 0.0, frac_y = 0.0;
     GtkWidget *w;
     int error_context;

     w = iform->scwin;
     if (w) {
	  GtkAdjustment *adj;

	  w = GTK_BIN(w)->child;
	  adj = gtk_viewport_get_hadjustment(GTK_VIEWPORT(w));
	  frac_x = adj->value / adj->upper;

	  adj = gtk_viewport_get_vadjustment(GTK_VIEWPORT(w));
	  frac_y = adj->value / adj->upper;
     }

     while ((children = gtk_container_children(GTK_CONTAINER(iform->target_vbox))) != NULL) {
	  gtk_container_remove(GTK_CONTAINER(iform->target_vbox),
			       children->data);
     }

     if(iform->oldlist) {
	  free_formlist(iform->oldlist);
	  iform->oldlist = NULL;
     }

     if(iform->formlist) {
	  free_formlist(iform->formlist);
	  iform->formlist = NULL;
     }

     error_context = error_new_context(_("Refreshing entry"), 
				       iform->parent_window);

     oldlist = formlist_from_entry(error_context, iform->server, iform->dn, 0);
#ifdef HAVE_LDAP_STR2OBJECTCLASS
     oldlist = add_schema_attrs(error_context, iform->server, oldlist);
#endif

     if(oldlist) {
	  newlist = dup_formlist(oldlist);
	  iform->formlist = newlist;
	  iform->oldlist = oldlist;

	  create_form_content(iform);

	  build_inputform(error_context, iform);
     }

     /* let the main loop run to get all the geometry sorted out for
	the new inputform, afterwards restore the previously set
	viewport position */

     while(gtk_events_pending())
	  gtk_main_iteration();

     if (iform->scwin) {
	  struct snapshot_info *si;

	  si = g_malloc0(sizeof(struct snapshot_info));
	  si->iform = iform;
	  si->x = frac_x;
	  si->y = frac_y;

	  vp_pos(NULL, si); 
     }

     error_flush(error_context);
}

void edit_entry(GqServer *server, const char *dn)
{
     GList *oldlist, *newlist;
     GtkWidget *vbox;
     struct inputform *iform;
     GtkWidget* edit_window;
     int error_context;

     edit_window = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
					   "entry-window", 500, 450); 

     gtk_window_set_title(GTK_WINDOW(edit_window), dn);
     
     gtk_widget_show(edit_window);
     g_signal_connect_swapped(edit_window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               edit_window);

     error_context = error_new_context("", edit_window);

     vbox = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox), 5);
     gtk_container_add(GTK_CONTAINER(edit_window), vbox);
     gtk_widget_show(vbox);

     iform = new_inputform();
     iform->parent_window = edit_window;
     iform->target_vbox = vbox;

     g_signal_connect_swapped(edit_window, "destroy",
                               G_CALLBACK(inputform_free),
                               iform);

     oldlist = formlist_from_entry(error_context, server, dn, 0);
#ifdef HAVE_LDAP_STR2OBJECTCLASS
     oldlist = add_schema_attrs(error_context, server, oldlist);
#endif

     if(oldlist) {
	  newlist = dup_formlist(oldlist);
	  iform->edit = 1;
	  iform->close_window = 1;
	  iform->server = g_object_ref(server);
	  iform->dn = g_strdup(dn);
	  iform->olddn = g_strdup(dn);
	  iform->formlist = newlist;
	  iform->oldlist = oldlist;

	  create_form_content(iform);
	  build_inputform(error_context, iform);
     }

     error_flush(error_context);
}


void new_from_entry(int error_context, GqServer *server,
		    const char *dn)
{
     GList *formlist;
     struct inputform *iform;

     iform = new_inputform();
     iform->server = g_object_ref(server);

#ifdef HAVE_LDAP_STR2OBJECTCLASS
     /* if schema stuff is available, construct formlist based on the objectclasses
	in the entry */
     formlist = formfill_from_entry_objectclass(error_context, server, dn);
#else
     /* no schema stuff: just use the current entry then, throw away all values
	except those in the objectclass attribute */
     formlist = formlist_from_entry(server, dn, 1);
#endif
     if(formlist) {
	  iform->formlist = formlist;
	  g_free_and_dup(iform->dn, dn);
	  create_form_window(iform);
	  create_form_content(iform);
	  build_inputform(error_context, iform);
     } else {
	  free_inputform(iform);
     }

}


static char *get_new_dn(struct inputform *iform)
{
     char *content, *content_enc;

     if(iform->dn_widget) {
	  content = gtk_editable_get_chars(GTK_EDITABLE(iform->dn_widget), 0, -1);
	  content_enc = encoded_string(content);
	  g_free(content);

	  return content_enc;
     }

     return NULL;
}


/*
 * update formlist from on-screen table
 */
void update_formlist(struct inputform *iform)
{
     GList *formlist, *fl, *children = NULL;
     GtkWidget *child;
     struct formfill *ff;
     int displaytype;
     char *old_dn, *content, *content_enc;

     formlist = iform->formlist;
     if(formlist == NULL)
	  return;

     /* walk the formlist and check the inputList for each */
     /* clear any values in formlist, they'll be overwritten shortly
	with more authorative (newer) data anyway */

     for (fl = formlist ; fl ; fl = fl->next ) {
	  ff = (struct formfill *) fl->data;
	  free_formfill_values(ff);

	  displaytype = ff->displaytype;

	  for (children = ff->widgetList ; children ;
	       children = children->next) {
	       child = GTK_WIDGET(children->data);

	       if(ff && displaytype) {
		    GQTypeDisplayClass* klass = g_type_class_ref(ff->dt_handler);
		    GByteArray *ndata = NULL;
		    content = NULL;

		    if (klass && klass->get_data) {
			 ndata = klass->get_data(ff, child);
		    } else if (content && strlen(content) > 0) {
			 int l;
			 content_enc = encoded_string(content);
			 g_free(content);

			 l = strlen(content_enc);

			 ndata = g_byte_array_new();
			 g_byte_array_set_size(ndata, l + 1);
			 memcpy(ndata->data, content_enc, l);
			 free(content_enc);

			 /* uuuhhh, so ugly... */
			 ndata->data[l] = 0;

			 g_byte_array_set_size(ndata, l);
		    }
		    /* don't bother adding in empty fields */
		    if(ndata) {
			 ff->values = g_list_append(ff->values, ndata);
		    }
		    g_type_class_unref(klass);
	       }
	  }
     }
     /* take care of the dn input widget */
     if(iform->dn_widget) {
	  content = gtk_editable_get_chars(GTK_EDITABLE(iform->dn_widget), 0, -1);
	  content_enc = encoded_string(content);
	  old_dn = iform->dn;
	  iform->dn = g_strdup(content_enc);
	  g_free(content);
	  free(content_enc);

	  if(old_dn) {
	       g_free(old_dn);
	       old_dn = NULL;
	  }
     }

}


void clear_table(struct inputform *iform)
{
     GtkWidget *vbox, *old_table, *new_table;
     GList *formlist;
     struct formfill *ff;

     vbox = iform->table->parent;
     old_table = iform->table;
     formlist = iform->formlist;

     gtk_container_remove(GTK_CONTAINER(vbox), old_table);
/*       gtk_widget_destroy(old_table); */

     /* table inside vbox, will self-expand */
     new_table = gtk_table_new(3, 2, FALSE);
     gtk_container_border_width(GTK_CONTAINER(new_table), 5);
     gtk_table_set_row_spacings(GTK_TABLE(new_table), 1);
     gtk_table_set_col_spacings(GTK_TABLE(new_table), 10);
     gtk_widget_show(new_table);
     gtk_box_pack_start(GTK_BOX(vbox), new_table, FALSE, TRUE, 0);
     iform->table = new_table;

     /* should use destroy signals on the widgets to remove widget
        references */

     for ( ; formlist ; formlist = formlist->next ) {
	  ff = (struct formfill *) formlist->data;
	  ff->label = ff->vbox = ff->morebutton = NULL;
	  g_list_free(ff->widgetList);
	  ff->widgetList = NULL;
     }
     
}


static void add_entry_from_formlist(struct inputform *iform)
{
     int ctx = error_new_context(_("Adding entry"), iform->parent_window);
     if (add_entry_from_formlist_no_close(ctx, iform)) {
	  destroy_editwindow(iform);
     }
     error_flush(ctx);
}


static void add_entry_from_formlist_and_select(struct inputform *iform)
{
     GQTreeWidget *tree;
     int rc;
     char *dn;

     int ctx = error_new_context(_("Adding entry"), iform->parent_window);

     dn = get_new_dn(iform);
     tree = iform->ctreeroot;
     rc = add_entry_from_formlist_no_close(ctx, iform);

     if (rc && dn && tree) {
	  show_dn(ctx, tree, NULL, dn, TRUE);
     }
     if (dn) free(dn);

     error_flush(ctx);
}


static int add_entry_from_formlist_no_close(int add_context,
					    struct inputform *iform)
{
     LDAP *ld;
     LDAPMod **mods, *mod;
     GQTreeWidget *ctree;
     GQTreeWidgetNode *node;
     GList *formlist;
     GqServer *server;
     struct formfill *ff;
     GqTab *tab;
     int res, cmod;
     int i;
     char *dn;
     char *parentdn, **rdn;
     LDAPControl c, *ctrls[2] = { NULL, NULL } ;

     c.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     c.ldctl_value.bv_val	= NULL;
     c.ldctl_value.bv_len	= 0;
     c.ldctl_iscritical	= 1;

     ctrls[0] = &c;

     formlist = iform->formlist;
     g_assert(formlist);

     /* handle impossible errors first - BTW: no need for I18N here*/
     if(!formlist) {
	  error_push(add_context,
		     "Hmmm, no formlist to build entry!");
	  return 0;
     }

     server = iform->server;
     g_assert(server);

     if(!server) {
	  error_push(add_context,
		     "Hmmm, no server!");
	  return 0;
     }

     update_formlist(iform);

     dn = iform->dn;
     if(!dn || dn[0] == 0) {
	  error_push(add_context, _("You must enter a DN for a new entry."));
	  return 0;
     }

     set_busycursor();

     ld = open_connection(add_context, server);
     if(!ld) {
	  set_normalcursor();
	  return 0;
     }

     /* build LDAPMod from formlist */
     cmod = 0;
     mods = g_malloc(sizeof(void *) * (g_list_length(formlist) + 1));
     while(formlist) {
	  ff = (struct formfill *) formlist->data;

	  if (! (ff->flags & FLAG_NO_USER_MOD)) {
	       if (g_list_length(ff->values) > 0) {
		    GQTypeDisplayClass* klass = g_type_class_ref(ff->dt_handler);
		    mod = klass->buildLDAPMod(ff,
						       LDAP_MOD_ADD,
						       ff->values);
		    mods[cmod++] = mod;
		    g_type_class_unref(klass);
	       }
	  }

	  formlist = formlist->next;
     }
     mods[cmod] = NULL;

     res = ldap_add_ext_s(ld, dn, mods, ctrls, NULL);

     if(res == LDAP_NOT_SUPPORTED) {
	  res = ldap_add_s(ld, dn, mods);
     }

     ldap_mods_free(mods, 1);

     if (res == LDAP_SERVER_DOWN) {
	  server->server_down++;
     }
     if (res == LDAP_REFERRAL) {
	  /* FIXME */
     }
     if(res != LDAP_SUCCESS) {
	  error_push(add_context, _("Error adding new entry '%1$s': '%2$s'"),
		     dn,
		     ldap_err2string(res));
	  push_ldap_addl_error(ld, add_context);
	  set_normalcursor();
	  close_connection(server, FALSE);
	  return 0;
     }

     /* Walk the list of browser tabs and refresh the parent
        node of the newly added entry to give visual feedback */

     /* construct parent DN */


     /* don't need the RDN of the current entry */
     parentdn = g_malloc(strlen(dn));
     parentdn[0] = 0;
     rdn = gq_ldap_explode_dn(dn, 0);

     /* FIXME: think about using openldap 2.1 specific stuff if
        available... */
     for(i = 1; rdn[i]; i++) {
	  if (parentdn[0]) strcat(parentdn, ","); /* Flawfinder: ignore */
	  strcat(parentdn, rdn[i]);		  /* Flawfinder: ignore */
     }
     gq_exploded_free(rdn);

     for( i = 0 ; (tab = mainwin_get_tab_nth(&mainwin, i)) != NULL ; i++) {
/*      for (tabs = g_list_first(mainwin.tablist) ; tabs ;  */
/*	  tabs = g_list_next(tabs)) { */
/*	  tab = GQ_TAB(tabs->data); */
	  if(tab->type == BROWSE_MODE) {
	       ctree = GQ_TAB_BROWSE(tab)->ctreeroot;
	       if(ctree == NULL)
		    continue;

	       node = tree_node_from_server_dn(ctree, server, parentdn);
	       if(node == NULL)
		    continue;

	       /* refresh the parent DN node... */
	       refresh_subtree(add_context, ctree, node);
	  }
     }

     g_free(parentdn);

     set_normalcursor();

     close_connection(server, FALSE);

     return 1;
}


void mod_entry_from_formlist(struct inputform *iform)
{
     GList *newlist, *oldlist;
     LDAPMod **mods;
     LDAP *ld;
     GqServer *server;
     int mod_context, res;
     char *olddn = NULL, *dn;
     GQTreeWidgetNode *node = NULL;
     GQTreeWidget *ctreeroot = NULL;
     int do_modrdn = 0;
     int error = 0;

     update_formlist(iform);

     mod_context = error_new_context(_("Problem modifying entry"), 
				     iform->parent_window);

     /* store  olddn to use with message later on - in case we change the DN */
     olddn = g_strdup(iform->olddn);
     dn = iform->dn;

     /* obtain all needed stuff from the iform ASAP, as it might be
	destroyed during refresh_subtree (through reselection of a
	CTreeRow and the selection callback). */

     server = iform->server;
     oldlist = iform->oldlist;
     newlist = iform->formlist;
     ctreeroot = iform->ctreeroot;

     do_modrdn = strcasecmp(olddn, dn);

     if( (ld = open_connection(mod_context, server)) == NULL) {
	  goto done;
     }

     if (do_modrdn) {
	  int rc;

#if HAVE_LDAP_CLIENT_CACHE
	  ldap_uncache_entry(ld, olddn);
#endif
	  
	  /* find node now, olddn will change during change_rdn */
	  if(iform->ctree_refresh) {
/*  	       printf("refresh %s to become %s\n", olddn, dn); */

	       node = tree_node_from_server_dn(ctreeroot, server, olddn);
	  }	       

	  if((rc = change_rdn(iform, mod_context)) != 0) {
/*  	       printf("error %d\n", rc); */

	       error = 1;
	       goto done;
	  }
     }

     mods = formdiff_to_ldapmod(oldlist, newlist);
     if(mods != NULL && mods[0] != NULL) {
	  LDAPControl ct, *ctrls[2] = { NULL, NULL } ;
	  ct.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
	  ct.ldctl_value.bv_val	= NULL;
	  ct.ldctl_value.bv_len	= 0;
	  ct.ldctl_iscritical	= 1;
	  
	  ctrls[0] = &ct;

/*   dump_mods(mods); */

	  res = ldap_modify_ext_s(ld, dn, mods, ctrls, NULL);

	  if(res == LDAP_NOT_SUPPORTED) {
	       res = ldap_modify_s(ld, dn, mods);
	  }

	  if (res == LDAP_SERVER_DOWN) {
	       server->server_down++;
	  }
	  
	  if(res == LDAP_SUCCESS) {
	       if (oldlist) {
		    free_formlist(oldlist);
		    iform->oldlist = NULL;
	       }
	       oldlist = dup_formlist(newlist);
	       iform->oldlist = oldlist;
	  } else {
	       error_push(mod_context, _("Error modifying entry '%1$s': %2$s"),
			  dn,
			  ldap_err2string(res));
	       push_ldap_addl_error(ld, mod_context);
	  } 
	  ldap_mods_free(mods, 1);
     }


     statusbar_msg(_("Modified %s"), olddn);
     /* free memory */

     if(iform->close_window)
	  destroy_editwindow(iform);

#if HAVE_LDAP_CLIENT_CACHE
     ldap_uncache_entry(ld, dn);
#endif

 done:
     g_free(olddn);
     close_connection(server, FALSE);

     /* refresh visual if requested by browse mode */
     if (do_modrdn && node && !error) {
	  refresh_subtree_new_dn(mod_context, ctreeroot, node, dn, 0);
     }

     error_flush(mod_context);
}

int change_rdn(struct inputform *iform, int context)
{
     GString *message = NULL;
     LDAP *ld;
     GqServer *server;
     int error, rc, i, remove_flag = 0;
     char *olddn, *dn;
     char **oldrdn, **rdn;
     char *noattrs[] = { LDAP_NO_ATTRS, NULL };
     LDAPMessage *res = NULL;

#if defined(HAVE_LDAP_RENAME)
     LDAPControl cc, *ctrls[2] = { NULL, NULL } ;

     /* prepare ManageDSAit in case we deal with referrals */
     cc.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     cc.ldctl_value.bv_val	= NULL;
     cc.ldctl_value.bv_len	= 0;
     cc.ldctl_iscritical	= 1;
     
     ctrls[0] = &cc;
#endif

     server = iform->server;
     if( (ld = open_connection(context, server)) == NULL)
	  return(1);

     olddn = iform->olddn;
     dn = iform->dn;

     oldrdn = gq_ldap_explode_dn(olddn, 0);
     rdn = gq_ldap_explode_dn(dn, 0);

     if (rdn == NULL) {
	  /* parsing error */

	  error_push(context, _("Cannot explode DN '%s'. Maybe problems with quoting or special characters. See RFC 2253 for details of DN syntax."),
		     dn);

	  error = 1;
     } else {
	  /* parsing OK */

	  message = g_string_sized_new(256);
	  
	  /* check if user really only attemps to change the RDN and not
	     any other parts of the DN */
	  error = 0;
	  for(i = 1; rdn[i]; i++) {
	       if(oldrdn[i] == NULL || rdn[i] == NULL || 
		  (strcasecmp(oldrdn[i], rdn[i]) != 0)) {
		    error_push(context,
			       _("You can only change the RDN of the DN (%s)"),
			       oldrdn[0]);
		    error = 1;
		    break;
	       }
	  }
     }

     if(!error) {
	  statusbar_msg(_("Modifying RDN to %s"), rdn[0]);

	  /* check to see if the rdn exists as an attribute. If it
             does set the remove flag. If it does not do not set the
             remove flag. This is due to the fact that in the latter
             case a set remove flag actually removes the object (at
             least from openldap 2.0 servers. This strange behaviour
             was pointed out by <gwu@acm.org>. */

	  rc = ldap_search_s(ld, olddn, LDAP_SCOPE_BASE, 
			     oldrdn[0], noattrs, 0, &res);
	  if (rc == LDAP_SUCCESS) {
	       LDAPMessage *e = ldap_first_entry(ld, res);
	       if (e) {
		    remove_flag = 1;
	       }
	  }
	  if (res) ldap_msgfree(res);
/*  	  printf("oldrdn[0]=%s, remove=%d\n", oldrdn[0], remove_flag); */

#if defined(HAVE_LDAP_RENAME)
	  /* see draft-ietf-ldapext-ldap-c-api-xx.txt for details */
	  rc = ldap_rename_s(ld,
			     olddn,		/* dn */
			     rdn[0],		/* newrdn */
			     NULL,		/* newparent */
			     remove_flag,	/* deleteoldrdn */
			     ctrls,		/* serverctrls */
			     NULL		/* clientctrls */
			     );

#else
	  rc = ldap_modrdn2_s(ld, olddn, rdn[0], remove_flag);
#endif
	  if(rc == LDAP_SUCCESS) {
	       /* get ready for subsequent DN changes */
	       g_free(olddn);
	       iform->olddn = g_strdup(dn);
	  } else {
	       if (rc == LDAP_SERVER_DOWN) {
		    server->server_down++;
	       }
	       error_push(context, _("Error renaming entry '%1$s': %2$s"),
			  olddn,
			  ldap_err2string(rc));
	       push_ldap_addl_error(ld, context);
	       error = 2;
	  }
     }

     if (oldrdn) gq_exploded_free(oldrdn);
     if (rdn) gq_exploded_free(rdn);
     if (message) g_string_free(message, TRUE);

     close_connection(server, FALSE);

     return(error);
}


/*
 * convert the differences between oldlist and newlist into
 * LDAPMod structures
 */
LDAPMod **formdiff_to_ldapmod(GList *oldlist, GList *newlist)
{
     GList *oldlist_tmp, *newlist_tmp, *values, *deleted_values, *added_values;
     LDAPMod **mods, *mod;
     struct formfill *oldform, *newform;
     int old_l, new_l, modcnt;

     oldlist_tmp = oldlist;
     newlist_tmp = newlist;

     old_l = g_list_length(oldlist);
     new_l = g_list_length(newlist);
     mods = malloc( sizeof(void *) * (( old_l > new_l ? old_l : new_l ) * 4) );
     if (mods == NULL) {
	  perror("formdiff_to_ldapmod");
	  exit(1);
     }
     mods[0] = NULL;
     modcnt = 0;

     /* pass 0: Actually delete those values marked with FLAG_DEL_ME. These
        will be picked up be the following passes */

     for ( ; newlist ; newlist = newlist->next ) {
	  newform = (struct formfill *) newlist->data;
	  if (newform->flags & FLAG_DEL_ME) {
	       free_formfill_values(newform);
	       newform->flags &= ~FLAG_DEL_ME;
	  }
     }

     newlist = newlist_tmp;

     /* pass 1: deleted attributes, and deleted/added values */
     for( ; oldlist ; oldlist = oldlist->next ) {
	  oldform = (struct formfill *) oldlist->data;

/*  	  if (oldform->flags & FLAG_NO_USER_MOD) continue; */
	  newform = lookup_attribute(newlist, oldform->attrname);
	  /* oldform->values can come up NULL if the attribute was in
	     the form only because add_attrs_by_oc() added it. Not a
	     delete in this case... */
	  if(oldform->values != NULL && 
	     (newform == NULL || newform->values == NULL)) {
	       /* attribute deleted */
	       mod = malloc(sizeof(LDAPMod));
	       if (mod == NULL) {
		    perror("formdiff_to_ldapmod");
		    exit(2);
	       }
	       mod->mod_op = LDAP_MOD_DELETE;

	       if (oldform->syntax && oldform->syntax->must_binary) {
		    mod->mod_type = g_strdup_printf("%s;binary", oldform->attrname);
	       }
	       else {
		    mod->mod_type = g_strdup(oldform->attrname);
	       }
	       mod->mod_values = NULL;
	       mods[modcnt++] = mod;
	  }
	  else {
	       GQTypeDisplayClass* klass = g_type_class_ref(oldform->dt_handler);

	       /* pass 1.1: deleted values */
	       deleted_values = NULL;
	       values = oldform->values;
	       while(values) {
		    if(!find_value(newform->values, (GByteArray *) values->data)) {
			 deleted_values = g_list_append(deleted_values, values->data);
		    }
		    values = values->next;
	       }

	       /* pass 1.2: added values */
	       added_values = NULL;
	       values = newform->values;
	       while(values) {
		    if(!find_value(oldform->values, (GByteArray *) values->data))
			 added_values = g_list_append(added_values, values->data);
		    values = values->next;
	       }

	       if(deleted_values && added_values) {
		    /* values deleted and added -- likely a simple edit.
		       Optimize this into a MOD_REPLACE. This could be
		       more work for the server in case of a huge number
		       of values, but who knows...
		    */
		    mod = klass->buildLDAPMod(oldform,
							    LDAP_MOD_REPLACE,
							    newform->values);
		    mods[modcnt++] = mod;
	       }
	       else {
		    if(deleted_values) {
			 mod = klass->buildLDAPMod(oldform,
								 LDAP_MOD_DELETE,
								 deleted_values);
			 mods[modcnt++] = mod;
		    } else if(added_values) {
			 mod = klass->buildLDAPMod(oldform,
								 LDAP_MOD_ADD,
								 added_values);
			 mods[modcnt++] = mod;
		    }
	       }

	       g_list_free(deleted_values);
	       g_list_free(added_values);
	       g_type_class_unref(klass);
	  }
     }
     oldlist = oldlist_tmp;

     /* pass 2: added attributes */
     while(newlist) {
	  newform = (struct formfill *) newlist->data;
	  if(lookup_attribute(oldlist, newform->attrname) == NULL) {
	       /* new attribute was added */

	       if(newform->values) {
		    GQTypeDisplayClass* klass = g_type_class_ref(newform->dt_handler);
		    mod = klass->buildLDAPMod(newform,
							    LDAP_MOD_ADD,
							    newform->values);

		    mods[modcnt++] = mod;
		    g_type_class_unref(klass);
	       }
	  }

	  newlist = newlist->next;
     }

     mods[modcnt] = NULL;

     return(mods);
}


char **glist_to_mod_values(GList *values)
{
     int valcnt;
     char **od_values;

     valcnt = 0;
     od_values = g_malloc(sizeof(char *) * (g_list_length(values) + 1));
     while(values) {
	  if(values->data)
	       od_values[valcnt++] = g_strdup(values->data);
	  values = values->next;
     }
     od_values[valcnt] = NULL;

     return(od_values);
}


struct berval **glist_to_mod_bvalues(GList *values)
{
     int valcnt = 0;
     struct berval **od_bvalues;

     od_bvalues = g_malloc(sizeof(struct berval *) * (g_list_length(values) + 1));

     while(values) {
	  if(values->data) {
	       int l = strlen(values->data);
	       int approx = (l / 4) * 3 + 10;
	       GByteArray *gb = g_byte_array_new();
	       if (gb == NULL) break; /* FIXME */
	       g_byte_array_set_size(gb, approx);
	       b64_decode(gb, values->data, l);
	       
	       od_bvalues[valcnt] = (struct berval*) g_malloc(sizeof(struct berval));
	       od_bvalues[valcnt]->bv_len = gb->len;
	       od_bvalues[valcnt]->bv_val = (gchar*)gb->data;
	       
	       g_byte_array_free(gb, FALSE);
	       valcnt++;
	  }
	  values = values->next;
     }
     od_bvalues[valcnt] = NULL;

     return(od_bvalues);
}


int find_value(GList *list, GByteArray *value)
{
     GByteArray *gb;

     while(list) {
	  gb = (GByteArray *) list->data;
	  if( gb->len == value->len && 
	      memcmp(gb->data, value->data, gb->len) == 0)
	       return(1);

	  list = list->next;
     }

     return(0);
}


void destroy_editwindow(struct inputform *iform)
{

     /* PS: Note: Should be able to remove the code for formlist and dn and
	leave the job to browsehash_free */
     if(iform->formlist) {
	  free_formlist(iform->formlist);
	  iform->formlist = NULL;
     }

     if(iform->dn) {
	  g_free(iform->dn);
	  iform->dn = NULL;
     }

     gtk_widget_destroy(iform->parent_window); /* OK, window is a toplevel widget */

     /* Now done in a destroy signal handler of the window, so it
        won't escape us (using browsehash_free)  */

}


void add_row(GtkWidget *button, struct inputform *iform)
{
     struct formfill *ff;
     int error_context;

     update_formlist(iform);

     ff = (struct formfill *) gtk_object_get_data(GTK_OBJECT(button), "formfill");
     if(!ff)
	  return;

     ff->num_inputfields++;

     error_context = error_new_context(_("Adding attribute value field"),
				       iform->parent_window);
/*       clear_table(iform); */
     build_or_update_inputform(error_context, iform, FALSE);
     error_flush(error_context);
}


GtkWidget *gq_new_arrowbutton(struct inputform *iform)
{
     GtkWidget *newabutton, *arrow;

     newabutton = gtk_button_new();
     GTK_WIDGET_UNSET_FLAGS(newabutton, GTK_CAN_FOCUS);
     g_signal_connect(newabutton, "clicked",
			G_CALLBACK(add_row),
			iform);
     arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
     gtk_widget_show(arrow);
     gtk_container_add(GTK_CONTAINER(newabutton), arrow);
     gtk_widget_show(newabutton);

     return(newabutton);
}

static void check_focus(GtkWidget *w, GtkWidget **focus) 
{
     if (GTK_WIDGET_HAS_FOCUS(w)) {
	  *focus = w;
     }
}


GtkWidget *find_focusbox(GList *formlist)
{
     GList *widgets;
     struct formfill *ff;

     if(!formlist)
	  return(NULL);

     for ( ; formlist ; formlist = formlist->next ) {
	  ff = (struct formfill *) formlist->data;
	  if (ff && ff->widgetList) {
	       for (widgets = ff->widgetList ; widgets ; 
		    widgets = widgets->next ) {
		    
		    if (GTK_WIDGET_HAS_FOCUS(GTK_WIDGET(widgets->data))) {
			 return GTK_WIDGET(widgets->data);
		    }

		    /* check children as well */
		    if (GTK_IS_CONTAINER(GTK_WIDGET(widgets->data))) {
			 GtkWidget *focus = NULL;
			 gtk_container_foreach(GTK_CONTAINER(GTK_WIDGET(widgets->data)),
					       (GtkCallback) check_focus,
					       &focus);
			 if (focus) return widgets->data;
		    }
	       }
	  }
     }

     return NULL;
}


/*
 * callback for entry or textbox buttons
 */
static void change_displaytype(GtkWidget* button, struct inputform *iform, int wanted_dt)
{
     GtkWidget *focusbox;
     struct formfill *ff, *focusform = NULL;
     int error_context;

     g_assert(wanted_dt == DISPLAYTYPE_ENTRY || wanted_dt == DISPLAYTYPE_TEXT);

     error_context = error_new_context(_("Changing display type"), button);

     update_formlist(iform);

     focusbox = find_focusbox(iform->formlist);

     if(focusbox == NULL) {
	  /* nothing focused */
	  goto done;
     }

     focusform = (struct formfill *) gtk_object_get_data(GTK_OBJECT(focusbox),
							 "formfill");

     if(focusform == NULL)
	  /* field can't be resized anyway */
	  return;

     iform->focusform = focusform;

     ff = (struct formfill *) gtk_object_get_data(GTK_OBJECT(focusbox),
						  "formfill");
     if(!ff) goto done;

     if (wanted_dt == DISPLAYTYPE_ENTRY) {
	  ff->displaytype = DISPLAYTYPE_ENTRY;
	  ff->dt_handler = get_dt_handler(ff->displaytype);
     } else if(wanted_dt == DISPLAYTYPE_TEXT) {
	  ff->displaytype = DISPLAYTYPE_TEXT;
	  ff->dt_handler = get_dt_handler(ff->displaytype);
     }

     /* redraw */
     clear_table(iform);

     gtk_widget_hide(iform->table);
     build_inputform(error_context, iform);
     gtk_widget_show(iform->table);

 done:
     error_flush(error_context);
}


static void do_hide_empty_attributes(int hidden, struct inputform *iform)
{
     GList *formlist;
     GList *children;
     GtkWidget *child;
     struct formfill *ff;
     int i = 0;
     int displaytype;
     int hideme;

     for (formlist=iform->formlist ; formlist ; formlist = formlist->next ) {
	  ff = (struct formfill *) formlist->data;
	  hideme = 1;
	  i++;

	  displaytype = ff->displaytype;

	  for (children = ff->widgetList ; children ;
	       children = children->next) {
	       child = GTK_WIDGET(children->data);

	       if (hidden) {
		    if (ff && displaytype) {
			 GQTypeDisplayClass* klass = g_type_class_ref(ff->dt_handler);
			 GByteArray *ndata = NULL;

			 if (klass && klass->get_data) {
			      ndata = klass->get_data(ff, child);
			 }
			 /* don't bother adding in empty fields */
			 if (ndata) {
			      hideme = 0;
			      g_byte_array_free(ndata, 1);
			 }
			 g_type_class_unref(klass);
		    }
	       } else {
		    if (child) gtk_widget_show(child);
	       }
	  }

	  if (hidden && hideme) {
	       if (ff->event_box) gtk_widget_hide(ff->event_box);
	       if (ff->label) gtk_widget_hide(ff->label);
	       if (ff->vbox) gtk_widget_hide(ff->vbox);
	       if (ff->morebutton) gtk_widget_hide(ff->morebutton);
	       for (children = ff->widgetList ; children ; 
		    children = children->next) {
		    child = GTK_WIDGET(children->data);
		    if (child) gtk_widget_hide(child);
	       }
	  } else {
	       if (ff->event_box) gtk_widget_show(ff->event_box);
	       if (ff->label) gtk_widget_show(ff->label);
	       if (ff->vbox) gtk_widget_show(ff->vbox);
	       if (ff->morebutton) gtk_widget_show(ff->morebutton);
	  }
     }
}


static void
hide_empty_attributes(GtkToggleToolButton *button, struct inputform *iform) {
     int hidden;

     hidden = gtk_toggle_tool_button_get_active(button);
     do_hide_empty_attributes(hidden, iform);
     /* store hide status, to be able to keep this info for
      * the next LDAP object to be shown in this browser */
     iform->hide_status = hidden;
}


void set_hide_empty_attributes(int hidden, struct inputform *iform)
{

     if(iform->hide_attr_button) {
	  gtk_toggle_tool_button_set_active(GTK_TOGGLE_TOOL_BUTTON(iform->hide_attr_button), hidden);
     }
     do_hide_empty_attributes(hidden, iform);
}


/* Datastructure used to communicate between the functions making up the
   add attribute dialog */

typedef struct {
     int breakloop;
     int rc;
     int destroyed;
     gchar **outbuf;
     GtkWidget *combo;
     GList *entries;
} attr_dialog_comm;


/* The set of functions implementing the dialog for the new attribute
   button for extensibleObjects */

/* gtk2 checked (multiple destroy callbacks safety), confidence 1 */
static void attr_destroy(GtkWidget *window, attr_dialog_comm *comm) {
     if (comm) {
	  comm->breakloop = 1;
	  comm->destroyed = 1;
     }
}

static void attr_ok(GtkWidget *button, attr_dialog_comm *comm) {
     *(comm->outbuf) = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(comm->combo)->entry), 0, -1);

     if (g_list_find_custom(comm->entries,
			    *(comm->outbuf),
			    (GCompareFunc) strcmp)) {
	  comm->breakloop = 1;
	  comm->rc = 1;
     } else {
	  *(comm->outbuf) = NULL;
     }

}

static void attr_cancel(GtkWidget *button, attr_dialog_comm *comm) {
     comm->breakloop = 1;
     comm->rc = 0;
}

/* pops up a dialog to select an attribute type via a GtkCombo. This
   functions waits for the data and puts it into outbuf. */

static int attr_popup(int error_context, 
		      const char *title,
		      GqServer *server,
		      gchar **outbuf,
		      GtkWidget *modal_for)
{
     GtkWidget *window, *vbox0, *vbox1, *vbox2, *label, *button, *hbox0;
     GtkWidget *f = gtk_grab_get_current();
     GList *gl;
     struct server_schema *ss;

     attr_dialog_comm comm = { 0, 0, 0, NULL, NULL, NULL };
     comm.outbuf = outbuf;
     *outbuf = NULL;

     if (modal_for) {
	  modal_for = gtk_widget_get_toplevel(modal_for);
     }

     ss = get_schema(error_context, server);

     if (!ss) {
	  error_push(error_context, _("Server schema not available."));
	  goto done;
     }
     
     /* This is a BAD hack - it solves a problem with the query popup
        dialog that locks up focus handling with all the
        window-managers I have been able to test this with. Actually,
        it should be sufficient to let go of the focus, but
        hiding/showing seems to work... (as I do not know how to
        release the focus in gtk) - Any gtk Hackers around? */
     if (f != NULL) {
	 gtk_widget_hide(f);
	 gtk_widget_show(f);
     }

     window = gtk_dialog_new();
/*       gtk_container_border_width(GTK_CONTAINER(window), 0); */
     gtk_window_set_title(GTK_WINDOW(window), title);
     gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
     g_signal_connect(window, "destroy",
			G_CALLBACK(attr_destroy),
			&comm);
     g_signal_connect_swapped(window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               window);

     vbox0 = GTK_DIALOG(window)->vbox;
     gtk_widget_show(vbox0);

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_container_border_width(GTK_CONTAINER(vbox1), CONTAINER_BORDER_WIDTH);
     gtk_box_pack_start(GTK_BOX(vbox0), vbox1, TRUE, TRUE, 0);
     
     label = gtk_label_new(title);
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(vbox1), label, TRUE, TRUE, 0);

     comm.combo = gtk_combo_new();
     gtk_combo_set_value_in_list(GTK_COMBO(comm.combo), TRUE, TRUE);

     comm.entries = NULL;
     
     for (gl = ss->at ; gl ; gl = gl->next) {
	  LDAPAttributeType *at = (LDAPAttributeType *) gl->data;
	  
	  if (at && at->at_names) {
	       int i;
	       for (i = 0 ; at->at_names[i] ; i++) {
		    comm.entries = g_list_append(comm.entries, at->at_names[i]);
	       }
	  }
     }

     comm.entries = g_list_sort(comm.entries, (GCompareFunc) strcmp);
     comm.entries = g_list_insert(comm.entries, "", 0);
	  
     gtk_combo_set_popdown_strings(GTK_COMBO(comm.combo),
				   comm.entries);
    
     GTK_WIDGET_SET_FLAGS(comm.combo, GTK_CAN_FOCUS);
     GTK_WIDGET_SET_FLAGS(comm.combo, GTK_CAN_DEFAULT);


     gtk_widget_set_sensitive(GTK_COMBO(comm.combo)->entry, FALSE);

     gtk_widget_show(comm.combo);
     gtk_box_pack_end(GTK_BOX(vbox1), comm.combo, TRUE, TRUE, 0);

     vbox2 = GTK_DIALOG(window)->action_area;
/*       gtk_container_border_width(GTK_CONTAINER(vbox2), CONTAINER_BORDER_WIDTH); */
     gtk_widget_show(vbox2);

     hbox0 = gtk_hbutton_box_new();
     gtk_widget_show(hbox0);
     gtk_box_pack_start(GTK_BOX(vbox2), hbox0, TRUE, TRUE, 0);

     button = gtk_button_new_from_stock(GTK_STOCK_OK);

     g_signal_connect(button, "clicked",
			G_CALLBACK(attr_ok), &comm);
     gtk_box_pack_start(GTK_BOX(hbox0), button, FALSE, FALSE, 0);
     GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
     GTK_WIDGET_SET_FLAGS(button, GTK_RECEIVES_DEFAULT);
     gtk_widget_grab_default(button);
     gtk_widget_show(button);

     button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

     g_signal_connect(button, "clicked",
			G_CALLBACK(attr_cancel),
			&comm);

     gtk_box_pack_end(GTK_BOX(hbox0), button, FALSE, FALSE, 0);
     gtk_widget_show(button);

/*       gtk_window_set_transient_for(GTK_WINDOW(window),  */
/*  				  GTK_WINDOW(getMainWin())); */


     gtk_widget_grab_focus(GTK_WIDGET(window));
     gtk_window_set_modal(GTK_WINDOW(window), TRUE);

     gtk_widget_show(window);
     gtk_widget_grab_focus(comm.combo);

     while (!comm.breakloop) {
	  gtk_main_iteration();
     }

     if (!comm.destroyed) {
	  gtk_widget_destroy(window);
     }

     if (comm.entries) g_list_free(comm.entries);
     comm.entries = NULL;

 done:
     return comm.rc;
}

/* Checks if the objectClass attribute contains "extensibleObject" */

static int is_extensible_object(struct inputform *iform)
{
     GList *f, *wl;
     GtkWidget *w;
     GByteArray *ndata = NULL;
     struct formfill *ff;

     if(!iform->formlist) return 0;

     for (f = iform->formlist ; f ; f = f->next) {
	  ff = (struct formfill *) f->data;

	  if (strcasecmp(ff->attrname, "objectClass") == 0) {
	       GQTypeDisplayClass* klass = NULL;
	       for (wl = ff->widgetList ; wl ; wl = wl->next) {
		    w = GTK_WIDGET(wl->data);

		    klass = g_type_class_ref(ff->dt_handler);

		    if (klass && klass->get_data) {
			 ndata = klass->get_data(ff, w);

			 if (ndata) {
			      if (strncasecmp((gchar*)ndata->data,
					      "extensibleObject",
					      ndata->len) == 0) {

				   g_byte_array_free(ndata, TRUE);
				   return 1;
			      }
			      g_byte_array_free(ndata, TRUE);
			 }
		    }
		    g_type_class_unref(klass);
	       }
	  }
     }

     return 0;
}


static void create_new_attr(GtkButton *button, struct inputform *iform)
{
     LDAPAttributeType *at;
     struct formfill *ff;
     GqServer *server;
     int rc;
     char *outbuf;
     int error_context;

     error_context = error_new_context(_("Creating new attribute"),
				       GTK_WIDGET(button));

     server = iform->server;
     if(!is_extensible_object(iform)) {
	  error_push(error_context, _("Not an 'extensibleObject'"));
	  goto done;
     }

     rc = attr_popup(error_context,
		     _("Select name of new attribute"),
		     server,
		     &outbuf,
		     iform->parent_window);

     if (rc && strlen(outbuf) > 0) {
	  at = find_canonical_at_by_at(get_schema(error_context, server), outbuf);
	  if (at) {
	       ff = new_formfill();
	       g_assert(ff);

	       ff->server = g_object_ref(server);
	       g_free(ff->attrname);
	       ff->attrname = g_strdup(outbuf);
	       ff->flags |= FLAG_EXTENSIBLE_OBJECT_ATTR;
	       if (at->at_single_value) {
		    ff->flags |= FLAG_SINGLE_VALUE;
	       }
	       set_displaytype(error_context, server, ff);
	       iform->formlist = formlist_append(iform->formlist, ff);

	       build_or_update_inputform(error_context, iform, FALSE);
	  }
     }

     if (outbuf) g_free(outbuf);

 done:
     error_flush(error_context);
}


/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
