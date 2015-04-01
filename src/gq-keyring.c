/* This file is part of GQ
 *
 * AUTHORS
 *     Sven Herzberg  <herzi@gnome-de.org>
 *
 * Copyright (C) 2006  Sven Herzberg <herzi@gnome-de.org>
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

#include "gq-keyring.h"

GnomeKeyringAttributeList*
gq_keyring_attribute_list_from_server(GqServer* server) {
	GnomeKeyringAttributeList* attributes = NULL;

	g_return_val_if_fail(GQ_IS_SERVER(server), NULL);

	attributes = gnome_keyring_attribute_list_new();
	gnome_keyring_attribute_list_append_string(attributes, "user", server->binddn);
	gnome_keyring_attribute_list_append_string(attributes, "server", server->ldaphost);
	gnome_keyring_attribute_list_append_uint32(attributes, "port", server->ldapport);
	gnome_keyring_attribute_list_append_string(attributes, "protocol", "ldap");
	return attributes;
}

static GList*
gq_keyring_find_servers(GqServer* server) {
	GnomeKeyringAttributeList* list = NULL;
	GList* found = NULL;

	list = gq_keyring_attribute_list_from_server(server);
	// FIXME: do async
	// FIXME: check result
	gnome_keyring_find_items_sync(GNOME_KEYRING_ITEM_NETWORK_PASSWORD, list, &found);
	gnome_keyring_attribute_list_free(list);
	list = NULL;

	return found;
}

void
gq_keyring_forget_password(GqServer* server) {
	GList* ret = NULL,
	     * item;

	g_return_if_fail(GQ_IS_SERVER(server));

	if(!gnome_keyring_is_available()) {
		return;
	}

	ret = gq_keyring_find_servers(server);
	for(item = ret; item; item = item->next) {
		GnomeKeyringFound* found = item->data;
		gnome_keyring_item_delete_sync(found->keyring, found->item_id);
		gnome_keyring_found_free(found);
		item->data = NULL;
	}

	g_list_free(ret);
}

gchar*
gq_keyring_get_password(GqServer* server) {
	GList* found = NULL;
	GList* item;
	GnomeKeyringAttributeList* list = NULL;
	gchar* retval = NULL;

	if(!gnome_keyring_is_available()) {
		return NULL;
	}

	found = gq_keyring_find_servers(server);

	if(found && found->next) {
		g_warning("FIXME: GQ cannot handle more than one stored server yet");
	}
	for(item = found; item; item = item->next) {
		retval = g_strdup(((GnomeKeyringFound*)item->data)->secret);
		break; // FIXME: support multiple store connections
	}

	g_list_foreach(found, (GFunc)gnome_keyring_found_free, NULL);
	g_list_free(found);
	return retval;
}

void
gq_keyring_save_server_password(GqServer* server) {
	GnomeKeyringAttributeList* list = NULL;
	gchar* display_name = NULL;
	guint32 id = 0;

	g_return_if_fail(GQ_IS_SERVER(server));

	if(!gnome_keyring_is_available()) {
		return;
	}

	// FIXME: don't always create a new item

	display_name = g_strdup_printf("%s@%s:%u", server->binddn, server->ldaphost, server->ldapport);
	list = gq_keyring_attribute_list_from_server(server);
	// FIXME: check whether we already have such a password
	// FIXME: check return value
	// FIXME: do async
	gnome_keyring_item_create_sync(NULL, GNOME_KEYRING_ITEM_NETWORK_PASSWORD,
			display_name, list,
			server->bindpw, TRUE,
			&id);
	gnome_keyring_attribute_list_free(list);
	g_free(display_name);
}

