/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (dt_jpeg.c) is
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

#include "dt_jpeg.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include <gdk-pixbuf/gdk-pixbuf.h>
#include <gdk-pixbuf/gdk-pixbuf-loader.h>

#include "common.h"
#include "util.h"
#include "errorchain.h"
#include "formfill.h"
#include "gq-tab-browse.h"
#include "input.h"
#include "tinput.h"
#include "encode.h"
#include "dt_jpeg.h"

/* 1x1 pixel transparent XPM */
static const char * empty_xpm[] = {
     "1 1 1 1",
     "       c None",
     " "
};


GtkWidget *dt_jpeg_get_data_widget(struct formfill *form,
				   GCallback *activatefunc,
				   gpointer funcdata)
{
     GtkWidget *alignment, *pixmap_widget = NULL;
     GdkPixbuf *pixbuf;
     GdkPixmap *pixmap;
     GdkBitmap *mask;

     alignment = gtk_alignment_new(0, 0, 0, 0);
     gtk_widget_show(alignment);

     pixbuf = gdk_pixbuf_new_from_xpm_data(empty_xpm);

     if (pixbuf) {
	  gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, &mask, 127);
	  gdk_pixbuf_unref(pixbuf);
	  pixmap_widget = gtk_pixmap_new(pixmap, mask);
	  gtk_widget_show(pixmap_widget);

	  gdk_pixmap_unref(pixmap);
	  if (mask) gdk_bitmap_unref(mask);

	  gtk_container_add(GTK_CONTAINER(alignment), pixmap_widget);
     }

     return alignment;
}

static void destroy_byte_array(gpointer gb) {
     g_byte_array_free((GByteArray*) gb, TRUE);
}

void dt_jpeg_store_data(struct formfill *form,
			GtkWidget *hbox,
			GtkWidget *data_widget,
			const GByteArray *data)
{
     if(data && data_widget) {
	  GtkWidget *pixmap_widget = GTK_BIN(data_widget)->child;
	  GdkPixbuf *pixbuf;

	  GdkPixmap *pixmap;
	  GdkBitmap *mask;

	  GByteArray *copy;
	  GdkPixbufLoader* loader = gdk_pixbuf_loader_new();

	  GError *error = NULL;

	  gdk_pixbuf_loader_write(loader, data->data, data->len, &error); /* FIXME - error handling */
	  if (!error) gdk_pixbuf_loader_close(loader, &error);
	  pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);

	  if (pixbuf) {
	       gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, &mask, 127);
	       gdk_pixbuf_unref(pixbuf);
	       gtk_pixmap_set(GTK_PIXMAP(pixmap_widget), pixmap, mask);

	       gdk_pixmap_unref(pixmap);
	       if (mask) gdk_bitmap_unref(mask);
	  }

	  copy = g_byte_array_new();
	  g_byte_array_append(copy, data->data, data->len);

	  gtk_object_set_data_full(GTK_OBJECT(data_widget), "data", copy,
				   destroy_byte_array);
	  /* FIXME: What's wrong with this: Either SHOULD work, shouldn't it?
	     Currently the app dumps core at random places if either
	     is in.  peter */
/*	  gtk_object_destroy(GTK_OBJECT(loader)); */
/*	  gtk_object_unref(GTK_OBJECT(loader)); */
     }
}

void dt_jpeg_delete_data(struct formfill *form,
			 GtkWidget *hbox_widget,
			 GtkWidget *data_widget)
{
     GtkWidget *pixmap_widget = GTK_BIN(data_widget)->child;
     GdkPixbuf *pixbuf;
     GdkPixmap *pixmap;
     GdkBitmap *mask;

     pixbuf = gdk_pixbuf_new_from_xpm_data(empty_xpm);

     if (pixbuf) {
	  gdk_pixbuf_render_pixmap_and_mask(pixbuf, &pixmap, &mask, 127);
	  gdk_pixbuf_unref(pixbuf);
	  gtk_pixmap_set(GTK_PIXMAP(pixmap_widget), pixmap, mask);

	  gdk_pixmap_unref(pixmap);
	  if (mask) gdk_bitmap_unref(mask);
     }

     gtk_object_remove_data(GTK_OBJECT(data_widget), "data");
}


GByteArray *dt_jpeg_get_data(struct formfill *form, GtkWidget *hbox)
{
     GByteArray *data;
     GByteArray *copy = NULL;
     GtkWidget *pixmap = dt_generic_binary_retrieve_data_widget(hbox);

     if (!pixmap) return NULL;

     data = gtk_object_get_data(GTK_OBJECT(pixmap), "data");
     if (data) {
	  copy = g_byte_array_new();
	  g_byte_array_append(copy, data->data, data->len);
     }
     return copy;
}

/* GType */
G_DEFINE_TYPE(GQDisplayJPEG, gq_display_jpeg, GQ_TYPE_DISPLAY_BINARY);

static void
gq_display_jpeg_init(GQDisplayJPEG* self) {}

static void
gq_display_jpeg_class_init(GQDisplayJPEGClass* self_class) {
	GQTypeDisplayClass* gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);
	GQDisplayBinaryGenericClass* gdbg_class = GQ_DISPLAY_BINARY_GENERIC_CLASS(self_class);

	gtd_class->name = Q_("displaytype|JPEG");
	gtd_class->selectable = TRUE;
	gtd_class->show_in_search_result = FALSE; // FIXME: make it possible to become true
	gtd_class->get_widget = dt_generic_binary_get_widget; // reused from dt_generic_binary
	gtd_class->get_data = dt_jpeg_get_data;
	gtd_class->set_data = dt_generic_binary_set_data; // reused from dt_generic_binary
	gtd_class->buildLDAPMod = bervalLDAPMod; // reused from display_type_handler

	gdbg_class->encode = NULL;
	gdbg_class->decode = NULL;
	gdbg_class->get_data_widget = dt_jpeg_get_data_widget;
	gdbg_class->store_data = dt_jpeg_store_data;
	gdbg_class->delete_data = dt_jpeg_delete_data;
	gdbg_class->show_entries = dt_generic_binary_show_entries;
}

