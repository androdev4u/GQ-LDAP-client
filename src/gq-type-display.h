/* this file is part of GQ, a GTK-based LDAP client
 *
 * AUTHORS
 *     Copyright (C) 1998-2003 Bert Vermeulen
 *     Copyright (C) 2002-2003 Peter Stamfest
 *     Copyright (C) 2006      Sven Herzberg
 *
 * This program is released under the GNU General Public License with
 * the additional exemption that compiling, linking, and/or using
 * OpenSSL is allowed.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef GQ_TYPE_DISPLAY_H
#define GQ_TYPE_DISPLAY_H

#include <glib-object.h>
#include "formfill.h"

G_BEGIN_DECLS

#define GQ_TYPE_TYPE_DISPLAY         (gq_type_display_get_type())
#define GQ_TYPE_DISPLAY(i)           (G_TYPE_CHECK_INSTANCE_CAST((i), GQ_TYPE_TYPE_DISPLAY, GQTypeDisplay))
#define GQ_TYPE_DISPLAY_CLASS(c)     (G_TYPE_CHECK_CLASS_CAST((c), GQ_TYPE_TYPE_DISPLAY, GQTypeDisplayClass))
#define DISPLAY_TYPE_HANDLER(objpointer) GQ_TYPE_DISPLAY_CLASS(objpointer)

typedef        GObject               GQTypeDisplay;
typedef struct _type_display_handler GQTypeDisplayClass;

GType gq_type_display_get_type(void);

struct _type_display_handler {
	GObjectClass  base_class;
	const char *name;
	gboolean selectable;
	gboolean show_in_search_result;

	GtkWidget* (*get_widget)(int error_context,
			      struct formfill *form,
			      GByteArray *data,
			      GCallback *activatefunc,
			      gpointer funcdata);
	GByteArray* (*get_data)(struct formfill *form,
			     GtkWidget *widget);
	void (*set_data)(struct formfill *form,
		      GByteArray *data,
		      GtkWidget *widget);
	LDAPMod* (*buildLDAPMod)(struct formfill *form,
			      int op,
			      GList *values);
};

G_END_DECLS

#endif /* GQ_TYPE_DISPLAY_H */

