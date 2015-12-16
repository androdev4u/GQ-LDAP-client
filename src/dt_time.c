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

/* $Id: dt_time.c 964 2006-09-02 23:18:58Z herzi $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

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
#include "dt_time.h"

static GtkWidget *dt_time_get_widget(int error_context,
				     struct formfill *form,
				     GByteArray *data,
				     GCallback *activatefunc,
				     gpointer funcdata);

static GByteArray *dt_time_get_data(struct formfill *form,
				    GtkWidget *widget);

static void dt_time_set_data(struct formfill *form,
			     GByteArray *data,
			     GtkWidget *widget);

typedef struct _cbdata {
     struct formfill *form;
     GtkWidget *hbox;
     GtkWidget *editwindow;
} cbdata;

/*
 * parse a generalized time into a struct tm. Is there a better way to
 * do this?? I wish I could use regular expressions for this.
 */

static int parse_time(const char *data, struct tm *tm, int *offset) {
     int n = 0, i, N;
     const char *c;
     char fmt[20];

     struct {
	  const char *fmt;
	  int *ptr;
	  int max;
     } parts[] = {
	  { "%4u", &tm->tm_year, 9999 },
	  { "%2u", &tm->tm_mon, 12 },
	  { "%2u", &tm->tm_mday, 31 },
	  { "%2u", &tm->tm_hour, 23 },
	  { "%2u", &tm->tm_min, 59 },
	  { "%2u", &tm->tm_sec, 59 },
	  { NULL, NULL, 0 },
     };

     if (!data) return 0;
     if (!tm) return 0;

     memset(tm, 0, sizeof(struct tm));

     for (i = 0, c = data ; parts[i].fmt ; i++) {
	  if (!isdigit(*c)) break;

	  /* construct format */
	  g_snprintf(fmt, sizeof(fmt), "%s%%n", parts[i].fmt);
	  if (sscanf(c, fmt, parts[i].ptr, &N)) {
	       if (N > 0) {
		    if (*parts[i].ptr > parts[i].max) {
			 *parts[i].ptr = parts[i].max;
		    }
		    if (*parts[i].ptr < 0) {
			 *parts[i].ptr = 0;
		    }
		    c += N;
		    n++;
	       } else {
		    break;
	       }
	  }
     }

     if (n >= 1) {
	  tm->tm_year -= 1900;
     }
     if (n >= 2) {
	  tm->tm_mon--;
     }

     if (offset && *c) {
	  /* have timezone! */

	  /* special case: UTC (Zulu) */
	  if (c[0] == 'Z') {
	       *offset = 0;
	       n++;
	  } else {
	       if (sscanf(c, "%d", offset)) n++;
	  }
     }

     return n;
}

static void tz_value_changed_callback(GtkAdjustment *adjustment,
				      GtkSpinButton *spin)
{
     int v = gtk_spin_button_get_value_as_int(spin);
     int sign = v > 0 ? 1 : -1;
     int h, m;

     v = ((v > 0) ? v : -v);
     h = v / 100;
     m = v % 100;

     if (m >= 60) {
	  v = sign * (100 * h + 59);
	  gtk_spin_button_set_value(spin, (gfloat) v);
     }
}

static void dt_time_ok_callback(GtkWidget *button, cbdata *cbd)
{
     GtkWidget *editwindow = cbd->editwindow;
     GtkWidget *w;
     guint y, m, d, H = 0, M = 0, S = 0, zone = 0;
     char buffer[60];

     w = gtk_object_get_data(GTK_OBJECT(editwindow), "calendar");
     if (w) {
	  gtk_calendar_get_date(GTK_CALENDAR(w), &y, &m, &d);
	  m++;
     }
     w = gtk_object_get_data(GTK_OBJECT(editwindow), "hour");
     if (w) H = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

     w = gtk_object_get_data(GTK_OBJECT(editwindow), "minute");
     if (w) M = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

     w = gtk_object_get_data(GTK_OBJECT(editwindow), "second");
     if (w) S = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

     w = gtk_object_get_data(GTK_OBJECT(editwindow), "timezone");
     if (w) zone = gtk_spin_button_get_value_as_int(GTK_SPIN_BUTTON(w));

     if (S != 0) {
	  g_snprintf(buffer, sizeof(buffer), "%04d%02d%02d%02d%02d%02d",
		  y, m, d, H, M, S);
     } else {
	  /* optimize zero seconds away */
	  g_snprintf(buffer, sizeof(buffer), "%04d%02d%02d%02d%02d",
		   y, m, d, H, M);
     }

     if (zone == 0) {
	  strcat(buffer, "Z");
     } else {
	  g_snprintf(buffer + strlen(buffer), sizeof(buffer) - strlen(buffer),
		   "%+05d", zone);
     }

/*      printf("%s\n", buffer); */

     w = gtk_object_get_data(GTK_OBJECT(cbd->hbox), "inputbox");
     gtk_entry_set_text(GTK_ENTRY(w), buffer);
     gtk_widget_destroy(editwindow);
}

