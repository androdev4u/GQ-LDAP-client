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

/* $Id: prefs.c 994 2006-09-16 14:12:20Z herzi $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "common.h"
#include "configfile.h"
#include "gq-keyring.h"
#include "gq-server-list.h"
#include "gq-tab-browse.h"
#include "ldif.h"
#include "mainwin.h"
#include "prefs.h"
#include "state.h"
#include "util.h"
#include "template.h"
#include "errorchain.h"
#include "debug.h"
#include "input.h"

/* for now: only allow a single preferences window */
static GtkWidget *prefswindow;

/* for now: only allow a single server window */
static GtkWidget *current_edit_server_window = NULL;

static GtkWidget *current_serverstab_serverclist = NULL;
GtkWidget *current_template_clist = NULL;   /* FIXME: used in
                                               template.c as well */



struct prefs_windata {
     GtkWidget *search_st[4];
     GtkWidget *ldif_format[2];

     GtkWidget *prefswindow;

     GtkWidget *templatelist;

     GtkWidget *showdn;
     GtkWidget *showoc;
     GtkWidget *show_rdn_only;
     GtkWidget *sort_search;
     GtkWidget *sort_browse;
     GtkWidget *restore_window_sizes;
     GtkWidget *restore_window_positions;
     GtkWidget *restore_search_history;
     GtkWidget *restore_tabs;

     GtkWidget *browse_use_user_friendly;

     /* template tab */
     GtkWidget *schemaserver;

     /* servers tab */
     GtkWidget *serverstab_server_clist;

     /* security */
     GtkWidget *never_leak_credentials;
     GtkWidget *do_not_use_ldap_conf;

};

struct server_windata {
     GtkWidget *editwindow;

     GtkWidget *servername;
     GtkWidget *ldaphost;
     GtkWidget *ldapport;
     GtkWidget *basedn;
     GtkWidget *bindtype;
     GtkWidget *binddn;
     GtkWidget *bindpw;
     GtkWidget *show_pw_toggle;
     GtkWidget *clear_pw;
     GtkWidget *searchattr;
     GtkWidget *maxentries;
     GtkWidget *localcachetimeout;
     GtkWidget *ask_pw;
     GtkWidget *hide_internal;
     GtkWidget *cacheconn;
     GtkWidget *show_ref;
     GtkWidget *enabletls;
};

static void create_serverstab(GtkWidget *target, struct prefs_windata *pw);
static void create_templatestab(GtkWidget *target, struct prefs_windata *pw);
static void create_browse_optionstab(GtkWidget *target,
				     struct prefs_windata *pw);
static void create_search_optionstab(GtkWidget *target,
				     struct prefs_windata *pw);
static void create_ldiftab(GtkWidget *target, struct prefs_windata *pw);

static void destroy_edit_server_window(GtkWidget *this, 
				       struct server_windata *sw);
static void create_guitab(GtkWidget *target, struct prefs_windata *);
static void create_security_tab(GtkWidget *target, struct prefs_windata *);

static void template_new_callback(GtkWidget *widget, struct prefs_windata *);
static void template_edit_callback(GtkWidget *widget, struct prefs_windata *);
static void template_selected_callback(GtkWidget *clist, gint row, gint column,
				       GdkEventButton *event, 
				       struct prefs_windata *data);
static void template_delete_callback(GtkWidget *widget,
				     struct prefs_windata *pw);





typedef struct _prefs_callback_data {
     struct server_windata *sw;
     GqServer *server;
     int edit_new_server;
     /* is the server a dynamically added one */
     gboolean transient;
} prefs_callback_data;




static prefs_callback_data *new_prefs_callback_data(struct server_windata *sw)
{
     prefs_callback_data *d = g_malloc(sizeof(prefs_callback_data));
     d->server = NULL;
     d->edit_new_server = 0;
     d->sw = sw;
     return d;
}

static void destroy_prefs_callback_data(prefs_callback_data *cb_data) {
     if (cb_data) {
	  if (cb_data->edit_new_server) {
	       if (cb_data->server) {
		    gq_server_list_remove(gq_server_list_get(), cb_data->server);
	       }
	  }
	  if(cb_data->server) {
	       g_object_unref(cb_data->server);
	       cb_data->server = NULL;
	  }
	  g_free(cb_data);
     }
}

static void
check_unique_name(GQServerList* list, GqServer* server, gpointer user_data) {
	gpointer* unique_and_server = user_data;
	gboolean* unique = unique_and_server[0];
	GqServer* newserver = unique_and_server[1];

	if(server != newserver && !strcmp(server->name, newserver->name)) {
		*unique = FALSE;
	}
}

static void server_edit_callback(GtkWidget *this, prefs_callback_data *cb_data)
{
     GtkWidget *window, *field;
     GqServer *server, *servers;
     int server_name_changed;
     const char *text, *passwdtext;
     char *ep = NULL;
#if HAVE_LDAP_CLIENT_CACHE
     int tmp;
#endif
     GList *I;
     struct server_windata *sw = cb_data->sw;
     gboolean save_ok;
     GqServer *save = gq_server_new();
     gboolean unique = TRUE;
     gpointer unique_and_server[2] = {
	     &unique,
	     cb_data->server
     };

     server = cb_data->server;
     copy_ldapserver(save, server);

     window = sw->editwindow;

     /* Name */
     field = sw->servername;
     text = gtk_entry_get_text(GTK_ENTRY(field));
     if(strcmp(text, server->name))
	  server_name_changed = 1;
     else
	  server_name_changed = 0;

     g_free_and_dup(server->name, text);

     /* make sure server name is unique */
	     gq_server_list_foreach(gq_server_list_get(), check_unique_name, unique_and_server);

	     if(!unique) {
		    /* already exists */
		    error_popup(_("Error adding new server"),
				_("A server by that name already exists\n\n"
				  "Please choose another name"),
				this);
		    goto done;
	     }

     /* LDAP host */
     field = sw->ldaphost;
     text = gtk_entry_get_text(GTK_ENTRY(field));
     g_free_and_dup(server->ldaphost, text);

     /* LDAP port */
     field = sw->ldapport;
     text = gtk_entry_get_text(GTK_ENTRY(field));
     ep = NULL;
     server->ldapport = strtol(text, &ep, 10);
     if (ep && *ep) {
	  if (*text) {
	       single_warning_popup(_("Port must be numeric or empty"));
	       goto done;
	  } else {
	       /* empty, might have LDAP URI */
	       server->ldapport = -1;
	  }
     }

     /* Base DN */
     field = sw->basedn;
     text = gtk_entry_get_text(GTK_ENTRY(field));
     g_free_and_dup(server->basedn, text);

     /* Bind DN */
     field = sw->binddn;
     text = gtk_entry_get_text(GTK_ENTRY(field));
     g_free_and_dup(server->binddn, text);

     /* Ask password */
     field = sw->ask_pw;
     server->ask_pw = GTK_TOGGLE_BUTTON(field)->active ? 1 : 0;

     /* Bind Password */
     passwdtext = gtk_entry_get_text(GTK_ENTRY(sw->bindpw));
     g_free(server->bindpw);
     if(!server->ask_pw && passwdtext) {
	  server->bindpw = g_strdup(passwdtext);
	  gq_keyring_save_server_password(server);
     } else {
	  server->bindpw = g_strdup("");
     }

     /* Search attribute */
     field = sw->searchattr;
     text = gtk_entry_get_text(GTK_ENTRY(field));
     g_free_and_dup(server->searchattr, text);

     /* Maximum entries */
     field = sw->maxentries;
     text = gtk_entry_get_text(GTK_ENTRY(field));
     ep = NULL;
     server->maxentries = strtol(text, &ep, 10);
     if (ep && *ep) {
	  if (*text) {
	       single_warning_popup(_("Maximum number of entries must be numeric or empty"));
	  } else {
	       /* empty, might have LDAP URI */
	       server->maxentries = 0;
	  }
     }

     /* Hide internal */
     field = sw->hide_internal;
     server->hide_internal = GTK_TOGGLE_BUTTON(field)->active ? 1 : 0;

     /* Cache connection */
     field = sw->cacheconn;
     server->cacheconn = GTK_TOGGLE_BUTTON(field)->active ? 1 : 0;

     /* Enable TLS */
     field = sw->enabletls;
     server->enabletls = GTK_TOGGLE_BUTTON(field)->active ? 1 : 0;

#if HAVE_LDAP_CLIENT_CACHE
     /* Local Cache Timeout */
     field = sw->localcachetimeout;
     text = gtk_entry_get_text(GTK_ENTRY(field));
     tmp = (int) strtol(text, &ep, 0);
     if (!ep || !*ep) server->local_cache_timeout = tmp;
#endif

     field = GTK_COMBO(sw->bindtype)->entry;
     text = gtk_entry_get_text(GTK_ENTRY(field));
     server->bindtype = tokenize(token_bindtype, text);

     /* connection info might have changed for this server -- close
	cached connection */
     close_connection(server, TRUE);
     canonicalize_ldapserver(server);

     if(server_name_changed) {
	  /* refresh clist in serverstab */
	  fill_serverlist_serverstab();
     }

     save_ok = TRUE;
     /* I do not really like this ad-hoc solution to check if a server
	is a transient server or a configured one, but ... */
     if (!is_transient_server(server)) {
	  /* so the cancel button doesn't really cancel :-) */
	  save_ok = save_config(window);
     }

/*      if (cb_data->edit_new_server) { */
/*	  /\* everything ok, take the server out of the cb_data (in case it */
/*	     was created anew, this will avoid that the cb_data destroy */
/*	     function deletes the ldapserver) *\/ */
/*	  cb_data->server = NULL; */
/*      } */

     if (save_ok) {
	  update_serverlist(&mainwin);
	  gtk_widget_destroy(window);
     } else {
	  copy_ldapserver(server, save);
     }

 done:
     if (save) g_object_unref(save);
}

