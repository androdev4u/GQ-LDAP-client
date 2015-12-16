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

/* $Id: dt_generic_binary.c 952 2006-08-26 11:17:35Z herzi $ */

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>

#define GTK_ENABLE_BROKEN  /* for the text widget - should be replaced */
#include <gtk/gtk.h>

#include "dt_generic_binary.h"

#include "common.h"
#include "errorchain.h"
#include "gq-tab-browse.h"
#include "syntax.h"
#include "util.h"
#include "formfill.h"
#include "input.h"
#include "tinput.h"
#include "encode.h"
#include "ldif.h" /* for b64_decode */

static void dt_generic_binary_import_button_clicked(GtkButton* button,
					    gpointer func_data);
static void dt_generic_binary_export_button_clicked(GtkButton* button,
					    gpointer func_data);
static void dt_generic_binary_delete_button_clicked(GtkButton* button,
					    GtkWidget *hbox);

GByteArray *dt_b64_encode(const char *val, int len);
GByteArray *dt_b64_decode(const char *val, int len);

void dt_generic_binary_show_entries(struct formfill *form,
				    GtkWidget *hbox, gboolean what)
{
     GtkWidget *b;

     b = GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(hbox), "export"));

     if (b) {
	  gtk_widget_set_sensitive(b, what);
     }

     b = GTK_WIDGET(gtk_object_get_data(GTK_OBJECT(hbox), "delete"));
     if (b) {
	  gtk_widget_set_sensitive(b, what);
     }
}


static void dt_generic_binary_create_menu(GtkWidget *hbox, GtkWidget *vbox,
					  GByteArray *data)
{
     GtkWidget *menu, *menu_item, *submenu;
     GtkWidget *arrow;
/*       GtkWidget *alignment; */

/*       alignment = gtk_alignment_new(0.5, 0, 0, 0); */
/*       gtk_widget_show(alignment); */
/*       gtk_box_pack_end(GTK_BOX(hbox), alignment, FALSE, FALSE, 0); */

     menu = gtk_menu_bar_new();
     gtk_widget_show(menu);
     gtk_box_pack_start(GTK_BOX(vbox), menu, FALSE, FALSE, 0);

     menu_item = gtk_menu_item_new();
     gtk_widget_show(menu_item);
     gtk_container_add(GTK_CONTAINER(menu), menu_item);

     arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_OUT);
     gtk_widget_show(arrow);
     gtk_container_add(GTK_CONTAINER(menu_item), arrow);

     submenu = gtk_menu_new();
     gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);

     gtk_object_set_data(GTK_OBJECT(hbox), "menu-widget", submenu);

     /* create buttons... */

     /* ------------------ Import -------------------- */

     menu_item = gtk_menu_item_new_with_label(_("Import..."));
     gtk_widget_show(menu_item);

     g_signal_connect(menu_item, "activate",
			G_CALLBACK(dt_generic_binary_import_button_clicked),
			hbox);

     gtk_menu_append(GTK_MENU(submenu), menu_item);

     /* ------------------ Export -------------------- */

     menu_item = gtk_menu_item_new_with_label(_("Export..."));
     gtk_widget_show(menu_item);
     gtk_widget_set_sensitive(menu_item, FALSE);

     g_signal_connect(menu_item, "activate",
			G_CALLBACK(dt_generic_binary_export_button_clicked),
			hbox);

     gtk_menu_append(GTK_MENU(submenu), menu_item);
     gtk_object_set_data(GTK_OBJECT(hbox), "export", GTK_WIDGET(menu_item));

     /* ------------------ Delete -------------------- */
     menu_item = gtk_menu_item_new_with_label(_("Delete"));
     gtk_widget_show(menu_item);
     gtk_widget_set_sensitive(menu_item, FALSE);

     g_signal_connect(menu_item, "activate",
			G_CALLBACK(dt_generic_binary_delete_button_clicked),
			hbox);

     gtk_menu_append(GTK_MENU(submenu), menu_item);
     gtk_object_set_data(GTK_OBJECT(hbox), "delete", GTK_WIDGET(menu_item));
}

