/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 Peter Stamfest
    Copyright (C) 2006      Sven Herzberg

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

#ifndef GQ_SERVER_H
#define GQ_SERVER_H

#include <ldap.h>
#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _GqServer GqServer;
typedef GObjectClass     GqServerClass;

#define GQ_TYPE_SERVER        (gq_server_get_type())
#define GQ_SERVER(i)          (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_SERVER, GqServer))
#define GQ_IS_SERVER(i)       (G_TYPE_CHECK_INSTANCE_TYPE((i), GQ_TYPE_SERVER))

struct server_schema {
	GList *oc;
	GList *at;
	GList *mr;
	GList *s;
};

GType     gq_server_get_type(void);
GqServer* gq_server_new     (void);

#define gq_server_get_name(i) (gchar const*)(GQ_IS_SERVER(i) ? (server->name) : (NULL))
#define gq_server_set_name(i, new_name) if(GQ_IS_SERVER(i)) {g_free_and_dup(server->name, new_name);}

void free_ldapserver(GqServer *server);

/** NOTE: copy_ldapserver ONLY copies the configuration
    information. It DOES NOT copy the operational stuff. Often you
    will want to call reset_ldapserver(target) alongside of
    copy_ldapserver.  */
void copy_ldapserver(GqServer *target,
		     const GqServer *source);

/** NOTE: reset_ldapserver sets the target refcount to 0 */
void reset_ldapserver(GqServer *target);

/* canonicalize_ldapserver - to be called whenever the server-related
   information gets changed for the server in order to recalculate
   some dependent information. Eg. a change to the ldaphost might
   change the fact that a server get specified via an ldap URI or
   not. Another example is the change of the ldaphost causing a change
   to the canonical name of the server. This is where the name
   originated. */
void canonicalize_ldapserver(GqServer *server);

typedef enum {
	SERVER_HAS_NO_SCHEMA = 1
} GqServerFlags;

struct _GqServer {
	GObject base_instance;

     char *name;
     char *ldaphost;
     int   ldapport;
     char *basedn;
     char *binddn;
     char *bindpw;
     char *pwencoding;
     /* split the "configuration" password from the one entered by
	hand. This simplifies the handling of the configured password
	considerably */
     char *enteredpw;
     int   bindtype;
     char *saslmechanism;
     char *searchattr;
     int   maxentries;
     int   cacheconn;
     int   enabletls;
     long  local_cache_timeout;
     int   ask_pw;
     int   show_ref;	/* obsolete - kept for configfile compatibility */
     int   hide_internal;

     /* the canonical name of the host. Essentially this is the
	corresponding LDAP URI for the ldaphost/port combination -
	maintained through canonicalize_ldapserver() */
     char *canon_name;

     /* a flag indicating if ldaphost seems to be a URI or not */
     int   is_uri;

     /* if quiet is non-zero open_connection will not pop-up any error
	or questions */
     int   quiet;

     /* internal data */

     LDAP *connection;
     int   incarnation;    /* number of bind operations done so far,
			      never decremented - this is a purely
			      statistical number */
     int   missing_closes; /* incremented on every open_connection,
			      decremented on each close,
			      close_connection really closes only if
			      this drops to zero */
     struct server_schema *ss;
     int   flags;

     int   version;
     /* server_down is a flag set by the SIGPIPE signal handler and in
	case of an LDAP_SERVER_DOWN error. It indicates that we should
	reconnect the next time open_connection gets called. There is
	no (simple) way to find out which connection has been broken
	in case of a SIGPIPE, thus we have to record this for every
	connection. We MIGHT instead check for ld_errno in the LDAP
	structure, but that is neither really documented (some man
	pages mention it though) nor is it actually available through
	ldap.h */
     int   server_down;
};

struct dn_on_server {
     GqServer *server;
     char *dn;
     int flags;				/* used to specify more
					 * information if needed */
};

struct dn_on_server *new_dn_on_server(const char *d, GqServer *s);
void free_dn_on_server(struct dn_on_server *s);

G_END_DECLS

#endif /* !GQ_SERVER_H */

