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

/* $Id: util.c 1150 2007-05-14 16:33:52Z herzi $ */

#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <pwd.h>
#include <errno.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include <lber.h>
#include <ldap.h>
#ifdef HAVE_LDAP_STR2OBJECTCLASS
#    include <ldap_schema.h>
#endif
#ifdef HAVE_SASL
#  ifdef HAVE_SASL_H
#    include <sasl.h>
#  else
#    include <sasl/sasl.h>
#  endif
#endif

#ifdef LDAP_OPT_NETWORK_TIMEOUT
#include <sys/time.h>
#endif

#include <glade/glade.h>

#include "common.h"
#include "configfile.h"
#include "errorchain.h"
#include "gq-keyring.h"
#include "gq-server-list.h"
#include "util.h"
#include "template.h"
#include "debug.h"
#include "schema.h"
#include "encode.h"
#include "mainwin.h"
#include "input.h"
#include "mainwin.h"		/* message_log_append */

#define TRY_VERSION3 1

LDAP *open_connection_ex(int open_context,
			 GqServer *server, int *ldap_errno);

#ifdef HAVE_SASL
static int util_ldap_sasl_interact(LDAP *ld, unsigned flags, void *defaults, void *in)
{
     sasl_interact_t *interact = in;
     GqServer *def = defaults;
     
     for (; interact->id != SASL_CB_LIST_END; interact++) {
	
	     switch (interact->id) {
		     case SASL_CB_AUTHNAME:
			     interact->result = def->binddn;
			     interact->len = strlen(def->binddn);
			     break;
	
		     case SASL_CB_PASS:
			      if (def->ask_pw) {
				      if (def->enteredpw[0]) {
					      interact->result = def->enteredpw;
					      interact->len = strlen(def->enteredpw);
				      }
			      }
			      else if (def->bindpw[0]) {
				      interact->result = def->bindpw;
				      interact->len = strlen(def->bindpw);
			      }
			     break;
	     }
     }
     return LDAP_SUCCESS;
}
#endif

static int
do_ldap_auth(LDAP *ld, GqServer *server, int open_context) {
	char *binddn = NULL;
	char *bindpw = NULL;
	int rc = LDAP_SUCCESS;

	if (server->binddn[0]) {
		binddn = server->binddn;
	}

	/* do not ever use the bindpw if we have turned on to ask
	 * for a password */
	/* Thanks to Tomas A. Maly <tomas_maly@yahoo.com> for
	 * indirectly causing me to check this area */
	if (server->ask_pw) {
		if (server->enteredpw[0])
			bindpw = server->enteredpw;
	}
	else if (server->bindpw[0])
		bindpw = server->bindpw;

	/* take care of special characters... */
	if (binddn) binddn = encoded_string(binddn);
	if (bindpw) bindpw = encoded_string(bindpw);

	switch (server->bindtype) {
		case BINDTYPE_KERBEROS:
#ifdef HAVE_KERBEROS
			rc = ldap_bind_s(ld, binddn, bindpw, LDAP_AUTH_KRBV4);
#else
			error_push(open_context, 
				_("Cannot use Kerberos bind with '%s'.\n"
				"GQ was compiled without Kerberos support.\n"
				"Run 'configure --help' for more information\n"),
				server->name);
			statusbar_msg_clear();
			/* XXX - should merge kerberos into sasl (gssapi) */
			rc = LDAP_AUTH_METHOD_NOT_SUPPORTED;
#endif
			break;
		case BINDTYPE_SASL:
#ifdef HAVE_SASL
			rc = ldap_sasl_interactive_bind_s(ld, NULL, NULL, NULL, NULL, LDAP_SASL_QUIET, util_ldap_sasl_interact, server);
			if (rc == LDAP_SUCCESS)
				break;
#else
			error_push(open_context, 
				_("Cannot use SASL bind with '%s'.\n"
				"GQ was compiled without SASL support.\n"
				"Run 'configure --help' for more information\n"),
				server->name);
			statusbar_msg_clear();
			rc = LDAP_AUTH_METHOD_NOT_SUPPORTED;
#endif
			break;
		default:
			rc = ldap_simple_bind_s(ld, binddn, bindpw);
			break;
	}

	if (binddn) free(binddn);
	if (bindpw) free(bindpw);
	
	return rc;
}

static int do_ldap_connect(LDAP **ld_out, GqServer *server,
			   int open_context, int flags)
{
     LDAP *ld = NULL;
     char *binddn = NULL, *bindpw = NULL;
     int rc = LDAP_SUCCESS;
     int i;
#ifdef LDAP_OPT_NETWORK_TIMEOUT
     struct timeval nettimeout;
#endif

     *ld_out = NULL;

     if (g_utf8_strchr(server->ldaphost, -1, ':') != NULL) {
#ifdef HAVE_LDAP_INITIALIZE
	  rc = ldap_initialize(&ld, server->ldaphost);
	  if (rc != LDAP_SUCCESS) {
	       ld = NULL;
	  }

	  if(!ld) {
	       error_push(open_context, 
			  _("Failed to initialize LDAP structure for server '%1$s': %2$s."),
			  server->name,
			  ldap_err2string(rc));
	  }
#else
	  ld = NULL;
	  error_push(open_context,
		     _("Cannot connect to '%s'. No URI support available. Your LDAP toolkit does not support LDAP URIs."),
		     server->name);
#endif
     } else {
	  ld = ldap_init(server->ldaphost, server->ldapport); 
	  if(!ld) {
	       error_push(open_context, 
			  _("Failed to initialize LDAP structure for server %1$s: %2$s."),
			  server->name,
			  strerror(errno));
	  }
     }

     if (ld) {
	  server->server_down = 0;
	  server->version = LDAP_VERSION2;

	  /* setup timeouts */
	  i = DEFAULT_LDAP_TIMEOUT;
	  ldap_set_option(ld, LDAP_OPT_TIMELIMIT, &i);
	  
#ifdef LDAP_OPT_NETWORK_TIMEOUT
	  nettimeout.tv_sec = DEFAULT_NETWORK_TIMEOUT;
	  nettimeout.tv_usec = 0;
	  ldap_set_option(ld, LDAP_OPT_NETWORK_TIMEOUT, &nettimeout);
#endif

#ifndef HAVE_OPENLDAP12
	  if (flags & TRY_VERSION3) {
	       /* try to use LDAP Version 3 */
	       
	       int version = LDAP_VERSION3;
	       if (ldap_set_option(ld, LDAP_OPT_PROTOCOL_VERSION,
				   &version) == LDAP_OPT_SUCCESS) {
		    server->version = LDAP_VERSION3;
/*  	       } else { */
/*  		    error_push(open_context, message); */
/*  		    push_ldap_addl_error(ld, open_context); */
	       }
	  }
#endif

	  if (server->enabletls) {
#if defined(HAVE_TLS)
	       {
		    if (server->version != LDAP_VERSION3) {
			 error_push(open_context,
				    _("Server '%s': Couldn't set protocol version to LDAPv3."),
				    server->name);
		    }
	       }

	       /* Let's turn on TLS */
	       rc = ldap_start_tls_s(ld, NULL, NULL);
	       if(rc != LDAP_SUCCESS) {
		    error_push(open_context,
			       _("Couldn't enable TLS on the LDAP connection to '%1$s': %2$s"),
			       server->name,
			       ldap_err2string(rc));
		    push_ldap_addl_error(ld, open_context);
		    ldap_unbind(ld);

		    return rc;
	       }
#else
	       error_push(open_context,
			  _("Cannot use TLS with server '%s'. TLS was not found to be supported by your LDAP libraries.\n"
			    "See README.TLS for more information.\n"),
			  server->name);
#endif /* defined(HAVE_TLS) */
	  }

	  if(server->local_cache_timeout >= 0) {
#if HAVE_LDAP_CLIENT_CACHE
	       ldap_enable_cache(ld, server->local_cache_timeout, 
				 DEFAULT_LOCAL_CACHE_SIZE);
#else
	       error_push(open_context, 
			  _("Cannot cache LDAP objects for server '%s'.\n"
			    "OpenLDAP client-side caching was not enabled.\n"
			    "See configure --help for more information.\n"),
			  server->name);
#endif
	  }
	  
	  /* perform the auth */
	  rc = do_ldap_auth(ld, server, open_context);

	  if (rc != LDAP_SUCCESS) {
	       /* Maybe we cannot use LDAPv3 ... try again */
	       error_push(open_context,
			  _("Couldn't bind LDAP connection to '%1$s': %2$s"),
			  server->name, ldap_err2string(rc));
	       push_ldap_addl_error(ld, open_context);
	       statusbar_msg_clear();
	       /* might as well clean this up */
	       ldap_unbind(ld);
	       ld = NULL;
	  } else {
	       /* always store connection handle, regardless of connection
		  caching -- call close_connection() after each operation
		  to do the caching thing or not */
	       server->connection = ld;
	       server->missing_closes = 1;
	       server->incarnation++;
	  }

     }

     /* experimental referral stuff ... */
     if (ld) {
	  rc = ldap_set_option(ld, LDAP_OPT_REFERRALS, LDAP_OPT_OFF);

	  if (rc != LDAP_OPT_SUCCESS ) {
	       error_push(open_context,
			  _("Could not disable following referrals on connection to server '%s'."), server->name);
	       push_ldap_addl_error(ld, open_context);

	       /* this is not a fatal error, so continue */
	  }
     }

     *ld_out = ld;
     return rc;
}

