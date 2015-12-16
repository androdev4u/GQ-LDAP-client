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

/* $Id: dt_text.c 952 2006-08-26 11:17:35Z herzi $ */

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include "common.h"
#include "gq-tab-browse.h"
#include "util.h"
#include "errorchain.h"
#include "formfill.h"
#include "input.h"
#include "tinput.h"
#include "encode.h"
#include "ldif.h" /* for b64_decode */
#include "syntax.h"
#include "dt_text.h"

static GByteArray *dt_text_get_data(struct formfill *form, GtkWidget *widget);
static void dt_text_set_data(struct formfill *form, GByteArray *data,
			     GtkWidget *widget);

GtkWidget *dt_text_get_widget(int error_context,
			      struct formfill *form, GByteArray *data,
			      GCallback *activatefunc,
			      gpointer funcdata)
{
    GtkWidget *inputbox;

    GtkWidget *scrolled;

    inputbox = gtk_text_view_new();
    gtk_widget_show(inputbox);


    scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled),
				   GTK_POLICY_AUTOMATIC,
				   GTK_POLICY_AUTOMATIC);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled),
					GTK_SHADOW_IN);
    gtk_container_add(GTK_CONTAINER(scrolled), inputbox);
    gtk_widget_show(scrolled);

    dt_text_set_data(form, data, scrolled);

    return scrolled;
}


static GByteArray *dt_text_get_data(struct formfill *form, GtkWidget *widget)
{
     GtkWidget *text = gtk_bin_get_child(GTK_BIN(widget));
     GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));
     GtkTextIter s, e;
     gchar *c;
     GByteArray *a = NULL;

     gtk_text_buffer_get_start_iter(b, &s);
     gtk_text_buffer_get_end_iter(b, &e);
     c = gtk_text_buffer_get_text(b, &s, &e, TRUE);

     if (!c) return NULL;

     if (strlen(c) != 0) {
	  a = g_byte_array_new();
	  g_byte_array_append(a, (guchar*)c, strlen(c));
     }
     g_free(c);

     return a;
}






static void realize_text(GtkWidget *text, gpointer user_data)
{
     /* go through great lenghts to calculate the line height */

     int height;
     PangoContext   *ctx   = gtk_widget_get_pango_context(GTK_WIDGET(text));
     PangoLayout    *lay   = pango_layout_new(ctx);
     PangoRectangle  rect;

     pango_layout_set_single_paragraph_mode(lay, FALSE);

     pango_layout_set_text(lay, "X\nX", 3); /* two lines */
     pango_layout_get_pixel_extents(lay, &rect, NULL);
     height = rect.height;

     pango_layout_set_text(lay, "X", 1); /* one line */
     pango_layout_get_pixel_extents(lay, &rect, NULL);

     height -= rect.height;  /* difference is height of one line ... */

     g_object_unref(lay);

     gtk_widget_set_size_request(GTK_WIDGET(text), 100,
				 DEFAULT_LINES * height);
}

static void dt_text_set_data(struct formfill *form, GByteArray *data,
			     GtkWidget *widget)
{
     GtkWidget *text = gtk_bin_get_child(GTK_BIN(widget));
     GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(text));

     if (data) {
	  gtk_text_buffer_set_text(b, (gchar*) data->data, data->len);
	  g_signal_connect(text, "realize",
			     G_CALLBACK(realize_text), NULL);
     }
}

/* GType */
G_DEFINE_TYPE(GQDisplayText, gq_display_text, GQ_TYPE_DISPLAY_ENTRY);

static void
gq_display_text_init(GQDisplayText* self) {}

static void
gq_display_text_class_init(GQDisplayTextClass* self_class) {
	GQTypeDisplayClass * gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);
	GQDisplayEntryClass* gde_class = GQ_DISPLAY_ENTRY_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Multi-line Text");
	gtd_class->selectable = TRUE;
	gtd_class->show_in_search_result = TRUE;
	gtd_class->get_widget = dt_text_get_widget;
	gtd_class->get_data = dt_text_get_data;
	gtd_class->set_data = dt_text_set_data;
	gtd_class->buildLDAPMod = bervalLDAPMod; // reuse method from dt_entry

	gde_class->encode = NULL;
	gde_class->decode = NULL;
}

