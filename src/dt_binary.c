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

/* $Id: dt_binary.c 952 2006-08-26 11:17:35Z herzi $ */

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#define GTK_ENABLE_BROKEN  /* for the text widget - should be replaced - FIXME */
#include <gtk/gtk.h>

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include "common.h"
#include "gq-tab-browse.h"
#include "util.h"
#include "errorchain.h"
#include "formfill.h"
#include "input.h"
#include "tinput.h"
#include "encode.h"
#include "ldif.h" /* for b64_decode */
#include "dt_binary.h"

#define DT_BINARY_ENTRY		1
#define DT_BINARY_TEXT		2

#define DT_BINARY_WIDGET_MASK	(DT_BINARY_ENTRY|DT_BINARY_TEXT)

#define DT_BINARY_HEX		0x10
#define DT_BINARY_BASE64	0x20
#define DT_BINARY_PLAIN		0x40

#define DT_BINARY_ENCODING_MASK	(DT_BINARY_HEX|DT_BINARY_BASE64|DT_BINARY_PLAIN)


static PangoFontDescription *fixed_font = NULL;

static GByteArray *dt_binary_get_data_internal(struct formfill *form,
					       GtkWidget *data_widget);

/*  static void destroy_byte_array(gpointer gb) { */
/*       g_byte_array_free((GByteArray*) gb, TRUE); */
/*  } */

struct tokenlist encodings[] = {
     { DT_BINARY_PLAIN,  "Plain", "P" },
     { DT_BINARY_BASE64, "Base64", "B" },
     { DT_BINARY_HEX,    "Hex", "H" },
     { 0, "", NULL }
};

struct encoding_data {
     struct formfill *form;
     GtkWidget *hbox;
     GtkWidget *menu_widget;
     GtkWidget *data_widget;
     int enc;
     char *code;
};

static void dt_binary_switch_to_encoding(struct formfill *form,
					 GtkWidget *hbox,
					 GtkWidget *data_widget,
					 int newtype);

static void dt_binary_set_encoding_in_menu(struct formfill *form,
					   GtkWidget *menu_widget,
					   int flags)
{
     GList *children;
     struct encoding_data *enc;

     for ( children = GTK_MENU_SHELL(menu_widget)->children ;
	   children ;  children = children->next ) {
	  enc =  gtk_object_get_data(GTK_OBJECT(children->data), "encoding");
	  if (enc && (enc->enc & flags)) {
	       GtkWidget *label =
		    gtk_object_get_data(GTK_OBJECT(enc->hbox), "label");

	       gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(children->data), TRUE);
	       gtk_label_set_text(GTK_LABEL(label), enc->code);
	       return;
	  }
     }
}



static void dt_binary_encoding_activated(GtkWidget *widget,
					 struct encoding_data *enc)
{
     GtkWidget *label =
	  gtk_object_get_data(GTK_OBJECT(enc->hbox), "label");

     gtk_label_set_text(GTK_LABEL(label), enc->code);

     dt_binary_switch_to_encoding(enc->form,
				  enc->hbox,
				  enc->data_widget, enc->enc);
}

GtkWidget *dt_binary_get_widget(int error_context,
				struct formfill *form,
				GByteArray *data,
				GCallback *activatefunc,
				gpointer funcdata)
{
     GtkWidget *w = dt_generic_binary_get_widget(error_context, form, data,
						 activatefunc, funcdata);

/*       GtkWidget *vbox = dt_generic_binary_retrieve_vbox_widget(w); */
     GtkWidget *menu = dt_generic_binary_retrieve_menu_widget(w);
     GtkWidget *data_widget = dt_generic_binary_retrieve_data_widget(w);

     GSList *group = NULL;
     GtkWidget *item, *s, *label;
     int i;
     struct encoding_data *enc;

     s = gtk_hseparator_new();
     gtk_widget_show(s);

     item = gtk_menu_item_new();
     gtk_widget_show(item);
     gtk_container_add(GTK_CONTAINER(item), s);

     gtk_menu_append(GTK_MENU(menu), item);

     for(i = 0; encodings[i].token ; i++) {
	  item = gtk_radio_menu_item_new_with_label(group,
						    encodings[i].keyword);

	  group = gtk_radio_menu_item_group(GTK_RADIO_MENU_ITEM(item));

	  enc = (struct encoding_data *)g_malloc(sizeof(struct encoding_data));
	  enc->form = form;
	  enc->hbox = w;
	  enc->menu_widget = menu;
	  enc->data_widget = data_widget;
	  enc->enc = encodings[i].token;
	  enc->code = (char*) encodings[i].data;

	  gtk_widget_show(item);
	  g_signal_connect(item, "activate",
			     G_CALLBACK(dt_binary_encoding_activated),
			     enc);

	  gtk_menu_append(GTK_MENU(menu), item);

	  /* store callback info with the object as well - this allows
             to find the menu_item for a given encoding */
	  gtk_object_set_data_full(GTK_OBJECT(item), "encoding", enc, g_free);
     }

     s = gtk_alignment_new(0,0,0,0);
     gtk_widget_show(s);

     label = gtk_label_new("P");
     gtk_widget_show(label);
     gtk_container_add(GTK_CONTAINER(s), label);
     gtk_box_pack_end(GTK_BOX(w), s, FALSE, TRUE, 0);

     gtk_object_set_data(GTK_OBJECT(w), "label", label);

     return w;
}


