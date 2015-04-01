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

#include "gq-tree-widget.h"

#ifndef USE_TREE_VIEW
struct _GqTreeWidget {
	GtkCTree base_instance;
};
struct _GqTreeWidgetClass {
	GtkCTreeClass base_class;
};
struct _GqTreeWidgetRow {
	GtkCTreeRow base_struct;
};
struct _GqTreeWidgetNode {
	GtkCTreeNode base_struct;
};
#else
#include <gtk/gtkcellrenderertext.h>
#include <gtk/gtktreeselection.h>
#include <gtk/gtktreestore.h>
#include <gtk/gtktreeview.h>

struct _GqTreeWidget {
	GtkTreeView   base_instance;
	GtkTreeModel* model;
};
struct _GqTreeWidgetClass {
	GtkTreeViewClass base_class;
};

enum {
	COLUMN_LABEL,
	COLUMN_DATA,
	N_COLUMNS
};
#endif

/* Refactored tree-widget handling functions: */
GQTreeWidget*
gq_tree_widget_new (void)
{
	return g_object_new(GQ_TYPE_TREE_WIDGET,
#ifndef USE_TREE_VIEW
			    "n-columns", 1,
#endif
			    NULL);
}

GqTreeWidgetNode*
gq_tree_widget_find_by_row_data_custom(GqTreeWidget*     self,
				       GqTreeWidgetNode* node,
				       gpointer          data,
				       GCompareFunc      func)
{
#ifndef USE_TREE_VIEW
	return gtk_ctree_find_by_row_data_custom(GTK_CTREE(self), node, data, func);
#else
#warning "FIXME: implement gq_tree_widget_find_by_row_data_custom()"
	g_warning("FIXME: implement gq_tree_widget_find_by_row_data_custom()");
	return NULL;
#endif
}

void
gq_tree_widget_pre_recursive (GqTreeWidget*     self,
			      GqTreeWidgetNode* node,
			      GqTreeWidgetFunc  func,
			      gpointer          data)
{
#ifndef USE_TREE_VIEW
	gtk_ctree_pre_recursive(GTK_CTREE(self), node, func, data);
#else
#warning "FIXME: implement gq_tree_widget_pre_recursive()"
	g_warning("FIXME: implement gq_tree_widget_pre_recursive()");
#endif
}

void
gq_tree_widget_pre_recursive_to_depth (GqTreeWidget*     self,
				       GqTreeWidgetNode* node,
				       gint              depth,
				       GqTreeWidgetFunc  func,
				       gpointer          data)
{
#ifndef USE_TREE_VIEW
	gtk_ctree_pre_recursive_to_depth(GTK_CTREE(self), node, depth, func, data);
#else
#warning "FIXME: implement gq_tree_widget_pre_recursive_to_depth()"
	g_warning("FIXME: implement gq_tree_widget_pre_recursive_to_depth()");
#endif
}

void
gq_tree_widget_scroll_to(GqTreeWidget*     self,
			 GqTreeWidgetNode* node,
			 gint              column,
			 gfloat            row_align,
			 gfloat            col_align)
{
#ifndef USE_TREE_VIEW
	gtk_ctree_node_moveto(GTK_CTREE(self), node, column, row_align, col_align);
#else
#warning "FIXME: implement gq_tree_widget_scroll_to()"
	g_warning("FIXME: implement gq_tree_widget_scroll_to()");
#endif
}

void
gq_tree_widget_set_column_auto_resize (GqTreeWidget*    self,
				       gint             column,
				       gboolean         auto_resize)
{
#ifndef USE_TREE_VIEW
	gtk_clist_set_column_auto_resize(GTK_CLIST(self), column, auto_resize);
#else
#warning "FIXME: implement gq_tree_widget_set_column_auto_resize()"
	//g_warning("FIXME: implement gq_tree_widget_set_column_auto_resize()");
#endif
}

void
gq_tree_widget_set_expand_callback(GqTreeWidget* self,
				   GCallback     callback,
				   gpointer      data)
{
#ifndef USE_TREE_VIEW
	g_signal_connect(self, "tree-expand",
			 callback, data);
#else
#warning "FIXME: implement gq_tree_widget_set_expand_callback()"
	//g_warning("FIXME: implement gq_tree_widget_set_expand_callback()");
#endif
}

