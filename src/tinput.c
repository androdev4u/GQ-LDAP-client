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

/* $Id: tinput.c 975 2006-09-07 18:44:41Z herzi $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_LDAP_STR2OBJECTCLASS

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>

#include <lber.h>
#include <ldap.h>
#include <ldap_schema.h>

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "util.h"
#include "configfile.h"
#include "template.h"
#include "schema.h"
#include "errorchain.h"
#include "tinput.h"
#include "formfill.h"
#include "input.h"

/*
 * fetch objectclasses in template from server and build GList of
 * formfill with it
 */
GList *formfill_from_template(int error_context, GqServer *server,
			      struct gq_template *template)
{
     GList *formlist, *oclist;
     struct server_schema *ss;
     struct formfill *oc;

     /* make sure the server's schema is fetched, or
	find_oc_by_oc_name() will fail... */
     ss = get_schema(error_context, server);

     /* list all the objectclasses first, with values */
     oclist = template->objectclasses;
     formlist = add_attrs_by_oc(error_context, server, oclist);

     /* fill in objectClass attributes */

     oc = lookup_attribute_using_schema(formlist, "objectClass", ss, NULL);
     if (oc) {
	  /* new_formfill initializes num_inputfields with 1 */ 
	  if (!oc->values) oc->num_inputfields--;
	  while (oclist) {
	       GByteArray *gb = g_byte_array_new();
	       g_byte_array_append(gb,
				   oclist->data,
				   strlen(oclist->data));
	       oc->values = g_list_append(oc->values, gb);
	       oc->num_inputfields++;
	       
	       oclist = oclist->next;
	  }
     }

     return(formlist);
}


/*
 * fetch objectclasses in entry from server and build GList of formfill with it
 */
GList *formfill_from_entry_objectclass(int error_context, 
				       GqServer *server, 
				       const char *dn)
{
     GList *formlist, *oclist, *tmplist;
     LDAP *ld;
     LDAPMessage *res, *entry;
     struct server_schema *ss;
     struct formfill *form;
     int msg, count;
     char **vals;
     char *oc_only[] = { "objectClass", NULL };
     LDAPControl c;
     LDAPControl *ctrls[2] = { NULL, NULL } ;

     /*  ManageDSAit  */
     c.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     c.ldctl_value.bv_val	= NULL;
     c.ldctl_value.bv_len	= 0;
     c.ldctl_iscritical	= 1;
     
     ctrls[0] = &c;

     formlist = NULL;

     set_busycursor();

     if( (ld = open_connection(error_context, server)) == NULL) {
          set_normalcursor();
          return(NULL);
     }

     msg = ldap_search_ext_s(ld, dn, 
			     LDAP_SCOPE_BASE,
			     "(objectClass=*)", 
			     oc_only,		/* attrs */
			     0,			/* attrsonly */
			     ctrls,		/* serverctrls */
			     NULL,		/* clientctrls */
			     NULL,		/* timeout */
			     LDAP_NO_LIMIT,	/* sizelimit */
			     &res);

     if(msg == LDAP_NOT_SUPPORTED) {
	  msg = ldap_search_s(ld, dn, LDAP_SCOPE_BASE,
			      "(objectClass=*)",
			      oc_only, 0, &res);
     }

     if(msg != LDAP_SUCCESS) {
	  if (msg == LDAP_SERVER_DOWN) {
	       server->server_down++;
	  }
	  statusbar_msg("%s", ldap_err2string(msg));
	  set_normalcursor();
	  close_connection(server, FALSE);
	  return(NULL);
     }

     entry = ldap_first_entry(ld, res);

     if(entry) {
	  count = 0;
	  oclist = NULL;
	  vals = ldap_get_values(ld, res, "objectClass");
	  if(vals) {
	       for(count = 0; vals[count]; count++)
		    oclist = g_list_append(oclist, g_strdup(vals[count]));
	       ldap_value_free(vals);
	  }

	  ss = get_schema(error_context, server);
	  formlist = add_attrs_by_oc(error_context, server, oclist);

	  /* add the values that were already in the "objectClass" attribute */
	  form = lookup_attribute_using_schema(formlist, "objectClass", ss, 
					       NULL);
	  if(form) {
	       tmplist = oclist;
	       while(tmplist) {
		    GByteArray *gb = g_byte_array_new();
		    g_byte_array_append(gb, tmplist->data, 
					strlen(tmplist->data));

		    g_free(tmplist->data);

		    form->values = g_list_append(form->values, gb);
		    form->num_inputfields++;
		    tmplist = tmplist->next;
	       }
	       /* got an off-by-one error someplace */
	       if(form->num_inputfields)
		    form->num_inputfields--;
	       g_list_free(oclist);
	  }

     }
     ldap_msgfree(res);

     close_connection(server, FALSE);
     set_normalcursor();

     return(formlist);
}


