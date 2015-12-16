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

/* $Id: dt_password.c 952 2006-08-26 11:17:35Z herzi $ */

#include "dt_password.h"

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>

#include <glib.h>
#include <glib/gi18n.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

#if defined(HAVE_LIBCRYPTO)
#include <openssl/rand.h>
#include <openssl/des.h>
#include <openssl/md5.h>
#include <openssl/md4.h>
#include <openssl/sha.h>
#endif /* defined(HAVE_LIBCRYPTO) */

#include "common.h"
#include "gq-tab-browse.h"
#include "errorchain.h"
#include "formfill.h"
#include "input.h"
#include "tinput.h"
#include "util.h"
#include "encode.h"
#include "ldif.h" /* for b64_decode */
#include "dtutil.h"
#include "dt_password.h"
#include "iconv-helpers.h"

GtkWidget *dt_password_get_widget(int error_context,
				  struct formfill *form, GByteArray *data,
				  GCallback *activatefunc,
				  gpointer funcdata)
{
     GtkWidget *hbox;
     GtkWidget *inputbox;
     GtkWidget *combo;
     GtkStyle *style;

     GList *cryptlist = NULL;
     int temp, max_width, i;


     hbox = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox);

     inputbox = gtk_entry_new();
     gtk_widget_show(inputbox);


     gtk_box_pack_start(GTK_BOX(hbox), inputbox, TRUE, TRUE, 0);

     /* XXX this will need an encoder wedge */
     if(activatefunc)
	  g_signal_connect_swapped(inputbox, "activate",
				    G_CALLBACK(activatefunc),
				    funcdata);

     combo = gtk_combo_new();
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_COMBO(combo)->entry, GTK_CAN_FOCUS);
#endif
     gtk_editable_set_editable(GTK_EDITABLE(GTK_COMBO(combo)->entry), FALSE);

     style = gtk_widget_get_style(GTK_COMBO(combo)->entry);

     cryptlist = NULL;
     temp = max_width = 0;

     for(i = 0 ; cryptmap[i].keyword[0] ; i++) {
	  temp = gdk_string_width(gtk_style_get_font(style), cryptmap[i].keyword);
	  max_width = MAX(max_width, temp);
	  cryptlist = g_list_append(cryptlist, (char *) cryptmap[i].keyword);
     }
     gtk_combo_set_popdown_strings(GTK_COMBO(combo), cryptlist);
     gtk_widget_set_usize(GTK_COMBO(combo)->entry, max_width + 20, -1);
     g_list_free(cryptlist);

     gtk_widget_show(combo);
     gtk_box_pack_start(GTK_BOX(hbox), combo, FALSE, FALSE, 0);

     dt_password_set_data(form, data, hbox);

     return hbox;
}

void dt_password_set_data(struct formfill *form, GByteArray *data,
			  GtkWidget *hbox)
{
     GQTypeDisplayClass* klass = g_type_class_ref(form->dt_handler);
     const char *crypt_type = "Clear";
     GList *boxchildren;
     GtkWidget *entry, *combo;
     gsize i;

     boxchildren = GTK_BOX(hbox)->children;
     entry = ((struct _GtkBoxChild *) boxchildren->data)->widget;
     combo = ((struct _GtkBoxChild *) boxchildren->next->data)->widget;

     editable_set_text(GTK_EDITABLE(entry), data,
		       DT_PASSWORD(klass)->encode,
		       DT_PASSWORD(klass)->decode);

     if(data && data->data[0] == '{') {
	  char crypt_prefix[12];
	  // FIXME: this loop could be nicer (strnstr and strncpy)
	  for(i = 1; data->data[i] && data->data[i] != '}' && i < sizeof(crypt_prefix); i++)
	       crypt_prefix[i - 1] = data->data[i];
	  crypt_prefix[i - 1] = '\0';
	  crypt_type = detokenize(cryptmap,
				  tokenize(cryptmap, crypt_prefix));
     }

     /* set the combo to the encoding type */
     gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry), crypt_type);
     g_type_class_unref(klass);
}

GByteArray *dt_password_get_data(struct formfill *form, GtkWidget *hbox)
{
     GByteArray *data;
     GtkWidget *combo;
     GList *boxchildren;
     gchar *crypt_type;

     boxchildren = GTK_BOX(hbox)->children;

     data = editable_get_text(GTK_EDITABLE(((struct _GtkBoxChild *) boxchildren->data)->widget));

     if(boxchildren->next) {
	  int cryptflag;
	  /* advance to crypt combo */
	  boxchildren = boxchildren->next;
	  combo = ((struct _GtkBoxChild *) boxchildren->data)->widget;
	  crypt_type =
	       gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(combo)->entry),
				      0, -1);

	  cryptflag = tokenize(cryptmap, crypt_type);

	  /* check to see if the content was already crypted.... */
	  if (data && data->data[0] != '{') {
	       /* need to crypt the stuff */
	       CryptFunc *cryptfunc = detokenize_data(cryptmap, cryptflag);
	       if (cryptfunc != NULL) {
		    GByteArray *crypted = cryptfunc((gchar*)data->data, data->len);
		    /* overwrite plain-text */
		    memset(data->data, 0, data->len);
		    g_byte_array_free(data, TRUE);
		    data = crypted;
	       }
	  }

	  g_free(crypt_type);
     }

     return data;
}

/* GType */
G_DEFINE_TYPE(GQDisplayPassword, gq_display_password, GQ_TYPE_DISPLAY_ENTRY);

static void
gq_display_password_init(GQDisplayPassword* self) {}

static void
gq_display_password_class_init(GQDisplayPasswordClass* self_class) {
	GQTypeDisplayClass * gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);
	GQDisplayEntryClass* gde_class = GQ_DISPLAY_ENTRY_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Password");
	gtd_class->selectable = TRUE;
	gtd_class->show_in_search_result = TRUE;
	gtd_class->get_widget = dt_password_get_widget;
	gtd_class->get_data = dt_password_get_data;
	gtd_class->set_data = dt_password_set_data;
	gtd_class->buildLDAPMod = bervalLDAPMod; // reuse method from dt_entry

	gde_class->encode = NULL;
	gde_class->decode = NULL;
}

