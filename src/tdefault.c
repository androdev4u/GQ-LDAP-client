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

/* $Id: tdefault.c 955 2006-08-31 19:15:21Z herzi $ */

#include <string.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "common.h"
#include "util.h"
#include "schema.h"
#include "template.h"
#include "tdefault.h"

#include "dt_password.h" /* for cryptmap */

void create_tdefault_edit_window(GtkWidget *dummy, GtkWidget *templatewin)
{
     GList *attrlist, *attrlist1, *attrlist2, *tduilist, *combolist;
     GtkWidget *tdefaultwin, *vbox1, *hbox1, *hbox2, *table1;
     GtkWidget *label, *button, *combo, *entry;
     GqServer *server;
     struct gq_template *tmpl;
     struct tdefault_ui *tdui;
     int num_attrs, row, i;
     char *default_type_labels[] = {
	  "Default value",
	  "Follow attribute",
	  "Next numeric",
	  "Password scheme",
	  NULL };

     tmpl = window2template(templatewin);
     if(!tmpl || !tmpl->objectclasses)
	  return;

     server = gtk_object_get_data(GTK_OBJECT(templatewin), "server");
     num_attrs = g_list_length(tmpl->objectclasses);
     attrlist = attrlist_by_oclist(server, tmpl->objectclasses);

     if(!attrlist)
	  return;

     tdefaultwin = gtk_window_new(GTK_WINDOW_TOPLEVEL);
     gtk_container_border_width(GTK_CONTAINER(tdefaultwin), 10);
     g_signal_connect(tdefaultwin, "key_press_event",
			G_CALLBACK(close_on_esc), tdefaultwin);
     gtk_window_set_title(GTK_WINDOW(tdefaultwin), "GQ: default values");

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_container_add(GTK_CONTAINER(tdefaultwin), vbox1);
     gtk_widget_show(vbox1);

     table1 = gtk_table_new(num_attrs + 2, 4, FALSE);
     gtk_widget_show(table1);
     gtk_table_set_row_spacings(GTK_TABLE(table1), 5);
     gtk_table_set_col_spacings(GTK_TABLE(table1), 13);
     gtk_box_pack_start(GTK_BOX(vbox1), table1, FALSE, FALSE, 0);

     /* "Hide" title in row 0 */
     label = gtk_label_new("Hide");
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 0, 1,
		      GTK_SHRINK, GTK_SHRINK, 0, 0);

     tduilist = NULL;
     row = 1;
     attrlist1 = attrlist;
     while(attrlist1) {
//	  printf("attr %s\n", attrlist1->data);
	  tdui = g_malloc(sizeof(struct tdefault_ui));

	  /* hide checkbox */
	  button = gtk_check_button_new();
	  gtk_widget_show(button);
	  tdui->hidden = button;
	  gtk_table_attach(GTK_TABLE(table1), button, 0, 1, row, row + 1,
			   GTK_SHRINK, GTK_SHRINK, 0, 0);

	  if(row == 1) {
	       /* DN */
	       tdui->attr = g_strdup("DN");
	       label = gtk_label_new("DN");
	       gtk_widget_show(label);
	       gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1,
				GTK_SHRINK, GTK_SHRINK, 0, 0);

	       tdui->type = NULL;
	       label = gtk_label_new("RDN attribute");
	       gtk_widget_show(label);
	       gtk_table_attach(GTK_TABLE(table1), label, 2, 3, 1, 2,
				GTK_SHRINK, GTK_SHRINK, 0, 0);

	  }
	  else {
	       /* attribute name */
	       tdui->attr = g_strdup(attrlist1->data);
	       label = gtk_label_new(attrlist1->data);
	       gtk_widget_show(label);
	       gtk_table_attach(GTK_TABLE(table1), label, 1, 2, row, row + 1,
				GTK_SHRINK, GTK_SHRINK, 0, 0);

	       /* type */
	       combo = gtk_combo_new();
	       gtk_widget_show(combo);
	       tdui->type = combo;
	       gtk_table_attach(GTK_TABLE(table1), combo, 2, 3, row, row + 1,
				GTK_SHRINK, GTK_SHRINK, 0, 0);
	       i = 0;
	       combolist = NULL;
	       while(default_type_labels[i])
		    combolist = g_list_append(combolist, default_type_labels[i++]);
	       gtk_combo_set_popdown_strings(GTK_COMBO(combo), combolist);
	       if (combolist) g_list_free(combolist);
	  }

	  hbox1 = gtk_hbox_new(FALSE, 0);
	  gtk_widget_show(hbox1);
	  gtk_table_attach(GTK_TABLE(table1), hbox1, 3, 4, row, row + 1,
			   GTK_FILL|GTK_EXPAND, GTK_FILL|GTK_EXPAND, 0, 0);

	  /* follow attribute combo */
	  combo = gtk_combo_new();
	  gtk_widget_show(combo);
	  tdui->followattr = combo;
	  combolist = NULL;
	  attrlist2 = attrlist;
	  while(attrlist2) {
	       if(strcasecmp(attrlist2->data, attrlist1->data)
		  && strcasecmp(attrlist2->data, "objectClass"))
		    combolist = g_list_append(combolist, attrlist2->data);
	       attrlist2 = attrlist2->next;
	  }
	  gtk_combo_set_popdown_strings(GTK_COMBO(combo), combolist);
	  if (combolist) g_list_free(combolist);
	  gtk_box_pack_start(GTK_BOX(hbox1), combo, FALSE, FALSE, 0);

	  if(row == 1) {
	       tdui->value = NULL;
	       tdui->passwdscheme = NULL;
	  }
	  else {
	       /* value entry box */
	       entry = gtk_entry_new();
	       gtk_widget_show(entry);
	       tdui->value = entry;
	       gtk_box_pack_start(GTK_BOX(hbox1), entry, FALSE, FALSE, 0);

	       /* password scheme combo */
	       combo = gtk_combo_new();
	       gtk_widget_show(combo);
	       tdui->passwdscheme = combo;
	       combolist = NULL;

	       for(i = 0; cryptmap[i].keyword[0]; i++) {
		    combolist = g_list_append(combolist,
					      (char *) cryptmap[i].keyword);
	       }

	       gtk_combo_set_popdown_strings(GTK_COMBO(combo), combolist);
	       if (combolist) g_list_free(combolist);

	       gtk_box_pack_start(GTK_BOX(hbox1), combo, FALSE, FALSE, 0);
	  }

	  /* try to initialize the type state to something reasonable,
	     based on the posixAccount objectclass at least */
	  if(row != 1) {
	       if(!strcasecmp(attrlist1->data, "uidNumber")) {
		    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(tdui->type)->entry), "Next numeric");
		    gtk_widget_hide(tdui->followattr);
		    gtk_widget_hide(tdui->value);
		    gtk_widget_hide(tdui->passwdscheme);
	       }
	       else if(!strcasecmp(attrlist1->data, "userPassword")) {
		    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(tdui->type)->entry), "Password scheme");
		    gtk_widget_hide(tdui->followattr);
		    gtk_widget_hide(tdui->value);
	       }
	       else {
		    gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(tdui->type)->entry), "Default value");
		    gtk_widget_hide(tdui->followattr);
		    gtk_widget_hide(tdui->passwdscheme);
	       }
	  }

	  if(row != 1)
	       g_signal_connect_swapped(GTK_COMBO(tdui->type)->entry, "changed",
					 G_CALLBACK(tdefault_type_changed), tdui);

	  tduilist = g_list_append(tduilist, tdui);
	  row++;
	  attrlist1 = attrlist1->next;
     }

     hbox2 = gtk_hbox_new(FALSE, 0);
     gtk_widget_show(hbox2);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox2, FALSE, FALSE, 10);

     button = gtk_button_new_with_label("  OK  ");
     GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
     GTK_WIDGET_SET_FLAGS(button, GTK_CAN_FOCUS);
     gtk_widget_show(button);
     gtk_widget_grab_default(button);
     gtk_box_pack_start(GTK_BOX(hbox2), button, FALSE, FALSE, 0);

     gtk_object_set_data(GTK_OBJECT(tdefaultwin), "tduilist", tduilist);

     gtk_widget_show(tdefaultwin);
     gtk_widget_grab_focus(button);

}


