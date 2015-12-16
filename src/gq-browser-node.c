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

#include "gq-browser-node.h"

GqServer*
gq_browser_node_get_server(GqBrowserNode* self)
{
	g_return_val_if_fail(GQ_IS_BROWSER_NODE(self), NULL);
	g_return_val_if_fail(GQ_BROWSER_NODE_GET_CLASS(self)->get_server, NULL);

	return GQ_BROWSER_NODE_GET_CLASS(self)->get_server(self);
}

/* GType */
G_DEFINE_TYPE(GqBrowserNode, gq_browser_node, G_TYPE_OBJECT);

static void
gq_browser_node_init(GqBrowserNode* self) {}

static void
gq_browser_node_class_init(GqBrowserNodeClass* self_class) {}