GtkWidget *dt_generic_binary_get_widget(int error_context,
					struct formfill *form,
					GByteArray *data,
					GCallback *activatefunc,
					gpointer funcdata)
{
     GQTypeDisplayClass* klass = g_type_class_ref(form->dt_handler);
     GtkWidget *hbox = gtk_hbox_new(FALSE, 5);
     GtkWidget *vbox;

     GtkWidget *data_widget =
	 DT_GENERIC_BINARY(klass)->get_data_widget(form,
							      activatefunc,
							      funcdata);

     gtk_box_pack_start(GTK_BOX(hbox), data_widget, TRUE, TRUE, 0);

     gtk_object_set_data(GTK_OBJECT(hbox), "data-widget", data_widget);
     gtk_object_set_data(GTK_OBJECT(hbox), "form", form);

     vbox = gtk_vbox_new(FALSE, 2);
     gtk_widget_show(vbox);
     gtk_box_pack_end(GTK_BOX(hbox), vbox, FALSE, TRUE, 0);

     gtk_object_set_data(GTK_OBJECT(hbox), "vbox-widget", vbox);

     dt_generic_binary_create_menu(hbox, vbox, data);

     DT_GENERIC_BINARY(klass)->store_data(form, hbox,
						     data_widget, data);

     if (data && data->len)
	  DT_GENERIC_BINARY(klass)->show_entries(form, hbox, TRUE);

     g_type_class_unref(klass);
     return hbox;
}

void dt_generic_binary_set_data(struct formfill *form, GByteArray *data,
				GtkWidget *hbox)
{
	GQTypeDisplayClass* klass = g_type_class_ref(form->dt_handler);
	DT_GENERIC_BINARY(klass)->store_data(form, hbox,
						     dt_generic_binary_retrieve_data_widget(hbox),
						     data);
	g_type_class_unref(klass);
}

/* The file selection widget and the string to store the chosen filename */


static GtkWidget *file_selector; /* FIXME - make non-global ASAP!!!!!!*/


static void dt_generic_binary_import_got_filename(GtkFileSelection *selector,
					  gpointer user_data)
{
     GQTypeDisplayClass* klass;
     GtkWidget *hbox = user_data;
     GtkWidget *data_widget;
     struct formfill *form;
     const char *selected_filename =
	  gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));

     GByteArray *ndata = NULL;

     int file;

     file = open(selected_filename, O_RDONLY);
     if (file >= 0) {
	  struct stat buf;
	  int rlen = 0;

	  ndata = g_byte_array_new();

	  if (fstat(file, &buf) == 0) {
	       if (g_byte_array_set_size(ndata, buf.st_size) != NULL) {
		    int n;

		    while ((n = read(file, ndata->data + rlen,
				    buf.st_size - rlen)) > 0) {
			 rlen += n;
			 /* check for overflow (concurrent update) */
			 if (rlen >= buf.st_size) break;
		    }
		    if (rlen < buf.st_size) {
			 g_byte_array_set_size(ndata, rlen);
		    }
	       } else {
		    g_byte_array_free(ndata, TRUE);
		    ndata = NULL;
		    /* ERROR - FIXME */
	       }
	  } else {
	       /* Error dialog - FIXME */
	       g_byte_array_free(ndata, TRUE);
	       ndata = NULL;
	  }

	  close(file);
     } else {
	  /* Error dialog - FIXME */
     }

     form = (struct formfill *) gtk_object_get_data(GTK_OBJECT(hbox),
						    "formfill");
     data_widget = dt_generic_binary_retrieve_data_widget(hbox);

     klass = g_type_class_ref(form->dt_handler);
     if (ndata && ndata->len > 0) {
	  DT_GENERIC_BINARY(klass)->store_data(form, hbox,
							  data_widget,
							  ndata);
	  g_byte_array_free(ndata, TRUE);

	  DT_GENERIC_BINARY(klass)->show_entries(form, hbox, TRUE);
     } else {
	  DT_GENERIC_BINARY(klass)->delete_data(form, hbox,
							   data_widget);
	  DT_GENERIC_BINARY(klass)->show_entries(form, hbox, FALSE);
     }

     g_type_class_unref(klass);
}

static void dt_generic_binary_import_button_clicked(GtkButton* button,
						    gpointer func_data)
{
     GtkWidget *hbox = func_data;

/*       struct formfill *form =  */
/*	  gtk_object_get_data(GTK_OBJECT(hbox), "formfill"); */

     /* Create the selector */

     file_selector =
	  gtk_file_selection_new(_("Please select a file containing data for this attribute"));

     gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));

     g_signal_connect(GTK_FILE_SELECTION(file_selector)->ok_button,
			"clicked", G_CALLBACK(dt_generic_binary_import_got_filename),
			hbox);

     /* Ensure that the dialog box is destroyed when the user clicks a button. */

     g_signal_connect_swapped(GTK_FILE_SELECTION(file_selector)->ok_button,
			       "clicked", G_CALLBACK(gtk_widget_destroy),
			       file_selector);

     g_signal_connect_swapped(GTK_FILE_SELECTION(file_selector)->cancel_button,
			       "clicked", G_CALLBACK(gtk_widget_destroy),
			       file_selector);

     /* Display that dialog */

     gtk_widget_show(file_selector);
     gtk_window_set_modal(GTK_WINDOW(file_selector), TRUE);
}


