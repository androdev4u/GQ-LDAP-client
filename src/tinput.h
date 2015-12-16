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

/* $Id: tinput.h 955 2006-08-31 19:15:21Z herzi $ */

#ifndef GQ_TINPUT_H_INCLUDED
#define GQ_TINPUT_H_INCLUDED

#include <glib.h>
#include "common.h"
#include "template.h"

GList *formfill_from_template(int error_context, 
			      GqServer *schemaserver, 
			      struct gq_template *template);

GList *formfill_from_entry_objectclass(int error_context,
				       GqServer *server,
				       const char *dn);

GList *add_attrs_by_oc(int error_context,
		       GqServer *server,
		       GList *oclist);

GList *add_schema_attrs(int error_context,
			GqServer *server,
			GList *value_list);

#endif /* GQ_TINPUT_H_INCLUDED */