static GList *add_oc_and_superiors(GList *oc_list, struct server_schema *ss,
				   LDAPObjectClass *oc) 
{
     /* check if superior(s) has/have been added as well */
     if (oc->oc_sup_oids) {
	  int i;

	  for (i = 0 ; oc->oc_sup_oids[i] ; i++) {
	       LDAPObjectClass *soc = 
		    find_oc_by_oc_name(ss, oc->oc_sup_oids[i]);
	       if (soc) {
		    oc_list = add_oc_and_superiors(oc_list, ss, soc);
	       }
	  }
     }

     /* add oc itself possibly after its superiors... */

     if (oc_list && g_list_find(oc_list, oc)) {
	  /* already added */
     } else {
	  oc_list = g_list_append(oc_list, oc);
     }

     return oc_list;
}

GList *add_attrs_by_oc(int error_context, GqServer *server,
		       GList *oclist)
{
     GList *formlist, *ocs_to_add = NULL, *l;
     LDAPObjectClass *oc;
     struct server_schema *ss;
     struct formfill *form;
     int i;


     if(oclist == NULL)
	  return(NULL);

     formlist = NULL;


     /* add the objectclass attribute in manually */
     form = new_formfill();
     g_assert(form);

     form->server = g_object_ref(server);

     g_free(form->attrname);
     form->attrname = g_strdup("objectClass");
     form->flags |= FLAG_MUST_IN_SCHEMA;
     set_displaytype(error_context, server, form);

     formlist = g_list_append(formlist, form);

     /* schema functions below need this */
     ss = get_schema(error_context, server);

     for ( l = oclist ; l ; l = l->next ) {
	  oc = find_oc_by_oc_name(ss, (char *) l->data);
	  if(oc) {
	       ocs_to_add = add_oc_and_superiors(ocs_to_add, ss, oc);
	  }
     }

     for (l = g_list_first(ocs_to_add) ; l ; l = g_list_next(l)) {
	  oc = (LDAPObjectClass *) l->data;
	  if(oc) {
	       LDAPAttributeType *at;

	       /* required attributes */
	       for(i=0; oc->oc_at_oids_must && oc->oc_at_oids_must[i]; i++) {
/*	            if (strcasecmp(oc->oc_at_oids_must[i], "objectClass")) { */
	            if (lookup_attribute_using_schema(formlist,
						      oc->oc_at_oids_must[i],
						      ss, &at) == NULL) {
		         form = new_formfill();
			 g_assert(form);

			 form->server = g_object_ref(server);

			 g_free(form->attrname);
			 form->attrname = g_strdup(oc->oc_at_oids_must[i]);
			 form->flags |= FLAG_MUST_IN_SCHEMA;
			 if (at && at->at_single_value) {
			      form->flags |= FLAG_SINGLE_VALUE;
			 }
			 if (at && at->at_no_user_mod) {
			      form->flags |= FLAG_NO_USER_MOD;
			 }
			 set_displaytype(error_context, server, form);
		         formlist = formlist_append(formlist, form);
		    }
	       }

	       /* allowed attributes */
	       i = 0;
	       while(oc->oc_at_oids_may && oc->oc_at_oids_may[i]) {
/*		    if (strcasecmp(oc->oc_at_oids_may[i], "objectClass")) { */
	            if (lookup_attribute_using_schema(formlist,
						      oc->oc_at_oids_may[i],
						      ss, &at) == NULL) {
		         form = new_formfill();
			 g_assert(form);

			 form->server = g_object_ref(server);

			 g_free(form->attrname);
			 form->attrname = g_strdup(oc->oc_at_oids_may[i]);
			 set_displaytype(error_context, server, form);
		         formlist = formlist_append(formlist, form);
			 if (at && at->at_single_value) {
			      form->flags |= FLAG_SINGLE_VALUE;
			 }
			 if (at && at->at_no_user_mod) {
			      form->flags |= FLAG_NO_USER_MOD;
			 }
		    }
		    i++;
	       }

	  }
     }

     return(formlist);
}


