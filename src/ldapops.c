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

/* $Id: ldapops.c 955 2006-08-31 19:15:21Z herzi $ */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ldap.h>
#include <glib/gi18n.h>
#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

#include "ldapops.h"
#include "util.h"
#include "errorchain.h"

static char * move_entry_internal(LDAPMessage *e,
				  char *source_dn,
				  GqServer *source_server, LDAP *sld,
				  char *target_dn,
				  GqServer *target_server, LDAP *tld,
				  int flags,
				  int error_context,
				  MoveProgressFunc progress);


/* Copy an entry to a new position below target_entry. This works
   cross-server if the right options get used */

char * move_entry(char *source_dn, GqServer *source_server,
		  char *target_dn, GqServer *target_server,
		  int flags,
		  MoveProgressFunc progress,
		  int err_ctx)
{
     char *newdn = NULL;
     LDAP *sld = NULL;
     LDAP *tld = NULL;
     LDAPMessage *res = NULL, *e;
     int rc;
     LDAPControl c;
     LDAPControl *ctrls[2] = { NULL, NULL } ;
     char *attrs[] = {
	  LDAP_ALL_USER_ATTRIBUTES,
	  "ref",
	  NULL
     };

     if ((source_server != target_server) &&
	 ((flags & MOVE_CROSS_SERVER) == 0))
	  goto fail;   /* only sometimes goto makes sense */

     if( (sld = open_connection(err_ctx, source_server)) == NULL)
	  goto fail;   /* only sometimes goto makes sense */

     if( (tld = open_connection(err_ctx, target_server)) == NULL)
	  goto fail;   /* only sometimes goto makes sense */

     /* prepare ManageDSAit: we move referrals, not the referred-to data */
     c.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     c.ldctl_value.bv_val	= NULL;
     c.ldctl_value.bv_len	= 0;
     c.ldctl_iscritical	= 1;
     
     ctrls[0] = &c;

     rc = ldap_search_ext_s(sld,
			    source_dn,		/* search base */
			    LDAP_SCOPE_BASE,	/* scope */
			    "(objectClass=*)",	/* filter */
			    attrs,		/* attrs */
			    0,			/* attrsonly */
			    ctrls,		/* serverctrls */
			    NULL,		/* clientctrls */
			    NULL,		/* timeout */
			    LDAP_NO_LIMIT,	/* sizelimit */
			    &res);

     if(rc == LDAP_NOT_SUPPORTED) {
	  rc = ldap_search_s(sld, source_dn, LDAP_SCOPE_BASE,
			     "(objectClass=*)", attrs, 0, &res);
     }
     
     if (rc != LDAP_SUCCESS) {
	  if (rc == LDAP_SERVER_DOWN) {
	       source_server->server_down++;
	  }
	  error_push(err_ctx, 
		     _("Error during base search for '%1$s': %2$s"),
		     source_dn,
		     ldap_err2string(ldap_result2error(sld, res, 0)));
	  push_ldap_addl_error(sld, err_ctx); 
/*  	  ldap_perror(sld, "search"); */
	  goto fail;
     }

     e = ldap_first_entry(sld, res);
     if (e == NULL) {
	  error_push(err_ctx, 
		     _("Error during base search for '%1$s': %2$s"),
		     source_dn,
		     ldap_err2string(ldap_result2error(sld, res, 0)));
	  push_ldap_addl_error(sld, err_ctx); 
	  /*  	       ldap_perror(sld, "ldap_first_entry"); */
	  goto fail;   /* only sometimes goto makes sense */
     }
     
     newdn = move_entry_internal(e, 
				 source_dn, source_server, sld, 
				 target_dn, target_server, tld, 
				 flags, err_ctx,
				 progress);

 fail:
     if (res) ldap_msgfree(res);
     if (sld) close_connection(source_server, FALSE);
     if (tld) close_connection(target_server, FALSE);

     return newdn;
}

