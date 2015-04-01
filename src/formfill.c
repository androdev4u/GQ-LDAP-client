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

/* $Id: formfill.c 975 2006-09-07 18:44:41Z herzi $ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <glib/gi18n.h>

#include <lber.h>
#include <ldap.h>

#include "common.h"
#include "configfile.h"
#include "util.h"
#include "formfill.h"
#include "ldif.h"
#include "syntax.h"
#include "schema.h"
#include "encode.h"

static GList *internalAttrs = NULL;

void init_internalAttrs() {
     internalAttrs = g_list_append(internalAttrs, "creatorsName");
     internalAttrs = g_list_append(internalAttrs, "creatorsName");
     internalAttrs = g_list_append(internalAttrs, "createTimestamp");
     internalAttrs = g_list_append(internalAttrs, "modifiersName");
     internalAttrs = g_list_append(internalAttrs, "modifyTimestamp");
     internalAttrs = g_list_append(internalAttrs, "subschemaSubentry");
}

gboolean isInternalAttr(const char *attr) {
     GList *filterout;
     gboolean filtered = FALSE;

     for (filterout = internalAttrs ; filterout ; 
	  filterout = filterout->next) {
	  if (strcasecmp(attr, filterout->data) == 0) {
	       filtered = TRUE;
	       break;
	  }
     }
     return filtered;
}

struct formfill *new_formfill(void)
{
     struct formfill *form;

     form = (struct formfill *) g_new0(struct formfill, 1);
     /* checking for NULL-ness actually is useless for memory
	allocated through g_new0 et. al. */
     g_assert(form);

     form->attrname = g_strdup("");
     form->num_inputfields = 1;
     form->displaytype = DISPLAYTYPE_ENTRY;
     form->dt_handler = get_dt_handler(form->displaytype);
     form->flags = 0;
     form->values = NULL;

     form->label = form->vbox = form->morebutton = NULL;
     form->widgetList = NULL;

     return(form);
}


void free_formlist(GList *formlist)
{
     GList *n, *f;

     for (f = formlist ; f ; f = n) {
	  n = f->next;
	  free_formfill( (struct formfill *) f->data);
     }
     g_list_free(formlist);
}


void free_formfill_values(struct formfill *form) {
     if (form) {
	  GList *vals = form->values;
	  
	  if (vals) {
	       while(vals) {
		    g_byte_array_free((GByteArray*)vals->data, TRUE);
		    vals->data = NULL;
		    vals = vals->next;
	       }
	       g_list_free(form->values);
	       form->values = NULL;
	  }
     }
}

void
free_formfill(struct formfill *form)
{
	g_free(form->attrname);
	form->attrname = NULL;

	free_formfill_values(form);
	if (form->widgetList) g_list_free(form->widgetList);

	if(form->server) {
		g_object_unref(form->server);
		form->server = NULL;
	}

	g_free(form);
}


/*
 * add form to formlist, checking first if the attribute type isn't
 * already in the list
 */
GList *formlist_append(GList *formlist, struct formfill *form)
{
     GList *fl;
     struct formfill *tmp;

     fl = formlist;
     while(fl) {

	  tmp = (struct formfill *) fl->data;
	  if(!strcmp(tmp->attrname, form->attrname)) {
	       /* already have this attribute */
	       free_formfill(form);
	       return(formlist);
	  }

	  fl = fl->next;
     }

     formlist = g_list_append(formlist, form);

     return(formlist);
}

char *attr_strip(const char *attr)
{
     char *c = g_strdup(attr);
     char *d = g_utf8_strchr(c, -1, ';'); /* UTF-8: doesn't hurt */

     if (d) {
	  *d = 0;
     }
     return c;
}

