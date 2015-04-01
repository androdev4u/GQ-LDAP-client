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

#ifndef GQ_BROWSER_MODEL_H
#define GQ_BROWSER_MODEL_H

#include <gtk/gtktreemodel.h>

G_BEGIN_DECLS

typedef GObject      GqBrowserModel;
typedef GObjectClass GqBrowserModelClass;

#define GQ_TYPE_BROWSER_MODEL        (gq_browser_model_get_type())

GType         gq_browser_model_get_type(void);
GtkTreeModel* gq_browser_model_new(void);

G_END_DECLS

#endif /* !GQ_BROWSER_MODEL_H */
