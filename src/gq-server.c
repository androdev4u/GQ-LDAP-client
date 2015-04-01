/* This file is part of GQ
 *
 * AUTHORS
 *     Sven Herzberg  <herzi@gnome-de.org>
 *
 * Copyright (C) 1998-2003 Bert Vermeulen
 * Copyright (C) 2002-2003 Peter Stamfest
 * Copyright (C) 2006      Sven Herzberg
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "gq-server.h"

#include "configfile.h"

GqServer*
gq_server_new(void)
{
     GqServer *newserver = g_object_new(GQ_TYPE_SERVER, NULL);

     /* well - not beautiful, but works */
     newserver->name = g_strdup("");
     newserver->ldaphost = g_strdup("");
     newserver->ldapport = 389;
     newserver->basedn = g_strdup("");
     newserver->binddn = g_strdup("");
     newserver->bindpw = g_strdup("");
     newserver->pwencoding = g_strdup("");
     newserver->enteredpw = g_strdup("");
     newserver->bindtype = DEFAULT_BINDTYPE;
     newserver->saslmechanism = g_strdup("");
     newserver->searchattr = g_strdup(DEFAULT_SEARCHATTR);
     newserver->maxentries = DEFAULT_MAXENTRIES;
     newserver->cacheconn = DEFAULT_CACHECONN;
     newserver->enabletls = DEFAULT_ENABLETLS;
     newserver->local_cache_timeout = DEFAULT_LOCAL_CACHE_TIMEOUT;
     newserver->ask_pw = DEFAULT_ASK_PW;
     newserver->hide_internal = DEFAULT_HIDE_INTERNAL;
     newserver->show_ref = DEFAULT_SHOW_REF;

     /* dynamic information */
     newserver->canon_name = g_strdup("");
     newserver->is_uri = 0;

     /* operational */
     newserver->connection = NULL;
     newserver->incarnation = 0;
     newserver->missing_closes = 0;
     newserver->ss = NULL;
     newserver->flags = 0;
     newserver->version = LDAP_VERSION2;
     newserver->server_down = 0;

     return(newserver);
}

/* saves typing */
#define DEEPCOPY(t,s,n) g_free_and_dup(t->n, s->n)
#define SHALLOWCOPY(t,s,n) t->n = s->n

/** NOTE: copy_ldapserver ONLY copies the configuration
    information. It DOES NOT copy the operational stuff. Often you
    will want to call reset_ldapserver(target) alongside of
    copy_ldapserver.  */

void copy_ldapserver(GqServer *target, 
		     const GqServer *source)
{
     /* configuration */

     DEEPCOPY   (target, source, name);
     DEEPCOPY   (target, source, ldaphost);
     SHALLOWCOPY(target, source, ldapport);
     DEEPCOPY   (target, source, basedn);
     DEEPCOPY   (target, source, binddn);
     DEEPCOPY   (target, source, bindpw);
     DEEPCOPY   (target, source, pwencoding);
     DEEPCOPY   (target, source, enteredpw);
     SHALLOWCOPY(target, source, bindtype);
     DEEPCOPY   (target, source, saslmechanism);
     DEEPCOPY   (target, source, searchattr);
     SHALLOWCOPY(target, source, maxentries);
     SHALLOWCOPY(target, source, cacheconn);
     SHALLOWCOPY(target, source, enabletls);
     SHALLOWCOPY(target, source, local_cache_timeout);
     SHALLOWCOPY(target, source, ask_pw);
     SHALLOWCOPY(target, source, hide_internal);
     SHALLOWCOPY(target, source, show_ref);

     DEEPCOPY   (target, source, canon_name);
     SHALLOWCOPY(target, source, is_uri);
}

/* resets the operational stuff */
/** NOTE: reset_ldapserver sets the target refcount to 0 */
void
reset_ldapserver(GqServer *target)
{
     if (target->connection) {
	  close_connection(target, TRUE);
     }
     target->connection = NULL;
     target->incarnation = 0;
     target->missing_closes = 0;
     target->ss = NULL;
     target->flags = 0;
     target->version = LDAP_VERSION2;
     target->server_down = 0;
}

void canonicalize_ldapserver(GqServer *server)
{
     /* FIXME: should use better URI check */
     server->is_uri =  g_utf8_strchr(server->ldaphost, -1, ':') != NULL;
     
     if (server->is_uri) {
	  g_free_and_dup(server->canon_name, server->ldaphost);
     } else {
	  /* construct an LDAP URI */
	  GString *str = g_string_sized_new(100);
	  g_string_sprintf(str, "ldap://%s:%d/", 
			   server->ldaphost, server->ldapport);
	  g_free(server->canon_name);
	  server->canon_name = str->str;
	  g_string_free(str, FALSE);
     }
}

/* GType */
G_DEFINE_TYPE(GqServer, gq_server, G_TYPE_OBJECT);

static void
gq_server_init(GqServer* self)
{}

static void
server_finalize(GObject* object)
{
	GqServer* self = GQ_SERVER(object);

	if(self->connection) {
		close_connection(self, 1);
	}

	g_free(self->name);
	g_free(self->ldaphost);
	g_free(self->basedn);
	g_free(self->binddn);
	g_free(self->bindpw);
	g_free(self->pwencoding);
	g_free(self->enteredpw);
	g_free(self->saslmechanism);
	g_free(self->searchattr);

	g_free(self->canon_name);

	G_OBJECT_CLASS(gq_server_parent_class)->finalize(object);
}

static void
gq_server_class_init(GqServerClass* self_class)
{
	GObjectClass* object_class = G_OBJECT_CLASS(self_class);
	object_class->finalize = server_finalize;
}

