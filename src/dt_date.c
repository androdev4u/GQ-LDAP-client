/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 2006 Philipp Hahn

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

#include <config.h>

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <time.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#include "common.h"
#include "util.h"
#include "formfill.h"
#include "dtutil.h"
#include "dt_date.h"

static GtkWidget *dt_date_get_widget(int error_context,
				     struct formfill *form,
				     GByteArray *data,
				     GtkSignalFunc *activatefunc,
				     gpointer funcdata);

static GByteArray *dt_date_get_data(struct formfill *form,
				    GtkWidget *widget);

static void dt_date_set_data(struct formfill *form,
			     GByteArray *data,
			     GtkWidget *widget);

typedef struct _cbdata {
     struct formfill *form;
     GtkWidget *hbox;
     GtkWidget *editwindow;
} cbdata;

static void dt_date_ok_callback(GtkWidget *button, cbdata *cbd)
{
     GtkWidget *editwindow = cbd->editwindow;
     GtkWidget *w;
     guint y, m, d;
     int sign;
     char buffer[10];
     time_t t = 0;
     struct tm tm;

     w = gtk_object_get_data(GTK_OBJECT(editwindow), "calendar");
     if (w) {
	  gtk_calendar_get_date(GTK_CALENDAR(w), &y, &m, &d);

	  memset(&tm, 0, sizeof(struct tm));
	  tm.tm_year = y - 1900;
	  tm.tm_mon = m;
	  tm.tm_mday = d;
	  t = mktime(&tm);
	  t /= 24*60*60;
     }

     snprintf(buffer, sizeof(buffer), "%ld", t);

     w = gtk_object_get_data(GTK_OBJECT(cbd->hbox), "inputbox");
     gtk_entry_set_text(GTK_ENTRY(w), buffer);
     gtk_widget_destroy(editwindow);
}

static void dt_date_edit_window(GtkWidget *button, cbdata *cbd)
{
     GtkWidget *hbox = cbd->hbox;
     GtkWidget *inputbox = dt_date_retrieve_inputbox(hbox);
     GtkWidget *editwindow, *main_vbox, *calendar, *w;
     GtkWidget *hbox2, *button2;
     GtkAdjustment *adj;
     char titlebuffer[1024]; /* FIXME: fixed buffer length */

     struct tm tm;
     int y, m, d;
     time_t t;
     struct tm *l;
     gchar *content;

     time(&t);
     l = localtime(&t); /* implicitly calls tzset() for us */

     y = l->tm_year;
     m = l->tm_mon;
     d = l->tm_mday;

     editwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
     cbd->editwindow = editwindow;

     snprintf(titlebuffer, sizeof(titlebuffer),
	      _("%s: choose date"), cbd->form->attrname);

     gtk_window_set_title(GTK_WINDOW(editwindow), titlebuffer);
     gtk_window_set_policy(GTK_WINDOW(editwindow), TRUE, TRUE, FALSE);

     main_vbox = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(main_vbox), 5);
     gtk_widget_show(main_vbox);
     gtk_container_add(GTK_CONTAINER(editwindow), main_vbox);

     calendar = gtk_calendar_new();
     gtk_box_pack_start(GTK_BOX(main_vbox), calendar, TRUE, TRUE, 0);
     gtk_widget_show(calendar);

     gtk_object_set_data(GTK_OBJECT(editwindow), "calendar", calendar);

     content = gtk_editable_get_chars(GTK_EDITABLE(inputbox), 0, -1);
     if (content) {
	  sscanf(content, "%ld", &t);
	  t *= 24*60*60;
	  gmtime_r(&t, &tm);
	  y = tm.tm_year;
	  m = tm.tm_mon;
	  d = tm.tm_mday;
	  g_free(content);
     }

     gtk_calendar_select_month(GTK_CALENDAR(calendar), m, y + 1900);
     gtk_calendar_select_day(GTK_CALENDAR(calendar), d);

     hbox2 = gtk_hbutton_box_new();
     gtk_box_pack_start(GTK_BOX(main_vbox), hbox2, FALSE, TRUE, 5);
     gtk_widget_show(hbox2);

     button2 = gtk_button_new_from_stock(GTK_STOCK_OK);
     gtk_widget_show(button2);
     gtk_signal_connect(GTK_OBJECT(button2), "clicked",
			GTK_SIGNAL_FUNC(dt_date_ok_callback),
			(gpointer) cbd);
     gtk_box_pack_start(GTK_BOX(hbox2), button2, FALSE, TRUE, 0);
     GTK_WIDGET_SET_FLAGS(button2, GTK_CAN_DEFAULT);
     gtk_widget_grab_default(button2);

     button2 = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
     gtk_box_pack_end(GTK_BOX(hbox2), button2, FALSE, TRUE, 0);
     gtk_signal_connect_object(GTK_OBJECT(button2), "clicked",
			       GTK_SIGNAL_FUNC(gtk_widget_destroy),
			       (gpointer) editwindow);
     gtk_widget_show(button2);

     gtk_widget_show(editwindow);
}