static void dt_time_edit_window(GtkWidget *button, cbdata *cbd)
{
     GtkWidget *hbox = cbd->hbox;
     GtkWidget *inputbox = dt_time_retrieve_inputbox(hbox);
     GtkWidget *editwindow, *main_vbox, *calendar, *w;
     GtkWidget *hbox2, *button2;
     GtkAdjustment *adj;
     char titlebuffer[1024]; /* FIXME: fixed buffer length */

     struct tm tm;
     int y, m, d, offset;
     int ofs_h, ofs_m, ofs_sign;
     time_t t;
     struct tm *l;
     gchar *content;
#if ! defined(HAVE_TM_GMTOFF) && defined(HAVE_TIMEZONE)
     extern long timezone;
#endif

     time(&t);
     l = localtime(&t); /* implicitly calls tzset() for us */

     y = l->tm_year + 1900;
     m = l->tm_mon + 1;
     d = l->tm_mday;

#ifdef HAVE_TM_GMTOFF
     ofs_sign = l->TM_GMTOFF < 0 ? 1 : -1;

     ofs_m = (l->TM_GMTOFF > 0 ? l->TM_GMTOFF : -l->TM_GMTOFF) / 60;

     ofs_h = ofs_m / 60;
     ofs_m = ofs_m % 60;
#else /* HAVE_TM_GMTOFF */
#    ifdef HAVE_TIMEZONE
     ofs_sign = timezone > 0 ? 1 : -1;

     ofs_m = (timezone > 0 ? timezone : -timezone) / 60;

     ofs_h = ofs_m / 60;
     ofs_m = ofs_m % 60;

     /* extern long timezone does not take DST into account */
     if (l->tm_isdst) {
	  ofs_h++;
     }

#    else /* HAVE_TIMEZONE */

     ofs_h = ofs_m = 0;

#    endif /* HAVE_TIMEZONE */
#endif /* HAVE_TM_GMTOFF */


     /* NOTE: generalizedTime includes offset relative to GMT while
	the timezone variable hold seconds west of GMT */
     offset = -ofs_sign * (100 * ofs_h + ofs_m);

#ifdef GTK_WINDOW_DIALOG
     editwindow = gtk_window_new(GTK_WINDOW_DIALOG);
#else
     editwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
#endif
     cbd->editwindow = editwindow;

     g_snprintf(titlebuffer, sizeof(titlebuffer),
	      _("%s: choose date and time"), cbd->form->attrname);

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
	  int n = parse_time(content, &tm, &offset);
/*	  printf("n=%d\n", n); */
	  if (n >= 1)
	       y = tm.tm_year + 1900;
	  if (n >= 2)
	       m = tm.tm_mon + 1;
	  if (n >= 3)
	       d = tm.tm_mday;
	  g_free(content);
     }

     gtk_calendar_select_month(GTK_CALENDAR(calendar), m - 1, y);
     gtk_calendar_select_day(GTK_CALENDAR(calendar), d);

     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_box_pack_start(GTK_BOX(main_vbox), hbox2, TRUE, TRUE, 5);
     gtk_widget_show(hbox2);

     w = gtk_label_new(_("Time [hh:mm:ss]"));
     gtk_box_pack_start(GTK_BOX(hbox2), w, FALSE, TRUE, 0);
     gtk_widget_show(w);

     w = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0,
							       0.0, 23.0,
							       1.0, 1.0,
							       1.0)),
			     1.0, 0);
     gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), tm.tm_hour);

     gtk_box_pack_start(GTK_BOX(hbox2), w, FALSE, FALSE, 10);
     gtk_widget_show(w);

     gtk_object_set_data(GTK_OBJECT(editwindow), "hour", w);

     w = gtk_label_new(_(":"));
     gtk_box_pack_start(GTK_BOX(hbox2), w, FALSE, TRUE, 0);
     gtk_widget_show(w);

     w = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0,
							       0.0, 59.0,
							       1.0, 1.0,
							       1.0)),
			     1.0, 0);
     gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), tm.tm_min);
     gtk_box_pack_start(GTK_BOX(hbox2), w, FALSE, FALSE, 10);
     gtk_widget_show(w);

     gtk_object_set_data(GTK_OBJECT(editwindow), "minute", w);

     w = gtk_label_new(_(":"));
     gtk_box_pack_start(GTK_BOX(hbox2), w, FALSE, TRUE, 0);
     gtk_widget_show(w);

     w = gtk_spin_button_new(GTK_ADJUSTMENT(gtk_adjustment_new(0.0,
							       0.0, 59.0,
							       1.0, 1.0,
							       1.0)),
			     1.0, 0);
     gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), tm.tm_sec);
     gtk_box_pack_start(GTK_BOX(hbox2), w, FALSE, FALSE, 10);
     gtk_widget_show(w);

     gtk_object_set_data(GTK_OBJECT(editwindow), "second", w);

     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_box_pack_start(GTK_BOX(main_vbox), hbox2, TRUE, TRUE, 0);
     gtk_widget_show(hbox2);

     w = gtk_label_new(_("Timezone (Offset) [+-hhmm]"));
     gtk_box_pack_start(GTK_BOX(hbox2), w, FALSE, TRUE, 0);
     gtk_widget_show(w);

     adj = GTK_ADJUSTMENT(gtk_adjustment_new(0.0,
					     -1259.0, 1259.0,
					     100.0, 100.0,
					     100.0));
     w = gtk_spin_button_new(adj, 100.0, 0);
     gtk_spin_button_set_snap_to_ticks(GTK_SPIN_BUTTON(w), FALSE);
     gtk_spin_button_set_value(GTK_SPIN_BUTTON(w), offset);
     g_signal_connect(adj, "value-changed",
			G_CALLBACK(tz_value_changed_callback),
			w);

     gtk_box_pack_start(GTK_BOX(hbox2), w, TRUE, TRUE, 10);
     gtk_widget_show(w);

     gtk_object_set_data(GTK_OBJECT(editwindow), "timezone", w);

     hbox2 = gtk_hbutton_box_new();
     gtk_box_pack_start(GTK_BOX(main_vbox), hbox2, FALSE, TRUE, 5);
     gtk_widget_show(hbox2);

     button2 = gtk_button_new_from_stock(GTK_STOCK_OK);
     gtk_widget_show(button2);
     g_signal_connect(button2, "clicked",
			G_CALLBACK(dt_time_ok_callback),
			cbd);
     gtk_box_pack_start(GTK_BOX(hbox2), button2, FALSE, TRUE, 0);
     GTK_WIDGET_SET_FLAGS(button2, GTK_CAN_DEFAULT);
     gtk_widget_grab_default(button2);

     button2 = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
     gtk_box_pack_end(GTK_BOX(hbox2), button2, FALSE, TRUE, 0);
     g_signal_connect_swapped(button2, "clicked",
			       G_CALLBACK(gtk_widget_destroy),
			       editwindow);
     gtk_widget_show(button2);

     gtk_widget_show(editwindow);
}