static void host_changed_callback(GtkEditable *host, GtkWidget *port)
{
     gchar *s = gtk_editable_get_chars(host, 0, -1);

     if (s) {
	  gtk_widget_set_sensitive(GTK_WIDGET(port),
				   (g_utf8_strchr(s, -1, ':') == NULL));
	  g_free(s);
     }
}

static void destroy_edit_server_window(GtkWidget *this,
				       struct server_windata *sw)
{
     if (this == current_edit_server_window) {
	  current_edit_server_window = NULL;
     }
     if (sw) g_free(sw);
}

static gboolean destroy_edit_server_window_on_esc(GtkWidget *widget,
						  GdkEventKey *event,
						  gpointer data)
{
     if(event && event->type == GDK_KEY_PRESS && event->keyval == GDK_Escape) {
	  gtk_widget_destroy(widget);
	  return(TRUE);
     }

     return(FALSE);
}

static void
toggle_pw_visibility(GtkToggleButton* toggle, GtkEntry* entry) {
	gboolean visible = gtk_toggle_button_get_active(toggle);

	if(!visible || question_popup(_("Cleartext Password"),
			  _("The password that you entered will be displayed in\n"
			    "the entry. This can be dangerous if someone is watching\n"
			    "at the screen you're currently working on (that includes\n"
			    "both people behind you and people that may watch your\n"
			    "desktop via VNC).\n\n"
			    "Do you still want to see the password?")))
	{
		// FIXME: let the user set a "don't display again" switch
		gtk_entry_set_visibility(entry, visible);
	} else if(visible) {
		// un-toggle the button if the user was not sure about
		// displaying the password
		gtk_toggle_button_set_active(toggle, FALSE);
	}
}

static void
ask_pw_toggled(GtkToggleButton *button, struct server_windata *sw) {
	gboolean sensitive = !gtk_toggle_button_get_active(button);
	gtk_widget_set_sensitive(sw->bindpw, sensitive);
	gtk_widget_set_sensitive(sw->show_pw_toggle, sensitive);
	gtk_widget_set_sensitive(sw->clear_pw, sensitive);
}

static void
clear_pw(GtkButton* button, prefs_callback_data *cb_data) {
	gtk_entry_set_text(GTK_ENTRY(cb_data->sw->bindpw), "");
	gq_keyring_forget_password(cb_data->server);
}

void
create_edit_server_window(GqServer *server,
			  GtkWidget *modalFor)
{
     GtkWidget *editwindow, *notebook;
     GtkWidget *main_vbox, *vbox1, *vbox2, *hbox;
     GtkWidget *table1, *table2, *table3;
     GtkWidget *label, *entry, *button;
     GtkWidget *okbutton, *cancelbutton;
     GtkWidget *bindpw, *host, *box;
     int y, z;
     int i;
     GString *title;
     char tmp[16];	/* fixed buffer: OK - used for printf("%d") only */
     GtkWidget *bindtype;
     GList *bindtypes = NULL;
     GtkTooltips *tips;
     prefs_callback_data *cb_data;
     struct server_windata *sw = NULL;
     gchar* password = NULL;

     if(current_edit_server_window) {
	  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sw->show_pw_toggle), FALSE);
	  gtk_window_present(GTK_WINDOW(current_edit_server_window));
	  return;
     }

     tips = gtk_tooltips_new();

     sw = g_malloc0(sizeof(struct server_windata));

     cb_data = new_prefs_callback_data(sw);
     cb_data->edit_new_server = (server == NULL);
     cb_data->transient = is_transient_server(server);

     if(!server) {
		gint i;
		server = gq_server_new();
		for(i = 1; i < G_MAXINT; i++) {
			gchar* name = g_strdup_printf(_("Untitled Server %d"), i);
			if(G_LIKELY(!gq_server_list_get_by_name(gq_server_list_get(),
								name))) {
				gq_server_set_name(server, name);
				g_free(name);
				break;
			}
			g_free(name);
		}
		gq_server_list_add(gq_server_list_get(), server);
     }

     cb_data->server = g_object_ref(server);

     editwindow = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
					  "serveredit", -1, -1);
     sw->editwindow = editwindow;
     current_edit_server_window = editwindow;

     if (modalFor) {
	  g_assert(GTK_IS_WINDOW(modalFor));
	  gtk_window_set_modal(GTK_WINDOW(editwindow), TRUE);
	  gtk_window_set_transient_for(GTK_WINDOW(editwindow),
				       GTK_WINDOW(modalFor));
     }

     gtk_object_set_data_full(GTK_OBJECT(editwindow),
			      "cb_data",
			      cb_data,
			      (GtkDestroyNotify) destroy_prefs_callback_data);

     title = g_string_sized_new(64);

     if(cb_data->edit_new_server) {
	  g_string_sprintf(title, _("New server"));
     } else {
	  g_string_sprintf(title, _("Server %s"), server->name);
     }
     if (cb_data->transient) {
	  g_string_sprintf(title, _("Transient server %s"), server->name);
     }

     gtk_window_set_title(GTK_WINDOW(editwindow), title->str);
     g_string_free(title, TRUE);

     gtk_window_set_policy(GTK_WINDOW(editwindow), TRUE, TRUE, FALSE);

     main_vbox = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(main_vbox),
				CONTAINER_BORDER_WIDTH);
     gtk_widget_show(main_vbox);
     gtk_container_add(GTK_CONTAINER(editwindow), main_vbox);

     notebook = gtk_notebook_new();
     gtk_widget_show(notebook);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_NOTEBOOK(notebook), GTK_CAN_FOCUS);