#ifdef USE_TREE_VIEW
static void
tree_select_row_wrapper(GtkTreeSelection* ts, gpointer* connection) {
	GtkTreeIter iter;
	if(gtk_tree_selection_get_selected(ts, NULL, &iter)) {
		void (*tree_select_row) (GqTreeWidget*     self,
					 GqTreeWidgetNode* node,
					 gint              column,
					 gpointer          user_data);
		tree_select_row = connection[2];
		tree_select_row(GQ_TREE_WIDGET(connection[0]), &iter, 0, connection[1]);
	}
}
#endif

void
gq_tree_widget_set_select_callback(GqTreeWidget*   self,
				   GCallback        callback,
				   gpointer         data)
{
#ifndef USE_TREE_VIEW
	g_signal_connect(self, "tree-select-row",
			 callback, data);
#else
	GtkTreeSelection* ts;
	ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
	gpointer* connection = g_new0(gpointer, 3);
	connection[0] = self;
	connection[1] = data;
	connection[2] = callback;
	g_signal_connect_data(ts, "changed",
			      G_CALLBACK(tree_select_row_wrapper), connection,
			      (GClosureNotify)g_free, 0);
#endif
}

void
gq_tree_widget_set_selection_mode(GqTreeWidget*    self,
				  GtkSelectionMode mode)
{
#ifndef USE_TREE_VIEW
	gtk_clist_set_selection_mode(GTK_CLIST(self), mode);
#else
	GtkTreeSelection* ts;
	ts = gtk_tree_view_get_selection(GTK_TREE_VIEW(self));
	gtk_tree_selection_set_mode(ts, mode);
#endif
}

void
gq_tree_widget_sort_node(GqTreeWidget*     self,
			 GqTreeWidgetNode* node)
{
#ifndef USE_TREE_VIEW
	gtk_ctree_sort_node(GTK_CTREE(self), node);
#else
#warning "FIXME: implement gq_tree_widget_sort_node()"
	g_warning("FIXME: implement gq_tree_widget_sort_node()");
#endif
}

void
gq_tree_widget_unselect(GqTreeWidget*     self,
			GqTreeWidgetNode* node)
{
#ifndef USE_TREE_VIEW
	gtk_ctree_unselect(GTK_CTREE(self), node);
#else
#warning "FIXME: implement gq_tree_widget_unselect()"
	g_warning("FIXME: implement gq_tree_widget_unselect()");
#endif
}

void
gq_tree_widget_node_set_row_data_full (GqTreeWidget*     self,
				       GqTreeWidgetNode* node,
				       gpointer          data,
				       GtkDestroyNotify  destroy)
{
#ifndef USE_TREE_VIEW
	g_return_if_fail(destroy != g_object_unref);
	gtk_ctree_node_set_row_data_full(GTK_CTREE(self), node, data, destroy);
#else
#warning "FIXME: implement gq_tree_widget_node_set_row_data_full()"
	g_warning("FIXME: implement gq_tree_widget_node_set_row_data_full()");
#endif
}

/* add dummy nodes to get expansion capability on the parent node: */
void
gq_tree_insert_dummy_node (GQTreeWidget*     tree_widget,
			   GQTreeWidgetNode* parent_node)
{
#ifndef USE_TREE_VIEW
     char *dummy[] = { "dummy", NULL };
     GQTreeWidgetNode *new_item;

     g_return_if_fail (tree_widget);
     g_return_if_fail (parent_node);

     new_item = gtk_ctree_insert_node (GTK_CTREE(tree_widget),
				       parent_node, NULL,
				       dummy,
				       0,
				       NULL, NULL, NULL, NULL,
				       TRUE, FALSE);
#else
#warning "FIXME: implement gq_tree_insert_dummy_node()"
     //g_warning("FIXME: implement gq_tree_insert_dummy_node()");
#endif
}

GQTreeWidgetNode*
gq_tree_insert_node (GQTreeWidget*     self,
		     GQTreeWidgetNode* parent_node,
		     GQTreeWidgetNode* sibling_node,
		     const gchar*      label,
		     gpointer          data,
		     void (*destroy_cb)(gpointer data))
{
#ifndef USE_TREE_VIEW
     const gchar *labels[] = { NULL, NULL };

     g_return_val_if_fail (self, NULL);
     g_return_val_if_fail (destroy_cb == g_object_unref, NULL);

     labels[0] = label;

     GQTreeWidgetNode * new_node = gtk_ctree_insert_node (self,
							  parent_node, NULL,
							  (char**) labels,  /* bug in the GTK2 API: should be const */
							  0,
							  NULL, NULL, NULL, NULL,
							  FALSE, FALSE);
     gtk_ctree_node_set_row_data_full (self,
				       new_node,
				       data,
				       destroy_cb);

     return new_node;
#else
	if(!parent_node) {
		if(!sibling_node) {
			GtkTreeStore* store;
			GtkTreeIter   iter;
			store = GTK_TREE_STORE(self->model);
			gtk_tree_store_append(store, &iter, NULL);
			gtk_tree_store_set(store, &iter,
					   COLUMN_LABEL,   label,
					   COLUMN_DATA,    data,
					   COLUMN_DESTROY, destroy_cb,
					   -1);
		} else {
			g_warning("FIXME: implement gq_tree_insert_node() for real sibling");
		}
	} else {
		g_warning("FIXME: implement gq_tree_insert_node() for real parent");
	}
	//g_print("gq_tree_insert_node(%p, %p, %p, \"%s\", %p, %p)\n",
	//	self, parent_node, sibling_node,
	//	label, data, destroy_cb);
	return NULL;
#endif
}

