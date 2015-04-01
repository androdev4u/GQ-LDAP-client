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

/* $Id: dt_entry.c 975 2006-09-07 18:44:41Z herzi $ */

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

#include "dt_entry.h"

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

GtkWidget *dt_entry_get_widget(int error_context,
			       struct formfill *form, GByteArray *data,
			       GCallback *activatefunc,
			       gpointer funcdata)
{
    GtkWidget *inputbox;


    inputbox = gtk_entry_new();
    if(activatefunc)
	g_signal_connect_swapped(inputbox, "activate",
				  G_CALLBACK(activatefunc),
				  funcdata);
    dt_entry_set_data(form, data, inputbox);

    return inputbox;
}

GByteArray *dt_entry_get_data(struct formfill *form, GtkWidget *widget)
{
    return editable_get_text(GTK_EDITABLE(widget));
}

void dt_entry_set_data(struct formfill *form, GByteArray *data,
		       GtkWidget *widget)
{
	GQTypeDisplayClass* klass = g_type_class_ref(form->dt_handler);
	editable_set_text(GTK_EDITABLE(widget), data,
		       DT_ENTRY(klass)->encode,
		       DT_ENTRY(klass)->decode);
	g_type_class_unref(klass);
}

/* GType */
G_DEFINE_TYPE(GQDisplayEntry, gq_display_entry, GQ_TYPE_TYPE_DISPLAY);

static void
gq_display_entry_init(GQDisplayEntry* self) {}

static void
gq_display_entry_class_init(GQDisplayEntryClass* self_class) {
	GQTypeDisplayClass* gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Entry");
	gtd_class->selectable = TRUE;
	gtd_class->show_in_search_result = TRUE;
	gtd_class->get_widget = dt_entry_get_widget;
	gtd_class->get_data   = dt_entry_get_data;
	gtd_class->set_data   = dt_entry_set_data;
	gtd_class->buildLDAPMod = bervalLDAPMod;

	self_class->encode = NULL;
	self_class->decode = NULL;
}