/** Returns a ldapserver object (either an existing or a newly
    created) usable to search below the base_url. 

    The server gets looked up in the following way:

    1) the credentials of the parent server get used with a
       newly created ldapserver

    2) The base_url gets looked up as the canonical name. If a match
       is found and the credentials for this server work a copy of the 
       thus found object gets returned

    3) An anonymous bind gets attempted.
  */
GqServer*
get_referral_server(int error_context,
		    GqServer *parent,
		    const char *base_url)
{
     LDAPURLDesc *desc = NULL;
     GqServer *newserver = NULL, *s;
     int ld_err;

     g_assert(parent);

     if (ldap_url_parse(base_url, &desc) == 0) {
	  GString *new_uri = g_string_sized_new(strlen(base_url));
	  g_string_sprintf(new_uri, "%s://%s:%d/",
			   desc->lud_scheme,
			   desc->lud_host,
			   desc->lud_port);

	  newserver = gq_server_new();

	  if (! config->never_leak_credentials) {
	       copy_ldapserver(newserver, parent);

	       g_free_and_dup(newserver->name,     new_uri->str);
	       g_free_and_dup(newserver->ldaphost, new_uri->str);
	       g_free_and_dup(newserver->basedn,   desc->lud_dn);

	       /* some sensible settings for the "usual" case:
		  Anonymous bind. Also show referrals */
	       newserver->ask_pw   = 0;
	       newserver->show_ref = 1;
	       newserver->quiet    = 1;

	       if (open_connection_ex(error_context,
				      newserver, &ld_err)) {
		    close_connection(newserver, FALSE);

		    statusbar_msg(_("Initialized temporary server-definition '%1$s' from existing server '%2$s'"), new_uri->str, parent->name);

		    goto done;
	       }
	       if (ld_err == LDAP_SERVER_DOWN) {
		    goto done;
	       }

	       reset_ldapserver(newserver);

	       /* check: do we have this server around already??? */
	       s = server_by_canon_name(new_uri->str, TRUE);

	       if (s) {
		    copy_ldapserver(newserver, s);

		    g_free_and_dup(newserver->name,     new_uri->str);
		    g_free_and_dup(newserver->ldaphost, new_uri->str);
		    g_free_and_dup(newserver->basedn,   desc->lud_dn);

		    /* some sensible settings for the "usual" case:
		       Anonymous bind. Also show referrals */
		    newserver->ask_pw   = 0;
		    newserver->show_ref = 1;
		    newserver->quiet    = 1;

		    if (open_connection_ex(error_context,
					   newserver, &ld_err)) {
			 close_connection(newserver, FALSE);
			 statusbar_msg(_("Initialized temporary server-definition '%1$s' from existing server '%2$s'"), new_uri->str, s->name);
			 goto done;
		    }
		    if (ld_err == LDAP_SERVER_DOWN) {
			 goto done;
		    }
	       }
	  }

	  reset_ldapserver(newserver);

	  /* anonymous */
	  copy_ldapserver(newserver, parent);
	  
	  g_free_and_dup(newserver->name,     new_uri->str);
	  g_free_and_dup(newserver->ldaphost, new_uri->str);
	  g_free_and_dup(newserver->basedn,   desc->lud_dn);

	  g_free_and_dup(newserver->binddn, "");
	  g_free_and_dup(newserver->bindpw, "");
	  g_free_and_dup(newserver->enteredpw, "");

	  newserver->bindtype = BINDTYPE_SIMPLE;

	  if (open_connection_ex(error_context, newserver, &ld_err)) {
	       close_connection(newserver, FALSE);
	       statusbar_msg(_("Initialized temporary server-definition '%1$s' from existing server '%2$s'"), new_uri->str, parent->name);
	       goto done;
	  }
	  if (ld_err == LDAP_SERVER_DOWN) {
	       goto done;
	  }
     }

 done:
     if (desc) ldap_free_urldesc(desc);
     if (newserver) {
	  newserver->quiet = 0;
	  canonicalize_ldapserver(newserver);
     }

     return newserver;
}

static gboolean
delete_on_escape(GtkDialog* dialog, GdkEventKey* ev) {
	if(ev->keyval == GDK_Escape) {
		gtk_dialog_response(dialog, GTK_RESPONSE_DELETE_EVENT);
		return TRUE;
	}
	return FALSE;
}