#endif
     gtk_box_pack_start(GTK_BOX(main_vbox), notebook, TRUE, TRUE, 0);

     /* "General" tab */

     vbox1 = gtk_vbox_new(FALSE, 20);
     gtk_widget_show(vbox1);
     gtk_container_border_width(GTK_CONTAINER(vbox1),
				CONTAINER_BORDER_WIDTH);
     label = gq_label_new(_("_General"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox1, label);

     table1 = gtk_table_new(4, 2, FALSE);
     gtk_widget_show(table1);
     gtk_box_pack_start(GTK_BOX(vbox1), table1, FALSE, FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(table1),
				CONTAINER_BORDER_WIDTH);
     gtk_table_set_row_spacings(GTK_TABLE(table1), 5);
     gtk_table_set_col_spacings(GTK_TABLE(table1), 13);

     /* Name */
     label = gq_label_new(_("_Name"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 0, 1,
		      GTK_FILL, GTK_FILL, 0, 0);
     gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);

     entry = gtk_entry_new();

     sw->servername = entry;

     gtk_entry_set_text(GTK_ENTRY(entry), server->name);
     gtk_widget_show(entry);
     g_signal_connect(entry, "activate",
			G_CALLBACK(server_edit_callback), cb_data);
     gtk_table_attach(GTK_TABLE(table1), entry, 1, 2, 0, 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
     gtk_widget_grab_focus(entry);

     gtk_tooltips_set_tip(tips, entry,
			  _("The nickname of the server definition"),
			  Q_("tooltip|The nickname is used to refer to this "
			     "server "
			     "definition throughout this application")
			  );

     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

     /* LDAP host */
     label = gq_label_new(_("LDAP _Host/URI"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 1, 2,
		      GTK_FILL, GTK_FILL, 0, 0);

     host = entry = gtk_entry_new();
     sw->ldaphost = entry;
     gtk_entry_set_text(GTK_ENTRY(entry), server->ldaphost);

     gtk_widget_show(entry);
     g_signal_connect(entry, "activate",
			G_CALLBACK(server_edit_callback), cb_data);
     gtk_table_attach(GTK_TABLE(table1), entry, 1, 2, 1, 2,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_tooltips_set_tip(tips, entry,
			  _("The host name or LDAP URI of the LDAP server"),
			  Q_("tooltip|Either use the name or IP address of "
			     "the server "
			     "or an LDAP URI (either ldap or ldaps). An URI "
			     "is recognized through the existance of a colon "
			     "in this field")
			  );

     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

     /* Port */
     label = gq_label_new(_("LDAP _Port"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 2, 3,
		      GTK_FILL, GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     sw->ldapport = entry;
     if (server->ldapport != 0) {
	  g_snprintf(tmp, sizeof(tmp), "%d", server->ldapport);
     } else {
	  *tmp = 0;
     }
     gtk_entry_set_text(GTK_ENTRY(entry), tmp);
     gtk_widget_show(entry);
     g_signal_connect(entry, "activate",
			G_CALLBACK(server_edit_callback), cb_data);
     gtk_table_attach(GTK_TABLE(table1), entry, 1, 2, 2, 3,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_tooltips_set_tip(tips, entry,
			  _("The port the LDAP server listens on"),
			  Q_("tooltip|If empty, the well-known LDAP port (389) "
			     "is assumed. This field is not available if an "
			     "LDAP URI is used.")
			  );

     /* Callback on HOST to enable/disable PORT if user enters a colon... */

     g_signal_connect(host, "changed",
			G_CALLBACK(host_changed_callback), entry);

     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

     g_signal_emit_by_name(host, "changed", NULL, NULL);

     /* Base DN */
     label = gq_label_new(_("_Base DN"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table1), label, 0, 1, 3, 4,
		      GTK_FILL, GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     sw->basedn = entry;
     gtk_entry_set_text(GTK_ENTRY(entry), server->basedn);
     gtk_widget_show(entry);
     g_signal_connect(entry, "activate",
			G_CALLBACK(server_edit_callback), cb_data);
     gtk_table_attach(GTK_TABLE(table1), entry, 1, 2, 3, 4,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_tooltips_set_tip(tips, entry,
			  _("The base DN of the server"),
			  Q_("tooltip|This base DN gets used in search mode, "
			     "usually "
			     "though, this application queries the server "
			     "for its 'namingContexts'")
			  );
     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);



     /* Add not if the server is transient */


     if (cb_data->transient) {
	  label = gtk_label_new(_("NOTE: This is a transient server definition. It has been added dynamically and it will not be saved to the permanent configuration."));

	  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
	  gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, FALSE, 0);
	  gtk_widget_show(label);
     }

     /* "Details" tab */

     vbox2 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox2);
     gtk_container_border_width(GTK_CONTAINER(vbox2),
				CONTAINER_BORDER_WIDTH);
     label = gq_label_new(_("_Details"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox2, label);

     table2 = gtk_table_new(8, 2, FALSE);
     gtk_widget_show(table2);
     gtk_box_pack_start(GTK_BOX(vbox2), table2, TRUE, TRUE, 0);
     gtk_container_border_width(GTK_CONTAINER(table2),
				CONTAINER_BORDER_WIDTH);
     gtk_table_set_row_spacings(GTK_TABLE(table2), 5);
     gtk_table_set_col_spacings(GTK_TABLE(table2), 13);
     y = 0;

     table3 = gtk_table_new(3, 2, FALSE);
     gtk_widget_show(table3);
     gtk_box_pack_start(GTK_BOX(vbox2), table3, TRUE, TRUE, 0);
     gtk_container_border_width(GTK_CONTAINER(table3),
				CONTAINER_BORDER_WIDTH);
     gtk_table_set_row_spacings(GTK_TABLE(table3), 5);
     gtk_table_set_col_spacings(GTK_TABLE(table3), 13);
     z = 0;

     /* Bind DN */
     label = gq_label_new(_("_Bind DN"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, y, y + 1,
		      GTK_FILL, GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     sw->binddn = entry;
     gtk_entry_set_text(GTK_ENTRY(entry), server->binddn);
     gtk_widget_show(entry);
     g_signal_connect(entry, "activate",
			G_CALLBACK(server_edit_callback), cb_data);
     gtk_table_attach(GTK_TABLE(table2), entry, 1, 2, y, y + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
     y++;

     gtk_tooltips_set_tip(tips, entry,
			  _("The DN to bind with to the LDAP server"),
			  Q_("tooltip|This is equivalent to a 'username'. "
			     "Ask the "
			     "LDAP administrator for the DN to use.")
			  );

     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

     /* Bind Password */
     label = gq_label_new(_("Bind _Password"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, y, y + 1,
		      GTK_FILL, GTK_FILL, 0, 0);

     box = gtk_hbox_new(FALSE, 3);
     gtk_table_attach(GTK_TABLE(table2), box, 1, 2, y, y + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
     y++;

     sw->bindpw = bindpw = entry = gtk_entry_new();
     gtk_box_pack_start_defaults(GTK_BOX(box), sw->bindpw);
     gtk_entry_set_visibility(GTK_ENTRY(entry), FALSE);

     password = gq_keyring_get_password(server);
     if(password) {
	gtk_entry_set_text(GTK_ENTRY(sw->bindpw), password ? password : "");
	g_signal_connect(sw->bindpw, "activate",
			 G_CALLBACK(server_edit_callback), cb_data);
	gnome_keyring_free_password(password);
	password = NULL;
     }

     gtk_tooltips_set_tip(tips, sw->bindpw,
			  _("The password to bind with to the LDAP server"),
			  Q_("tooltip|This is related to the bind DN. Note that the "
			     "password gets stored in a configuration file. "
			     "Recent versions of this application actually "
			     "scramble the password, but this scrambling can "
			     "easily be reverted. Do not use a valuable "
			     "password here.")
			  );


     gtk_label_set_mnemonic_widget(GTK_LABEL(label), sw->bindpw);
     gtk_widget_show_all(box);

     /* the show password button */
     sw->show_pw_toggle = gtk_toggle_button_new();
     gtk_container_add(GTK_CONTAINER(sw->show_pw_toggle),
		       gtk_image_new_from_stock(GTK_STOCK_DIALOG_AUTHENTICATION, GTK_ICON_SIZE_MENU));
     gtk_box_pack_start(GTK_BOX(box), sw->show_pw_toggle, FALSE, FALSE, 0);
     gtk_tooltips_set_tip(tips, sw->show_pw_toggle,
		          _("Show the password"),
			  Q_("tooltip|Clicking this button asks the user whether "
			     "he really wants to see the password and if that's "
			     "the case, displays the password. Clicking this button "
			     "once more will hide the password."));
     g_signal_connect(sw->show_pw_toggle, "toggled",
		      G_CALLBACK(toggle_pw_visibility), sw->bindpw);
     gtk_widget_show_all(sw->show_pw_toggle);

     /* the delete password button */
     sw->clear_pw = gtk_button_new();
     gtk_container_add(GTK_CONTAINER(sw->clear_pw),
		       gtk_image_new_from_stock(GTK_STOCK_CLEAR, GTK_ICON_SIZE_MENU));
     gtk_box_pack_start(GTK_BOX(box), sw->clear_pw, FALSE, FALSE, 0);
     gtk_tooltips_set_tip(tips, sw->clear_pw,
		          _("Clear the Password"),
			  Q_("tooltip|Clicking this button asks the user whether "
			     "he really wants to clear the password and if that's "
			     "the case, clears the password entry and deletes the "
			     "password from the keyring."));
     g_signal_connect(sw->clear_pw, "clicked",
		      G_CALLBACK(clear_pw), cb_data);
     gtk_widget_show_all(sw->clear_pw);

     /* Bind type */
     label = gq_label_new(_("Bind t_ype"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, y, y + 1,
		      GTK_FILL, GTK_FILL, 0, 0);

     bindtype = gtk_combo_new();
     sw->bindtype = bindtype;

     for (i = 0;
	  token_bindtype[i].keyword && strlen(token_bindtype[i].keyword);
	  i++) {
	  bindtypes = g_list_append(bindtypes,
				    GINT_TO_POINTER(token_bindtype[i].keyword));
     }

     gtk_combo_set_popdown_strings(GTK_COMBO(bindtype), bindtypes);
     g_list_free(bindtypes);

     gtk_entry_set_editable(GTK_ENTRY(GTK_COMBO(bindtype)->entry), FALSE);
     gtk_widget_show(bindtype);

     gtk_list_select_item(GTK_LIST(GTK_COMBO(bindtype)->list),
			  server->bindtype);
     gtk_table_attach(GTK_TABLE(table2), bindtype, 1, 2, y, y + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
     y++;

     gtk_tooltips_set_tip(tips, GTK_WIDGET(GTK_COMBO(bindtype)->entry),
			  _("How to bind to the LDAP server"),
			  Q_("tooltip|gq supports several different bind "
			     "types, like Simple, Kerberos or SASL binds.")
			  );

     gtk_label_set_mnemonic_widget(GTK_LABEL(label),
				   GTK_COMBO(bindtype)->entry);

     /* Search attribute */
     label = gq_label_new(_("_Search Attribute"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, y, y + 1,
		      GTK_FILL, GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     sw->searchattr = entry;
     gtk_entry_set_text(GTK_ENTRY(entry), server->searchattr);
     gtk_widget_show(entry);
     g_signal_connect(entry, "activate",
			G_CALLBACK(server_edit_callback), cb_data);
     gtk_table_attach(GTK_TABLE(table2), entry, 1, 2, y, y + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
     y++;

     gtk_tooltips_set_tip(tips, entry,
			  _("The attribute to 'search' in a search tab."),
			  Q_("tooltip|Search mode in the search tab searches "
			     "this attribute. This alleviates the user to "
			     "always use a proper LDAP filter.")
			  );
     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

     /* Maximum entries */
     label = gq_label_new(_("_Maximum entries"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, y, y + 1,
		      GTK_FILL, GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     sw->maxentries = entry;
     g_snprintf(tmp, sizeof(tmp), "%d", server->maxentries);
     gtk_entry_set_text(GTK_ENTRY(entry), tmp);
     gtk_widget_show(entry);
     g_signal_connect(entry, "activate",
			G_CALLBACK(server_edit_callback), cb_data);
     gtk_table_attach(GTK_TABLE(table2), entry, 1, 2, y, y + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
     y++;

     gtk_tooltips_set_tip(tips, entry,
			  _("The maximum number of entries to return in "
			    "search mode."),
			  Q_("tooltip|NOTE: A server might impose stricter "
			     "limits")
			  );

     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

#if HAVE_LDAP_CLIENT_CACHE
     /* Use local cache */
     label = gq_label_new(_("LDAP cache timeo_ut"));
     gtk_misc_set_alignment(GTK_MISC(label), 0.0, .5);
     gtk_widget_show(label);
     gtk_table_attach(GTK_TABLE(table2), label, 0, 1, y, y + 1,
		      GTK_FILL, GTK_FILL, 0, 0);

     entry = gtk_entry_new();
     sw->localcachetimeout = entry;
     g_snprintf(tmp, sizeof(tmp), "%ld", server->local_cache_timeout);
     gtk_entry_set_text(GTK_ENTRY(entry), tmp);
     gtk_widget_show(entry);
     g_signal_connect(entry, "activate",
			G_CALLBACK(server_edit_callback), cb_data);
     gtk_table_attach(GTK_TABLE(table2), entry, 1, 2, y, y + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
     y++;

     gtk_tooltips_set_tip(tips, entry,
			  _("Should the OpenLDAP client-side cache be used? "
			    "And what is its timeout? Anything greater than "
			    "-1 turns on the cache."),
			  Q_("tooltip|Using this might speed up LDAP "
			     "operations, but it may also lead to slightly "
			     "out-of-date data.")
			  );
     gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);

#endif

     /* Ask password on first connect */
     button = gq_check_button_new_with_label(_("_Ask password on first connect"));
     sw->ask_pw = button;
     g_signal_connect(button, "toggled",
			G_CALLBACK(ask_pw_toggled), sw);

     gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), server->ask_pw);

#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_table_attach(GTK_TABLE(table3), button, 0, 1, z, z + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_tooltips_set_tip(tips, button,
			  _("Should the application ask for a bind password?"),
			  Q_("tooltip|This disables the password entered via "
			     "the preferences dialog. ")
			  );

     /* Hide internal attributes */
     button = gq_check_button_new_with_label(_("_Hide internal attributes"));
     sw->hide_internal = button;
     if(server->hide_internal)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_table_attach(GTK_TABLE(table3), button, 1, 2, z, z + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
     z++;
     gtk_tooltips_set_tip(tips, button,
			  _("Do not show attributes internal to the LDAP "
			    "server"),
			  Q_("tooltip|At least OpenLDAP allows to view "
			     "several "
			     "interesting attributes. Setting this option "
			     "turns them off.")
			  );

     /* Cache connections */
     button = gq_check_button_new_with_label(_("Cach_e connection"));
     sw->cacheconn = button;
     if(server->cacheconn)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_table_attach(GTK_TABLE(table3), button, 0, 1, z, z + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);

     gtk_tooltips_set_tip(tips, button,
			  _("If set: Do not close the connection between "
			    "LDAP operations"),
			  Q_("tooltip|Setting this may speed up LDAP "
			     "operations, as it does not require the overhead "
			     "to open a new "
			     "connection to the server for every operation. "
			     "OTOH it might put additional stress on the "
			     "server (depends on what you call 'stress')")
			  );

     z++;

     /* Enable TLS */
     button = gq_check_button_new_with_label(_("Enable _TLS"));
     sw->enabletls = button;
     if(server->enabletls)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_table_attach(GTK_TABLE(table3), button, 0, 1, z, z + 1,
		      GTK_EXPAND | GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
     z++;

     gtk_tooltips_set_tip(tips, button,
			  _("Should we use Transport Layer Security?"),
			  Q_("tooltip|Almost but not quite entirely SSL.")
			  );

     /* OK and Cancel buttons */
     hbox = gtk_hbutton_box_new(); /* FALSE, 13); */
/*       gtk_container_border_width(GTK_CONTAINER(hbox), 10); */
     gtk_widget_show(hbox);
     gtk_box_pack_start(GTK_BOX(main_vbox), hbox, FALSE, TRUE, 10);


     okbutton = gtk_button_new_from_stock(GTK_STOCK_OK);

     gtk_widget_show(okbutton);
     g_signal_connect(okbutton, "clicked",
			G_CALLBACK(server_edit_callback),
			cb_data);
     gtk_box_pack_start(GTK_BOX(hbox), okbutton, FALSE, TRUE, 10);
     GTK_WIDGET_SET_FLAGS(okbutton, GTK_CAN_DEFAULT);
     gtk_widget_grab_default(okbutton);

     cancelbutton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);

     gtk_widget_show(cancelbutton);
     g_signal_connect_swapped(cancelbutton, "clicked",
			       G_CALLBACK(gtk_widget_destroy),
			       editwindow);
     g_signal_connect(editwindow, "key_press_event",
			G_CALLBACK(destroy_edit_server_window_on_esc),
			sw);
     g_signal_connect(editwindow, "destroy",
			G_CALLBACK(destroy_edit_server_window),
			sw);


     gtk_box_pack_end(GTK_BOX(hbox), cancelbutton, FALSE, TRUE, 10);

     gtk_widget_show(editwindow);

     statusbar_msg(_("Server properties window opened for server '%s'"),
		   server->name);
}


static void
serverstab_deletebutton_callback(GtkWidget*            widget,
				 struct prefs_windata* pw)
{
	GtkWidget *clist = pw->serverstab_server_clist;
	gint row = GPOINTER_TO_INT(gtk_object_get_data(GTK_OBJECT(clist), "selected-row"));
	GqServer *server = gtk_clist_get_row_data(GTK_CLIST(clist), row);

	if (server) {
	       GQServerList* list = gq_server_list_get();

	       g_object_ref(server);
	       gq_server_list_remove(list, server);

	       if (save_config(widget)) {
		    /* unref server due to removal of the server from
		       the list of servers */
		    g_object_unref(server);

		    gtk_object_remove_data(GTK_OBJECT(clist),
					   "selected-row");

		    fill_serverlist_serverstab();
		    update_serverlist(&mainwin);
	       } else {
		    /* undo changes */
		    gq_server_list_add(list, server);
	       }
	       g_object_unref(server);
	}
}

static void serverstab_newbutton_callback(GtkWidget *widget, 
					  struct prefs_windata *pw)
{
	create_edit_server_window(NULL, pw->prefswindow);
}


static void serverstab_editbutton_callback(GtkWidget *widget, 
					   struct prefs_windata *pw)
{
     GtkWidget *clist = pw->serverstab_server_clist;
     void *data = gtk_object_get_data(GTK_OBJECT(clist),
				      "selected-row");

     gint row = GPOINTER_TO_INT(data);
     GqServer *server = gtk_clist_get_row_data(GTK_CLIST(clist),
							row);
     /* quietly ignore editbutton if no server selected */
     if(server) create_edit_server_window(server, pw->prefswindow);
}


static void
server_selected_callback(GtkWidget*            clist,
			 gint                  row,
			 gint                  column,
			 GdkEventButton*       event, 
			 struct prefs_windata* pw)
{
	gtk_object_set_data(GTK_OBJECT(clist), 
			    "selected-row", GINT_TO_POINTER(row));

	if(event && event->type == GDK_2BUTTON_PRESS) {
		GqServer *server = GQ_SERVER(gtk_clist_get_row_data(GTK_CLIST(clist), row));
		create_edit_server_window(server, pw->prefswindow);
	}
}


static void server_unselected_callback(GtkWidget *clist, gint row, gint column,
				       GdkEventButton *event, gpointer data)
{
     gtk_object_remove_data(GTK_OBJECT(clist), "selected-row");
}

static void
add_single_server(GQServerList* list,
		  GqServer*     server,
		  gpointer      user_data)
{
	gpointer * clist_and_row = user_data;
	GtkWidget* clist = clist_and_row[0];
	gint     * row   = clist_and_row[1];
	gint       rrow;

	rrow = gtk_clist_append(GTK_CLIST(clist), &server->name);
	gtk_clist_set_row_data_full(GTK_CLIST(clist), *row,
				    g_object_ref(server), g_object_unref);
	(*row)++;
}

void fill_serverlist_serverstab(void)
{
     GtkWidget *clist = current_serverstab_serverclist;
     gint row = 0;
     gpointer clist_and_row[2] = {
	     clist,
	     &row
     };

     if(!GTK_IS_CLIST(clist))
	  return;

     gtk_clist_freeze(GTK_CLIST(clist));
     gtk_clist_clear(GTK_CLIST(clist));

     gq_server_list_foreach(gq_server_list_get(), add_single_server, clist_and_row);

     gtk_clist_thaw(GTK_CLIST(clist));
}


static void prefs_okbutton_callback(GtkWidget *button,
				    struct prefs_windata *pw)
{
     unsigned int type;
     /* use a dummy configuration to store current/old preferences for
	rollback */
     struct gq_config *save = new_config();

     /* Search type */
     for(type = 0; type < sizeof(pw->search_st)/sizeof(pw->search_st[0]);
	 type++) {
	  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw->search_st[type])))
	       break;
     }
     config->search_argument = type;

     /* saves a lot of typing */
#define CONFIG_TOGGLE_BUTTON(c,p,s,n) { \
	(s)->n = (c)->n; \
	(c)->n = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON((p)->n)); \
	}

     /* WHEN ADDING STUFF: DO NOT FORGET TO CODE ROLLBACK AS WELL */
     /* Show DN */
     CONFIG_TOGGLE_BUTTON(config, pw, save, showdn);

     /* Show RDN only */
     CONFIG_TOGGLE_BUTTON(config, pw, save, show_rdn_only);

     /* Sorting: browse mode */
     CONFIG_TOGGLE_BUTTON(config, pw, save, sort_browse);

     /* browse mode: Use user-friendly attribute names */
     CONFIG_TOGGLE_BUTTON(config, pw, save, browse_use_user_friendly);

     /* browse mode: Use user-friendly attribute names */
     CONFIG_TOGGLE_BUTTON(config, pw, save, browse_use_user_friendly);

     /* GUI */
     /* restore window sizes */
     CONFIG_TOGGLE_BUTTON(config, pw, save, restore_window_sizes);

     /* restore window positions */
     CONFIG_TOGGLE_BUTTON(config, pw, save, restore_window_positions);

     /* restore search history */
     CONFIG_TOGGLE_BUTTON(config, pw, save, restore_search_history);

     /* WHEN ADDING STUFF: DO NOT FORGET TO CODE ROLLBACK AS WELL */
     /* restore tabs */
     CONFIG_TOGGLE_BUTTON(config, pw, save, restore_tabs);

     /* never_leak_credentials */
     CONFIG_TOGGLE_BUTTON(config, pw, save, never_leak_credentials);

     /* do_not_use_ldap_conf */
     CONFIG_TOGGLE_BUTTON(config, pw, save, do_not_use_ldap_conf);

     /* WHEN ADDING STUFF: DO NOT FORGET TO CODE ROLLBACK AS WELL */
     /* Search type */
     for(type = 0; type < sizeof(pw->search_st)/sizeof(pw->search_st[0]);
	 type++) {
	  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw->search_st[type])))
	       break;
     }
     save->search_argument   = config->search_argument;
     config->search_argument = type;

     /* do_not_use_ldap_conf */
     CONFIG_TOGGLE_BUTTON(config, pw, save, do_not_use_ldap_conf);

     /* LDIF: format */
     for(type = 0; type < sizeof(pw->ldif_format)/sizeof(pw->ldif_format[0]);
	 type++) {
	  if(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(pw->ldif_format[type])))
	       break;
     }
     save->ldifformat = config->ldifformat;
     config->ldifformat = type;

     g_free_and_dup(save->schemaserver, config->schemaserver);
     if(pw->schemaserver) {
	  g_free_and_dup(config->schemaserver, 
			 gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(pw->schemaserver)->entry)));
     }

     if (save_config(pw->prefswindow)) {
	  gtk_widget_destroy(pw->prefswindow);
     } else {
/* saves typing */
#define CONFIG_ROLLBACK(o,s,n) (o)->n = (s)->n
	  CONFIG_ROLLBACK(config, save, showdn);
	  CONFIG_ROLLBACK(config, save, showoc);
	  CONFIG_ROLLBACK(config, save, show_rdn_only);
	  CONFIG_ROLLBACK(config, save, sort_search);
	  CONFIG_ROLLBACK(config, save, sort_browse);
	  CONFIG_ROLLBACK(config, save, browse_use_user_friendly);
	  CONFIG_ROLLBACK(config, save, restore_window_sizes);
	  CONFIG_ROLLBACK(config, save, restore_window_positions);
	  CONFIG_ROLLBACK(config, save, restore_search_history);
	  CONFIG_ROLLBACK(config, save, restore_tabs);
	  CONFIG_ROLLBACK(config, save, never_leak_credentials);
	  CONFIG_ROLLBACK(config, save, do_not_use_ldap_conf);

	  CONFIG_ROLLBACK(config, save, search_argument);
	  CONFIG_ROLLBACK(config, save, ldifformat);
	  g_free_and_dup(config->schemaserver, save->schemaserver);
     }

     free_config(save);
}

static void destroy_prefswindow(GtkWidget *window, 
				struct prefs_windata *pw) {
     g_assert(pw);
     g_assert(window == prefswindow);

     prefswindow = NULL;
     g_free(pw);
}


void create_prefs_window(struct mainwin_data *win)
{
     GtkWidget *label, *vbox2;
     GtkWidget *notebook;
     GtkWidget *vbox_search_options, *vbox_browse_options;
     GtkWidget *vbox_servers, *vbox_templates, *vbox_ldif, *vbox_gui;
     GtkWidget *vbox_sec;
     GtkWidget *hbox_buttons, *okbutton, *cancelbutton;

     struct prefs_windata *pw = NULL;

     if (prefswindow) {
	  gtk_window_present(GTK_WINDOW(prefswindow));
	  return;
     }

     pw = g_malloc0(sizeof(struct prefs_windata));

/*      prefswindow = gtk_window_new(GTK_WINDOW_TOPLEVEL); */
/*      gtk_widget_set_usize(prefswindow, 520, 470); */

     prefswindow = stateful_gtk_window_new(GTK_WINDOW_TOPLEVEL,
					   "prefswindow", 520, 470); 
     pw->prefswindow = prefswindow;

     g_assert(win);

     gtk_window_set_modal(GTK_WINDOW(prefswindow), TRUE);
     gtk_window_set_transient_for(GTK_WINDOW(prefswindow), 
				  GTK_WINDOW(win->mainwin));

     gtk_container_border_width(GTK_CONTAINER(prefswindow), 
				CONTAINER_BORDER_WIDTH);
     gtk_window_set_title(GTK_WINDOW(prefswindow), _("Preferences"));
     gtk_window_set_policy(GTK_WINDOW(prefswindow), TRUE, TRUE, FALSE);
     g_signal_connect_swapped(prefswindow, "key_press_event",
			       G_CALLBACK(close_on_esc),
			       prefswindow);

     g_signal_connect(prefswindow, "destroy",
			G_CALLBACK(destroy_prefswindow),
			pw);

     vbox2 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox2);
     gtk_container_add(GTK_CONTAINER(prefswindow), vbox2);

     notebook = gtk_notebook_new();
     gtk_notebook_set_scrollable(GTK_NOTEBOOK(notebook), TRUE);
     gtk_widget_show(notebook);
     gtk_box_pack_start(GTK_BOX(vbox2), notebook, TRUE, TRUE, 0);

     /* Search Options tab */
     vbox_search_options = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox_search_options), 
				CONTAINER_BORDER_WIDTH);
     create_search_optionstab(vbox_search_options, pw);
     gtk_widget_show(vbox_search_options);
     label = gq_label_new(_("Search _Options"));
     gtk_widget_show(label);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_NOTEBOOK(notebook), GTK_CAN_FOCUS);
#endif
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			      vbox_search_options, label);

     /* Browse Options tab */
     vbox_browse_options = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox_browse_options),
				CONTAINER_BORDER_WIDTH);
     create_browse_optionstab(vbox_browse_options, pw);
     gtk_widget_show(vbox_browse_options);
     label = gq_label_new(_("Browse O_ptions"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook),
			      vbox_browse_options, label);

     /* Servers tab */
     vbox_servers = gtk_vbox_new(FALSE, 0);
     create_serverstab(vbox_servers, pw);
     gtk_widget_show(vbox_servers);
     label = gq_label_new(_("_Servers"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_servers, label);

#ifdef HAVE_LDAP_STR2OBJECTCLASS
     /* Templates tab */
     vbox_templates = gtk_vbox_new(FALSE, 0);
     create_templatestab(vbox_templates, pw);
     gtk_widget_show(vbox_templates);
     label = gq_label_new(_("_Templates"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_templates, label);
#else
     vbox_templates = NULL;
#endif

     /* LDIF tab */
     vbox_ldif = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox_ldif),
				CONTAINER_BORDER_WIDTH);
     create_ldiftab(vbox_ldif, pw);
     gtk_widget_show(vbox_ldif);
     label = gq_label_new(_("_LDIF"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_ldif, label);


     /* GUI tab */
     vbox_gui = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox_gui),
				CONTAINER_BORDER_WIDTH);
     create_guitab(vbox_gui, pw);
     gtk_widget_show(vbox_gui);
     label = gq_label_new(_("_GUI"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_gui, label);

     /* Security tab */
     vbox_sec = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox_sec),
				CONTAINER_BORDER_WIDTH);
     create_security_tab(vbox_sec, pw);
     gtk_widget_show(vbox_sec);
     label = gq_label_new(_("Securit_y"));
     gtk_widget_show(label);
     gtk_notebook_append_page(GTK_NOTEBOOK(notebook), vbox_sec, label);

     /* OK and Cancel buttons outside notebook */
     hbox_buttons = gtk_hbutton_box_new(); /* FALSE, 0); */
     gtk_widget_show(hbox_buttons);
     gtk_box_pack_start(GTK_BOX(vbox2), hbox_buttons, FALSE, TRUE, 10);