static GList *formlist_from_entry_iteration(int error_context, 
					    LDAP *ld,
					    GqServer *server,
					    const char *dn, 
					    int ocvalues_only,
					    char **attrs,
					    GList *formlist)
{
     LDAPControl c;
     LDAPControl *ctrls[2] = { NULL, NULL } ;
     LDAPMessage *res, *entry;
     int rc, i;
     char *attr; /* , **vals; */
     struct formfill *form;
     BerElement *ber;
     struct berval **bervals;
     
     c.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     c.ldctl_value.bv_val	= NULL;
     c.ldctl_value.bv_len	= 0;
     c.ldctl_iscritical	= 1;
     
     ctrls[0] = &c;


     rc = ldap_search_ext_s(ld,
			    dn,			/* search base */
			    LDAP_SCOPE_BASE,	/* scope */
			    "objectClass=*",	/* filter */
			    attrs,		/* attrs */
			    0,			/* attrsonly */
			    ctrls,		/* serverctrls */
			    NULL,			/* clientctrls */
			    NULL,			/* timeout */
			    LDAP_NO_LIMIT,	/* sizelimit */
			    &res);

     if(rc == LDAP_NOT_SUPPORTED) {
	  rc = ldap_search_s(ld, dn, LDAP_SCOPE_BASE, "(objectclass=*)",  
			     server->hide_internal ? NULL : attrs, 0, &res);
     }

     if (rc == LDAP_SERVER_DOWN) {
	  server->server_down++;
     } else if (rc != LDAP_SUCCESS) {
	  statusbar_msg("Searching for '%1$s' on '%2$s': %3$s", 
			dn, server->name,
			ldap_err2string(rc));
	  return(formlist);
     } 

     if (rc == LDAP_SUCCESS) {
	  entry = ldap_first_entry(ld, res);
	  if(entry) {
	       char *cc;

	       for(attr = ldap_first_attribute(ld, entry, &ber); attr ;
		   attr = ldap_next_attribute(ld, entry, ber)) {
		    gboolean oc;
		    /* filter out some internal attributes... */
		    cc = attr_strip(attr);
		    if (server->hide_internal) {
			 if (isInternalAttr(cc)) {
			      ldap_memfree(attr);
			      if (cc) g_free(cc);
			      continue;
			 }
		    }

		    form = new_formfill();
		    g_assert(form);

		    form->server = g_object_ref(server);

		    g_free(form->attrname);
		    form->attrname = g_strdup(cc);
		    if (cc) g_free(cc);

		    oc  = strcasecmp(attr, "objectClass") == 0;

		    if(ocvalues_only == 0 || oc) {
			 bervals = ldap_get_values_len(ld, res, attr);
			 if(bervals) {
			      for(i = 0; bervals[i]; i++) {
				   GByteArray *gb = g_byte_array_new();
				   g_byte_array_append(gb,
						       (guchar*)bervals[i]->bv_val,
						       bervals[i]->bv_len);
				   form->values = g_list_append(form->values,
								gb);
			      }

			      form->num_inputfields = i;
			      ldap_value_free_len(bervals);
			 }
		    }

		    set_displaytype(error_context, server, form);
		    formlist = g_list_append(formlist, form);
		    ldap_memfree(attr);
	       }
#ifndef HAVE_OPENLDAP12
	       /* this bombs out with openldap 1.2.x libs... */
	       /* PSt: is this still true??? - introduced a configure check */
	       if(ber) {
		    ber_free(ber, 0);
	       }
#endif
	  }
	  
	  if (res) ldap_msgfree(res);
	  res = NULL;
     } 
     return formlist;
}

GList *formlist_from_entry(int error_context, 
			   GqServer *server, const char *dn, 
			   int ocvalues_only)
{
     GList *formlist;
     LDAP *ld;
     char *attrs[]    = { LDAP_ALL_USER_ATTRIBUTES,
			  LDAP_ALL_OPERATIONAL_ATTRIBUTES,
			  "ref",
			  NULL,
     };
     char *ref_attrs[] = { LDAP_ALL_USER_ATTRIBUTES,
			   "ref",
			   NULL
     };

     formlist = NULL;

     set_busycursor();

     statusbar_msg(_("Fetching %1$s from %2$s"), dn, server->name);

     if( (ld = open_connection(error_context, server)) == NULL) {
          set_normalcursor();
          return(NULL);
     }

     formlist = 
	  formlist_from_entry_iteration(error_context, 
					ld,
					server,
					dn,
					ocvalues_only,
					server->hide_internal ? ref_attrs : attrs,
					NULL);

     close_connection(server, FALSE);
     set_normalcursor();

     return(formlist);
}