static gchar*
get_password_from_dialog(GqServer* server) {
	GladeXML    * xml = NULL;
	GtkSizeGroup* group_left  = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkSizeGroup* group_right = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
	GtkWidget   * old_grab = gtk_grab_get_current();
	gchar const * password = NULL;
	gchar       * string = NULL;

	xml = glade_xml_new(PACKAGE_PREFIX "/share/gq/gq.glade", "password_dialog", NULL);
	if(!xml) {
		GtkWidget* dlg = gtk_message_dialog_new(NULL, // FIXME: set modal parent
							GTK_DIALOG_MODAL,
							GTK_MESSAGE_ERROR,
							GTK_BUTTONS_CLOSE,
							"%s", _("The user interface definition could not be "
								"loaded, please make sure that GQ is correctly "
								"installed."));
		gtk_dialog_run(GTK_DIALOG(dlg));
		gtk_widget_hide(dlg);
		gtk_widget_destroy(dlg);
	}

	// FIXME: the GtkCTree has got the pointer grabbed
	if(old_grab) {
		gtk_widget_hide(old_grab);
		gtk_widget_show(old_grab);
	}

	g_signal_connect(glade_xml_get_widget(xml, "password_dialog"), "key-press-event",
			 G_CALLBACK(delete_on_escape), NULL);

	gtk_size_group_add_widget(group_left, glade_xml_get_widget(xml, "label_hostname"));
	gtk_size_group_add_widget(group_left, glade_xml_get_widget(xml, "label_bind_dn"));
	gtk_size_group_add_widget(group_left, glade_xml_get_widget(xml, "label_bind_mode"));
	gtk_size_group_add_widget(group_left, glade_xml_get_widget(xml, "label_password"));
	gtk_size_group_add_widget(group_right, glade_xml_get_widget(xml, "input_hostname"));
	gtk_size_group_add_widget(group_right, glade_xml_get_widget(xml, "input_bind_dn"));
	gtk_size_group_add_widget(group_right, glade_xml_get_widget(xml, "input_bind_mode"));
	gtk_size_group_add_widget(group_right, glade_xml_get_widget(xml, "input_password"));

	if(!gnome_keyring_is_available()) {
		gtk_widget_hide(glade_xml_get_widget(xml, "checkbutton_save_password"));
	}

	if(server->name && server->ldaphost &&
	   (!strcmp(server->name, server->ldaphost))) {
		// Server Name and Hostname are equal, do not print twice
		// TRANSLATORS: "hostname:port"
		string = g_strdup_printf(_("%s:%d"), server->name, server->ldapport);
	} else {
		// TRANSLATORS: "servername (hostname:port)"
		string = g_strdup_printf(_("%s (%s:%d)"), server->name, server->ldaphost, server->ldapport);
	}
	gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(xml, "input_hostname")), string);
	g_free(string);

	gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(xml, "input_bind_dn")), server->binddn);

	// TRANSLATORS: "(Simple,Kerberos,SASL) Authentication"
	string = g_strdup_printf(_("%s Authentication"), token_bindtype[server->bindtype].keyword);
	// FIXME: get this translatable as a whole text
	gtk_label_set_text(GTK_LABEL(glade_xml_get_widget(xml, "input_bind_mode")), string);
	g_free(string);
	string = NULL;

	switch(gtk_dialog_run(GTK_DIALOG(glade_xml_get_widget(xml, "password_dialog")))) {
	case GTK_RESPONSE_OK:
		g_free_and_dup(server->bindpw, gtk_entry_get_text(GTK_ENTRY(glade_xml_get_widget(xml, "input_password"))));
		string = g_strdup (server->bindpw);

		// FIXME: store password only from successful connections
		if(gnome_keyring_is_available() &&
		   gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(glade_xml_get_widget(xml, "checkbutton_save_password")))) {
			gq_keyring_save_server_password(server);
		}
		break;
	case GTK_RESPONSE_HELP:
		// FIXME: add documentation and something about the login dialog
	default:
		break;
	}
	gtk_widget_destroy(GTK_WIDGET(glade_xml_get_widget(xml, "password_dialog")));
	g_object_unref(xml);
	g_object_unref(group_left);
	g_object_unref(group_right);

	return string;
}

/*
 * open connection to LDAP server, and store connection for caching
 */
LDAP*
open_connection_ex(int open_context, GqServer *server, int *ldap_errno)
{
     LDAP *ld;
     int rc;
     GString *message = NULL;
     int newpw = 0;

     if (ldap_errno) *ldap_errno = LDAP_SUCCESS;
     if(!server) return NULL;

     server->missing_closes++;

     /* reuse previous connection if available */
     if(server->connection) {
	  if (server->server_down == 0)
	       return(server->connection);
	  else {
	       /* do not leak file descriptors in case we need to
                * "rebind" */
	       ldap_unbind(server->connection);
	       server->connection = NULL;
	  }
     }

     if (server->ask_pw &&
	 server->binddn[0] && /* makes sense only if we bind as someone */
	 /*	 server->bindpw[0] == 0 &&  */
	 server->enteredpw[0] == 0) {
	  char *ret = NULL;

	  if (server->quiet) {
	       return NULL;
	  }

	  ret = gq_keyring_get_password(server);

	  if(!ret) {
		  ret = get_password_from_dialog(server);
	  }

	  if (ret) { /* FIXME: somehow get modal_for widget */
	       if (ret) {
		    g_free(server->enteredpw);
		    server->enteredpw = ret;
		    newpw = 1;
	       } else {
		    server->enteredpw[0] = 0;
	       }
	  } else {
	       return NULL;
	  }
     }

     if(server->ldapport == 389) {
	  statusbar_msg(_("Connecting to %s"), server->ldaphost);
     } else {
	  statusbar_msg(_("Connecting to %1$s port %2$d"),
			server->ldaphost, server->ldapport);
     }

     ld = NULL;
/*      open_context = error_new_context(_("Error connecting to server")); */

     rc = do_ldap_connect(&ld, server, open_context,
			  TRY_VERSION3);
     if (rc == LDAP_PROTOCOL_ERROR) {
	  /* this might be an indication that the server does not
	     understand LDAP_VERSION3 - retry without trying VERSION3 */
	  rc = do_ldap_connect(&ld, server, open_context, 0);
	  if (rc == LDAP_SUCCESS) error_clear(open_context);
     } else if (rc == LDAP_INVALID_CREDENTIALS) {
	  /* Should the server configuration pop up? */
     }

     /* this is not quite OK: FIXME */
     if (server->quiet) error_clear(open_context);
/*      error_flush(open_context); */

     if (rc != LDAP_SUCCESS) {
	  if (newpw) server->enteredpw[0] = 0;
     }

     if (ldap_errno) *ldap_errno = rc;

     return(ld);
}

LDAP *open_connection(int open_context, GqServer *server)
{
     return open_connection_ex(open_context, server, NULL);
}

/*
 * called after every set of LDAP operations. This preserves the connection
 * if caching is enabled for the server. Set always to TRUE if you want to
 * close the connection regardless of caching.
 */
void
close_connection(GqServer *server, int always)
{
	server->missing_closes--;

	if(server->connection &&
	   ((server->missing_closes == 0 && !server->cacheconn) || always))
	{
		/* definitely close this connection */
		ldap_unbind(server->connection);
		server->connection = NULL;
	}
}


/*
 * clear cached server schema
 */
void clear_server_schema(GqServer *server)
{
#ifdef HAVE_LDAP_STR2OBJECTCLASS
     GList *list;
     struct server_schema *ss;

     if(server->ss) {
	  ss = server->ss;

	  /* objectclasses */
	  list = ss->oc;
	  if(list) {
	       while(list) {
		    ldap_objectclass_free(list->data);
		    list = list->next;
	       }
	       g_list_free(ss->oc);
	  }

	  /* attribute types */
	  list = ss->at;
	  if(list) {
	       while(list) {
		    ldap_attributetype_free(list->data);
		    list = list->next;
	       }
	       g_list_free(ss->at);
	  }

	  /* matching rules */
	  list = ss->mr;
	  if(list) {
	       while(list) {
		    ldap_matchingrule_free(list->data);
		    list = list->next;
	       }
	       g_list_free(ss->mr);
	  }

	  /* syntaxes */
	  list = ss->s;
	  if(list) {
	       while(list) {
		    ldap_syntax_free(list->data);
		    list = list->next;
	       }
	       g_list_free(ss->s);
	  }

	  FREE(server->ss, "struct server_schema");
	  server->ss = NULL;
     }
     else
	  server->flags &= ~SERVER_HAS_NO_SCHEMA;
#endif

}


