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

/* $Id: filter.h 818 2003-11-03 21:07:58Z stamfest $ */

#ifndef GQ_FILTER_H_INCLUDED
#define GQ_FILTER_H_INCLUDED

#define FILTER_TABSIZE           5
#define FILTER_EDIT_FONT         big_fixed

#define MAX_FILTERNAME_LEN       128
#define MAX_LDAPFILTER_LEN       1024

#include "common.h"

struct gq_filter {
     char *name;
     char *ldapfilter;
     char *servername;
     char *basedn;
};

struct gq_filter *new_filter(void);
void free_filter(struct gq_filter *filter);
void copy_filter(struct gq_filter *target, const struct gq_filter *source);

/* struct gq_filter *check_filtername(const char *filtername); */
void add_filter(GtkWidget *filternamebox);
/* void name_popup(void); */
void filter_selected(struct gq_filter *filter);
void show_filters(void);
/* void filterlist_row_selected(GtkCList *filter_clist, int row, int column, */
/* 				  GdkEventButton *event, gpointer data); */
/* void filterlist_row_unselected(GtkCList *filter_clist, int row, int column, */
/* 			       GdkEventButton *event, gpointer data); */
void add_new_filter_callback(GtkCList *filter_clist);
/* void delete_filter(GtkWidget *widget, GtkCList *filter_clist); */
/* void remove_from_filtermenu(struct gq_filter *filter); */
/* void edit_filter(GtkCList *filter_clist, int is_new_filter, int row, struct gq_filter *filter); */
/* void save_filter(GtkWidget *window); */
/* char *indent_filter(char *filter); */
/* char *unindent_filter(char *indented); */
/* void indent_toggled(GtkToggleButton *indent, gpointer editbox); */


#endif /* GQ_FILTER_H_INCLUDED */
