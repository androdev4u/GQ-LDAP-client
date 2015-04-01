/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (dt_clist.h) is
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

/* $Id: dt_clist.h 937 2006-05-25 13:51:47Z herzi $ */

#ifndef DT_CLIST_H_INCLUDED
#define DT_CLIST_H_INCLUDED

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#ifdef HAVE_LIBCRYPTO

#include "syntax.h"
#include "dt_generic_binary.h"

typedef GQDisplayBinaryGeneric      GQDisplayCList;
typedef struct _GQDisplayCListClass GQDisplayCListClass;

#define DT_CLIST_EMPTY_HEIGHT 25
#define DT_CLIST_FULL_HEIGHT 100

#define GQ_TYPE_DISPLAY_CLIST         (gq_display_clist_get_type())
#define GQ_DISPLAY_CLIST_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_DISPLAY_CLIST, GQDisplayCListClass))
#define DT_CLIST(objpointer)          GQ_DISPLAY_CLIST_CLASS(objpointer)

GType gq_display_clist_get_type(void);

struct _GQDisplayCListClass {
     GQDisplayBinaryGenericClass dt_generic;

     void (*fill_clist)(struct formfill *form,
			GtkWidget *hbox, GtkWidget *data_widget,
			GByteArray *internal,
			GtkWidget *clist);
     void (*fill_details)(struct formfill *form,
			  GtkWidget *hbox, GtkWidget *data_widget,
			  GByteArray *internal,
			  GtkWidget *clist);
};

/* Methods, only to be used by subclasses */
GtkWidget *dt_clist_get_widget(int error_context,
			       struct formfill *form,
			       GByteArray *data,
			       GCallback *activatefunc,
			       gpointer funcdata);

GtkWidget *dt_clist_get_data_widget(struct formfill *form,
				    GCallback *activatefunc,
				    gpointer funcdata);

GByteArray *dt_clist_get_data(struct formfill *form,
			      GtkWidget *hbox);

void dt_clist_store_data(struct formfill *form, 
			 GtkWidget *hbox,
			 GtkWidget *data_widget,
			 const GByteArray *data);

void dt_clist_delete_data(struct formfill *form,
			  GtkWidget *hbox, 
			  GtkWidget *data_widget);

void dt_clist_show_entries(struct formfill *form, 
			   GtkWidget *hbox, gboolean what);

void free_internal_data(GByteArray *gb);

#endif /* HAVE_LIBCRYPTO */

#endif

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