/*
 * delete entry
 */
gboolean delete_entry_full(int delete_context,
			   GqServer *server, char *dn,
			   gboolean recursive)
{
     LDAP *ld;
     int msg;
     gboolean rc = TRUE;
     LDAPControl c;
     LDAPControl *ctrls[2] = { NULL, NULL } ;
     LDAPMessage *res = NULL, *e;

     c.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     c.ldctl_value.bv_val	= NULL;
     c.ldctl_value.bv_len	= 0;
     c.ldctl_iscritical	= 1;
     
     ctrls[0] = &c;

     /* FIXME confirm-mod check here */

     set_busycursor();

     if( (ld = open_connection(delete_context, server) ) == NULL) {
	  set_normalcursor();
	  return(FALSE);
     }

     if (recursive) {
	  int something_to_do = 1;
	  static char* attrs [] = {"dn", NULL};

	  while (something_to_do) {
	       something_to_do = 0;
	       msg = ldap_search_ext_s(ld, dn,
				       LDAP_SCOPE_ONELEVEL,
				       "(objectClass=*)",	
				       (char **)attrs,	/* attrs */
				       1,		/* attrsonly */
				       ctrls,		/* serverctrls */
				       NULL,		/* clientctrls */
				       NULL,		/* timeout */
				       LDAP_NO_LIMIT,	/* sizelimit */
				       &res);

	       if(msg == LDAP_NOT_SUPPORTED) {
		    msg = ldap_search_s(ld, dn, LDAP_SCOPE_ONELEVEL,
					"(objectClass=*)",
					(char **) attrs, 1, &res);
	       }

	       if(msg == LDAP_SUCCESS) {
		    for (e = ldap_first_entry(ld, res); e ; 
			 e = ldap_next_entry(ld, e)) {
			 char *dn2 = ldap_get_dn(ld, e);
			 gboolean delok = delete_entry_full(delete_context, 
							    server, dn2, TRUE);
			 if (dn2) ldap_memfree(dn2);

			 if (delok) {
			      something_to_do = 1;
			 } else {
			      goto done;
			 }
		    }
	       } else if (msg == LDAP_SERVER_DOWN) {
		    server->server_down++;
		    goto done;
	       }
	       if (res) ldap_msgfree(res);
	       res = NULL;
	  }
     }

     statusbar_msg(_("Deleting: %s"), dn);

     msg = ldap_delete_ext_s(ld, dn, ctrls, NULL);

     if(msg == LDAP_NOT_SUPPORTED) {
	  msg = ldap_delete_s(ld, dn);
     }

#if HAVE_LDAP_CLIENT_CACHE
     ldap_uncache_entry(ld, dn);
#endif

     if(msg != LDAP_SUCCESS) {
	  error_push(delete_context, 
		     "Error deleting DN '%1$s' on '%2$s': %3$s", 
		     dn, server->name, ldap_err2string(msg));
	  rc = FALSE;
     }
     else {
	  statusbar_msg(_("Deleted %s"), dn);
     }

 done:
     if (res) ldap_msgfree(res);
     set_normalcursor();
     close_connection(server, FALSE);

     return(rc);
}


gboolean delete_entry(int delete_context,
		      GqServer *server, char *dn) 
{
     return delete_entry_full(delete_context, server, dn, FALSE);
}

gboolean do_recursive_delete(int delete_context, 
			     GqServer *server, char* dn)
{
     return delete_entry_full(delete_context, server, dn, TRUE);
}

/*
 * display hourglass cursor on mainwin
 */
void set_busycursor(void)
{
     GdkCursor *busycursor;

     busycursor = gdk_cursor_new(GDK_WATCH);
     gdk_window_set_cursor(mainwin.mainwin->window, busycursor);
     gdk_cursor_destroy(busycursor);

}


/*
 * set mainwin cursor to default
 */
void set_normalcursor(void)
{

     gdk_window_set_cursor(mainwin.mainwin->window, NULL);

}

/*
 * callback for key_press_event on a widget, destroys obj if key was esc
 */
int close_on_esc(GtkWidget *widget, GdkEventKey *event, gpointer obj)
{
     if(event && event->type == GDK_KEY_PRESS && event->keyval == GDK_Escape) {
	  gtk_widget_destroy(GTK_WIDGET(obj));
	  g_signal_stop_emission_by_name(widget, "key_press_event");
	  return(TRUE);
     }

     return(FALSE);
}


/*
 * callback for key_press_event on a widget, calls func if key was esc
 */
int func_on_esc(GtkWidget *widget, GdkEventKey *event, GtkWidget *window)
{
     void (*func)(GtkWidget *);

     if(event && event->type == GDK_KEY_PRESS && event->keyval == GDK_Escape) {
	  func = gtk_object_get_data(GTK_OBJECT(window), "close_func");
	  func(widget);
	  g_signal_stop_emission_by_name(widget, "key_press_event");
	  return(TRUE);
     }

     return(FALSE);
}


int tokenize(const struct tokenlist *list, const char *keyword)
{
     int i;

     for(i = 0; list[i].keyword && strlen(list[i].keyword); i++)
	  if(!strcasecmp(list[i].keyword, keyword))
	       return(list[i].token);

     return(0);
}


const char *detokenize(const struct tokenlist *list, int token)
{
     int i;

     for(i = 0; list[i].keyword && strlen(list[i].keyword); i++)
	  if(list[i].token == token)
	       return(list[i].keyword);

     return(list[0].keyword);
}


const void *detokenize_data(const struct tokenlist *list, int token)
{
     int i;

     for(i = 0; strlen(list[i].keyword); i++)
	  if(list[i].token == token)
	       return(list[i].data);

     return(list[0].data);
}

/*
 * return pointer to username (must be freed)
 */
char *get_username(void)
{
     struct passwd *pwent;
     char *username;

     username = NULL;
     pwent = getpwuid(getuid());
     if(pwent && pwent->pw_name)
	  username = strdup(pwent->pw_name);
     endpwent();

     return(username);
}


/* these should probably belong to struct mainwin_data */
static guint context = 0, msgid = 0;

/*
 * display message in main window's statusbar, and flushes the
 * GTK event queue
 */
void statusbar_msg(const char *fmt, ...)
{
     /* do not use g_string_sprintf, as it usually does not support
	numbered arguments */
     int len = strlen(fmt) * 2;
     char *buf;
     int n;

     if (len > 0) {
	  for (;;) {
	       va_list ap;
	       buf = g_malloc(len);
	       *buf = 0;
	       
	       va_start(ap, fmt);
	       n = g_vsnprintf(buf, len, fmt, ap);
	       va_end(ap);
	       
	       if (n > len || n == -1) {
		    g_free(buf);
		    len *= 2;
		    continue;
	       }
	       break;
	  } 
     } else {
	  buf = g_strdup("");
     }

     statusbar_msg_clear();

     msgid = gtk_statusbar_push(GTK_STATUSBAR(mainwin.statusbar), 
				context, buf);
     message_log_append(buf);
     g_free(buf);

     /* make sure statusbar gets updated right away */
     while(gtk_events_pending())
	  gtk_main_iteration();
}

void statusbar_msg_clear()
{
     if(!context)
	  context =
	       gtk_statusbar_get_context_id(GTK_STATUSBAR(mainwin.statusbar),
					    "mainwin");
     if(msgid)
	  gtk_statusbar_remove(GTK_STATUSBAR(mainwin.statusbar),
			       context, msgid);
}