static GtkWidget *dt_date_get_widget(int error_context,
				     struct formfill *form,
				     GByteArray *data,
				     GtkSignalFunc *activatefunc,
				     gpointer funcdata)
{
     GtkWidget *hbox, *inputbox, *button;
     cbdata *cbd;

     hbox = gtk_hbox_new(FALSE, 5);

     inputbox = gtk_entry_new();
     if(activatefunc)
	  gtk_signal_connect_object(GTK_OBJECT(inputbox), "activate",
				    GTK_SIGNAL_FUNC(activatefunc),
				    (gpointer) funcdata);

     gtk_box_pack_start(GTK_BOX(hbox), inputbox, TRUE, TRUE, 0);
     gtk_widget_show(inputbox);

     button = gtk_button_new_with_label("...");
     gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, TRUE, 0);

     cbd = g_malloc(sizeof(cbdata));
     cbd->form = form;
     cbd->hbox = hbox;

     gtk_signal_connect(GTK_OBJECT(button), "clicked",
			GTK_SIGNAL_FUNC(dt_date_edit_window),
			(gpointer) cbd);

     gtk_object_set_data_full(GTK_OBJECT(hbox), "cbdata", cbd, g_free);

     gtk_widget_show(button);

     gtk_object_set_data(GTK_OBJECT(hbox), "inputbox", inputbox);
     gtk_object_set_data(GTK_OBJECT(hbox), "button", button);

     dt_date_set_data(form, data, hbox);

     return hbox;
}

static GByteArray *dt_date_get_data(struct formfill *form,
				    GtkWidget *widget)
{
     return editable_get_text(GTK_EDITABLE(dt_date_retrieve_inputbox(widget)));
}

static void dt_date_set_data(struct formfill *form,
			     GByteArray *data,
			     GtkWidget *widget)
{
     editable_set_text(GTK_EDITABLE(dt_date_retrieve_inputbox(widget)),
		       data,
		       NULL, NULL);
}

GtkWidget *dt_date_retrieve_inputbox(GtkWidget *hbox)
{
     return gtk_object_get_data(GTK_OBJECT(hbox), "inputbox");
}

/* GType */
G_DEFINE_TYPE(GQDisplayDate, gq_display_date, GQ_TYPE_TYPE_DISPLAY);

static void
gq_display_date_init(GQDisplayDate* self) {}

static void
gq_display_date_class_init(GQDisplayDateClass* self_class) {
	GQTypeDisplayClass* gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Generalized Date");
	gtd_class->selectable = TRUE;
	gtd_class->show_in_search_result = TRUE;
	gtd_class->get_widget = dt_date_get_widget;
	gtd_class->get_data = dt_date_get_data;
	gtd_class->set_data = dt_date_set_data;
	gtd_class->buildLDAPMod = bervalLDAPMod;
};

