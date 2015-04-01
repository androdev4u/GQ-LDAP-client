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

/* schema parsing strictness, see OpenLDAP's ldap_schema.h */
/* allow for missing OID's and bogus extra quotes */

#ifndef GQ_SCHEMA_H_INCLUDED
#define GQ_SCHEMA_H_INCLUDED

#include <ldap.h>
#include <ldap_schema.h>

#include "common.h"

#define GQ_SCHEMA_PARSE_FLAG    0x03

struct server_schema *get_schema(int error_context, GqServer *server);
struct server_schema *get_server_schema(int error_context,
					GqServer *server);

int sort_oc(LDAPObjectClass *oc1, LDAPObjectClass *oc2);
int sort_at(LDAPAttributeType *at1, LDAPAttributeType *at2);
int sort_mr(LDAPMatchingRule *mr1, LDAPMatchingRule *mr2);
int sort_s(LDAPSyntax *s1, LDAPSyntax *s2);

LDAPObjectClass *find_oc_by_oc_name(struct server_schema *ss, char *ocname);
GList *attrlist_by_oclist(GqServer *server, GList *oclist);

#endif