/*
 * return pointer to GqServer matching the canonical name
 */
GqServer *server_by_canon_name(const char *name,
					gboolean include_transient)
{
     GqServer *server = NULL;
     GList *I;

     if(name == NULL || name[0] == '\0')
	  return NULL;

     server = gq_server_list_get_by_canon_name(gq_server_list_get(), name);
     if(!server && include_transient) {
	  for (I = transient_servers ; I ; I = g_list_next(I)) {
	       if(!strcmp(((GqServer*)I->data)->canon_name, name)) {
		    server = I->data;
		    break;
	       }
	  }
     }
     return server;
}

gboolean
is_transient_server(const GqServer *server) {
	g_return_val_if_fail(GQ_IS_SERVER(server), FALSE);
	return !gq_server_list_contains(gq_server_list_get(), server);
}

/*
 * check if entry has a subtree
 */
int is_leaf_entry(int error_context,
		  GqServer *server, char *dn)
{
     LDAP *ld;
     LDAPMessage *res;
     int msg, is_leaf, rc;
     char *attrs[] = {
	  LDAP_NO_ATTRS,
	  NULL
     };
     LDAPControl c;
     LDAPControl *ctrls[2] = { NULL, NULL } ;

     /*  ManageDSAit  */
     c.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
     c.ldctl_value.bv_val	= NULL;
     c.ldctl_value.bv_len	= 0;
     c.ldctl_iscritical	= 1;
     
     ctrls[0] = &c;

     is_leaf = 0;

     set_busycursor();

     if( (ld = open_connection(error_context, server) ) == NULL) {
	  set_normalcursor();
	  return(-1);
     }

     statusbar_msg(_("Checking subtree for %s"), dn);

     rc = ldap_search_ext(ld, dn, LDAP_SCOPE_ONELEVEL, "(objectclass=*)",
			   attrs,		/* attrs */
			   0,			/* attrsonly */
			   ctrls,		/* serverctrls */
			   NULL,		/* clientctrls */
			   NULL,		/* timeout */
			   LDAP_NO_LIMIT,	/* sizelimit */
			   &msg);

     if(rc != -1) {
	  if( (ldap_result(ld, msg, 0, NULL, &res) != LDAP_RES_SEARCH_ENTRY))
	       is_leaf = 1;
	  ldap_msgfree(res);
	  ldap_abandon(ld, msg);
     }

     close_connection(server, FALSE);
     set_normalcursor();
     statusbar_msg_clear();

     return(is_leaf);
}

/*
 * check if child is a direct subentry of possible_parent
 */
gboolean is_direct_parent(char *child, char *possible_parent) 
{
     /* SHOULD use gq_ldap_explode_dn for this */
     char *c = g_utf8_strchr(child, -1, ',');
     if (c && (strcasecmp(c + 1, possible_parent) == 0)) return TRUE;
     return FALSE;
}

/*
 * check if child is a (possibly indirect) subentry of possible_ancestor
 */
gboolean is_ancestor(char *child, char *possible_ancestor) 
{
     char **rdn = gq_ldap_explode_dn(child, FALSE);
     GString *s;
     int n;
     gboolean rc = FALSE;

     for (n = 0 ; rdn[n] ; n++) {
     }

     s = g_string_new("");
     for (n-- ; n >= 0 ; n--) {
	  g_string_insert(s, 0, rdn[n]);
	  if ((strcasecmp(s->str, possible_ancestor) == 0)) {
	       rc = TRUE;
	       break;
	  }
	  g_string_insert(s, 0, ",");
     }

     g_string_free(s, TRUE);
     gq_exploded_free(rdn);

     return rc;
}

GList *ar2glist(char *ar[])
{
     GList *tmp;
     int i;

     if(ar == NULL) {
	  /* gtk_combo_set_popdown_strings() can't handle an
	     empty list, so give it a list with an empty entry */
	  tmp = g_list_append(NULL, "");
	  return(tmp);
     }

     tmp = NULL;
     i = 0;
     while(ar[i])
	  tmp = g_list_append(tmp, ar[i++]);

     return(tmp);
}


/*
 * pops up a warning dialog (with hand icon), and displays all messages in
 * the GList , one per line. The GList is freed here.
 */
void warning_popup(GList *messages)
{
     GList *list;
     GtkWidget *window, *vbox1, *vbox2, *vbox3, *label, *hbox0;
     GtkWidget *hbox1, *ok_button;
     GtkWidget *pixmap;

     window = gtk_dialog_new();
     gtk_container_border_width(GTK_CONTAINER(window), CONTAINER_BORDER_WIDTH);
     gtk_window_set_title(GTK_WINDOW(window), _("Warning"));
     gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
     vbox1 = GTK_DIALOG(window)->vbox;
     gtk_widget_show(vbox1);

     hbox1 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 10);
     pixmap = gtk_image_new_from_file(PACKAGE_PREFIX "/share/pixmaps/gq/warning.xpm");
     gtk_widget_show(pixmap);
     gtk_box_pack_start(GTK_BOX(hbox1), pixmap, TRUE, TRUE, 10);

     vbox2 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox2);
     gtk_box_pack_start(GTK_BOX(hbox1), vbox2, TRUE, TRUE, 10);

     list = messages;
     while(list) {
	  label = gtk_label_new((char *) list->data);
	  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
	  gtk_widget_show(label);
	  gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
	  list = list->next;
     }

     /* OK button */
     vbox3 = GTK_DIALOG(window)->action_area;
     gtk_widget_show(vbox3);

     hbox0 = gtk_hbutton_box_new();
     gtk_widget_show(hbox0);
     gtk_box_pack_start(GTK_BOX(vbox3), hbox0, TRUE, TRUE, 0);

     ok_button = gtk_button_new_from_stock(GTK_STOCK_OK);

     g_signal_connect_swapped(ok_button, "clicked",
			       G_CALLBACK(gtk_widget_destroy),
			       window);
     gtk_box_pack_start(GTK_BOX(hbox0), ok_button, FALSE, FALSE, 0);
     GTK_WIDGET_SET_FLAGS(ok_button, GTK_CAN_DEFAULT);
     gtk_widget_grab_default(ok_button);
     gtk_widget_show(ok_button);

     /* what does this mean? PS: 20030928 - FIXME */
     g_signal_connect_swapped(window, "destroy",
			       G_CALLBACK(gtk_widget_destroy),
			       window);

     g_signal_connect_swapped(window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               window);

     gtk_widget_show(window);

     g_list_free(messages);

}


void single_warning_popup(char *message)
{
     GList *msglist;

     msglist = g_list_append(NULL, message);
     warning_popup(msglist);

}


#ifdef HAVE_LDAP_STR2OBJECTCLASS
GList *find_at_by_mr_oid(GqServer *server, const char *oid)
{
     GList *list, *srvlist;
     LDAPAttributeType *at;

     list = NULL;
     srvlist = server->ss->at;
     while(srvlist) {
	  at = (LDAPAttributeType *) srvlist->data;

	  if( (at->at_equality_oid && !strcasecmp(oid, at->at_equality_oid)) ||
	      (at->at_ordering_oid && !strcasecmp(oid, at->at_ordering_oid)) ||
	      (at->at_substr_oid && !strcasecmp(oid, at->at_substr_oid)) ) {
	       if(at->at_names && at->at_names[0])
		    list = g_list_append(list, at->at_names[0]);
	       else
		    list = g_list_append(list, at->at_oid);
	  }
	  srvlist = srvlist->next;
     }

     return(list);
}