static GtkWidget *dt_time_get_widget(int error_context,
				     struct formfill *form,
				     GByteArray *data,
				     GCallback *activatefunc,
				     gpointer funcdata)
{
     GtkWidget *hbox, *inputbox, *button;
     cbdata *cbd;

     hbox = gtk_hbox_new(FALSE, 5);

     inputbox = gtk_entry_new();
     if(activatefunc)
	  g_signal_connect_swapped(inputbox, "activate",
				    G_CALLBACK(activatefunc),
				    funcdata);

     gtk_box_pack_start(GTK_BOX(hbox), inputbox, TRUE, TRUE, 0);
     gtk_widget_show(inputbox);

     button = gtk_button_new_with_label(_("..."));
     gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, TRUE, 0);

     cbd = g_malloc(sizeof(cbdata));
     cbd->form = form;
     cbd->hbox = hbox;

     g_signal_connect(button, "clicked",
			G_CALLBACK(dt_time_edit_window),
			cbd);

     gtk_object_set_data_full(GTK_OBJECT(hbox), "cbdata", cbd, g_free);

     gtk_widget_show(button);

     gtk_object_set_data(GTK_OBJECT(hbox), "inputbox", inputbox);
     gtk_object_set_data(GTK_OBJECT(hbox), "button", button);

     dt_time_set_data(form, data, hbox);

     return hbox;
}

static GByteArray *dt_time_get_data(struct formfill *form,
				    GtkWidget *widget)
{
     return editable_get_text(GTK_EDITABLE(dt_time_retrieve_inputbox(widget)));
}

static void dt_time_set_data(struct formfill *form,
			     GByteArray *data,
			     GtkWidget *widget)
{
     editable_set_text(GTK_EDITABLE(dt_time_retrieve_inputbox(widget)),
		       data,
		       NULL, NULL);
}

GtkWidget *dt_time_retrieve_inputbox(GtkWidget *hbox)
{
     return gtk_object_get_data(GTK_OBJECT(hbox), "inputbox");
}

/* GType */
G_DEFINE_TYPE(GQDisplayTime, gq_display_time, GQ_TYPE_TYPE_DISPLAY);

static void
gq_display_time_init(GQDisplayTime* self) {}

static void
gq_display_time_class_init(GQDisplayTimeClass* self_class) {
	GQTypeDisplayClass* gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Generalized Time");
	gtd_class->selectable = TRUE;
	gtd_class->show_in_search_result = TRUE;
	gtd_class->get_widget = dt_time_get_widget;
	gtd_class->get_data = dt_time_get_data;
	gtd_class->set_data = dt_time_set_data;
	gtd_class->buildLDAPMod = bervalLDAPMod;
}

