/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 Peter Stamfest <peter@stamfest.at>

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

/* $Id: prefs.h 955 2006-08-31 19:15:21Z herzi $ */

#ifndef GQ_PREFS_H_INCLUDED
#define GQ_PREFS_H_INCLUDED

#include <gtk/gtk.h>
#include "common.h"

#include "mainwin.h"

/* fwd decl */
struct prefs_widgets;

void create_edit_server_window(GqServer *server, 
			       GtkWidget *modalFor);
void fill_serverlist_serverstab(void);
void create_prefs_window(struct mainwin_data *win);

#endif

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