LDAPAttributeType *find_canonical_at_by_at(struct server_schema *schema,
					   const char *attr)
{
     GList *atlist;
     char **n;
     
     if (!schema) return NULL;
     
     for ( atlist = schema->at ; atlist ; atlist = atlist->next ) {
	  LDAPAttributeType *at = (LDAPAttributeType *) atlist->data;
	  if (!at) continue;
	  
	  for (n = at->at_names ; n && *n ; n++) {
/*  	       printf("%s ", *n); */
	       if (strcasecmp(*n, attr) == 0) {
		    /* found! */
		    return at;
	       }
	  }
     }
     return NULL;
}

GList *find_at_by_s_oid(GqServer *server, const char *oid)
{
     GList *list, *srvlist;
     LDAPAttributeType *at;

     list = NULL;
     srvlist = server->ss->at;
     while(srvlist) {
	  at = (LDAPAttributeType *) srvlist->data;

	  if(at->at_syntax_oid && !strcasecmp(oid, at->at_syntax_oid)) {
	       if(at->at_names && at->at_names[0])
		    list = g_list_append(list, at->at_names[0]);
	       else
		    list = g_list_append(list, at->at_oid);
	  }
	  srvlist = srvlist->next;
     }

     return(list);
}


GList *find_mr_by_s_oid(GqServer *server, const char *oid)
{
     GList *list, *srvlist;
     LDAPMatchingRule *mr;

     list = NULL;
     srvlist = server->ss->mr;
     while(srvlist) {
	  mr = (LDAPMatchingRule *) srvlist->data;

	  if(mr->mr_syntax_oid && !strcasecmp(oid, mr->mr_syntax_oid)) {
	       if(mr->mr_names && mr->mr_names[0])
		    list = g_list_append(list, mr->mr_names[0]);
	       else
		    list = g_list_append(list, mr->mr_oid);
	  }
	  srvlist = srvlist->next;
     }

     return(list);
}


GList *find_oc_by_at(int error_context,
		     GqServer *server, const char *atname)
{
     GList *list, *oclist;
     LDAPObjectClass *oc;
     int i, found;
     struct server_schema *ss = NULL;

     list = NULL;

     if (server == NULL) return NULL;
     ss = get_schema(error_context, server);
     if (ss == NULL) return NULL;

     oclist = ss->oc;
     while(oclist) {
	  oc = (LDAPObjectClass *) oclist->data;

	  found = 0;

	  if(oc->oc_at_oids_must) {
	       i = 0;
	       while(oc->oc_at_oids_must[i] && !found) {
		    if(!strcasecmp(atname, oc->oc_at_oids_must[i])) {
			 found = 1;
			 if(oc->oc_names && oc->oc_names[0])
			      list = g_list_append(list, oc->oc_names[0]);
			 else
			      list = g_list_append(list, oc->oc_oid);
		    }
		    i++;
	       }
	  }

	  if(oc->oc_at_oids_may) {
	       i = 0;
	       while(oc->oc_at_oids_may[i] && !found) {
		    if(!strcasecmp(atname, oc->oc_at_oids_may[i])) {
			 found = 1;
			 if(oc->oc_names && oc->oc_names[0])
			      list = g_list_append(list, oc->oc_names[0]);
			 else
			      list = g_list_append(list, oc->oc_oid);
		    }
		    i++;
	       }
	  }

	  oclist = oclist->next;
     }

     return(list);
}

const char *find_s_by_at_oid(int error_context, GqServer *server,
			     const char *oid)
{
     GList *srvlist;
     LDAPAttributeType *at;
     char **c;
     struct server_schema *ss = NULL;

     if (server == NULL) return NULL;
     ss = get_schema(error_context, server);

     if(ss == NULL || ss->at == NULL)
	  return(NULL);

     srvlist = ss->at;
     while(srvlist) {
	  at = (LDAPAttributeType *) srvlist->data;

	  for (c = at->at_names ; c && *c ; c++) {
	      if (!strcasecmp(oid, *c)) {
		  return at->at_syntax_oid;
	      }
	  }
	  srvlist = srvlist->next;
     }

     return NULL;
}

#else /* HAVE_LDAP_STR2OBJECTCLASS */


/* fall back to attributeName to find syntax. */

struct oid2syntax_t {
     const char *oid;
     const char *syntax;
};

static struct oid2syntax_t oid2syntax[] = {
     { "userPassword", "1.3.6.1.4.1.1466.115.121.1.40" },
     { "jpegPhoto", "1.3.6.1.4.1.1466.115.121.1.28" },
     { "audio", "1.3.6.1.4.1.1466.115.121.1.4" },
     { "photo", "1.3.6.1.4.1.1466.115.121.1.4" },
     { NULL, NULL },
};

const char *find_s_by_at_oid(GqServer *server, const char *oid)
{
     struct oid2syntax_t *os;
     for (os = oid2syntax ; os->oid ; os++) {
	  if (strcasecmp(os->oid, oid) == 0) return os->syntax;
     }
     
     return "1.3.6.1.4.1.1466.115.121.1.3";
}


#endif /* HAVE_LDAP_STR2OBJECTCLASS */


struct gq_template *find_template_by_name(const char *templatename)
{
     GList *I;

     for(I = config->templates ; I ; I = g_list_next(I)) {
	  struct gq_template *template = 
	       (struct gq_template *) I->data;
	  if(!strcasecmp(templatename, template->name))
	       return template;
     }
     return NULL;
}



void dump_mods(LDAPMod **mods)
{
     LDAPMod *mod;
     int cmod, cval;

     cmod = 0;
     while(mods[cmod]) {
	  mod = mods[cmod];
	  switch(mod->mod_op) {
	  case LDAP_MOD_ADD: printf("LDAP_MOD_ADD"); break;
	  case LDAP_MOD_DELETE: printf("LDAP_MOD_DELETE"); break;
	  case LDAP_MOD_REPLACE: printf("LDAP_MOD_REPLACE"); break;
	  case LDAP_MOD_BVALUES: printf("LDAP_MOD_BVALUES"); break;
	  }
	  printf(" %s\n", mod->mod_type);
	  cval = 0;
	  while(mod->mod_values && mod->mod_values[cval]) {
	       printf("\t%s\n", mod->mod_values[cval]);
	       cval++;
	  }

	  cmod++;
     }


}

struct popup_comm {
     int destroyed;
     int ended;
     int rc;
};

static struct popup_comm *new_popup_comm() 
{
     struct popup_comm *n = g_malloc0(sizeof(struct popup_comm));
     n->ended = n->destroyed = n->rc = 0;
     return n;
}

static void free_popup_comm(struct popup_comm *c)
{
     g_assert(c);
     g_free(c);
}

/* gtk2 checked (multiple destroy callbacks safety), confidence 0.7 */
static void query_destroy(GtkWidget *window, struct popup_comm *comm) {
     g_assert(comm);
     if (!comm) return;
     if (!comm->ended)
	  gtk_main_quit(); /* quits only nested main loop */
     comm->destroyed = 1;
}

static void query_ok(GtkWidget *button, struct popup_comm *comm) {
     gtk_main_quit();
     comm->rc = 1;
}

static void query_cancel(GtkWidget *button, struct popup_comm *comm) {
     gtk_main_quit();
     comm->rc = 0;
}

