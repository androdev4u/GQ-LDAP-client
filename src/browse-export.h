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

/* $Id: browse-export.h 960 2006-09-02 20:42:46Z herzi $ */

#ifndef GQ_BROWSE_EXPORT_H_INCLUDED
#define GQ_BROWSE_EXPORT_H_INCLUDED

#include <glib.h>		/* GList */
#include <gtk/gtk.h>		/* GtkWidget */

#include "gq-server.h"		/* GqServer */

/* to_export is a GList of dn_on_server objects */
void export_many(int error_context, GtkWidget *transient_for, 
		 GList *to_export);

#endif


/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */

