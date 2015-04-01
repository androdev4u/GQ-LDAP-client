/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 Peter Stamfest

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

/* $Id: dtutil.c 942 2006-08-04 14:14:36Z herzi $ */

#include <glib.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
#include <string.h>

#include "dtutil.h"

void editable_changed_cb(GtkWidget *object,
			 gpointer user_data)
{
     gtk_object_set_data(GTK_OBJECT(object),
			 "editable_changed_flag", GINT_TO_POINTER(1));
}

void editable_set_text(GtkEditable *entry, GByteArray *data,
		       GByteArray* (*encode)(const char *val, int len),
		       GByteArray* (*decode)(const char *val, int len))
{
     int len;
     guchar nul[] = { '\0' };
     int pos = 0;
     GByteArray *copy = NULL, *encoded;
     gboolean del = FALSE;

     gtk_editable_delete_text(entry, 0, -1);

     if (data) {
	  len = data->len;

	  /* encode data */
	  if (encode) {
	       encoded = encode((gchar*)data->data, data->len);
	       del = TRUE;
	  } else {
	       encoded = data;
	       del = FALSE;
	  }

	  /* append a NUL byte, so we can be sure that we have a NUL
	     terminated string (though there may be a NUL somewhere in the
	     data already) */

	  g_byte_array_append(encoded, nul, 1);

	  gtk_editable_insert_text(entry, (gchar*)encoded->data,
				   strlen((gchar*)encoded->data), &pos);

	  gtk_editable_set_position(entry, 0);

	  /* strip the extra NUL byte again... */
	  g_byte_array_set_size(encoded, len);

	  if (del) g_byte_array_free(encoded, TRUE);
	  encoded = NULL;

	  /* store a copy of the original data with the entry... */
	  copy = g_byte_array_new();
	  g_byte_array_append(copy, data->data, len);
     }

     if (copy)
	  gtk_object_set_data_full(GTK_OBJECT(entry),
				   "original_data", copy,
				   (GtkDestroyNotify) g_byte_array_free);

     gtk_object_set_data(GTK_OBJECT(entry),
			 "decoder", decode);

     gtk_object_set_data(GTK_OBJECT(entry),
			 "editable_changed_flag", GINT_TO_POINTER(0));
     g_signal_connect(entry, "changed",
			G_CALLBACK(editable_changed_cb),
			NULL);
}

GByteArray *editable_get_text(GtkEditable *entry)
{
     int changed = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(entry),
						       "editable_changed_flag"));
     GByteArray *data = NULL;
     char *c;
     int l;
     GByteArray* (*decode)(const char *val, int len);

     if (changed) {
	  decode = gtk_object_get_data(GTK_OBJECT(entry), "decoder");

	  c = gtk_editable_get_chars(GTK_EDITABLE(entry), 0, -1);

	  if (!c) return NULL;

	  l = strlen(c);
	  if (l == 0) {
	       g_free(c);
	       return NULL;
	  }

	  if (decode) {
	       data = decode(c, strlen(c));
	  } else {
	       data = g_byte_array_new();
	       g_byte_array_append(data, (guchar*)c, strlen(c));
	  }
	  g_free(c);
     } else {
	  GByteArray *original = gtk_object_get_data(GTK_OBJECT(entry),
						     "original_data");
	  if (original) {
	       /* copy data, if any */
	       data = g_byte_array_new();
	       g_byte_array_append(data, original->data, original->len);
	  }
     }
     return data;
}


/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
