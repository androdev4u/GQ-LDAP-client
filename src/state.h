/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 by Peter Stamfest

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

/* $Id: state.h 960 2006-09-02 20:42:46Z herzi $ */

#ifndef GQ_STATE_H_INCLUDED
#define GQ_STATE_H_INCLUDED

#include <gtk/gtk.h>


GtkWidget *stateful_gtk_window_new(GtkWindowType type,
				   const char *name,
				   int w, int h);

void init_state();
void save_state();

/* Never use non-ASCII entity names */
struct state_entity *lookup_entity(const char *entity_name);
void rm_value(const char *state_name);
int state_value_get_int(const char *state_name,
			const char *value_name,
			int def);
void state_value_set_int(const char *state_name,
			 const char *value_name,
			 int n);

const char *state_value_get_string(const char *state_name,
				   const char *value_name,
				   const char *def);
void state_value_set_string(const char *state_name,
			    const char *value_name,
			    const char *c);


const GList *state_value_get_list(const char *state_name,
				  const char *value_name);

/* convenience functions for common list manipulations */
GList *copy_list_of_strings(const GList *src);
GList *free_list_of_strings(GList *l);

/* The list set her MUST be a list of strings - nothing else will work */
void state_value_set_list(const char *state_name,
			  const char *value_name,
			  const GList *n);


gboolean exists_entity(const char *entity_name) ;


#endif

/* 
   Local Variables:
   c-basic-offset: 4
   End:
 */
