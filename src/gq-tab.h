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

#ifndef GQ_TAB_H
#define GQ_TAB_H

#include <gtk/gtkwidget.h>

G_BEGIN_DECLS

typedef struct _GqTab      GqTab;
typedef struct _GqTabClass GqTabClass;

#define GQ_TYPE_TAB         (gq_tab_get_type())
#define GQ_TAB(i)           (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_TAB, GqTab))
#define GQ_TAB_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_TAB, GqTabClass))
#define GQ_TAB_GET_CLASS(i) (G_TYPE_INSTANCE_GET_CLASS((i), GQ_TYPE_TAB, GqTabClass))

GType gq_tab_get_type(void);

enum {
	SEARCH_MODE = 1,
	BROWSE_MODE = 2,
	SCHEMA_MODE = 3
};

struct pbar_win;

struct _GqTab {
	GObject base_class;
	int type;
	struct mainwin_data *win;	/* what window does this tab
					   belong to */
	GtkWidget *focus;
	GtkWidget *content;		/* what is the "content"
					   widget of the tab */
};

struct _GqTabClass {
	GObjectClass base_class;

	void (*save_snapshot)(int error_context, char *state_name, GqTab *);
	void (*restore_snapshot)(int error_context,
				 char *state_name, GqTab *,
				 struct pbar_win *progress);
};

G_END_DECLS

#endif /* !GQ_TAB_H */
