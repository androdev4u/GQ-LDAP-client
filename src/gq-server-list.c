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

#include "gq-server-list.h"

#include "gq-server.h"
#include "util.h" // FIXME: for close_connection()

struct _GQServerListPrivate {
	GList* servers;
	GHashTable* servers_by_name;
	GHashTable* servers_by_canonical_name;
};

/* FIXME: think whether we'd like to have really this way */
static GQServerList* instance = NULL;

void
gq_server_list_add(GQServerList* self, GqServer* server) {
	g_return_if_fail(GQ_IS_SERVER_LIST(self));
	g_return_if_fail(GQ_IS_SERVER(server));
	g_return_if_fail(!gq_server_list_contains(self, server));

	g_object_ref(server);
	self->priv->servers = g_list_prepend(self->priv->servers, server);
	g_hash_table_insert(self->priv->servers_by_name          , server->name      , server);
	g_hash_table_insert(self->priv->servers_by_canonical_name, server->canon_name, server);
}

gboolean
gq_server_list_contains(GQServerList const* self, GqServer const* server) {
	g_return_val_if_fail(GQ_IS_SERVER_LIST(self), FALSE);
	g_return_val_if_fail(GQ_IS_SERVER(server), FALSE);

	return gq_server_list_get_by_name(self, server->name) == server;
}

static void
gsl_foreach(gpointer server, gpointer data) {
	gpointer* self_func_and_data = data;
	((GQServerListForeachFunc)self_func_and_data[1])(GQ_SERVER_LIST(self_func_and_data[0]), GQ_SERVER(server), self_func_and_data[2]);
}

void
gq_server_list_foreach(GQServerList* self, GQServerListForeachFunc func, gpointer user_data) {
	gpointer self_func_and_data[3] = {
		self,
		func,
		user_data
	};
	g_return_if_fail(GQ_IS_SERVER_LIST(self));
	g_return_if_fail(func);

	g_list_foreach(self->priv->servers, gsl_foreach, self_func_and_data);
}

GQServerList*
gq_server_list_get(void) {
	return g_object_new(GQ_TYPE_SERVER_LIST, NULL);
}

GqServer*
gq_server_list_get_by_canon_name(GQServerList const* self, gchar const* canonical_name) {
	g_return_val_if_fail(GQ_IS_SERVER_LIST(self), NULL);
	g_return_val_if_fail(canonical_name && *canonical_name, NULL);

	return g_hash_table_lookup(self->priv->servers_by_canonical_name, canonical_name);
}

GqServer*
gq_server_list_get_by_name(GQServerList const* self, gchar const* name) {
	g_return_val_if_fail(GQ_IS_SERVER_LIST(self), NULL);
	g_return_val_if_fail(name && *name, NULL);

	return g_hash_table_lookup(self->priv->servers_by_name, name);
}

GqServer*
gq_server_list_get_server(GQServerList const* self, guint server) {
	g_return_val_if_fail(GQ_IS_SERVER_LIST(self), NULL);
	g_return_val_if_fail(server < gq_server_list_n_servers(self), NULL);

	return GQ_SERVER(g_list_nth_data(self->priv->servers, server));
}

guint
gq_server_list_n_servers(GQServerList const* self) {
	g_return_val_if_fail(GQ_IS_SERVER_LIST(self), 0);

	return g_hash_table_size(self->priv->servers_by_name);
}

void
gq_server_list_remove(GQServerList* self, GqServer* server) {
	g_return_if_fail(GQ_IS_SERVER_LIST(self));
	g_return_if_fail(GQ_IS_SERVER(server));
	g_return_if_fail(gq_server_list_contains(self, server));

	self->priv->servers = g_list_remove(self->priv->servers, server);
	g_hash_table_remove(self->priv->servers_by_name, server->name);
	g_hash_table_remove(self->priv->servers_by_canonical_name, server->canon_name);
}

/* GType */
G_DEFINE_TYPE(GQServerList, gq_server_list, G_TYPE_OBJECT);

static void
gq_server_list_init(GQServerList* self) {
	self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, GQ_TYPE_SERVER_LIST, GQServerListPrivate);
	self->priv->servers_by_name           = g_hash_table_new(g_str_hash, g_str_equal);
	self->priv->servers_by_canonical_name = g_hash_table_new(g_str_hash, g_str_equal);
}

static GObject*
gsl_constructor(GType type, guint n_params, GObjectConstructParam* params) {
	if(!instance) {
		GObject* retval = G_OBJECT_CLASS(gq_server_list_parent_class)->constructor(type, n_params, params);
		instance = GQ_SERVER_LIST(retval);
		return retval;
	} else {
		GObject* retval = G_OBJECT(instance);
		g_object_freeze_notify(retval); // to avoid a warning; glib bug 315053
		// FIXME: think whether or not we should ref the thing here
		return retval;
	}
}

static void
gsl_finalize_per_server(GqServer* server) {
	close_connection(server, TRUE);
	g_object_unref(server);
}

static void
gsl_finalize(GObject* object) {
	GQServerList* self = GQ_SERVER_LIST(object);

	instance = NULL;

	g_list_foreach(self->priv->servers, (GFunc)gsl_finalize_per_server, NULL);
	g_list_free(self->priv->servers);
	self->priv->servers = NULL;

	g_hash_table_destroy(self->priv->servers_by_name);
	self->priv->servers_by_name = NULL;
	g_hash_table_destroy(self->priv->servers_by_canonical_name);
	self->priv->servers_by_canonical_name = NULL;

	G_OBJECT_CLASS(gq_server_list_parent_class)->finalize(object);
}

static void
gq_server_list_class_init(GQServerListClass* self_class) {
	GObjectClass* go_class = G_OBJECT_CLASS(self_class);

	/* GObjectClass */
	go_class->constructor = gsl_constructor;
	go_class->finalize    = gsl_finalize;

	/* GQServerListClass */
	g_type_class_add_private(self_class, sizeof(GQServerListPrivate));
}