/*       gtk_container_border_width(GTK_CONTAINER(hbox_buttons), 10); */

     okbutton = gtk_button_new_from_stock(GTK_STOCK_OK);
     gtk_widget_show(okbutton);
     g_signal_connect(okbutton, "clicked",
			G_CALLBACK(prefs_okbutton_callback),
			pw);
     gtk_box_pack_start(GTK_BOX(hbox_buttons), okbutton, FALSE, TRUE, 0);
     GTK_WIDGET_SET_FLAGS(okbutton, GTK_CAN_DEFAULT);
     gtk_widget_grab_focus(okbutton);
     gtk_widget_grab_default(okbutton);

     cancelbutton = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
     gtk_widget_show(cancelbutton);
     gtk_box_pack_end(GTK_BOX(hbox_buttons), cancelbutton, FALSE, TRUE, 0);
     g_signal_connect_swapped(cancelbutton, "clicked",
			       G_CALLBACK(gtk_widget_destroy),
			       GTK_OBJECT(prefswindow));

     gtk_widget_show(prefswindow);

     statusbar_msg(_("Preferences window opened"));
}


void create_serverstab(GtkWidget *target, struct prefs_windata *pw)
{
     GtkWidget *vbox0, *vbox1, *vbox2, *hbox1, *hbox2, *scrwin;
     GtkWidget *button_new, *button_edit, *button_delete;
     GtkWidget *server_clist;

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_container_add(GTK_CONTAINER(target), vbox1);
     gtk_container_border_width(GTK_CONTAINER(vbox1),
				CONTAINER_BORDER_WIDTH);

     hbox2 = gtk_hbox_new(FALSE, 25);
     gtk_widget_show(hbox2);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox2, TRUE, TRUE, 0);

     /* scrolled window to hold the server clist */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
     gtk_box_pack_start(GTK_BOX(hbox2), scrwin, TRUE, TRUE, 0);

     server_clist = gtk_clist_new(1);
     pw->serverstab_server_clist = server_clist;

