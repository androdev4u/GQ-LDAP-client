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

/* $Id: dt_text.h 937 2006-05-25 13:51:47Z herzi $ */

#ifndef DT_TEXT_H_INCLUDED
#define DT_TEXT_H_INCLUDED

#include "dt_entry.h"

typedef GQDisplayEntry      GQDisplayText;
typedef GQDisplayEntryClass GQDisplayTextClass;

#define GQ_TYPE_DISPLAY_TEXT         (gq_display_text_get_type())
#define DT_TEXT(objpointer) ((dt_text_handler *)(objpointer))

GType gq_display_text_get_type(void);

/* Methods, only to be used by subclasses */
GtkWidget *dt_text_get_widget(int error_context,
			      struct formfill *form, GByteArray *data,
			      GCallback *activatefunc,
			      gpointer funcdata);

#define DEFAULT_LINES 5

#endif
