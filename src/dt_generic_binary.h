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

/* $Id: dt_generic_binary.h 937 2006-05-25 13:51:47Z herzi $ */

#ifndef DT_GENERIC_BINARY_H_INCLUDED
#define DT_GENERIC_BINARY_H_INCLUDED

#include "gq-type-display.h"

G_BEGIN_DECLS

typedef        GQTypeDisplay              GQDisplayBinaryGeneric;
typedef struct _dt_generic_binary_handler GQDisplayBinaryGenericClass;

#define GQ_TYPE_DISPLAY_BINARY_GENERIC         (gq_display_binary_generic_get_type())
#define GQ_DISPLAY_BINARY_GENERIC_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_DISPLAY_BINARY_GENERIC, GQDisplayBinaryGenericClass))
#define DT_GENERIC_BINARY(objpointer)          GQ_DISPLAY_BINARY_GENERIC_CLASS(objpointer)

GType gq_display_binary_generic_get_type(void);

typedef struct _dt_generic_binary_handler {
     GQTypeDisplayClass dt_handler;

     GByteArray* (*encode)(const char *val, int len);
     GByteArray* (*decode)(const char *val, int len);

     GtkWidget* (*get_data_widget)(struct formfill *form,
				   GCallback *activatefunc,
				   gpointer funcdata);
     void (*store_data)(struct formfill *form,
			GtkWidget *hbox,
			GtkWidget *data_widget,
			const GByteArray *data);
     void (*delete_data)(struct formfill *form,
			 GtkWidget *hbox,
			 GtkWidget *data_widget);
     void (*show_entries)(struct formfill *form,
			  GtkWidget *hbox, gboolean what);
} dt_generic_binary_handler;

GQTypeDisplay *dt_generic_binary_get_handler();

/* Methods, only to be used by subclasses */
GtkWidget *dt_generic_binary_get_widget(int error_context,
					struct formfill *form,
					GByteArray *data,
					GCallback *activatefunc,
					gpointer funcdata);
void dt_generic_binary_set_data(struct formfill *form, GByteArray *data,
				GtkWidget *hbox);

void dt_generic_binary_show_entries(struct formfill *form,
				    GtkWidget *hbox, gboolean what);

/* helper functions (non-methods), only to be used by subclasses */
GtkWidget *dt_generic_binary_retrieve_data_widget(GtkWidget *hbox);
GtkWidget *dt_generic_binary_retrieve_menu_widget(GtkWidget *hbox);
GtkWidget *dt_generic_binary_retrieve_vbox_widget(GtkWidget *hbox);
struct formfill *dt_generic_binary_retrieve_formfill(GtkWidget *hbox);

G_END_DECLS

#endif

/*
   Local Variables:
   c-basic-offset: 5
   End:
 */
