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

/* $Id: schema.c 955 2006-08-31 19:15:21Z herzi $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_LDAP_STR2OBJECTCLASS

#include <lber.h>
#include <ldap.h>
#include <ldap_schema.h>

#include <stdio.h>
#include <string.h>

#include <glib/gi18n.h>

#include "common.h"
#include "configfile.h"
#include "gq-server-list.h"
#include "schema.h"
#include "util.h"
#include "debug.h"
#include "errorchain.h"



/*
 * examine server's root DSE for schema info, store directly
 * in ldapserver struct
 */
struct server_schema *get_schema(int error_context, GqServer *server)
{
     struct server_schema *ss;

     if(server == NULL)
	  return(NULL);

     if(server->ss)
	  return(server->ss);

     set_busycursor();

     ss = get_server_schema(error_context, server);
     if(ss == NULL) {
	  /* server didn't publish a schema, try the last resort
	     schemaserver */
	  server = gq_server_list_get_by_name(gq_server_list_get(), config->schemaserver);
	  if(server == NULL) {
	       error_push(error_context,
			  _("Cannot find last-resort schema server '%s'"),
			  config->schemaserver);
	       ss = NULL;
	       goto done;
	  }

	  if(server->ss) {
	       ss = server->ss;
	       goto done;
	  }

	  ss = get_server_schema(error_context, server);
	  if (ss == NULL) {
	       error_push(error_context,
			  _("Cannot obtain schema from last-resort schema server '%s'"),
			  server->name);
	  } else {
	       statusbar_msg(_("Using schema from last-resort schema server '%s'"),
			     server->name);
	  }
     }
 done:
     set_normalcursor();
     return(ss);
}


