/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2002 Bert Vermeulen
    Copyright (c) 2002-2003 Peter Stamfest <peter@stamfest.at>

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

/* $Id: tdefault.h 592 2003-09-29 19:24:17Z stamfest $ */

#ifndef GQ_TDEFAULT_H_INCLUDED
#define GQ_TDEFAULT_H_INCLUDED

#include <glib.h>
#include <gtk/gtk.h>

struct tdefault_ui {
     char *attr;
     GtkWidget *hidden;
     GtkWidget *type;
     GtkWidget *followattr;
     GtkWidget *value;
     GtkWidget *passwdscheme;
};

#define DEFAULT_TYPE_VALUE       1
#define DEFAULT_TYPE_FOLLOW      2
#define DEFAULT_TYPE_NEXTNUM     3
#define DEFAULT_TYPE_PASSWD      4

struct tdefault {
     char *attr;
     int hidden;
     int type;
     char *followattr;
     char *value;
     int passwdscheme;
};



void create_tdefault_edit_window(GtkWidget *dummy, GtkWidget *templatewin);
void tdefault_type_changed(struct tdefault_ui *tdui);

#endif /* GQ_TDEFAULT_H_INCLUDED */