#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(server_clist, GTK_CAN_FOCUS);
#endif
     current_serverstab_serverclist = server_clist;
     gtk_widget_set_usize(scrwin, 200, 300);
     gtk_clist_set_selection_mode(GTK_CLIST(server_clist),
				  GTK_SELECTION_SINGLE);
     gtk_clist_set_shadow_type(GTK_CLIST(server_clist), GTK_SHADOW_ETCHED_IN);
     gtk_widget_show(server_clist);

     g_signal_connect(server_clist, "select_row",
			G_CALLBACK(server_selected_callback), pw);
     g_signal_connect(server_clist, "unselect_row",
			G_CALLBACK(server_unselected_callback), NULL);

     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrwin),
					   server_clist);
     fill_serverlist_serverstab();

     vbox0 = gtk_vbox_new(FALSE, 10);
     gtk_widget_show(vbox0);
     gtk_box_pack_start(GTK_BOX(hbox2), vbox0, FALSE, FALSE, 0);

     vbox2 = gtk_vbutton_box_new(); /*FALSE, 10); */
     gtk_widget_show(vbox2);
     gtk_box_pack_start(GTK_BOX(vbox0), vbox2, FALSE, FALSE, 0);

     /* New button */

     button_new = gtk_button_new_from_stock(GTK_STOCK_NEW);