void tdefault_type_changed(struct tdefault_ui *tdui)
{
     char *type, *value;

     type = gtk_editable_get_chars(GTK_EDITABLE(GTK_COMBO(tdui->type)->entry), 0, -1);
     value = gtk_editable_get_chars(GTK_EDITABLE(tdui->value), 0, -1);
     if(!strcasecmp(type, "Default value")) {
	  gtk_widget_hide(tdui->followattr);
	  gtk_widget_show(tdui->value);
	  gtk_widget_hide(tdui->passwdscheme);
	  /* if there's only '%s' in the value, that was done by "Follow attribute"...
	     that's useless for this type, so delete it */
	  if(!strcasecmp(value, "%s"))
	       gtk_editable_delete_text(GTK_EDITABLE(tdui->value), 0, -1);
     }
     else if(!strcasecmp(type, "Follow attribute")) {
	  gtk_widget_show(tdui->followattr);
	  gtk_widget_show(tdui->value);
	  gtk_widget_hide(tdui->passwdscheme);
	  /* if the value is empty, put in '%s' when switching to this attribute */
	  if(strlen(value) == 0)
	       gtk_entry_set_text(GTK_ENTRY(tdui->value), "%s");
     }
     else if(!strcasecmp(type, "Next numeric")) {
	  gtk_widget_hide(tdui->followattr);
	  gtk_widget_hide(tdui->value);
	  gtk_widget_hide(tdui->passwdscheme);
     }
     else if(!strcasecmp(type, "Password scheme")) {
	  gtk_widget_hide(tdui->followattr);
	  gtk_widget_hide(tdui->value);
	  gtk_widget_show(tdui->passwdscheme);
     }

     g_free(type);
     g_free(value);

}