static char * move_entry_internal(LDAPMessage *e,
				  char *source_dn,
				  GqServer *source_server, LDAP *sld,
				  char *target_dn,
				  GqServer *target_server, LDAP *tld,
				  int flags,
				  int error_context,
				  MoveProgressFunc progress)
{
     /*      int i; */
     int rc;
     LDAPMessage *res = NULL;
     char *result = NULL;
     char **sdn =  NULL;
     char **tdn =  NULL;
     char *newdn = NULL;
     int newlen;
     char *a;
     int n = 0, i;
     BerElement *berptr = NULL;
     LDAPMod **mods;
     gboolean ok = TRUE;
/*      char *dn_only[] = { "dn", NULL }; */
     LDAPControl c;
     LDAPControl *ctrls[2] = { NULL, NULL } ;

     c.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     c.ldctl_value.bv_val	= NULL;
     c.ldctl_value.bv_len	= 0;
     c.ldctl_iscritical		= 1;
     
     ctrls[0] = &c;

     sdn =  gq_ldap_explode_dn(source_dn, 0);
     tdn =  gq_ldap_explode_dn(target_dn, 0);
     newlen = strlen(target_dn) + strlen(sdn[0]) + 10;
     newdn = g_malloc(newlen);
    
     g_snprintf(newdn, newlen, "%s,%s", sdn[0], target_dn);

#if defined(HAVE_LDAP_RENAME)
     /* first try to do a rename operation */
     /* need V3 for ldap_rename (?) */

     if (sld == tld && (flags & MOVE_DELETE_MOVED) && 
	 source_server->version == LDAP_VERSION3) {
	  rc = ldap_rename_s(sld,
			     source_dn,		/* dn */
			     sdn[0],		/* newrdn */
			     target_dn,		/* newparent */
			     1,			/* deleteoldrdn */
			     ctrls,		/* serverctrls */
			     NULL		/* clientctrls */
			     );

	  if (rc == LDAP_SUCCESS) {
/*  	       printf("ldap_rename %s -> %s rc=%d\n", */
/*  		      source_dn, target_dn, rc); */
	       result = newdn;
	       goto done;
	  } else if (rc == LDAP_SERVER_DOWN) {
	       source_server->server_down++;
  	       error_push(error_context, 
			  _("Error renaming entry '%1$s': %2$s"),
			  source_dn,
			  ldap_err2string(rc));
	       goto done;
	  } else {
	       /* we probably have a subtree - do not indicate an error */
/*  	       error_push(error_context, ldap_err2string(rc)); */
/*  	       push_ldap_addl_error(sld, error_context);  */

	  }
     }
#endif

     /* count attributes */
     for (a = ldap_first_attribute(sld, e, &berptr) ; 
	  a ; a = ldap_next_attribute(sld, e, berptr) ) {
	  n++;
	  ldap_memfree(a);
     }
#ifndef HAVE_OPENLDAP12
     /* this bombs out with openldap 1.2.x libs... */
     /* PSt: is this still true??? - introduced a configure check */
     if (berptr) ber_free(berptr, 0);
#endif

/*  	  printf("%d Attributes to move\n", n); */

     mods = calloc(sizeof(LDAPMod *), n+1);
     
     berptr = NULL;
     for (i = 0, a = ldap_first_attribute(sld, e, &berptr) ; 
	  a ; a = ldap_next_attribute(sld, e, berptr), i++ ) {
	  /* should work for ;binary as-is */
	  struct berval **bervals = ldap_get_values_len(sld, e, a);
	  
	  mods[i] = calloc(sizeof(LDAPMod), 1);
	  mods[i]->mod_op = LDAP_MOD_ADD | LDAP_MOD_BVALUES;
	  mods[i]->mod_type = a;
	  mods[i]->mod_vals.modv_bvals = bervals;
     }
#ifndef HAVE_OPENLDAP12
     /* this bombs out with openldap 1.2.x libs... */
     /* PSt: is this still true??? - introduced a configure check */
     if (berptr) ber_free(berptr, 0);
#endif

     rc = ldap_add_s(tld, newdn, mods);
/*       printf("ldap_add %s rc=%d\n", newdn, rc); */
     for (i = 0 ; i < n ; i++) {
	  ldap_memfree(mods[i]->mod_type);
	  mods[i]->mod_type = NULL;
     }
     ldap_mods_free(mods, 1);
     
     if (rc != LDAP_SUCCESS) {
	  error_push(error_context, 
		     _("Error adding new entry '%1$s': %2$s"),
		     newdn,
		     ldap_err2string(rc));
	  push_ldap_addl_error(tld, error_context); 
/*  	  ldap_perror(sld, "ldap_add"); */
	  goto done;
     } else {
/*  	  printf("ldap_add %s -> %s rc=%d\n", */
/*  		 source_dn, newdn, rc); */

	  if (progress) {
	       progress(source_dn, target_dn, newdn);
	  }

	  if (flags & MOVE_RECURSIVELY) {
	       gboolean recheck = TRUE;
	       /* prepare ManageDSAit: we move referrals, not the
		  referred-to data */
	       char *attrs[] = {
		    LDAP_ALL_USER_ATTRIBUTES,
		    "ref",
		    NULL
	       };


	       if (sdn) gq_exploded_free(sdn);
	       if (tdn) gq_exploded_free(tdn);
	       sdn = tdn = NULL;
	       
	       if (res) ldap_msgfree(res);
	       res = NULL;
	       
	       /* If we are allowed to do the deleting, we should retry
                  again after each bunch we copy, as the server may be
                  limiting the number of entries returned. But if we
                  have deleted the stuff, we will receive other
                  entries ... and copying may continue */
	       do {
		    res = NULL;

		    recheck = FALSE;
		    /* check for entries below ... */
		    
		    rc = ldap_search_ext_s(sld,
					   source_dn,	/* search base */
					   LDAP_SCOPE_ONELEVEL,	/* scope */
					   "(objectClass=*)",	/* filter */
					   attrs,	/* attrs */
					   0,		/* attrsonly */
					   ctrls,	/* serverctrls */
					   NULL,	/* clientctrls */
					   NULL,	/* timeout */
					   LDAP_NO_LIMIT,	/* sizelimit */
					   &res);

		    if(rc == LDAP_NOT_SUPPORTED) {
			 rc = ldap_search_s(sld, source_dn,
					    LDAP_SCOPE_ONELEVEL,
					    "(objectClass=*)",
					    attrs, 0, &res);
		    }

		    if (rc == LDAP_SUCCESS) {
			 for(e = ldap_first_entry(sld, res) ; 
			     e ;
			     e = ldap_next_entry(sld, e)) {
			      char *dn = ldap_get_dn(sld, e);
			      char *ndn;
			      
			      ndn = move_entry_internal(e,
							dn,
							source_server,
							sld,
							newdn, 
							target_server,
							tld, 
							flags,
							error_context,
							progress);
			      
			      if (ndn) {
				   free(ndn);
				   recheck = TRUE;
			      } else {
				   ok = FALSE;
			      }
			      
			      if (dn) ldap_memfree(dn);
			 }			      
		    } else if (rc == LDAP_SERVER_DOWN) {
			 source_server->server_down++;
			 error_push(error_context, 
				    _("Error searching below '%1$s': %2$s"),
				    source_dn,
				    ldap_err2string(rc));
		    } else {
			 error_push(error_context, 
				    _("Error searching below '%1$s': %2$s"),
				    source_dn, ldap_err2string(rc));
			 push_ldap_addl_error(sld, error_context); 
		    }			 
		    if (res) {
			 ldap_msgfree(res);
			 res = NULL;
		    }
	       } while (flags & MOVE_DELETE_MOVED && recheck);
	  }

	  /* Should move deletion out of the moving business and delay
	     it until _after_ we have ended the moving

	     OR should we delete and retry in order to circumvent any
	     sizelimits???
	  */
	  
	  if (flags & MOVE_DELETE_MOVED && ok) {
	       rc = ldap_delete_ext_s(sld, source_dn, ctrls, NULL);

	       if(rc == LDAP_NOT_SUPPORTED) {
		    rc = ldap_delete_s(sld, source_dn);
	       }


#if HAVE_LDAP_CLIENT_CACHE
	       ldap_uncache_entry(sld, source_dn);
#endif
	       if (rc == LDAP_SUCCESS || rc == LDAP_NO_SUCH_OBJECT) {
	       } else {
		    if (rc == LDAP_SERVER_DOWN) {
			 source_server->server_down++;
		    }
		    error_push(error_context, 
			       _("Error deleting '%1$s': %2$s"),
			       source_dn, ldap_err2string(rc));
		    push_ldap_addl_error(sld, error_context); 
		    goto done;
	       }
	  }
     }
     
     if (ok)
	  result = newdn;

     /* only rarely am I using labels and goto, but sometimes it makes sense */
 done:
     if (sdn) gq_exploded_free(sdn);
     if (tdn) gq_exploded_free(tdn);
     if (result == NULL && newdn) free(newdn);

     if (res) ldap_msgfree(res);

     return result;
}



/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