#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(button_new, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button_new);
     g_signal_connect(button_new, "clicked",
			G_CALLBACK(serverstab_newbutton_callback),
			pw);
     gtk_box_pack_start(GTK_BOX(vbox2), button_new, FALSE, TRUE, 0);

     /* Edit button */
     button_edit = gq_button_new_with_label(_("_Edit"));
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(button_edit, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button_edit);
     g_signal_connect(button_edit, "clicked",
			G_CALLBACK(serverstab_editbutton_callback),
			pw);
     gtk_box_pack_start(GTK_BOX(vbox2), button_edit, FALSE, TRUE, 0);

     /* Delete button */
     button_delete = gtk_button_new_from_stock(GTK_STOCK_DELETE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(button_delete, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button_delete);
     g_signal_connect(button_delete, "clicked",
			G_CALLBACK(serverstab_deletebutton_callback),
			pw);
     gtk_box_pack_start(GTK_BOX(vbox2), button_delete, FALSE, TRUE, 0);

     hbox1 = gtk_hbox_new(FALSE, 10);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, TRUE, 0);
     gtk_container_border_width(GTK_CONTAINER(hbox1),
				CONTAINER_BORDER_WIDTH);
}


#ifdef HAVE_LDAP_STR2OBJECTCLASS
void create_templatestab(GtkWidget *target, struct prefs_windata *pw)
{
     GtkWidget *vbox0, *vbox1, *vbox2, *hbox1, *hbox2, *scrwin;
     GtkWidget *button_new, *button_edit, *button_delete;
     GtkWidget *template_clist, *label, *combo;

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_widget_show(vbox1);
     gtk_container_add(GTK_CONTAINER(target), vbox1);
     gtk_container_border_width(GTK_CONTAINER(vbox1),
				CONTAINER_BORDER_WIDTH);

     hbox1 = gtk_hbox_new(FALSE, 10);
     gtk_widget_show(hbox1);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);


     /* Schema server */
     label = gq_label_new(_("Last _resort schema server"));
     gtk_widget_show(label);
     gtk_box_pack_start(GTK_BOX(hbox1), label, FALSE, FALSE, 0);

     combo = gtk_combo_new();
     pw->schemaserver = combo;
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_COMBO(combo)->entry, GTK_CAN_FOCUS);
#endif
     fill_serverlist_combo(combo);
     if(strlen(config->schemaserver))
	gtk_entry_set_text(GTK_ENTRY(GTK_COMBO(combo)->entry),
			   config->schemaserver);
     gtk_widget_show(combo);
     gtk_box_pack_start(GTK_BOX(hbox1), combo, FALSE, FALSE, 0);

     hbox2 = gtk_hbox_new(FALSE, 25);
     gtk_widget_show(hbox2);
     gtk_box_pack_start(GTK_BOX(vbox1), hbox2, TRUE, TRUE, 10);

     /* scrolled window to hold the server clist */
     scrwin = gtk_scrolled_window_new(NULL, NULL);
     gtk_widget_set_usize(scrwin, 200, 300);
     gtk_widget_show(scrwin);
     gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrwin),
				    GTK_POLICY_AUTOMATIC,
				    GTK_POLICY_AUTOMATIC);
     gtk_box_pack_start(GTK_BOX(hbox2), scrwin, TRUE, TRUE, 0);

     template_clist = gtk_clist_new(1);
     current_template_clist = template_clist;

     pw->templatelist = template_clist;
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(template_clist, GTK_CAN_FOCUS);
#endif
     gtk_clist_set_selection_mode(GTK_CLIST(template_clist),
				  GTK_SELECTION_SINGLE);
     gtk_clist_set_shadow_type(GTK_CLIST(template_clist),
			       GTK_SHADOW_ETCHED_IN);
     g_signal_connect(template_clist, "select_row",
			G_CALLBACK(template_selected_callback), pw);
     gtk_widget_show(template_clist);
     gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(scrwin),
					   template_clist);

     fill_clist_templates(template_clist);

     vbox0 = gtk_vbox_new(FALSE, 10);
     gtk_widget_show(vbox0);
     gtk_box_pack_start(GTK_BOX(hbox2), vbox0, FALSE, FALSE, 0);

     vbox2 = gtk_vbutton_box_new(); /*FALSE, 10); */
     gtk_widget_show(vbox2);
     gtk_box_pack_start(GTK_BOX(vbox0), vbox2, FALSE, FALSE, 0);

     /* New button */
     button_new = gtk_button_new_from_stock(GTK_STOCK_NEW);

