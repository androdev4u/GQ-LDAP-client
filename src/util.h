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

/* $Id: util.h 955 2006-08-31 19:15:21Z herzi $ */

#ifndef GQ_UTIL_H_INCLUDED
#define GQ_UTIL_H_INCLUDED

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib.h>
#include <gtk/gtk.h>
#include <ldap.h>

#include <ldap_schema.h>

#include "common.h"

LDAP *open_connection(int open_context, GqServer *server);
void close_connection(GqServer *server, int always);
void clear_server_schema(GqServer *server);

gboolean delete_entry_full(int delete_context,
			   GqServer *server, char *dn,
			   gboolean recursive);
gboolean delete_entry(int delete_context, GqServer *server, char *dn);
gboolean do_recursive_delete(int delete_context,
			     GqServer *server, char* dn);

void set_busycursor(void);
void set_normalcursor(void);

int close_on_esc(GtkWidget *widget, GdkEventKey *event, gpointer obj);
int func_on_esc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window);

struct tokenlist {
     const int token;
     const char keyword[32];
     const void *data;
};

/* tokenlist funtions */
int tokenize(const struct tokenlist *list, const char *keyword);
const char *detokenize(const struct tokenlist *list, int token);
const void *detokenize_data(const struct tokenlist *list, int token);

char *get_username(void);
/* G_GNUC_PRINTF is the __attribute__ ((format (printf, 1, 2))) stuff,
   but portable */
void statusbar_msg(const char *fmt, ...) G_GNUC_PRINTF(1, 2);
void statusbar_msg_clear();

GqServer *server_by_canon_name(const char *name, 
					gboolean include_transient);

GqServer *get_referral_server(int error_context, 
				       GqServer *parent, 
				       const char *base_url);

/* returns TRUE if server is NOT in the config ldapserver list */
gboolean is_transient_server(const GqServer *server);

int is_leaf_entry(int error_context, GqServer *server, char *dn);
gboolean is_direct_parent(char *child, char *possible_parent);
gboolean is_ancestor(char *child, char *possible_ancestor);
GList *ar2glist(char *ar[]);
void warning_popup(GList *messages);
void single_warning_popup(char *message);
GList *find_at_by_mr_oid(GqServer *server, const char *oid);
GList *find_at_by_s_oid(GqServer *server, const char *oid);
GList *find_mr_by_s_oid(GqServer *server, const char *oid);
GList *find_oc_by_at(int error_context, 
		     GqServer *server, const char *atname);

LDAPAttributeType *find_canonical_at_by_at(struct server_schema *schema,
					   const char *attr);

struct gq_template *find_template_by_name(const char *templatename);
void dump_mods(LDAPMod **mods);
const char *find_s_by_at_oid(int error_context, GqServer *server,
			     const char *oid);
int query_popup(const char *title, gchar **outbuf, gboolean is_password,
		GtkWidget *modal_for);
int question_popup(const char *title, const char *question);

GList *get_suffixes(int error_context, GqServer *server);

#ifndef HAVE_LDAP_STR2DN
#define gq_ldap_explode_dn ldap_explode_dn
#define gq_exploded_free ldap_value_free
#else
char **gq_ldap_explode_dn(const char *dn, int dummy);
void gq_exploded_free(char **exploded_dn);
#endif

/* a wrappers around some widget creationg functions to allow the use
   of underscores to indicate accelerators */
GtkWidget *gq_label_new(const char *text);
GtkWidget *gq_radio_button_new_with_label(GSList *group,
					  const gchar *label);
GtkWidget *gq_menu_item_new_with_label(const gchar *text);
GtkWidget *gq_check_button_new_with_label(const gchar *text);
GtkWidget *gq_button_new_with_label(const gchar *text);
GtkWidget *gq_toggle_button_new_with_label(const gchar *text);

#endif

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