/* pops up a dialog to retrieve user data via a GtkEntry. This
   functions waits for the data and puts it into outbuf.

   inout_buf afterward points to allocate memory that has to be free'd
   using g_free. As an input parameter it points to the current value
   of the information to be entered (if not NULL). The passed-in
   information will not be changed.
*/

int query_popup(const char *title, gchar **inout_buf, gboolean is_password,
		GtkWidget *modal_for)
{
     GtkWidget *window, *vbox1, *vbox2, *label, *inputbox, *button, *hbox0;
     int rc;
     GtkWidget *f = gtk_grab_get_current();
     struct popup_comm *comm = new_popup_comm();

     /* This is a BAD hack - it solves a problem with the query popup
        dialog that locks up focus handling with all the
        window-managers I have been able to test this with. Actually,
        it should be sufficient to let go of the focus, but
        hiding/showing seems to work... (as I do not know how to
        release the focus in gtk) - Any gtk Hackers around? */
     if (f != NULL) {
	 gtk_widget_hide(f);
	 gtk_widget_show(f);
     }

     if (modal_for) {
	  modal_for = gtk_widget_get_toplevel(modal_for);
     }

     window = gtk_dialog_new();
     gtk_container_border_width(GTK_CONTAINER(window), CONTAINER_BORDER_WIDTH);
     gtk_window_set_title(GTK_WINDOW(window), title);
     gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
     g_signal_connect(window, "destroy",
			G_CALLBACK(query_destroy),
			comm);
     g_signal_connect_swapped(window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               window);

     if (modal_for) {
	  gtk_window_set_modal(GTK_WINDOW(window), TRUE);
	  gtk_window_set_transient_for(GTK_WINDOW(window),
				       GTK_WINDOW(modal_for));
     }

     vbox1 = GTK_DIALOG(window)->vbox;
     gtk_widget_show(vbox1);

     label = gtk_label_new(title);
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(vbox1), label, TRUE, TRUE, 0);

     inputbox = gtk_entry_new();

     GTK_WIDGET_SET_FLAGS(inputbox, GTK_CAN_FOCUS);
     GTK_WIDGET_SET_FLAGS(inputbox, GTK_CAN_DEFAULT);
     if (is_password) {
	  gtk_entry_set_visibility(GTK_ENTRY(inputbox), FALSE);
     }

     if (inout_buf && *inout_buf) {
	  int pos = 0;
	  gtk_editable_insert_text(GTK_EDITABLE(inputbox), 
				   *inout_buf, strlen(*inout_buf), &pos);
     }

     gtk_widget_show(inputbox);
     g_signal_connect(inputbox, "activate",
			G_CALLBACK(query_ok), comm);
     gtk_box_pack_end(GTK_BOX(vbox1), inputbox, TRUE, TRUE, 5);

     vbox2 = GTK_DIALOG(window)->action_area;
     gtk_container_border_width(GTK_CONTAINER(vbox2), 0);
     gtk_widget_show(vbox2);

     hbox0 = gtk_hbutton_box_new();
     gtk_widget_show(hbox0);
     gtk_box_pack_start(GTK_BOX(vbox2), hbox0, TRUE, TRUE, 0);

     button = gtk_button_new_from_stock(GTK_STOCK_OK);

     g_signal_connect(button, "clicked",
			G_CALLBACK(query_ok), comm);
     gtk_box_pack_start(GTK_BOX(hbox0), button, FALSE, FALSE, 0);
     GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
     GTK_WIDGET_SET_FLAGS(button, GTK_RECEIVES_DEFAULT);
     gtk_widget_grab_default(button);
     gtk_widget_show(button);

     button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

     g_signal_connect(button, "clicked",
			G_CALLBACK(query_cancel), 
			comm);

     gtk_box_pack_end(GTK_BOX(hbox0), button, FALSE, FALSE, 0);
     gtk_widget_show(button);

/*       gtk_window_set_transient_for(GTK_WINDOW(window),  */
/*  				  GTK_WINDOW(getMainWin())); */
     gtk_widget_grab_focus(GTK_WIDGET(window));
     gtk_window_set_modal(GTK_WINDOW(window), TRUE);

     gtk_widget_show(window);
     gtk_widget_grab_focus(inputbox);

     gtk_main();
     comm->ended = 1;

     if (! comm->destroyed && comm->rc) {
	 *inout_buf = gtk_editable_get_chars(GTK_EDITABLE(inputbox), 0, -1);
     } else {
	 *inout_buf = NULL;
     }

     if (! comm->destroyed ) {
	 gtk_widget_destroy(window);
     }

     rc = comm->rc;
     free_popup_comm(comm);

     return rc;
}


/* pops up a question dialog to ask the user a simple question. This
   functions waits for the answer and returns it. */

int question_popup(const char *title, const char *question)
{
     GtkWidget *window, *hbox0, *hbox1, *vbox1, *hbox2, *label, *button, *pixmap;
     int rc;
     GtkWidget *f = gtk_grab_get_current();
     struct popup_comm *comm = new_popup_comm();

     /* This is a BAD hack - it solves a problem with the query popup
        dialog that locks up focus handling with all the
        window-managers I have been able to test this with. Actually,
        it should be sufficient to let go of the focus, but
        hiding/showing seems to work... (as I do not know how to
        release the focus in gtk) - Any gtk Hackers around? */
     if (f != NULL) {
	 gtk_widget_hide(f);
	 gtk_widget_show(f);
     }

     window = gtk_dialog_new();
     gtk_container_border_width(GTK_CONTAINER(window), CONTAINER_BORDER_WIDTH);
     gtk_window_set_title(GTK_WINDOW(window), title);
     gtk_window_set_policy(GTK_WINDOW(window), FALSE, FALSE, FALSE);
     g_signal_connect(window, "destroy",
			G_CALLBACK(query_destroy),
			comm);
     g_signal_connect_swapped(window, "key_press_event",
                               G_CALLBACK(close_on_esc),
                               window);

     gtk_widget_show(window);

     vbox1 = GTK_DIALOG(window)->vbox;
     gtk_widget_show(vbox1);

     hbox1 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 10);
     pixmap = gtk_image_new_from_file(PACKAGE_PREFIX "/share/pixmaps/gq/warning.xpm");
     gtk_widget_show(pixmap);
     gtk_box_pack_start(GTK_BOX(hbox1), pixmap, TRUE, TRUE, 10);

     label = gtk_label_new(question);
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.0);
     gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_LEFT);
     gtk_widget_show(label);
     gtk_box_pack_end(GTK_BOX(hbox1), label, TRUE, TRUE, 0);

     hbox2 = GTK_DIALOG(window)->action_area;
     gtk_widget_show(hbox2);

     hbox0 = gtk_hbutton_box_new();
     gtk_widget_show(hbox0);
     gtk_box_pack_start(GTK_BOX(hbox2), hbox0, TRUE, TRUE, 0);

     button = gtk_button_new_from_stock(GTK_STOCK_YES);

     g_signal_connect(button, "clicked",
			G_CALLBACK(query_ok), comm);
     gtk_box_pack_start(GTK_BOX(hbox0), button, FALSE, FALSE, 0);
     GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
     GTK_WIDGET_SET_FLAGS(button, GTK_RECEIVES_DEFAULT);
     gtk_widget_grab_default(button);
     gtk_widget_show(button);

     button = gtk_button_new_from_stock(GTK_STOCK_NO);

     g_signal_connect(button, "clicked",
			G_CALLBACK(query_cancel), 
			comm);

     gtk_box_pack_end(GTK_BOX(hbox0), button, FALSE, FALSE, 0);
     gtk_widget_show(button);