static void dt_generic_binary_export_got_filename(GtkFileSelection *selector,
						  gpointer user_data)
{
     GQTypeDisplayClass* klass;
     GtkWidget *hbox = user_data;
     const char *selected_filename =
	  gtk_file_selection_get_filename(GTK_FILE_SELECTION(file_selector));

     /* write file from pixbuf data */
     GByteArray *data;
     struct formfill *form;
     int file;

     form = (struct formfill *) gtk_object_get_data(GTK_OBJECT(hbox),
						    "formfill");
     klass = g_type_class_ref(form->dt_handler);
     data = DISPLAY_TYPE_HANDLER(klass)->get_data(form, hbox);
     g_type_class_unref(klass);

     if (data) {
	  file = open(selected_filename, O_WRONLY | O_TRUNC | O_CREAT, 0666);
	  if (file >= 0) {
	       int n;
	       unsigned int wrote = 0;

	       while( (n = write(file, data->data + wrote,
				 data->len - wrote)) > 0 &&
		      wrote < data->len) {
		    wrote += n;
	       }
	       if (n < 0) {
		    /* ERROR - show dialog - FIXME */
	       }
	       close(file);
	  }

	  g_byte_array_free(data, TRUE);
     }
}

static void dt_generic_binary_export_button_clicked(GtkButton* button,
						    gpointer func_data)
{
     GtkWidget *hbox = func_data;

     /* Create the selector */

     file_selector =
	  gtk_file_selection_new(_("Please select a file to export data to for this attribute"));
     gtk_file_selection_hide_fileop_buttons(GTK_FILE_SELECTION(file_selector));

     g_signal_connect(GTK_FILE_SELECTION(file_selector)->ok_button,
			"clicked", G_CALLBACK(dt_generic_binary_export_got_filename),
			hbox);

     /* Ensure that the dialog box is destroyed when the user clicks a button. */

     g_signal_connect_swapped(GTK_FILE_SELECTION(file_selector)->ok_button,
			       "clicked", G_CALLBACK(gtk_widget_destroy),
			       file_selector);

     g_signal_connect_swapped(GTK_FILE_SELECTION(file_selector)->cancel_button,
			       "clicked", G_CALLBACK(gtk_widget_destroy),
			       file_selector);

     /* Display that dialog */

     gtk_widget_show(file_selector);
     gtk_window_set_modal(GTK_WINDOW(file_selector), TRUE);

}

static void dt_generic_binary_delete_button_clicked(GtkButton* button,
						    GtkWidget *hbox)
{
     GQTypeDisplayClass* klass;
     GtkWidget *data_widget = dt_generic_binary_retrieve_data_widget(hbox);

     struct formfill *form;
     form = (struct formfill *) gtk_object_get_data(GTK_OBJECT(hbox),
						    "formfill");
     klass = g_type_class_ref(form->dt_handler);
     DT_GENERIC_BINARY(klass)->delete_data(form, hbox, data_widget);
     DT_GENERIC_BINARY(klass)->show_entries(form, hbox, FALSE);
     g_type_class_unref(klass);
}

/* Utility functions */

GtkWidget *dt_generic_binary_retrieve_data_widget(GtkWidget *hbox)
{
     return gtk_object_get_data(GTK_OBJECT(hbox), "data-widget");
}

GtkWidget *dt_generic_binary_retrieve_menu_widget(GtkWidget *hbox)
{
     return gtk_object_get_data(GTK_OBJECT(hbox), "menu-widget");
}

GtkWidget *dt_generic_binary_retrieve_vbox_widget(GtkWidget *hbox)
{
     return gtk_object_get_data(GTK_OBJECT(hbox), "vbox-widget");
}

struct formfill *dt_generic_binary_retrieve_formfill(GtkWidget *hbox)
{
     return gtk_object_get_data(GTK_OBJECT(hbox), "form");
}

/* GType */
G_DEFINE_ABSTRACT_TYPE(GQDisplayBinaryGeneric, gq_display_binary_generic, GQ_TYPE_TYPE_DISPLAY);

static void
gq_display_binary_generic_init(GQDisplayBinaryGeneric* self) {}

static void
gq_display_binary_generic_class_init(GQDisplayBinaryGenericClass* self_class) {
	GQTypeDisplayClass* gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Generic Binary");
	gtd_class->selectable = FALSE;
	gtd_class->show_in_search_result = TRUE;
	gtd_class->get_widget = dt_generic_binary_get_widget;
	gtd_class->get_data = NULL;
	gtd_class->set_data = NULL;
	gtd_class->buildLDAPMod = bervalLDAPMod;

	self_class->encode = NULL;
	self_class->decode = NULL;
	self_class->get_data_widget = NULL;
	self_class->store_data = NULL;
	self_class->delete_data = NULL;
	self_class->show_entries = dt_generic_binary_show_entries;
}