/*
 * adds all attributes allowed in schema to formlist
 */
GList *add_schema_attrs(int error_context, GqServer *server,
			GList *value_list)
{
     GList *oclist, *schema_list, *tmplist, *tmpvallist, *addendum;
     struct formfill *form, *oldform, *newform;

     struct server_schema *ss;

     form = lookup_attribute(value_list, "objectclass");
     if(form == NULL || form->values == NULL) {
	  /* not even an objectclass attribute type here... give up */
	  return(value_list);
     }

     /* build temporary list of object classes from formfill list */

     oclist = NULL;
     tmplist = form->values;
     while (tmplist) {
	  GByteArray *gb = (GByteArray*) tmplist->data;
	  /* we know this is plain text... */
	  oclist = g_list_append(oclist, g_strndup((gchar*)gb->data, gb->len));
	  tmplist = tmplist->next;
     }

     ss = get_schema(error_context, server);
     schema_list = add_attrs_by_oc(error_context, server, oclist);
     g_list_foreach(oclist, (GFunc) g_free, NULL);
     g_list_free(oclist);

     /* merge value_list's values into schema_list */
     tmplist = schema_list;
     while(tmplist) {
	  form = (struct formfill *) tmplist->data;
	  oldform = lookup_attribute_using_schema(value_list, form->attrname,
						  ss, NULL);
	  if(oldform) {
	       form->num_inputfields = oldform->num_inputfields;
	       form->displaytype = oldform->displaytype;
	       form->dt_handler = oldform->dt_handler;
	       form->flags |= oldform->flags; /* to keep FLAG_MUST_IN_SCHEMA */
	       tmpvallist = oldform->values;
	       while(tmpvallist) {
		    GByteArray *oldgb = (GByteArray *) tmpvallist->data;
		    GByteArray *gb = g_byte_array_new();
		    g_byte_array_append(gb, oldgb->data, oldgb->len);

		    form->values = g_list_append(form->values, gb);
		    tmpvallist = tmpvallist->next;
	       }
	  }
	  tmplist = tmplist->next;
     }

     /* final inverse check for attrs that were in the original entry (value_list)
	but not allowed according to the schema (schema_list) */
     addendum = NULL;
     for(tmplist=value_list; tmplist; tmplist = tmplist->next) {
	  form = (struct formfill *) tmplist->data;
	  newform = lookup_attribute_using_schema(schema_list, form->attrname,
						  ss, NULL);
	  if(newform == NULL) {
	       /* attribute type not in schema */
	       newform = new_formfill();
	       newform->server = g_object_ref(server);

	       newform->attrname = g_strdup(form->attrname);
	       newform->num_inputfields = form->num_inputfields;
	       newform->displaytype = form->displaytype;
	       newform->dt_handler = form->dt_handler;
	       newform->flags = form->flags | FLAG_NOT_IN_SCHEMA;
	       tmpvallist = form->values;
	       while(tmpvallist) {
		    GByteArray *oldgb = (GByteArray *) tmpvallist->data;
		    GByteArray *gb = g_byte_array_new();
		    g_byte_array_append(gb, oldgb->data, oldgb->len);

		    newform->values = g_list_append(newform->values, gb);
		    tmpvallist = tmpvallist->next;
	       }
	       addendum = g_list_append(addendum, newform);
	  }
     }
     if(addendum) {
	  for(;addendum; addendum = addendum->next) {
	       schema_list = g_list_append(schema_list, addendum->data);
	  }
	  g_list_free(addendum);
     }

     free_formlist(value_list);

     return(schema_list);
}


#endif

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