struct server_schema *get_server_schema(int error_context, 
					GqServer *server)
{
     BerElement *berptr;
     LDAP *ld;
     LDAPMessage *res, *e; 
     LDAPObjectClass *oc;
     LDAPAttributeType *at;
     LDAPMatchingRule *mr;
     LDAPSyntax *s;
     struct server_schema *ss;
     int i, msg, retcode;
     char *attr, **vals;
     const char *errp;
     char *subschema = NULL;
     const char *subschemasubentry[] = { "subschemaSubentry",
					 NULL };
     const char *schema_attrs[] = { "objectClasses",
				    "attributeTypes",
				    "matchingRules",
				    "ldapSyntaxes",
				    NULL };

     if(server->flags & SERVER_HAS_NO_SCHEMA)
	  return(NULL);

     if( (ld = open_connection(error_context, server)) == NULL) {
	  return(NULL);
     }

     ss = NULL;
     server->flags |= SERVER_HAS_NO_SCHEMA;
     statusbar_msg(_("subschemaSubentry search on root DSE on server '%s'"),
		   server->name);
     msg = ldap_search_ext_s(ld, "", LDAP_SCOPE_BASE, "(objectclass=*)",
			     (char**) subschemasubentry, 0, 
			     NULL, NULL, NULL, LDAP_NO_LIMIT,
			     &res);

     if(msg == LDAP_NOT_SUPPORTED) {
	  msg = ldap_search_s(ld, "", LDAP_SCOPE_BASE, "(objectClass=*)",
			      (char **) subschemasubentry, 0, &res);
     }

     if(msg != LDAP_SUCCESS) {
	  if (msg == LDAP_SERVER_DOWN) {
	       server->server_down++;
	  }
	  statusbar_msg("%s", ldap_err2string(msg));
	  close_connection(server, FALSE);
	  return(NULL);
     }

     if(res == NULL) {
	  statusbar_msg(_("No schema information found on server '%s'"),
			server->name);
	  close_connection(server, FALSE);
	  return(NULL);
     }

     if( (e = ldap_first_entry(ld, res)) ) {
	  if( (attr = ldap_first_attribute(ld, res, &berptr)) ) {
	       if( (vals = ldap_get_values(ld, res, attr)) ) {
		    subschema = g_strdup(vals[0]);
		    ldap_value_free(vals);
	       }
	       ldap_memfree(attr);
	  }
#ifndef HAVE_OPENLDAP_12
	  if(berptr)
	       ber_free(berptr, 0);
#endif
     }
     ldap_msgfree(res);

     if(subschema == NULL) {
	  statusbar_msg(_("No schema information found on server '%s'"),
			server->name);
	  close_connection(server, FALSE);
	  return(NULL);
     }

     statusbar_msg(_("Schema search on '%1$s' on server '%2$s'"),
		   subschema, server->name);

     msg = ldap_search_ext_s(ld, subschema, LDAP_SCOPE_BASE,
			     "(objectClass=*)", (char**) schema_attrs, 0, 
			     NULL, NULL, NULL, LDAP_NO_LIMIT,
			     &res);

     if(msg == LDAP_NOT_SUPPORTED) {
	  msg = ldap_search_s(ld, subschema, LDAP_SCOPE_BASE,
			      "(objectClass=*)", (char**) schema_attrs, 0,
			      &res);
     }

     g_free(subschema);

     if(msg != LDAP_SUCCESS) {
	  if (msg == LDAP_SERVER_DOWN) {
	       server->server_down++;
	  }
	  statusbar_msg("%s", ldap_err2string(msg));
	  close_connection(server, FALSE);
	  return(NULL);
     }

     if(res == NULL) {
	  statusbar_msg(_("No schema information found on server '%s'"), 
			server->name);
	  close_connection(server, FALSE);
	  return(NULL);
     }

     ss = MALLOC(sizeof(struct server_schema), "struct server_schema");

     if(ss == NULL) {
	  close_connection(server, FALSE);
	  return(NULL);
     }

     ss->oc = ss->at = ss->mr = ss->s = NULL;

     for(e = ldap_first_entry(ld, res); e; e = ldap_next_entry(ld, e)) {
	  for(attr = ldap_first_attribute(ld, res, &berptr); attr;
	      attr = ldap_next_attribute(ld, res, berptr)) {

	       vals = ldap_get_values(ld, res, attr);
	       if(vals) {
		    for(i = 0; vals[i]; i++) {
			 if(!strcasecmp(attr, "objectClasses")) {
			      oc = ldap_str2objectclass(vals[i], &retcode, &errp,
							GQ_SCHEMA_PARSE_FLAG);
			      if(oc)
				   ss->oc = g_list_append(ss->oc, oc);
			 }
			 else if(!strcasecmp(attr, "attributeTypes")) {
			      at = ldap_str2attributetype(vals[i], &retcode, &errp,
							  GQ_SCHEMA_PARSE_FLAG);
			      if(at)
				   ss->at = g_list_append(ss->at, at);
			 }
			 else if(!strcasecmp(attr, "matchingRules")) {
			      mr = ldap_str2matchingrule(vals[i], &retcode, &errp,
							 GQ_SCHEMA_PARSE_FLAG);
			      if(mr)
				   ss->mr = g_list_append(ss->mr, mr);
			 }
			 else if(!strcasecmp(attr, "ldapSyntaxes")) {
			      s = ldap_str2syntax(vals[i], &retcode, &errp,
						  GQ_SCHEMA_PARSE_FLAG);
			      if(s)
				   ss->s = g_list_append(ss->s, s);
			 }
		    }
		    ldap_value_free(vals);
	       }
	       ldap_memfree(attr);
	  }
#ifndef HAVE_OPENLDAP12
	  if(berptr)
	       ber_free(berptr, 0);
#endif
     }
     ldap_msgfree(res);

     if(ss->oc)
	  ss->oc = g_list_sort(ss->oc, (GCompareFunc) sort_oc);

     if(ss->at)
	  ss->at = g_list_sort(ss->at, (GCompareFunc) sort_at);

     if(ss->mr)
	  ss->mr = g_list_sort(ss->mr, (GCompareFunc) sort_mr);

     if(ss->s)
	  ss->s = g_list_sort(ss->s, (GCompareFunc) sort_s);

     if(!ss->s && !ss->at && !ss->oc && !ss->s) {
	  FREE(ss, "struct server_schema");
	  ss = NULL;
     }
     else
	  server->flags &= ~SERVER_HAS_NO_SCHEMA;

     close_connection(server, FALSE);

     /* cache server schema */
     server->ss = ss;

     return(ss);
}