/*       gtk_window_set_transient_for(GTK_WINDOW(window),  */
/*  				  GTK_WINDOW(getMainWin())); */
     gtk_widget_grab_focus(GTK_WIDGET(window));
     gtk_window_set_modal(GTK_WINDOW(window), TRUE);

     gtk_widget_show(window);

     gtk_main();
     comm->ended = 1;

     if (! comm->destroyed) {
	 gtk_widget_destroy(window);
     }

     rc = comm->rc;
     free_popup_comm(comm);

     return rc;
}


/*
 * get all suffixes a server considers itself authorative for.
 */

GList *get_suffixes(int error_context, GqServer *server)
{
     LDAP *ld;
     LDAPMessage *res, *e;
     int msg, i;
     int num_suffixes = 0;
     char **vals;
     char *ldapv3_config[] = {
	  "namingcontexts",
	  NULL
     };
     
     GList *suffixes = NULL;
     
     set_busycursor();
     
     if( (ld = open_connection(error_context, server)) == NULL) {
	  set_normalcursor();
	  return NULL;
     }
     
     /* try LDAP V3 style config */
     statusbar_msg(_("Base search on NULL DN on server '%s'"), server->name);
     msg = ldap_search_s(ld, "", LDAP_SCOPE_BASE, "(objectclass=*)",
			 ldapv3_config, 0, &res);
     if(msg == LDAP_SUCCESS) {
	  e = ldap_first_entry(ld, res);
	  while (e) {
	       vals = ldap_get_values(ld, e, "namingcontexts");
	       if (vals) {
		    for(i = 0; vals[i]; i++) {
			 suffixes = g_list_append(suffixes, g_strdup(vals[i]));
/*  			 add_suffix(entry, ctreeroot, node, vals[i]); */
			 num_suffixes++;
		    }
		    ldap_value_free(vals);
	       }
	       e = ldap_next_entry(ld, e);
	  }
	  ldap_msgfree(res);
     } else if (msg == LDAP_SERVER_DOWN) {
	  server->server_down++;
	  /* do not try V2 in case of server down problems */
     }

     if(num_suffixes == 0) {
	  /* try Umich style config */
	  statusbar_msg(_("Base search on cn=config"));
	  msg = ldap_search_s(ld, "cn=config", LDAP_SCOPE_BASE,
			      "(objectclass=*)", NULL, 0, &res);
	  
	  if(msg == LDAP_SUCCESS) {
	       e = ldap_first_entry(ld, res);
	       while (e) {
		    char **valptr;
		    
		    vals = ldap_get_values(ld, e, "database");
		    if (vals) {
			 for (valptr = vals; valptr && *valptr; valptr++) {
			      char *p = *valptr;
				   
			      i = 0;
			      while (p[i] && p[i] != ':')
				   i++;
			      while (p[i] && (p[i] == ':' || p[i] == ' '))
				   i++;
			      if (p[i]) {
				   int len = strlen(p + i);
					
				   while (p[i + len - 1] == ' ')
					len--;
				   p[i + len] = '\0';

				   suffixes = g_list_append(suffixes,
							    g_strdup(p + i));
/*				   add_suffix(entry, ctreeroot, node, p + i); */
				   num_suffixes++;
			      }
			 }
			 ldap_value_free(vals);
		    }
		    e = ldap_next_entry(ld, e);
	       }
	       ldap_msgfree(res);
	  } else if (msg == LDAP_SERVER_DOWN) {
	       server->server_down++;
	  }
     }


     /* add the configured base DN if it's a different one */
     if(strlen(server->basedn) && (g_list_find_custom(suffixes, server->basedn, (GCompareFunc) strcmp) == NULL)) {
		suffixes = g_list_append(suffixes, g_strdup(server->basedn));
	        num_suffixes++;
     }

     set_normalcursor();
     close_connection(server, FALSE);

     statusbar_msg(ngettext("One suffix found", "%d suffixes found",
			    num_suffixes),
		   num_suffixes);

     return g_list_first(suffixes);
}

#ifdef HAVE_LDAP_STR2DN

/* OpenLDAP 2.1 both deprecated and broke ldap_explode_dn in one go (I
   won't comment on this).

   NOTE: this is just a first try to adapt code from Pierangelo
   Masarati <masarati@aero.polimi.it>
*/

gchar**
gq_ldap_explode_dn(gchar const* dn, int dummy) {
	int i, rc;
#if LDAP_API_VERSION > 2004
	LDAPDN parts;
#else
	LDAPDN *parts;
#endif
	GArray* array = NULL;
	gchar **retval = NULL;

	rc = ldap_str2dn(dn, &parts, LDAP_DN_FORMAT_LDAPV3);

	if (rc != LDAP_SUCCESS || parts == NULL) {
		return NULL;
	}

	array = g_array_new(TRUE, TRUE, sizeof(gchar*));
	for(i = 0; parts[i]; i++) {
		gchar* part = NULL;
		ldap_rdn2str(
#if LDAP_API_VERSION > 2004
			parts[i],
#else
			parts[0][i],
#endif
			&part, LDAP_DN_FORMAT_LDAPV3 | LDAP_DN_PRETTY );
		if(part && *part) {
			/* don't append the last (empty) part, to be compatible
			 * to ldap_explode_dn() */
			g_array_append_val(array, part);
		}
	}
	retval = (gchar**)array->data;
	g_array_free(array, FALSE);
	return retval;
}

void gq_exploded_free(char **exploded_dn)
{
     if (exploded_dn) free(exploded_dn);
}

#endif

static GtkWidget *enable_uline(GtkWidget *label) {
     gtk_label_set_use_underline(GTK_LABEL(label), TRUE);
     return label;
}

static GtkWidget *bin_enable_uline(GtkWidget *w) {
     GtkWidget *l;
     l = gtk_bin_get_child(GTK_BIN(w));
     enable_uline(l);
     return w;
}


GtkWidget *gq_label_new(const char *text) {
     return gtk_label_new_with_mnemonic(text);
}


GtkWidget *gq_radio_button_new_with_label(GSList *group,
					  const gchar *label) 
{
     return bin_enable_uline(gtk_radio_button_new_with_label(group, label));
}

GtkWidget *gq_menu_item_new_with_label(const gchar *text) {
     return bin_enable_uline(gtk_menu_item_new_with_label(text));
}

GtkWidget *gq_check_button_new_with_label(const gchar *text) {
     return bin_enable_uline(gtk_check_button_new_with_label(text));
}

GtkWidget *gq_button_new_with_label(const gchar *text) {
     return bin_enable_uline(gtk_button_new_with_label(text));
}

GtkWidget *gq_toggle_button_new_with_label(const gchar *text) {
     return bin_enable_uline(gtk_toggle_button_new_with_label(text));
}


struct dn_on_server *new_dn_on_server(const char *d,
				      GqServer *s)
{
     struct dn_on_server *dos = g_malloc0(sizeof(struct dn_on_server));
     dos->server = g_object_ref(s);

     if (d) {
	  dos->dn = g_strdup(d);
     } else {
	  dos->dn = NULL;
     }

     return dos;
}

void free_dn_on_server(struct dn_on_server *s)
{
     if (s) {
	  g_free(s->dn);
	  s->dn = NULL;
	  g_object_unref(s->server);
	  s->server = NULL;

	  g_free(s);
     }
}


/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
