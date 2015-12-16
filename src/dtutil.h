/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (dtutil.c) is
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

/* $Id: dtutil.h 931 2006-05-24 14:22:01Z herzi $ */

#ifndef GQ_DTUTIL_H_INCLUDED
#define GQ_DTUTIL_H_INCLUDED

#include <gtk/gtk.h>

void editable_changed_cb(GtkWidget *object, gpointer user_data);
void editable_set_text(GtkEditable *entry, GByteArray *data,
		       GByteArray* (*encode)(const char *val, int len),
		       GByteArray* (*decode)(const char *val, int len));
GByteArray *editable_get_text(GtkEditable *entry);

#endif

/*
   Local Variables:
   c-basic-offset: 5
   End:
 */
