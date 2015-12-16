/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (dt_clist.c) is
    Copyright (C) 2002 by Peter Stamfest and Bert Vermeulen

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

/* $Id: dt_clist.c 942 2006-08-04 14:14:36Z herzi $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#ifdef HAVE_LIBCRYPTO

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#define GTK_ENABLE_BROKEN  /* for the text widget - should be replaced - FIXME */
#include <gtk/gtk.h>

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/asn1.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>

#include "common.h"
#include "util.h"
#include "input.h"	/* CONTAINER_BORDER_WIDTH */
#include "formfill.h"
#include "dt_clist.h"

static void dt_clist_details_button_clicked(GtkButton* button,
					   GtkWidget *data_widget);

GtkWidget *dt_clist_get_widget(int error_context,
			       struct formfill *form,
			       GByteArray *data,
			       GCallback *activatefunc,
			       gpointer funcdata)
{
     GQTypeDisplayClass* klass = g_type_class_ref(form->dt_handler);
     GtkWidget *w = dt_generic_binary_get_widget(error_context, form, data,
						 activatefunc, funcdata);

     GtkWidget *menu = dt_generic_binary_retrieve_menu_widget(w);
     GtkWidget *item, *s;

     s = gtk_hseparator_new();
     gtk_widget_show(s);

     item = gtk_menu_item_new();
     gtk_widget_show(item);
     gtk_container_add(GTK_CONTAINER(item), s);

     gtk_menu_append(GTK_MENU(menu), item);

     item = gtk_menu_item_new_with_label(_("Details..."));
     gtk_widget_show(item);

     gtk_object_set_data(GTK_OBJECT(w), "details", GTK_WIDGET(item));

     g_signal_connect(item, "activate",
			G_CALLBACK(dt_clist_details_button_clicked),
			w);

/*       gtk_widget_set_sensitive(item, FALSE); */
     gtk_menu_append(GTK_MENU(menu), item);

     /* PSt: FIXME: hardcoded sizes are BAD */
     if (data) {
	  DT_CLIST(klass)->dt_generic.show_entries(form, w, TRUE);
	  gtk_widget_set_usize(w, -1, DT_CLIST_FULL_HEIGHT);
     } else {
	  DT_CLIST(klass)->dt_generic.show_entries(form, w, FALSE);
	  gtk_widget_set_usize(w, -1, DT_CLIST_EMPTY_HEIGHT);
     }

     g_type_class_unref(klass);

     return w;
}

GtkWidget *dt_clist_get_data_widget(struct formfill *form, 
				    GCallback *activatefunc,
				    gpointer funcdata) 
{
     GtkWidget *data_widget;
     GtkWidget *clist;
     int i;

     data_widget = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(data_widget),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
     gtk_widget_show(data_widget);

     clist = gtk_clist_new(2);
     gtk_widget_show(clist);

     for ( i = 0 ; i < 2 ; i ++ ) {
	  gtk_clist_set_column_width(GTK_CLIST(clist), i, 
				     gtk_clist_optimal_column_width(GTK_CLIST(clist), i));
     }

     gtk_container_add(GTK_CONTAINER(data_widget), clist);

     return data_widget;
}

static void dt_clist_details_button_clicked(GtkButton* button,
					    GtkWidget *widget)
{
     GtkWidget *window, *vbox, *hbox1, *ok_button, *scrwin;
     GtkWidget *text;
     GtkWidget *data_widget = 
	  dt_generic_binary_retrieve_data_widget(widget);
     struct formfill *form;
     GQTypeDisplayClass* klass;
     GtkCList *clist;
     GByteArray *data = gtk_object_get_data(GTK_OBJECT(data_widget), "data");

     form = dt_generic_binary_retrieve_formfill(widget);
     clist = (GtkCList*) GTK_BIN(data_widget)->child;

     klass =  g_type_class_ref(form->dt_handler);

     window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
     gtk_container_border_width(GTK_CONTAINER(window), 
				CONTAINER_BORDER_WIDTH);
     gtk_window_set_title(GTK_WINDOW(window), _("Attribute Details"));
     gtk_window_set_default_size(GTK_WINDOW(window), 670, 560);

     /* What is this? PS: 20030929 - FIXME */
     g_signal_connect_swapped(window, "destroy",
                               G_CALLBACK(gtk_widget_destroy),
                               window);
     g_signal_connect_swapped(window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               window);

     vbox = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox);
     gtk_container_add(GTK_CONTAINER(window), vbox);

     hbox1 = gtk_hbutton_box_new();
     gtk_widget_show(hbox1);
/*      gtk_container_border_width(GTK_CONTAINER(hbox1), 12); */
     gtk_box_pack_end(GTK_BOX(vbox), hbox1, FALSE, FALSE, 5);

     ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);
     gtk_widget_show(ok_button);
     gtk_box_pack_start(GTK_BOX(hbox1), ok_button, FALSE, FALSE, 0);
     GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
     GTK_WIDGET_SET_FLAGS(ok_button, GTK_RECEIVES_DEFAULT);
     gtk_widget_grab_default(ok_button);
     g_signal_connect_swapped(ok_button, "clicked",
                               G_CALLBACK(gtk_widget_destroy),
                               window);


     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
     gtk_widget_show(scrwin);
     gtk_box_pack_start(GTK_BOX(vbox), scrwin, TRUE, TRUE, 0);


     text = gtk_text_new(NULL, NULL);
     gtk_widget_show(text);
     gtk_container_add(GTK_CONTAINER(scrwin), text);

     if (DT_CLIST(klass)->fill_details) {
	  DT_CLIST(klass)->fill_details(form,
						   data_widget, text,
						   data, GTK_WIDGET(clist));
     }

     gtk_widget_show(window);
     g_type_class_unref(klass);
}