#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(button_new, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button_new);
     g_signal_connect(button_new, "clicked",
			G_CALLBACK(template_new_callback),
			pw);
     gtk_box_pack_start(GTK_BOX(vbox2), button_new, FALSE, TRUE, 0);

     /* Edit button */
     button_edit = gq_button_new_with_label(_("_Edit"));
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(button_edit, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button_edit);
     g_signal_connect(button_edit, "clicked",
			G_CALLBACK(template_edit_callback),
			pw);
     gtk_box_pack_start(GTK_BOX(vbox2), button_edit, FALSE, TRUE, 0);

     /* Delete button */
     button_delete = gtk_button_new_from_stock(GTK_STOCK_DELETE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(button_delete, GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button_delete);
     g_signal_connect(button_delete, "clicked",
			G_CALLBACK(template_delete_callback),
			pw);
     gtk_box_pack_start(GTK_BOX(vbox2), button_delete, FALSE, TRUE, 0);
}

#endif     

void create_search_optionstab(GtkWidget *target, struct prefs_windata *pw)
{
     GtkWidget *stframe, *vbox_st, *viewframe, *vbox_view;
     GtkWidget *stradiobutton, *dnbutton; /* , *ocbutton; */
     GtkWidget *hbox_options;
/* , *sort_search_button; */
     GSList *stgroup;

     hbox_options = gtk_hbox_new(TRUE, 10);
     gtk_widget_show(hbox_options);
     gtk_box_pack_start(GTK_BOX(target), hbox_options, FALSE, TRUE, 5);

     /* Search type frame in Options tab */
     stframe = gtk_frame_new(_("Search type"));
     gtk_widget_show(stframe);
     gtk_box_pack_start(GTK_BOX(hbox_options), stframe, FALSE, TRUE, 0);
     vbox_st = gtk_vbox_new(TRUE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox_st),
				CONTAINER_BORDER_WIDTH);
     gtk_widget_show(vbox_st);
     gtk_container_add(GTK_CONTAINER(stframe), vbox_st);

     stradiobutton = gq_radio_button_new_with_label(NULL, _("_Begins with"));
     if(config->search_argument == SEARCHARG_BEGINS_WITH)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(stradiobutton), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_RADIO_BUTTON(stradiobutton), GTK_CAN_FOCUS);
#endif
     pw->search_st[0] = stradiobutton;
     gtk_box_pack_start(GTK_BOX(vbox_st), stradiobutton, TRUE, TRUE, 3);
     gtk_widget_show(stradiobutton);

     stgroup = gtk_radio_button_group(GTK_RADIO_BUTTON(stradiobutton));
     stradiobutton = gq_radio_button_new_with_label(stgroup, _("_Ends with"));
     if(config->search_argument == SEARCHARG_ENDS_WITH)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(stradiobutton), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_RADIO_BUTTON(stradiobutton), GTK_CAN_FOCUS);
#endif
     pw->search_st[1] = stradiobutton;
     gtk_box_pack_start(GTK_BOX(vbox_st), stradiobutton, TRUE, TRUE, 3);
     gtk_widget_show(stradiobutton);

     stgroup = gtk_radio_button_group(GTK_RADIO_BUTTON(stradiobutton));
     stradiobutton = gq_radio_button_new_with_label(stgroup, _("_Contains"));
     if(config->search_argument == SEARCHARG_CONTAINS)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(stradiobutton), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_RADIO_BUTTON(stradiobutton), GTK_CAN_FOCUS);
#endif
     pw->search_st[2] = stradiobutton;
     gtk_box_pack_start(GTK_BOX(vbox_st), stradiobutton, TRUE, TRUE, 3);
     gtk_widget_show(stradiobutton);

     stgroup = gtk_radio_button_group(GTK_RADIO_BUTTON(stradiobutton));
     stradiobutton = gq_radio_button_new_with_label(stgroup, _("E_quals"));
     if(config->search_argument == SEARCHARG_EQUALS)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(stradiobutton), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_RADIO_BUTTON(stradiobutton), GTK_CAN_FOCUS);
#endif
     pw->search_st[3] = stradiobutton;
     gtk_box_pack_start(GTK_BOX(vbox_st), stradiobutton, TRUE, TRUE, 3);
     gtk_widget_show(stradiobutton);

     /* View frame in Options tab */
     viewframe = gtk_frame_new(_("View"));
     gtk_widget_show(viewframe);
     gtk_box_pack_start(GTK_BOX(target), viewframe, FALSE, TRUE, 5);
     vbox_view = gtk_vbox_new(TRUE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox_view),
				CONTAINER_BORDER_WIDTH);
     gtk_widget_show(vbox_view);
     gtk_container_add(GTK_CONTAINER(viewframe), vbox_view);

     /* Show Distinguished Name checkbox */
     dnbutton = gq_check_button_new_with_label(_("Show _Distinguished Name"));

     pw->showdn = dnbutton;
     if(config->showdn)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(dnbutton), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(dnbutton), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(dnbutton);
     gtk_box_pack_start(GTK_BOX(vbox_view), dnbutton, FALSE, TRUE, 5);
}


void create_browse_optionstab(GtkWidget *target, struct prefs_windata *pw)
{
     GtkWidget *viewframe, *vbox_view;
     GtkWidget *show_rdn_only_button, *sort_browse_button;
     GtkTooltips *tips;

     tips = gtk_tooltips_new();

  /* View frame in Options tab */
     viewframe = gtk_frame_new(_("View"));
     gtk_widget_show(viewframe);
     gtk_box_pack_start(GTK_BOX(target), viewframe, FALSE, TRUE, 5);
     vbox_view = gtk_vbox_new(TRUE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox_view),
				CONTAINER_BORDER_WIDTH);
     gtk_widget_show(vbox_view);
     gtk_container_add(GTK_CONTAINER(viewframe), vbox_view);

  /* show rdn only button */
     show_rdn_only_button = gq_check_button_new_with_label(_("Show Relative _Distinguished Name only"));
     pw->show_rdn_only = show_rdn_only_button;
     if(config->show_rdn_only)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(show_rdn_only_button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(show_rdn_only_button), GTK_CAN_FOCUS);
#endif
     gtk_tooltips_set_tip(tips, show_rdn_only_button,
			  _("If set, only show the most specific part of the "
			    "DN in the object tree."),
			  Q_("tooltip|")
			  );

     gtk_widget_show(show_rdn_only_button);
     gtk_box_pack_start(GTK_BOX(vbox_view), show_rdn_only_button, FALSE, FALSE, 5);

  /* Sort in browse mode button */
     sort_browse_button = gq_check_button_new_with_label(_("Sort _results"));
     pw->sort_browse = sort_browse_button;
     if(config->sort_browse)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(sort_browse_button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(sort_browse_button), GTK_CAN_FOCUS);
#endif
     gtk_tooltips_set_tip(tips, sort_browse_button,
			  _("If set, turns on sorting of entries shown in a "
			    "browse tree. Changing this only has an effect "
			    "for new browse tabs."),
			  Q_("tooltip|")
			  );

     gtk_widget_show(sort_browse_button);
     gtk_box_pack_start(GTK_BOX(vbox_view), sort_browse_button, FALSE, FALSE, 5);


     /* browse_use_user_friendly button */
     pw->browse_use_user_friendly = gq_check_button_new_with_label(_("Use _user friendly attribute names"));

     if(config->browse_use_user_friendly)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(pw->browse_use_user_friendly), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(pw->browse_use_user_friendly),
			    GTK_CAN_FOCUS);
#endif
     gtk_tooltips_set_tip(tips, pw->browse_use_user_friendly,
			  _("If set, turns on to use user-friendly attribute "
			    "names (if configured) in browse mode."),
			  Q_("tooltip|")
			  );

     gtk_widget_show(pw->browse_use_user_friendly);
     gtk_box_pack_start(GTK_BOX(vbox_view), pw->browse_use_user_friendly,
			FALSE, FALSE, 5);

}