GList *dup_formlist(GList *oldlist)
{
     GList *newlist, *oldval, *newval;
     struct formfill *oldform, *newform;

     newlist = NULL;

     while(oldlist) {
	  oldform = (struct formfill *) oldlist->data;
	  newform = new_formfill();
	  g_assert(newform);

	  /* no problem with NUL bytes here - new_formfill returns a
             completly NUL inited object */
	  if(oldform->server) {
		  newform->server = g_object_ref(oldform->server);
	  } else {
		  newform->server = NULL;
	  }

	  g_free(newform->attrname);
	  newform->attrname = g_strdup(oldform->attrname);
	  newform->num_inputfields = oldform->num_inputfields;
	  newform->displaytype = oldform->displaytype;
	  newform->dt_handler = oldform->dt_handler;
	  newform->flags = oldform->flags;
	  oldval = oldform->values;
	  newval = NULL;
	  while(oldval) {
	       if(oldval->data) {
		    GByteArray *old = (GByteArray*) (oldval->data);
		    GByteArray *gb = g_byte_array_new();
		    g_byte_array_append(gb, old->data, old->len);

		    newval = g_list_append(newval, gb);
	       }

	       oldval = oldval->next;
	  }
	  newform->values = newval;
	  newlist = g_list_append(newlist, newform);

	  oldlist = oldlist->next;
     }

     return(newlist);
}


void dump_formlist(GList *formlist)
{
     GList *values;
     struct formfill *form;

     while(formlist) {
	  form = (struct formfill *) formlist->data;
	  printf("%s\n", form->attrname);
	  values = form->values;

	  while(values) {
	       if(values->data) {
		    GByteArray *d = (GByteArray*) values->data;
		    printf("\t");
		    fwrite(d->data, d->len, 1, stdout);
		    printf("\n");
	       }
	       values = values->next;
	  }

	  formlist = formlist->next;
     }

}


struct formfill *lookup_attribute(GList *formlist, char *attr)
{
     struct formfill *form;

     while(formlist) {
	  form = (struct formfill *) formlist->data;
	  if(form->attrname && !strcasecmp(form->attrname, attr))
	       return(form);

	  formlist = formlist->next;
     }

     return(NULL);
}

/* Returns an existing formfill object (if any) for attribute attr
   from the passed-in list.  The server_schema gets consulted to also
   check for aliases. The canonical attribute type may be retrieved
   when passing a non NULL value for attrtype */
struct formfill *lookup_attribute_using_schema(GList *formlist, 
					       const char *attr,
					       struct server_schema *schema,
					       LDAPAttributeType **attrtype)
{
     struct formfill *form;

#ifdef HAVE_LDAP_STR2OBJECTCLASS
     char **n;
     LDAPAttributeType *trythese = NULL;

     if (attrtype) *attrtype = NULL;

     if (schema) {
	  trythese = find_canonical_at_by_at(schema, attr);
	  if (attrtype) *attrtype = trythese;
     }
#endif /* HAVE_LDAP_STR2OBJECTCLASS */

     for( ; formlist ; formlist = formlist->next ) {
	  form = (struct formfill *) formlist->data;

#ifdef HAVE_LDAP_STR2OBJECTCLASS
	  if (trythese && form->attrname) {
	       for (n = trythese->at_names ; n && *n ; n++) {
		    if(!strcasecmp(form->attrname, *n)) return(form);
	       }
	  } else {
	       if(form->attrname && !strcasecmp(form->attrname, attr))
		    return(form);
	  }
#else /* HAVE_LDAP_STR2OBJECTCLASS */
	  if(form->attrname && !strcasecmp(form->attrname, attr))
	       return(form);
#endif /* HAVE_LDAP_STR2OBJECTCLASS */
     }

     return(NULL);
}

/*
 * sure it is nice to use serverschema for this
 *
 */
int find_displaytype(int error_context, GqServer *server, 
		     struct formfill *form)
{
     return get_display_type_of_attr(error_context, server, form->attrname);
}

/*
 * now we use the serverschema for this
 *
 * hint, hint
 *
 */
void set_displaytype(int error_context, 
		     GqServer *server, struct formfill *form)
{
     struct attr_settings *as;

     /* this is ugly... all of this needs a cleanup!!!! - FIXME */
     LDAPAttributeType *at =
	  find_canonical_at_by_at(server->ss, form->attrname);

     if (at && at->at_no_user_mod) {
	  form->flags |= FLAG_NO_USER_MOD;
     }

     form->syntax = get_syntax_handler_of_attr(error_context, server, 
					       form->attrname, NULL);

     as = lookup_attr_settings(form->attrname);
     if (as && as->defaultDT != -1) {
	  form->displaytype = as->defaultDT;
     } else {
	  form->displaytype = find_displaytype(error_context, server, form);
     }
     form->dt_handler = get_dt_handler(form->displaytype);
}

/*
   Local Variables:
   c-basic-offset: 5
   End:
 */