GtkWidget *dt_binary_get_data_widget(struct formfill *form,
				     GCallback *activatefunc,
				     gpointer funcdata)
{
     GtkWidget *data_widget;
     GtkWidget *inputbox;
     int *flags = (int*) g_malloc(sizeof(int));
     *flags = DT_BINARY_PLAIN | DT_BINARY_ENTRY;

     data_widget = gtk_alignment_new(0,0,1,1);
     gtk_widget_show(data_widget);

/*       inputbox = gtk_text_new(NULL, NULL); */
/*       gtk_widget_show(inputbox); */
/*       gtk_text_set_editable(GTK_TEXT(inputbox), TRUE); */
     inputbox = gtk_entry_new();
     gtk_widget_show(inputbox);
     gtk_entry_set_editable(GTK_ENTRY(inputbox), TRUE);

     gtk_container_add(GTK_CONTAINER(data_widget), inputbox);

     gtk_object_set_data_full(GTK_OBJECT(data_widget), "flags", flags, g_free);

     return data_widget;
}

static void dt_binary_store_data_internal(struct formfill *form,
					  GtkWidget *hbox,
					  GtkWidget *data_widget,
					  const GByteArray *data)
{
     int *flags = gtk_object_get_data(GTK_OBJECT(data_widget), "flags");
     GtkWidget *widget = GTK_BIN(data_widget)->child;

     if(data) {
	 GByteArray *encoded;

	 if (*flags & DT_BINARY_HEX) {
	      encoded = dt_hex_encode((gchar*)data->data, data->len);
	 } else if (*flags & DT_BINARY_PLAIN) {
	      /* check if data does NOT contain a NUL byte .... */
	      unsigned int i;
	      int fail = 0;
	      for (i = 0 ; i < data->len ; i++) {
		   if (data->data[i] == 0) {
			/* bang - NUL byte where it shouldn't be */
			fail = 1;
			break;
		   }
	      }
	      if (fail) {
		   encoded = dt_b64_encode((gchar*)data->data, data->len);
		   /* wrong encoding */
		   *flags = (*flags & ~DT_BINARY_ENCODING_MASK) | DT_BINARY_BASE64;
		   /* adjust menu! */

		   dt_binary_set_encoding_in_menu(form,
						  dt_generic_binary_retrieve_menu_widget(hbox),
						  *flags);
	      } else {
		   encoded = (GByteArray *) data;
	      }
	 } else {
	      encoded = dt_b64_encode((gchar*)data->data, data->len);
	 }
	 if (*flags & DT_BINARY_TEXT) {
	      GdkFont *font = gdk_font_load("-misc-fixed-medium-r-*-*-*-140-*-*-*-*-*-*,fixed");
	      GtkText *text = GTK_TEXT(widget);

	      if(!font) {
		      PangoFontDescription* desc = pango_font_description_from_string("Fixed");
		      font = gdk_font_from_description(desc);
		      pango_font_description_free(desc);
	      }

	      g_return_if_fail(font);

	      gtk_text_freeze(text);
	      gtk_text_set_point(text, 0);
	      gtk_text_forward_delete(text, gtk_text_get_length(text));
	      gtk_text_insert(text, font, NULL, NULL,
			      (gchar*)encoded->data, encoded->len);
	      gtk_text_thaw(text);
	      gdk_font_unref(font);
	 } else if (*flags & DT_BINARY_ENTRY) {
	      gtk_entry_set_text(GTK_ENTRY(widget), (gchar*)encoded->data);
	 }

	 if (encoded != data)
	      g_byte_array_free(encoded, TRUE);
     }
}

static void dt_binary_switch_to_type(struct formfill *form,
				     GtkWidget *hbox,
				     GtkWidget *data_widget,
				     int newtype)
{
     GtkWidget *widget = NULL;
     GtkWidget *oldwidget = GTK_BIN(data_widget)->child;
     int *flags = gtk_object_get_data(GTK_OBJECT(data_widget), "flags");

     if (newtype & DT_BINARY_TEXT)  newtype = DT_BINARY_TEXT;
     if (newtype & DT_BINARY_ENTRY) newtype = DT_BINARY_ENTRY;

     if (newtype != (*flags & DT_BINARY_WIDGET_MASK)) {
	  GByteArray *data = dt_binary_get_data_internal(form, data_widget);
	  if (newtype & DT_BINARY_TEXT) {
	       widget = gtk_text_new(NULL, NULL);
	       gtk_widget_show(widget);
	       gtk_text_set_editable(GTK_TEXT(widget), TRUE);

	       if (!fixed_font) {
		    fixed_font = pango_font_description_from_string("fixed");
	       }
	       gtk_widget_modify_font(widget, fixed_font);
	  }
	  if (newtype & DT_BINARY_ENTRY) {
	       widget = gtk_entry_new();
	       gtk_widget_show(widget);
	       gtk_entry_set_editable(GTK_ENTRY(widget), TRUE);
	  }
	  *flags = (*flags & ~DT_BINARY_WIDGET_MASK) | newtype;

	  if (oldwidget) {
	       gtk_container_remove(GTK_CONTAINER(data_widget), oldwidget);
	  }
	  gtk_container_add(GTK_CONTAINER(data_widget), widget);
	  if (data) {
	       dt_binary_store_data_internal(form, hbox, data_widget, data);
	       g_byte_array_free(data, TRUE);
	  }
     }
}

static void dt_binary_switch_to_encoding(struct formfill *form,
					 GtkWidget *hbox,
					 GtkWidget *data_widget,
					 int newtype)
{
     int *flags = gtk_object_get_data(GTK_OBJECT(data_widget), "flags");

     if (newtype & DT_BINARY_BASE64)  newtype = DT_BINARY_BASE64;
     if (newtype & DT_BINARY_HEX) newtype = DT_BINARY_HEX;
     if (newtype & DT_BINARY_PLAIN) newtype = DT_BINARY_PLAIN;

     if (newtype != (*flags & DT_BINARY_ENCODING_MASK)) {
	  GByteArray *data = dt_binary_get_data_internal(form, data_widget);
	  *flags = (*flags & ~DT_BINARY_ENCODING_MASK) | newtype;
	  if (data) {
	       dt_binary_store_data_internal(form, hbox, data_widget, data);
	       g_byte_array_free(data, TRUE);
	  }
     }
}

void dt_binary_store_data(struct formfill *form,
			  GtkWidget *hbox,
			  GtkWidget *data_widget,
			  const GByteArray *data)
{
     if(data) {
	  dt_binary_switch_to_type(form, hbox, data_widget, DT_BINARY_TEXT);
	  dt_binary_store_data_internal(form, hbox, data_widget, data);
     }
}

static GByteArray *dt_binary_get_data_internal(struct formfill *form,
					       GtkWidget *data_widget)
{
     GByteArray *copy = NULL;
     int l;

     GtkWidget *inputbox =
	  GTK_BIN(data_widget)->child;
     gchar *content;

     int *flags = gtk_object_get_data(GTK_OBJECT(data_widget), "flags");

     if (!inputbox) return NULL;

     content = gtk_editable_get_chars(GTK_EDITABLE(inputbox), 0, -1);
     if (!content) return NULL;

     l = strlen(content);
     if (l == 0) {
	  g_free(content);
	  return NULL;
     }

     if (*flags & DT_BINARY_HEX) {
	  copy = dt_hex_decode(content, strlen(content));
     } else if (*flags & DT_BINARY_PLAIN) {
	  copy = g_byte_array_new();
	  g_byte_array_append(copy, (guchar*)content, strlen(content));
     } else {
	  copy = dt_b64_decode(content, strlen(content));
     }

     g_free(content);

     return copy;
}

GByteArray *dt_binary_get_data(struct formfill *form, GtkWidget *hbox)
{
     GtkWidget *data_widget = dt_generic_binary_retrieve_data_widget(hbox);
     return dt_binary_get_data_internal(form, data_widget);
}


static void dt_binary_delete_data_internal(struct formfill *form,
					   GtkWidget *data_widget)
{
     int *flags = gtk_object_get_data(GTK_OBJECT(data_widget), "flags");
     GtkWidget *widget = GTK_BIN(data_widget)->child;

     if (*flags & DT_BINARY_TEXT) {
	  GtkText *text = GTK_TEXT(widget);

	  gtk_text_freeze(text);
	  gtk_text_set_point(text, 0);
	  gtk_text_forward_delete(text, gtk_text_get_length(text));
	  gtk_text_thaw(text);
     } else if (*flags & DT_BINARY_ENTRY) {
	  gtk_entry_set_text(GTK_ENTRY(widget), "");
     }

}

void dt_binary_delete_data(struct formfill *form,
			   GtkWidget *hbox,
			   GtkWidget *data_widget)
{
/*       int *flags = gtk_object_get_data(GTK_OBJECT(data_widget), "flags"); */
/*       GtkWidget *widget = GTK_BIN(data_widget)->child; */

     dt_binary_delete_data_internal(form, data_widget);
     dt_binary_switch_to_type(form, hbox, data_widget, DT_BINARY_ENTRY);
}

GByteArray *dt_b64_encode(const char *val, int len)
{
     GByteArray *gb;
     GString *b64 = g_string_sized_new(len * 4 / 3 + 4);

     if (!b64) return(NULL);

     // b64 encode data
     b64_encode(b64, (char*)val, len);

     /* impedance mismatch -- turn GString into GByteArray */

     gb = g_byte_array_new();
     if (!gb) return NULL;
     g_byte_array_append(gb, (guchar*)b64->str, b64->len);
     g_string_free(b64, TRUE);

     return gb;
}

GByteArray *dt_b64_decode(const char *val, int len)
{
/*       int approx = (len / 4) * 3 + 10; */
     GByteArray *gb = g_byte_array_new();
     if (gb == NULL) {
	  return(NULL);
     }
/*       g_byte_array_set_size(gb, approx); */

     b64_decode(gb, val, len);
     return gb;
}

GByteArray *dt_hex_encode(const char *val, int len)
{
     GByteArray *gb;
     int i;
     char linebuf[180], buf[20];
     linebuf[0] = 0;
     gb = g_byte_array_new();

/*       g_byte_array_set_size(gb, len * 3 + 6 * len / 16); */
     for ( i = 0 ; i < len ; i++ ) {
	  if (i % 16 == 0) {
	       if (i > 0) {
		    strcat(linebuf, "\n");
		    g_byte_array_append(gb, (guchar*)linebuf, strlen(linebuf));
	       }
	       sprintf(linebuf, "%06x:", i);  /* Flawfinder: ignore */
	  }
	  sprintf(buf, " %02x", (unsigned char) val[i]); /* Flawfinder: ignore */
	  strcat(linebuf, buf); /* Flawfinder: ignore */
     }

     g_byte_array_append(gb, (guchar*)linebuf, strlen(linebuf));

     return gb;
}

GByteArray *dt_hex_decode(const char *val, int len)
{
     GByteArray *gb = g_byte_array_new();
     int precolon = 1;
     int i;
     int nibble = 0;
     unsigned char b = 0;
     const char *c;

     if (gb == NULL) {
	  return(NULL);
     }


     for (i = 0, c = val ; i < len ; i++, c++) {
	  if (*c == ':') {
	       precolon = 0;
	       continue;
	  }
	  if (precolon) continue;
	  if (*c == '\n' || *c == '\r') {
	       precolon = 1;
	       continue;
	  }
	  if (isspace(*c)) continue;
	  if (isxdigit(*c)) {
	       int n = *c - '0';
	       if (n > 9) n = tolower(*c) - 'a' + 10;
	       if (nibble == 1) {
		    b = 16 * b + n;
		    nibble = 0;
		    g_byte_array_append(gb, &b, 1);
	       } else {
		    b = n;
		    nibble = 1;
	       }
	  }
     }
     b = 0;
     g_byte_array_append(gb, &b, 1);
     /* trust GByteArray to keep the NUL byte.... */

     g_byte_array_set_size(gb, gb->len - 1);

     return gb;
}

/* GType */
G_DEFINE_TYPE(GQDisplayBinary, gq_display_binary, GQ_TYPE_DISPLAY_BINARY_GENERIC);

static void
gq_display_binary_init(GQDisplayBinary* self) {}

static void
gq_display_binary_class_init(GQDisplayBinaryClass* self_class) {
	GQTypeDisplayClass* gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);
	GQDisplayBinaryGenericClass* gdbg_class = GQ_DISPLAY_BINARY_GENERIC_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Binary");
	gtd_class->selectable = TRUE;
	gtd_class->show_in_search_result = FALSE;
	gtd_class->get_widget = dt_binary_get_widget; // reused from dt_generic_binary
	gtd_class->get_data   = dt_binary_get_data;
	gtd_class->set_data   = dt_generic_binary_set_data; // reused from dt_generic_binary
	gtd_class->buildLDAPMod = bervalLDAPMod;

	gdbg_class->encode = dt_b64_encode;
	gdbg_class->decode = dt_b64_decode;
	gdbg_class->get_data_widget = dt_binary_get_data_widget;
	gdbg_class->store_data = dt_binary_store_data;
	gdbg_class->delete_data = dt_binary_delete_data;
	gdbg_class->show_entries = dt_generic_binary_show_entries;
}