void create_ldiftab(GtkWidget *target, struct prefs_windata *pw)
{
     GtkWidget *formatframe;
     GtkWidget *formatradio;
     GSList *formatgroup;
     GtkWidget *vbox1;

     /* Format frame */
     formatframe = gtk_frame_new(_("Format"));
     gtk_widget_show(formatframe);
     gtk_box_pack_start(GTK_BOX(target), formatframe, FALSE, TRUE, 5);

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox1),
				CONTAINER_BORDER_WIDTH);
     gtk_container_add(GTK_CONTAINER(formatframe), vbox1);
     gtk_widget_show(vbox1);

     formatradio = gq_radio_button_new_with_label(NULL, _("_UMich/OpenLDAP style (no comments/version)"));
     if(config->ldifformat == LDIF_UMICH)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(formatradio), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_RADIO_BUTTON(formatradio), GTK_CAN_FOCUS);
#endif
     pw->ldif_format[0] = formatradio;
     gtk_box_pack_start(GTK_BOX(vbox1), formatradio, TRUE, TRUE, 3);
     gtk_widget_show(formatradio);

     formatgroup = gtk_radio_button_group(GTK_RADIO_BUTTON(formatradio));
     formatradio = gq_radio_button_new_with_label(formatgroup, _("LDIF Version _1 (RFC2849)"));
     if(config->ldifformat == LDIF_V1)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(formatradio), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_RADIO_BUTTON(formatradio), GTK_CAN_FOCUS);
#endif
     pw->ldif_format[1] = formatradio;
     gtk_box_pack_start(GTK_BOX(vbox1), formatradio, TRUE, TRUE, 3);
     gtk_widget_show(formatradio);

}


static void create_guitab(GtkWidget *target, struct prefs_windata *pw)
{
     GtkWidget *persistframe;
     GtkWidget *vbox1, *button;
     GtkTooltips *tips;

     tips = gtk_tooltips_new();

     /* Persistency frame */
     persistframe = gtk_frame_new(_("Persistency"));
     gtk_widget_show(persistframe);
     gtk_box_pack_start(GTK_BOX(target), persistframe, FALSE, TRUE, 5);

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox1),
				CONTAINER_BORDER_WIDTH);
     gtk_container_add(GTK_CONTAINER(persistframe), vbox1);
     gtk_widget_show(vbox1);

     /* Restore Window Sizes checkbox */
     button = gq_check_button_new_with_label(_("Restore Window Si_zes"));
     pw->restore_window_sizes = button;

     if(config->restore_window_sizes)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, TRUE, 5);

     gtk_tooltips_set_tip(tips, button,
			  _("Turn on if the sizes of some windows should be "
			    "saved and restored across program invocations."),
			  Q_("tooltip|")
			  );

     /* Restore Window Positions checkbox */
     button = gq_check_button_new_with_label(_("Restore Window Pos_itions"));
     pw->restore_window_positions = button;

     if(config->restore_window_positions)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, TRUE, 5);

     gtk_tooltips_set_tip(tips, button,
			  _("If turned on, the program will try to save and "
			    "restore the on-screen position of some windows "
			    "across program invocations. This will not work "
			    "with certain window managers."),
			  Q_("tooltip|")
			  );

     /* Restore Search History checkbox */
     button = gq_check_button_new_with_label(_("Restore Search _History"));
     pw->restore_search_history = button;

     if(config->restore_search_history)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, TRUE, 5);

     gtk_tooltips_set_tip(tips, button,
			  _("If set then save and restore the search "
			    "history across program invocations."),
			  Q_("tooltip|")
			  );

     /* Restore Tabs */
     button = gq_check_button_new_with_label(_("Restore Ta_bs"));
     pw->restore_tabs = button;

     if(config->restore_tabs)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, TRUE, 5);
     
     gtk_tooltips_set_tip(tips, button,
			  _("If set then save and restore the state "
			    "of the main window tabs."),
			  Q_("tooltip|")
			  );
}


static void create_security_tab(GtkWidget *target, struct prefs_windata *pw)
{
     GtkWidget *frame;
     GtkWidget *vbox1, *button;
     GtkTooltips *tips;

     tips = gtk_tooltips_new();

     /* Persistency frame */
     frame = gtk_frame_new(_("Security"));
     gtk_widget_show(frame);
     gtk_box_pack_start(GTK_BOX(target), frame, FALSE, TRUE, 5);

     vbox1 = gtk_vbox_new(FALSE, 0);
     gtk_container_border_width(GTK_CONTAINER(vbox1),
				CONTAINER_BORDER_WIDTH);
     gtk_container_add(GTK_CONTAINER(frame), vbox1);
     gtk_widget_show(vbox1);

     /* Restore Window Sizes checkbox */
     button = gq_check_button_new_with_label(_("_Never leak credentials"));
     pw->never_leak_credentials = button;

     if(config->never_leak_credentials)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, TRUE, 5);

     gtk_tooltips_set_tip(tips, button,
			  _("Turn off if you want to use heuristics to find the credentials needed to follow referrals. The problems with these heuristics is that they may leak credential information: If you follow a referral to some untrusted server, then your currently used credentials might get sent to this untrusted server. This might allow an attacker to sniff credentials during transit to or on the untrusted server. If turned on, a referral will always use an anonymous bind."),
			  Q_("tooltip|")
			  );

     /* Do not use ldap.conf ... checkbox */
     button = gq_check_button_new_with_label(_("Do not _use ldap.conf/.ldaprc file"));
     pw->do_not_use_ldap_conf = button;

     if(config->do_not_use_ldap_conf)
	  gtk_toggle_button_set_state(GTK_TOGGLE_BUTTON(button), TRUE);
#ifdef OLD_FOCUS_HANDLING
     GTK_WIDGET_UNSET_FLAGS(GTK_CHECK_BUTTON(button), GTK_CAN_FOCUS);
#endif
     gtk_widget_show(button);
     gtk_box_pack_start(GTK_BOX(vbox1), button, FALSE, TRUE, 5);

     gtk_tooltips_set_tip(tips, button,
			  _("Turn off the standard use of the system-wide ldap.conf configuration file and/or the per-user .ldaprc. This works by setting the environment variable LDAPNOINIT. Note that the this feature only set this variable, but never deletes it. This means that the default behaviour when not selecting this depends on the environment variable being set or not prior to the start of gq. Changing this will only affect future program runs."),
			  Q_("tooltip|")
			  );
}


#ifdef HAVE_LDAP_STR2OBJECTCLASS
void template_new_callback(GtkWidget *widget, struct prefs_windata *pw)
{
     GqServer *server;
     const char *servername;

     if (pw->schemaserver == NULL) return;

     if( (servername = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(pw->schemaserver)->entry))) == NULL)
	  return;

     if( (server = gq_server_list_get_by_name(gq_server_list_get(), servername)) == NULL)
	  return;

     create_template_edit_window(server, NULL, pw->prefswindow);
}


void template_edit_callback(GtkWidget *widget, struct prefs_windata *pw)
{
     GqServer *server;
     const char *servername, *templatename;

     if (pw->schemaserver == NULL)
	  return;

     if( (servername = gtk_entry_get_text(GTK_ENTRY(GTK_COMBO(pw->schemaserver)->entry))) == NULL)
	  return;

     if( (server = gq_server_list_get_by_name(gq_server_list_get(), servername)) == NULL)
	  return;

     if (pw->templatelist == NULL)
	  return;

     if( (templatename = get_clist_selection(pw->templatelist)) == NULL)
	  return;

     create_template_edit_window(server, templatename, pw->prefswindow);

}


void template_selected_callback(GtkWidget *clist, gint row, gint column,
				GdkEventButton *event, struct prefs_windata *data)
{
     if (event) {
	  if(event->type == GDK_2BUTTON_PRESS) {
	       template_edit_callback(NULL, data);
	  }
     }
}


void template_delete_callback(GtkWidget *widget, struct prefs_windata *pw)
{
     GList *list;
     struct gq_template *tmpl;
     char *templatename;

     if( pw->templatelist == NULL)
	  return;

     if( (templatename = get_clist_selection(pw->templatelist)) == NULL)
	  return;

     if( (tmpl = find_template_by_name(templatename))) {
	  int index = g_list_index(config->templates, tmpl);
	  config->templates = g_list_remove(config->templates, tmpl);

	  if (save_config(widget)) {
	       if( (list = GTK_CLIST(pw->templatelist)->selection))
		    gtk_clist_remove(GTK_CLIST(pw->templatelist), 
				     GPOINTER_TO_INT(list->data));
	  } else {
	       /* save failed - undo changes */
	       config->templates = g_list_insert(config->templates, tmpl, index);
	  }
     }
}
#endif


/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
