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

/* $Id: template.h 955 2006-08-31 19:15:21Z herzi $ */

#ifndef GQ_TEMPLATE_H_INCLUDED
#define GQ_TEMPLATE_H_INCLUDED

#include <glib.h>
#include "common.h"

struct gq_template {
    char *name;
    GList *objectclasses;
};

struct gq_template *new_template(void);
void free_template(struct gq_template *);

void create_template_edit_window(GqServer *server, 
				 const char *template_name, 
				 GtkWidget *parent);
gboolean delete_edit_window(GtkWidget *window);
void fill_new_template(GtkWidget *window);
void arrow_button_callback(GtkWidget *widget, GtkWidget *window);
int clistcasecmp(GtkCList *clist, gconstpointer row1, gconstpointer row2);
void move_objectclass(GtkWidget *source, GtkWidget *destination, 
		      const char *objectclass);
void save_template_callback(GtkWidget *widget, GtkWidget *window);
struct gq_template *window2template(GtkWidget *window);
void fill_clist_templates(GtkWidget *clist);
char *get_clist_selection(GtkWidget *clist);
int get_clist_row_by_text(GtkWidget *clist, const char *text);


#endif
