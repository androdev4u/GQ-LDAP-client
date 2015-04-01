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

/* $Id: dt_numstr.c 952 2006-08-26 11:17:35Z herzi $ */

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

#include "dt_numstr.h"

#include "common.h"
#include "gq-tab-browse.h"
#include "util.h"
#include "errorchain.h"
#include "input.h"
#include "tinput.h"
#include "encode.h"
#include "ldif.h" /* for b64_decode */
#include "syntax.h"
#include "dtutil.h"

static void dt_numstr_verify(GtkWidget *entry)
{
     char *txt = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);
     gunichar c;

     if (txt) {
	  GString *s = g_string_sized_new(strlen(txt));
	  int nope = 0;
	  char *t;
	  gunichar space = g_utf8_get_char(" ");

	  /* allow only digits and spaces */
	  for(t = txt, c = g_utf8_get_char(t) ;
	      c ;
	      t = g_utf8_next_char(t), c = g_utf8_get_char(t)) {
	       if (g_unichar_isdigit(c) || c == space) {
		    g_string_append_unichar(s, c);
	       } else {
		    nope = 1;
	       }
	  }

	  if (nope) {
	       gtk_entry_set_text(GTK_ENTRY(entry), s->str);
	  }

	  g_string_free(s, TRUE);
	  g_free(txt);
     }
     return;
}

GtkWidget *dt_numstr_get_widget(int error_context,
				struct formfill *form, GByteArray *data,
				GCallback *activatefunc,
				gpointer funcdata)
{
    GtkWidget *hbox, *inputbox, *label;

    hbox = gtk_hbox_new(FALSE, 0);
    gtk_widget_show(hbox);

    inputbox = gtk_entry_new();
    gtk_widget_show(inputbox);
    if(activatefunc)
	g_signal_connect_swapped(inputbox, "activate",
				  G_CALLBACK(activatefunc),
				  funcdata);

    g_signal_connect(inputbox, "changed",
		       G_CALLBACK(dt_numstr_verify),
		       NULL);

    gtk_box_pack_start(GTK_BOX(hbox), inputbox, TRUE, TRUE, 0);

    label = gtk_label_new(_("(ns)"));
    gtk_widget_show(label);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    gtk_object_set_data(GTK_OBJECT(hbox), "inputbox", inputbox);

    dt_numstr_set_data(form, data, hbox);

    return hbox;
}

GByteArray *dt_numstr_get_data(struct formfill *form, GtkWidget *widget)
{
    return editable_get_text(GTK_EDITABLE(dt_numstr_retrieve_inputbox(widget)));
}

void dt_numstr_set_data(struct formfill *form, GByteArray *data,
		       GtkWidget *widget)
{
     GQTypeDisplayClass* klass = g_type_class_ref(form->dt_handler);
     editable_set_text(GTK_EDITABLE(dt_numstr_retrieve_inputbox(widget)),
		       data,
		       DT_NUMSTR(klass)->encode,
		       DT_NUMSTR(klass)->decode);
     g_type_class_unref(klass);
}

GtkWidget *dt_numstr_retrieve_inputbox(GtkWidget *hbox)
{
     return gtk_object_get_data(GTK_OBJECT(hbox), "inputbox");
}

/* GType */
G_DEFINE_TYPE(GQDisplayNumstr, gq_display_numstr, GQ_TYPE_DISPLAY_ENTRY);

static void
gq_display_numstr_init(GQDisplayNumstr* self) {}

static void
gq_display_numstr_class_init(GQDisplayNumstrClass* self_class) {
	GQTypeDisplayClass* gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);
	GQDisplayEntryClass* gde_class = GQ_DISPLAY_ENTRY_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Numeric String");
	gtd_class->selectable = TRUE;
	gtd_class->show_in_search_result = TRUE;
	gtd_class->get_widget = dt_numstr_get_widget;
	gtd_class->get_data = dt_numstr_get_data;
	gtd_class->set_data = dt_numstr_set_data;
	gtd_class->buildLDAPMod = bervalLDAPMod;

	gde_class->encode = NULL;
	gde_class->decode = NULL;
}