void
gq_tree_remove_node (GQTreeWidget *tree_widget,
		     GQTreeWidgetNode *node)
{
#ifndef USE_TREE_VIEW
     g_return_if_fail (tree_widget);
     g_return_if_fail (node);

     gtk_ctree_remove_node (tree_widget, node);
#else
#warning "FIXME: implement gq_tree_remove_node()"
     g_warning("FIXME: implement gq_tree_remove_node()");
#endif
}

void
gq_tree_remove_children (GQTreeWidget *tree_widget,
			 GQTreeWidgetNode *parent_node)
{
#ifndef USE_TREE_VIEW
     g_return_if_fail (tree_widget);
     g_return_if_fail (parent_node);

     while (GTK_CTREE_ROW (parent_node)->children) {
	  gq_tree_remove_node (tree_widget, GTK_CTREE_ROW (parent_node)->children);
     }
#else
#warning "FIXME: implement gq_tree_remove_children()"
     g_warning("FIXME: implement gq_tree_remove_children()");
#endif
}


char*
gq_tree_get_node_text (GQTreeWidget *tree_widget,
		       GQTreeWidgetNode *node)
{
#ifndef USE_TREE_VIEW
     char *currtext = NULL;
     gtk_ctree_get_node_info (tree_widget,
			      node,
			      &currtext, /* gchar **text */
			      NULL, /* spacing */
			      NULL, /* GdkPixmap **pixmap_closed */
			      NULL, /* GdkBitmap **mask_closed */
			      NULL, /* GdkPixmap **pixmap_opened */
			      NULL, /* GdkBitmap **mask_opened */
			      NULL, /* gboolean *is_leaf */
			      NULL  /* gboolean *expanded */
			      );
     return currtext;
#else
#warning "FIXME: implement gq_tree_get_node_text()"
     g_warning("FIXME: implement gq_tree_get_node_text()");
     return NULL;
#endif
}

void
gq_tree_set_node_text (GQTreeWidget *tree_widget,
		       GQTreeWidgetNode *node,
		       const char *text)
{
#ifndef USE_TREE_VIEW
     gtk_ctree_node_set_text (tree_widget, node, 0, text);
#else
#warning "FIXME: implement gq_tree_set_node_text()"
     g_warning("FIXME: implement gq_tree_set_node_text()");
#endif
}

gpointer
gq_tree_get_node_data (GQTreeWidget*     self,
		       GQTreeWidgetNode* node)
{
#ifndef USE_TREE_VIEW
     g_return_val_if_fail (self, NULL);
     g_return_val_if_fail (node, NULL);

     return gtk_ctree_node_get_row_data (self, node);
#else
     gpointer retval = NULL;
     gtk_tree_model_get(gtk_tree_view_get_model(GTK_TREE_VIEW(self)), node,
		        COLUMN_DATA, &retval,
			-1);
     return retval;
#endif
}


GQTreeWidgetNode*
gq_tree_get_root_node (GQTreeWidget *tree_widget)
{
#ifndef USE_TREE_VIEW
     g_return_val_if_fail (tree_widget, NULL);

     return gtk_ctree_node_nth (tree_widget, 0);
#else
#warning "FIXME: implement gq_tree_get_root_node()"
     g_warning("FIXME: implement gq_tree_get_root_node()");
     return NULL;
#endif
}

void
gq_tree_fire_expand_callback (GQTreeWidget *tree_widget,
			      GQTreeWidgetNode *node)
{
#ifndef USE_TREE_VIEW
     /* toggle expansion twice to fire the expand callback and to
	return to the current expansion state */
     gtk_ctree_toggle_expansion (tree_widget, node);
     gtk_ctree_toggle_expansion (tree_widget, node);
#else
#warning "FIXME: implement gq_tree_fire_expand_callback()"
     g_warning("FIXME: implement gq_tree_fire_expand_callback()");
#endif
}

