/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (dt_date.h) is
    Copyright (C) 2006 by Philipp Hahn

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

#ifndef DT_DATE_H_INCLUDED
#define DT_DATE_H_INCLUDED

#include "config.h"

#include "syntax.h"

typedef GQTypeDisplay      GQDisplayDate;
typedef GQTypeDisplayClass GQDisplayDateClass;

#define GQ_TYPE_DISPLAY_DATE         (gq_display_date_get_type())
#define DT_DATE(objpointer) ((dt_date_handler *)(objpointer))

GType gq_display_date_get_type(void);

GtkWidget *dt_date_retrieve_inputbox(GtkWidget *widget);

#endif /* DT_DATE_H_INCLUDED */

