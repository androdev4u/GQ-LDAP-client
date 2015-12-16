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

#ifndef GQ_TREE_WIDGET_H
#define GQ_TREE_WIDGET_H

#undef USE_TREE_VIEW
#ifndef USE_TREE_VIEW
#include <gtk/gtkctree.h>
#else
#include <gtk/gtktreemodel.h>
#endif

G_BEGIN_DECLS

typedef struct _GqTreeWidgetClass GqTreeWidgetClass;
#ifndef USE_TREE_VIEW
typedef GtkCTree     GqTreeWidget;
typedef GtkCTreeNode GqTreeWidgetNode;
typedef GtkCTreeRow  GqTreeWidgetRow;
typedef GtkCTreeFunc GqTreeWidgetFunc;
#define GQ_TREE_WIDGET_ROW(i)  GTK_CTREE_ROW(i)
#define GQ_TREE_WIDGET_NODE(i) GTK_CTREE_NODE(i)
#else
typedef struct _GqTreeWidget      GqTreeWidget;
typedef GtkTreeIter               GqTreeWidgetNode;
typedef struct {
	GqTreeWidgetNode* parent;
	GqTreeWidgetNode* sibling;
} GqTreeWidgetRow;
typedef GCallback                 GqTreeWidgetFunc;
#define GQ_TREE_WIDGET_ROW(i)       ((GqTreeWidgetRow*)(i))
#define GQ_TREE_WIDGET_NODE(i)      ((GqTreeWidgetNode*)(i))
#endif

/* compatibility for the old definition */
typedef GqTreeWidget     GQTreeWidget;
typedef GqTreeWidgetNode GQTreeWidgetNode;
typedef GqTreeWidgetRow  GQTreeWidgetRow;
typedef GqTreeWidgetFunc GQTreeWidgetFunc;

#define GQ_TYPE_TREE_WIDGET         (gq_tree_widget_get_type())
#define GQ_TREE_WIDGET(i)           (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_TREE_WIDGET, GqTreeWidget))


GType         gq_tree_widget_get_type          (void);
GQTreeWidget* gq_tree_widget_new               (void);
GqTreeWidgetNode* gq_tree_widget_find_by_row_data_custom(GqTreeWidget*     self,
							 GqTreeWidgetNode* node,
							 gpointer          data,
							 GCompareFunc      func);
void              gq_tree_widget_pre_recursive          (GqTreeWidget*     self,
							 GqTreeWidgetNode* node,
							 GqTreeWidgetFunc  func,
							 gpointer          data);
void              gq_tree_widget_pre_recursive_to_depth (GqTreeWidget*     self,
							 GqTreeWidgetNode* node,
							 gint              depth,
							 GqTreeWidgetFunc  func,
							 gpointer          data);
void              gq_tree_widget_scroll_to              (GqTreeWidget*     self,
							 GqTreeWidgetNode* node,
							 gint              column,
							 gfloat            row_align,
							 gfloat            col_align);
void          gq_tree_widget_set_column_auto_resize(GqTreeWidget*    self,
						gint             column,
						gboolean         auto_resize);
void          gq_tree_widget_set_expand_callback(GqTreeWidget*   self,
						GCallback        callback,
						gpointer         data);
void          gq_tree_widget_set_select_callback(GqTreeWidget*   self,
						GCallback        callback,
						gpointer         data);
void          gq_tree_widget_set_selection_mode(GqTreeWidget*    self,
						GtkSelectionMode mode);
void              gq_tree_widget_sort_node              (GqTreeWidget*     self,
							 GqTreeWidgetNode* node);
void              gq_tree_widget_unselect               (GqTreeWidget*     self,
							 GqTreeWidgetNode* node);

void              gq_tree_widget_node_set_row_data_full (GqTreeWidget*     self,
							 GqTreeWidgetNode* node,
							 gpointer          data,
							 GtkDestroyNotify  destroy);

void gq_tree_insert_dummy_node (GQTreeWidget *ctree,
			   GQTreeWidgetNode *parent_node);

GQTreeWidgetNode* gq_tree_insert_node (GQTreeWidget *ctree,
		     GQTreeWidgetNode *parent_node,
		     GQTreeWidgetNode *sibling_node,
		     const gchar* label,
		     gpointer data,
		     void (*destroy_cb)(gpointer data));

void              gq_tree_remove_node (GQTreeWidget *tree_widget,
		     GQTreeWidgetNode *node);

void              gq_tree_remove_children (GQTreeWidget *tree_widget,
			 GQTreeWidgetNode *parent_node);

char*             gq_tree_get_node_text (GQTreeWidget *tree_widget,
		       GQTreeWidgetNode *node);

void              gq_tree_set_node_text (GQTreeWidget *tree_widget,
		       GQTreeWidgetNode *node,
		       const char *text);

gpointer          gq_tree_get_node_data (GQTreeWidget *tree_widget,
		       GQTreeWidgetNode *node);

GQTreeWidgetNode* gq_tree_get_root_node (GQTreeWidget *tree_widget);

void              gq_tree_fire_expand_callback (GQTreeWidget *tree_widget,
			      GQTreeWidgetNode *node);

GQTreeWidgetNode* gq_tree_get_parent_node (GQTreeWidget *tree_widget,
			 GQTreeWidgetNode *node);

gboolean          gq_tree_is_node_expanded (GQTreeWidget *tree_widget,
				GQTreeWidgetNode *node);

void              gq_tree_expand_node (GQTreeWidget *tree_widget,
                    GQTreeWidgetNode *node);

void              gq_tree_toggle_expansion (GQTreeWidget *tree_widget,
                         GQTreeWidgetNode *node);

void              gq_tree_select_node (GQTreeWidget *tree_widget,
                    GQTreeWidgetNode *node);

GQTreeWidgetNode* gq_tree_get_node_at (GQTreeWidget *tree_widget,
                    gint x,
                    gint y);
G_END_DECLS

#endif /* !GQ_TREE_WIDGET_H */
