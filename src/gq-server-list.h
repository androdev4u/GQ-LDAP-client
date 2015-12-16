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

#ifndef GQ_SERVER_LIST_H
#define GQ_SERVER_LIST_H

/* #include <glib/gmacros.h> */
#include "gq-server.h" // for GqServer

G_BEGIN_DECLS

typedef struct _GQServerList        GQServerList;
typedef struct _GQServerListClass   GQServerListClass;
typedef struct _GQServerListPrivate GQServerListPrivate;

#define GQ_TYPE_SERVER_LIST         (gq_server_list_get_type())
#define GQ_SERVER_LIST(i)           (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_SERVER_LIST, GQServerList))
#define GQ_SERVER_LIST_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_SERVER_LIST, GQServerListClass))
#define GQ_IS_SERVER_LIST(i)        (G_TYPE_CHECK_INSTANCE_TYPE((i), GQ_TYPE_SERVER_LIST))
#define GQ_IS_SERVER_LIST_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE((c), GQ_TYPE_SERVER_LIST))
#define GQ_SERVER_LIST_GET_CLASS(i) (G_TYPE_INSTANCE_GET_CLASS((i), GQ_TYPE_SERVER_LIST, GQServerListClass))

typedef void (*GQServerListForeachFunc) (GQServerList* self,
					 GqServer* server,
					 gpointer user_data);

GType         gq_server_list_get_type         (void);

GQServerList* gq_server_list_get              (void);
gboolean      gq_server_list_contains         (GQServerList const* self,
					       GqServer const* server);
void          gq_server_list_foreach          (GQServerList* self,
					       GQServerListForeachFunc func,
					       gpointer user_data);
GqServer*     gq_server_list_get_server       (GQServerList const* self,
					       guint               server);
GqServer*     gq_server_list_get_by_name      (GQServerList const* self,
					       gchar const       * name); // was util.c:998:server_by_name
GqServer*     gq_server_list_get_by_canon_name(GQServerList const* self,
					       gchar const       * canonical_name);
guint         gq_server_list_n_servers	      (GQServerList const* self);
void          gq_server_list_add	      (GQServerList      * self,
					       GqServer * server);
void          gq_server_list_remove	      (GQServerList      * self,
					       GqServer * server);

struct _GQServerList {
	GObject base_instance;

	GQServerListPrivate* priv;
};

struct _GQServerListClass {
	GObjectClass base_class;
};

G_END_DECLS

#endif /* !GQ_SERVER_LIST_H */