void free_internal_data(GByteArray *gb) {
     if (gb) g_byte_array_free(gb, TRUE);
}

void dt_clist_store_data(struct formfill *form, 
			 GtkWidget *hbox,
			 GtkWidget *data_widget,
			 const GByteArray *data)
{
     GtkCList *clist = (GtkCList*) GTK_BIN(data_widget)->child;
     int i;

     gtk_clist_freeze(clist);
     gtk_clist_clear(clist);

     gtk_object_remove_data(GTK_OBJECT(data_widget), "data"); 

     if(data) {
	  GByteArray *internal = g_byte_array_new();
	  g_byte_array_append(internal, data->data, data->len);

	  gtk_object_set_data_full(GTK_OBJECT(data_widget), "data", 
				   internal,
				   (GtkDestroyNotify) free_internal_data);
	  if (internal->len > 0) {
	       GQTypeDisplayClass* klass = g_type_class_ref(form->dt_handler);
	       if (DT_CLIST(klass)->fill_clist) {
		    DT_CLIST(klass)->fill_clist(form, hbox,
							   data_widget,
							   internal, 
							   GTK_WIDGET(clist));
	       }
	       g_type_class_unref(klass);
	  }

	  for ( i = 0 ; i < 2 ; i ++ ) {
	       gtk_clist_set_column_width(GTK_CLIST(clist), i, 
					  gtk_clist_optimal_column_width(clist, i));
	  }
     }
     gtk_clist_thaw(clist);
}

GByteArray *dt_clist_get_data(struct formfill *form, GtkWidget *hbox)
{
     GtkWidget *data_widget = dt_generic_binary_retrieve_data_widget(hbox);
     GtkWidget *widget = GTK_BIN(data_widget)->child;
     
     if(widget) {
	  GByteArray *internal;
	  GByteArray *copy = NULL;
	  internal = (GByteArray *) gtk_object_get_data(GTK_OBJECT(data_widget),
							"data");

	  if (internal) {
	       copy = g_byte_array_new();
	       g_byte_array_append(copy, internal->data, internal->len);
	  }

	  return copy;
     }
     return NULL;
}

void dt_clist_delete_data(struct formfill *form,
			  GtkWidget *hbox, 
			  GtkWidget *data_widget)
{
     GtkCList *clist = (GtkCList*) GTK_BIN(data_widget)->child;
     int i;

     gtk_clist_freeze(clist);
     gtk_clist_clear(clist);
     
     gtk_object_remove_data(GTK_OBJECT(data_widget), "data");
     gtk_widget_set_usize(hbox, -1, DT_CLIST_EMPTY_HEIGHT);

     for ( i = 0 ; i < 2 ; i ++ ) {
	  gtk_clist_set_column_width(clist, i, 
				     gtk_clist_optimal_column_width(clist, i));
     }
	  
     gtk_clist_thaw(clist);
}

void dt_clist_show_entries(struct formfill *form,
			   GtkWidget *hbox, gboolean what)
{
     GtkWidget *b;
     gpointer *p = gtk_object_get_data(GTK_OBJECT(hbox), "details");

     if (p) {
	  b = GTK_WIDGET(p);
	  dt_generic_binary_show_entries(form, hbox, what);

	  if (b) {
	       gtk_widget_set_sensitive(b, what);
	  }
     }
}

/* GType */
G_DEFINE_ABSTRACT_TYPE(GQDisplayCList, gq_display_clist, GQ_TYPE_DISPLAY_BINARY_GENERIC);

static void
gq_display_clist_init(GQDisplayCList* self) {}

static void
gq_display_clist_class_init(GQDisplayCListClass* self_class) {
	GQTypeDisplayClass* gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);
	GQDisplayBinaryGenericClass* gdbg_class = GQ_DISPLAY_BINARY_GENERIC_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Column List");
	gtd_class->selectable = FALSE;
	gtd_class->show_in_search_result = FALSE;
	gtd_class->get_widget = dt_clist_get_widget;
	gtd_class->get_data = dt_clist_get_data;
	gtd_class->set_data = dt_generic_binary_set_data; // reused from dt_generic_binary
	gtd_class->buildLDAPMod = bervalLDAPMod;

	gdbg_class->encode = NULL;
	gdbg_class->decode = NULL;
	gdbg_class->get_data_widget = dt_clist_get_data_widget;
	gdbg_class->store_data = dt_clist_store_data;
	gdbg_class->delete_data = dt_clist_delete_data;
	gdbg_class->show_entries = dt_clist_show_entries;

	self_class->fill_clist = NULL;
	self_class->fill_details = NULL;
}

#endif /* HAVE_LIBCRYPTO */

