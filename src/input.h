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

#ifndef GQ_INPUT_H_INCLUDED
#define GQ_INPUT_H_INCLUDED

#include <glib.h>
#include <gtk/gtk.h>
#include "common.h"

struct inputform {
     GtkWidget *parent_window;
     GtkWidget *target_vbox;
     GtkWidget *scwin;
     GtkWidget *hide_attr_button;
     GtkWidget *table;
     GtkWidget *add_as_new_button;
     GtkWidget *dn_widget;
     GQTreeWidget *ctreeroot;
     GQTreeWidgetNode *ctree_refresh;
     GtkWidget *selected_ctree_refresh;
     void *activate;
     GList *formlist;
     GList *oldlist;
     struct formfill *focusform;
     int edit;
     int close_window;
     int hide_status;
     GqServer *server;
     char *dn;
     char *olddn;
};

struct jumptable {
     char *jumpstring;
     void *jumpfunc;
};


struct inputform *new_inputform();
void free_inputform(struct inputform *iform);
/* old name */
#define inputform_free free_inputform

void save_input_snapshot(int error_context,
			 struct inputform *iform, const char *state_name);
void restore_input_snapshot(int error_context, 
			    struct inputform *iform, const char *state_name);

/* Maybe we will align attribute labels differently in the future.. */
#define LABEL_JUSTIFICATION	0.0
#define CONTAINER_BORDER_WIDTH	6

void create_form_window(struct inputform *form);
void create_form_content(struct inputform *form);
void build_or_update_inputform(int error_context,
			       struct inputform *form, gboolean build);
void build_inputform(int error_context,
		     struct inputform *iform);
void edit_entry(GqServer *server, const char *dn);
void new_from_entry(int error_context, GqServer *server,
		    const char *dn);
void update_formlist(struct inputform *iform);
void clear_table(struct inputform *iform);
void mod_entry_from_formlist(struct inputform *iform);
int change_rdn(struct inputform *iform, int context);
LDAPMod **formdiff_to_ldapmod(GList *oldlist, GList *newlist);
char **glist_to_mod_values(GList *values);
struct berval **glist_to_mod_bvalues(GList *values);
int find_value(GList *list, GByteArray *value);
void destroy_editwindow(struct inputform *iform);
void add_row(GtkWidget *button, struct inputform *form);
GtkWidget *gq_new_arrowbutton(struct inputform *iform);
GtkWidget *find_focusbox(GList *formlist);
void set_hide_empty_attributes(int hidden, struct inputform *form);

GdkWindow *get_any_gdk_window(GtkWidget *w);

#endif
