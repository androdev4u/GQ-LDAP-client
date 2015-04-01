/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 by Peter Stamfest and Bert Vermeulen

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

/* $Id: dt_numstr.h 937 2006-05-25 13:51:47Z herzi $ */

#ifndef DT_NUMSTR_H_INCLUDED
#define DT_NUMSTR_H_INCLUDED

#include "formfill.h"
#include "syntax.h"

#include "dt_entry.h"

typedef GQDisplayEntry      GQDisplayNumstr;
typedef GQDisplayEntryClass GQDisplayNumstrClass;

#define GQ_TYPE_DISPLAY_NUMSTR         (gq_display_numstr_get_type())
#define GQ_DISPLAY_NUMSTR_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_DISPLAY_ENTRY, GQDisplayNumstrClass))
#define DT_NUMSTR(objpointer)          GQ_DISPLAY_NUMSTR_CLASS(objpointer)

GType gq_display_numstr_get_type(void);

/* Methods, only to be used by subclasses */
GtkWidget *dt_numstr_get_widget(int error_context,
				struct formfill *form, GByteArray *data,
				GCallback *activatefunc,
				gpointer funcdata);
GByteArray *dt_numstr_get_data(struct formfill *form, GtkWidget *widget);
void dt_numstr_set_data(struct formfill *form, GByteArray *data,
			GtkWidget *widget);

GtkWidget *dt_numstr_retrieve_inputbox(GtkWidget *hbox);

#endif

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