GQTreeWidgetNode*
gq_tree_get_parent_node (GQTreeWidget *tree_widget,
			 GQTreeWidgetNode *node)
{
#ifndef USE_TREE_VIEW
     GQTreeWidgetNode *parent;

     g_return_val_if_fail (tree_widget, NULL);
     g_return_val_if_fail (node, NULL);

     return GTK_CTREE_ROW(node)->parent;
#else
#warning "FIXME: implement gq_tree_get_parent_node()"
     g_warning("FIXME: implement gq_tree_get_parent_node()");
     return NULL;
#endif
}

gboolean
gq_tree_is_node_expanded (GQTreeWidget *tree_widget,
			  GQTreeWidgetNode *node)
{
#ifndef USE_TREE_VIEW
     gboolean is_expanded;

     g_return_val_if_fail (tree_widget, FALSE);
     g_return_val_if_fail (node, FALSE);

     gtk_ctree_get_node_info (tree_widget,
			      node,
			      NULL, /* text */
			      NULL, /* spacing */
			      NULL, /* pixmap_closed */
			      NULL, /* mask_closed */
			      NULL, /* pixmap_opened */
			      NULL, /* mask_opened */
			      NULL, /* is_leaf */
			      &is_expanded);

     return is_expanded;
#else
#warning "FIXME: implement gq_tree_is_node_expanded()"
     g_warning("FIXME: implement gq_tree_is_node_expanded()");
     return FALSE;
#endif
}

void
gq_tree_expand_node (GQTreeWidget *tree_widget,
		     GQTreeWidgetNode *node)
{
#ifndef USE_TREE_VIEW
     g_return_if_fail (tree_widget);
     g_return_if_fail (node);

     gtk_ctree_expand (tree_widget, node);
#else
#warning "FIXME: implement gq_tree_expand_node()"
     g_warning("FIXME: implement gq_tree_expand_node()");
#endif
}

void
gq_tree_toggle_expansion (GQTreeWidget *tree_widget,
			  GQTreeWidgetNode *node)
{
#ifndef USE_TREE_VIEW
     g_return_if_fail (tree_widget);
     g_return_if_fail (node);

     gtk_ctree_toggle_expansion (tree_widget, node);
#else
#warning "FIXME: implement gq_tree_toggle_expansion()"
     g_warning("FIXME: implement gq_tree_toggle_expansion()");
#endif
}

void
gq_tree_select_node (GQTreeWidget *tree_widget,
		     GQTreeWidgetNode *node)
{
#ifndef USE_TREE_VIEW
     g_return_if_fail (tree_widget);
     g_return_if_fail (node);

     gtk_ctree_select (tree_widget, node);
#else
#warning "FIXME: implement gq_tree_select_node()"
     g_warning("FIXME: implement gq_tree_select_node()");
#endif
}

GQTreeWidgetNode*
gq_tree_get_node_at (GQTreeWidget *tree_widget,
		     gint x,
		     gint y)
{
#ifndef USE_TREE_VIEW
     int rc, row, column;

     g_return_val_if_fail (tree_widget, NULL);

     rc = gtk_clist_get_selection_info (GTK_CLIST(tree_widget), x, y,
					&row, &column);
     if (rc == 0) return NULL;

     return gtk_ctree_node_nth(GTK_CTREE(tree_widget), row);
#else
#warning "FIXME: implement gq_tree_get_node_at()"
     g_warning("FIXME: implement gq_tree_get_node_at(");
     return NULL;
#endif
}

/* GType */
G_DEFINE_TYPE(GqTreeWidget, gq_tree_widget,
#ifndef USE_TREE_VIEW
	      GTK_TYPE_CTREE
#else
	      GTK_TYPE_TREE_VIEW
#endif
	      );

static void
gq_tree_widget_init(GqTreeWidget* self) {
#ifdef USE_TREE_VIEW
	GtkTreeView* tv = GTK_TREE_VIEW(self);
	gtk_tree_view_set_headers_visible(tv, FALSE);
	self->model = GTK_TREE_MODEL(gtk_tree_store_new(N_COLUMNS, G_TYPE_STRING, G_TYPE_OBJECT));
	gtk_tree_view_set_model(tv, self->model);
	gtk_tree_view_insert_column_with_attributes(tv, -1,
						    NULL, gtk_cell_renderer_text_new(),
						    "text", COLUMN_LABEL,
						    NULL);
#endif
}

static void
gq_tree_widget_class_init(GqTreeWidgetClass* self_class) {}

