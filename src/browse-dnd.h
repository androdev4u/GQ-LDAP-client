/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (browse-dnd.h) is
    Copyright (C) 2002 by Peter Stamfest <peter@stamfest.at>

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

/* $Id: browse-dnd.h 952 2006-08-26 11:17:35Z herzi $ */

#ifndef GQ_BROWSE_DND_H_INCLUDED
#define GQ_BROWSE_DND_H_INCLUDED

#include <gtk/gtk.h>
#include "common.h"

#include "gq-tab.h"  /* GqTab */

/* Flags used in new_dnd_refresh on top of the options for
   refresh_subtree_with_options. Thus these option values share the
   same "space". Watch out! */

#define SELECT_AT_END		256

void browse_dnd_setup(GtkWidget *ctreeroot, GqTab *tab);
void copy_entry(GtkWidget *widget, GqTab *tab);
void copy_entry_all(GtkWidget *widget, GqTab *tab);
void paste_entry(GtkWidget *widget, GqTab *tab);

#endif

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
