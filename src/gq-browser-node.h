/* This file is part of GQ
 *
 * AUTHORS
 *     Sven Herzberg  <herzi@gnome-de.org>
 *
 * Copyright (C) 1998-2003 Bert Vermeulen
 * Copyright (C) 2002-2003 Peter Stamfest
 * Copyright (C) 2006      Sven Herzberg <herzi@gnome-de.org>
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

#ifndef GQ_BROWSER_NODE_H
#define GQ_BROWSER_NODE_H

#include "gq-server.h"
#include "gq-tab.h"
#include "gq-tree-widget.h"

G_BEGIN_DECLS

typedef GObject                    GqBrowserNode;
typedef struct _GqBrowserNodeClass GqBrowserNodeClass;

#define GQ_TYPE_BROWSER_NODE         (gq_browser_node_get_type())
#define GQ_BROWSER_NODE(i)           (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_BROWSER_NODE, GqBrowserNode))
#define GQ_BROWSER_NODE_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_BROWSER_NODE, GqBrowserNodeClass))
#define GQ_IS_BROWSER_NODE(i)        (G_TYPE_CHECK_INSTANCE_TYPE((i), GQ_TYPE_BROWSER_NODE))
#define GQ_IS_BROWSER_NODE_CLASS(c)  (G_TYPE_CHECK_CLASS_TYPE((c), GQ_TYPE_BROWSER_NODE))
#define GQ_BROWSER_NODE_GET_CLASS(i) (G_TYPE_INSTANCE_GET_CLASS((i), GQ_TYPE_BROWSER_NODE, GqBrowserNodeClass))

/* Callback types to avoid a lot of warnings when assigning to the
   virtual function table */

typedef void (*browse_entry_expand)(GqBrowserNode *entry,
				    int error_context,
				    GQTreeWidget *ctreeroot,
				    GQTreeWidgetNode *node,
				    GqTab *tab);
typedef void (*browse_entry_select)(GqBrowserNode *entry,
				    int error_context,
				    GQTreeWidget *ctreeroot,
				    GQTreeWidgetNode *node,
				    GqTab *tab);
typedef void (*browse_entry_refresh)(GqBrowserNode *entry,
				     int error_context,
				     GQTreeWidget *ctreeroot,
				     GQTreeWidgetNode *node,
				     GqTab *tab);
typedef char* (*browse_entry_get_name)(GqBrowserNode *entry,
				       gboolean long_form);
typedef void (*browse_entry_popup)(GqBrowserNode *entry,
				   GtkWidget *menu,
				   GQTreeWidget *ctreeroot,
				   GQTreeWidgetNode *ctree_node,
				   GqTab *tab);

struct _GqBrowserNodeClass {
	GObjectClass base_class;

	/* destructor */
	void (*destroy)(GqBrowserNode *entry);

	/* expansion callback of corresponding GtkCtreeNode */
	browse_entry_expand		expand;

	/* select callback of corresponding GtkCtreeNode */
	browse_entry_select		select;

	/* refresh callback of corresponding GtkCtreeNode */
	browse_entry_refresh		refresh;

	/* Gets the user visible name of the node, must be freed using g_free */
	browse_entry_get_name		get_name;

	/* implements the popup menu for the entry */
	browse_entry_popup			popup;

	GqServer* (*get_server) (GqBrowserNode* self);
};

GType     gq_browser_node_get_type  (void);
GqServer* gq_browser_node_get_server(GqBrowserNode* self);

G_END_DECLS

#endif /* !GQ_BROWSER_NODE_H */

