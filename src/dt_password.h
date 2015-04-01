/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen
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

/* $Id: dt_password.h 946 2006-08-24 17:37:36Z herzi $ */

#ifndef DT_PASSWORD_H_INCLUDED
#define DT_PASSWORD_H_INCLUDED

#include "dt_entry.h"
#include "gq-hash.h"

G_BEGIN_DECLS

typedef GQDisplayEntry      GQDisplayPassword;
typedef GQDisplayEntryClass GQDisplayPasswordClass;

#define GQ_TYPE_DISPLAY_PASSWORD         (gq_display_password_get_type())
#define GQ_DISPLAY_PASSWORD_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_DISPLAY_PASSWORD, GQDisplayPasswordClass))
#define DT_PASSWORD(objpointer)          GQ_DISPLAY_PASSWORD_CLASS(objpointer)

GType gq_display_password_get_type(void);

/* Methods, only to be used by subclasses */
GtkWidget *dt_password_get_widget(int error_context,
				  struct formfill *form, GByteArray *data,
				  GCallback *activatefunc,
				  gpointer funcdata);
GByteArray *dt_password_get_data(struct formfill *form, GtkWidget *widget);
void dt_password_set_data(struct formfill *form, GByteArray *data,
			  GtkWidget *widget);

G_END_DECLS

#endif /* DT_PASSWORD_H_INCLUDED */

