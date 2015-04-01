/*
    GQ -- a GTK-based LDAP client is
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 Peter Stamfest
    
    This file is
    Copyright (c) 2003 by Peter Stamfest <peter@stamfest.at>

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

/* $Id: progress.c 924 2006-05-23 17:46:11Z herzi $ */

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

#include <stdio.h>
#include <stdarg.h>

#include <glib/gi18n.h>

#include "common.h"
#include "progress.h"

#include "util.h"
#include "input.h"

static void pbar_cancelled(GtkWidget *button, struct pbar_win *w) 
{
    w->cancelled = TRUE;
    gtk_widget_destroy(w->win);
}

static void pbar_destroyed(GtkWidget *button, struct pbar_win *w) 
{
    w->destroyed = TRUE;
}

struct pbar_win *create_progress_bar_in_window(const char *title)
{
    GtkWidget *popupwin, *vbox1, *vbox2, *pbar, *label;
    GtkWidget *hbox0, *hbox1, *button;

    GtkWidget *image;
    GtkIconSet *iconset;

    struct pbar_win *w = 
	g_malloc0(sizeof(struct pbar_win)); /* FIXME: free me */

    w->win = popupwin = gtk_dialog_new();
/*     gtk_window_set_default_size(popupwin, 500, 50); */
    g_signal_connect(popupwin, "destroy", 
		       G_CALLBACK(pbar_destroyed),
		       w);

    gtk_widget_realize(popupwin);
    gtk_window_set_title(GTK_WINDOW(popupwin), title);
    gtk_window_set_policy(GTK_WINDOW(popupwin), FALSE, FALSE, FALSE);
    vbox1 = GTK_DIALOG(popupwin)->vbox;
    gtk_widget_show(vbox1);

    hbox1 = gtk_hbox_new(FALSE, 0);
    gtk_container_border_width(GTK_CONTAINER(hbox1), 
			       CONTAINER_BORDER_WIDTH);
    
    gtk_widget_show(hbox1);
    gtk_box_pack_start(GTK_BOX(vbox1), hbox1, FALSE, FALSE, 0);

    iconset =  gtk_style_lookup_icon_set(gtk_widget_get_style(popupwin),
					 GTK_STOCK_DIALOG_INFO);
    image = gtk_image_new_from_icon_set(iconset, GTK_ICON_SIZE_DIALOG);
    gtk_widget_show(image);
    gtk_box_pack_start(GTK_BOX(hbox1), image, FALSE, FALSE, 0);

    vbox1 = gtk_vbox_new(FALSE, 0);
    gtk_box_pack_end(GTK_BOX(hbox1), vbox1, TRUE, TRUE, 0);
    gtk_widget_show(vbox1);

    w->pbar = pbar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox1), pbar, FALSE, FALSE, 0);
    gtk_progress_set_activity_mode(GTK_PROGRESS(pbar), TRUE);
    gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(pbar), 0.333333333333);
    gtk_widget_show(pbar);
    
    w->label = label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(vbox1), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    vbox2 = GTK_DIALOG(popupwin)->action_area;
    gtk_widget_show(vbox2);
    
    hbox0 = gtk_hbutton_box_new();
    gtk_container_border_width(GTK_CONTAINER(hbox0), 0);
    gtk_box_pack_end(GTK_BOX(vbox2), hbox0, TRUE, FALSE, 0);
    gtk_widget_show(hbox0);

    button = gtk_button_new_from_stock(GTK_STOCK_CANCEL);
    g_signal_connect(button, "clicked", 
		       G_CALLBACK(pbar_cancelled),
		       w);
    
    GTK_WIDGET_SET_FLAGS(button, GTK_CAN_DEFAULT);
    gtk_box_pack_start(GTK_BOX(hbox0), button, TRUE, TRUE, 0);
    gtk_widget_grab_default(button);
    gtk_widget_show(button);

    gtk_widget_show(popupwin);

    return w;
}

void update_progress(struct pbar_win *w, const char *msg_fmt, ...)
{
    if (!w->destroyed) {
	if (msg_fmt) {
	    char msg[1024];
	    
	    va_list ap;
	    va_start(ap, msg_fmt);
	    
	    g_vsnprintf(msg, sizeof(msg), msg_fmt, ap);

	    va_end(ap);
	    
	    gtk_label_set_text(GTK_LABEL(w->label), msg);
	}
	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(w->pbar));
	while (gtk_events_pending()) {
	    gtk_main_iteration();
	}
    }
}

void free_progress(struct pbar_win *w)
{
    if (!w->destroyed) {
	gtk_widget_destroy(w->win); /* FIXME: first check if destroyed */
    }
    g_free(w);
}
