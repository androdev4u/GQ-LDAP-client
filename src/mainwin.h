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

/* $Id: mainwin.h 960 2006-09-02 20:42:46Z herzi $ */

#ifndef GQ_MAINWIN_H_INCLUDED
#define GQ_MAINWIN_H_INCLUDED

#include <gtk/gtk.h>
#include "common.h"
#include "gq-tab.h"

struct mainwin_data {
     GtkWidget *mainwin;
     GtkWidget *mainbook;
     GtkWidget *statusbar;
     GtkWidget *filtermenu;
     GHashTable *lastofmode;

     /* the message log window */
     GtkWidget *ml_window;
     GtkWidget *ml_text;
     GtkTextBuffer *ml_buffer;
};

extern struct mainwin_data mainwin;

GqTab *get_last_of_mode(int mode);
void go_to_page(GqTab *tab);
void enter_last_of_mode(GqTab *tab);

void fill_serverlist_combo(GtkWidget *combo);
void cleanup(struct mainwin_data *win);
void create_mainwin(struct mainwin_data *);
void mainwin_update_filter_menu(struct mainwin_data *win);

GqTab *new_modetab(struct mainwin_data *, int mode);
void cleanup_all_tabs(struct mainwin_data *win);
void cleanup_tab(GqTab *tab);
void update_serverlist(struct mainwin_data *win);

/* gboolean ctrl_b_hack(GtkWidget *widget, GdkEventKey *event, gpointer obj); */
/* gboolean ctrl_w_hack(GtkWidget *widget, GdkEventKey *event, gpointer obj); */

/* return the GqTab for the n'th tab in the gq main window win */
GqTab *mainwin_get_tab_nth(struct mainwin_data *win, int n);
GqTab *mainwin_get_current_tab(GtkWidget *notebook);

void message_log_append(const char *buf);
void clear_message_history();

#define MESSAGE_LOG_MAX 1000

#endif


/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
