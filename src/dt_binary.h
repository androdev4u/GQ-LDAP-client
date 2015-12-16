/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (dt_binary.h) is
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

/* $Id: dt_binary.h 937 2006-05-25 13:51:47Z herzi $ */

#ifndef DT_BINARY_H_INCLUDED
#define DT_BINARY_H_INCLUDED

#include "syntax.h"
#include "dt_generic_binary.h"

typedef GQDisplayBinaryGeneric      GQDisplayBinary;
typedef GQDisplayBinaryGenericClass GQDisplayBinaryClass;

#define GQ_TYPE_DISPLAY_BINARY         (gq_display_binary_get_type())
#define DT_BINARY(objpointer) ((dt_binary_handler*)(objpointer))

GType gq_display_binary_get_type(void);

/* Methods, only to be used by subclasses */
GtkWidget *dt_binary_get_widget(int error_context,
				struct formfill *form,
				GByteArray *data,
				GCallback *activatefunc,
				gpointer funcdata);

GtkWidget *dt_binary_get_data_widget(struct formfill *form,
				     GCallback *activatefunc,
				     gpointer funcdata);
GByteArray *dt_binary_get_data(struct formfill *form, GtkWidget *widget);

void dt_binary_store_data(struct formfill *form,
			  GtkWidget *hbox,
			  GtkWidget *data_widget,
			  const GByteArray *data);

void dt_binary_delete_data(struct formfill *form,
			   GtkWidget *hbox,
			   GtkWidget *data_widget);

GByteArray *dt_b64_encode(const char *val, int len);
GByteArray *dt_b64_decode(const char *val, int len);
GByteArray *dt_hex_encode(const char *val, int len);
GByteArray *dt_hex_decode(const char *val, int len);

#endif