/*
 * sort objectclasses by first name or OID
 */
int sort_oc(LDAPObjectClass *oc1, LDAPObjectClass *oc2)
{

     if(oc1->oc_names && oc2->oc_names)
	  return(strcasecmp(oc1->oc_names[0], oc2->oc_names[0]));
     else if (oc1->oc_oid && oc2->oc_oid)
	  return(strcasecmp(oc1->oc_oid, oc2->oc_oid));

     return(0);
}


/*
 * sort attribute types by first name or (if no name available) OID
 */
int sort_at(LDAPAttributeType *at1, LDAPAttributeType *at2)
{

     if(at1->at_names && at1->at_names[0] && at2->at_names && at2->at_names[0])
	  return(strcasecmp(at1->at_names[0], at2->at_names[0]));
     else
	  if(at1->at_oid && at2->at_oid)
	       return(strcasecmp(at1->at_oid, at2->at_oid));

     return(0);
}


/*
 * sort matching rules by first name or (if none available) OID
 */
int sort_mr(LDAPMatchingRule *mr1, LDAPMatchingRule *mr2)
{

     if(mr1->mr_names && mr1->mr_names[0] && mr2->mr_names && mr2->mr_names[0])
	  return(strcasecmp(mr1->mr_names[0], mr2->mr_names[0]));
     else
	  if(mr1->mr_oid && mr2->mr_oid)
	       return(strcasecmp(mr1->mr_oid, mr2->mr_oid));

     return(0);
}


/*
 * sort syntaxes by description or (if none available) by OID
 */
int sort_s(LDAPSyntax *s1, LDAPSyntax *s2)
{

     if(s1->syn_desc && s1->syn_desc[0] && s2->syn_desc && s2->syn_desc[0])
	  return(strcasecmp(s1->syn_desc, s2->syn_desc));
     else
	  if(s1->syn_oid && s2->syn_oid)
	       return(strcasecmp(s1->syn_oid, s2->syn_oid));

     return(0);
}


/*
 * find objectclass in server by its (canonical) name or OID if no name available
 */
LDAPObjectClass *find_oc_by_oc_name(struct server_schema *ss, char *ocname)
{
     GList *list;
     LDAPObjectClass *oc;

     if(ss == NULL || ss->oc == NULL)
	  return(NULL);

     oc = NULL;
     list = ss->oc;
     while(list) {
	  oc = (LDAPObjectClass *) list->data;
	  if( (oc->oc_names && oc->oc_names[0] && !strcasecmp(ocname, oc->oc_names[0]) )
	      || (oc->oc_oid && !strcasecmp(ocname, oc->oc_oid)) )
	       break;
	  list = list->next;
     }

     return(oc);
}


GList *attrlist_by_oclist(GqServer *server, GList *oclist)
{
     GList *attrlist;
     LDAPObjectClass *oc;
     struct server_schema *ss;
     int i;
     char *objectclass;

     ss = server->ss;
     if(!ss || !ss->oc || !ss->at)
	  return(NULL);

     attrlist = NULL;
     while(oclist) {
	  objectclass = oclist->data;
	  oc = find_oc_by_oc_name(ss, objectclass);
	  if(oc) {
	       if(oc->oc_at_oids_must)
		    for(i = 0; oc->oc_at_oids_must[i]; i++)
			 attrlist = g_list_append(attrlist, oc->oc_at_oids_must[i]);
	       if(oc->oc_at_oids_may)
		    for(i = 0; oc->oc_at_oids_may[i]; i++)
			 attrlist = g_list_append(attrlist, oc->oc_at_oids_may[i]);

	  }

	  oclist = oclist->next;
     }

     return(attrlist);
}


#endif  /* HAVE_LDAP_STR2OBJECTCLASS */

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
