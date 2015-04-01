/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (dt_time.h) is
    Copyright (C) 2002 by Peter Stamfest

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

/* $Id: dt_time.h 937 2006-05-25 13:51:47Z herzi $ */

#ifndef DT_TIME_H_INCLUDED
#define DT_TIME_H_INCLUDED

#include "syntax.h"

typedef GQTypeDisplay      GQDisplayTime;
typedef GQTypeDisplayClass GQDisplayTimeClass;

#define GQ_TYPE_DISPLAY_TIME         (gq_display_time_get_type())
#define DT_TIME(objpointer) ((dt_time_handler *)(objpointer))

GType gq_display_time_get_type(void);

GtkWidget *dt_time_retrieve_inputbox(GtkWidget *widget);

#endif /* DT_TIME_H_INCLUDED */

