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

/* $Id: syntax.h 955 2006-08-31 19:15:21Z herzi $ */

#ifndef GQ_SYNTAX_H_INCLUDED
#define GQ_SYNTAX_H_INCLUDED

#include "formfill.h"
#include "gq-type-display.h"

/* This is not as generic as one could wish, but we have to somehow map
   syntaxes to our code... */

struct syntax_handler {
     const char *syntax_oid;
     const char *desc;
     int displaytype;
     int (*displayTypeFunc)(const char *attr, const char *syn_oid);
     int must_binary;
};

struct syntax_handler *get_syntax_handler_of_attr(int error_context,
						  GqServer *server,
						  const char *attrname,
						  const char *oid);

int get_display_type_of_attr(int error_context,
			     GqServer *server,
			     const char *attrname);

GType get_dt_handler(int type);
int get_dt_from_handler(GType h);

int show_in_search(int error_context, GqServer *server,
		   const char *attrname);

void init_syntaxes(void);

/* Return a list of _display_type_handler objects selectable by the user */
GList *get_selectable_displaytypes();

/* Utility functions */
LDAPMod *bervalLDAPMod(struct formfill *form, int op, GList *values);

GByteArray *identity(const char *val, int len);

#endif

/*
   Local Variables:
   c-basic-offset: 5
   End:
 */
