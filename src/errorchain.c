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

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include <stdio.h>
#include <string.h>

#include "errorchain.h"
#include "common.h"
#include "util.h"
#include "debug.h"
#include "input.h"
#include "encode.h"
#include "mainwin.h"		/* message_log_append */

static struct errchain *error_chain_by_context(int q);

static GList *chains = NULL;
static int error_context = 0;

static struct errchain *new_errchain()
{
     struct errchain *new_chain;
     new_chain = g_malloc0(sizeof(struct errchain));
     new_chain->title = g_strdup("");
     new_chain->messages = NULL;
     return new_chain;
}

static void free_errchain(struct errchain *chain)
{
     if (chain) {
	  g_free(chain->title);
	  g_list_foreach(chain->messages, (GFunc) g_free, NULL);
	  g_list_free(chain->messages);
	  if (chain->transient_for) {
	       gtk_widget_unref(chain->transient_for);
	  }
	  g_free(chain);
     }
}

int error_new_context(const char *title, GtkWidget *modal_for)
{
     struct errchain *new_chain;

     new_chain = new_errchain();
     g_free_and_dup(new_chain->title, title);

     chains = g_list_append(chains, new_chain);
     new_chain->context = error_context++;

     if (modal_for) {
	  modal_for = gtk_widget_get_toplevel(modal_for);
     }
     
     new_chain->transient_for = modal_for;
     if (modal_for) gtk_widget_ref(modal_for);

     return new_chain->context;
}



static void error_push_production_v(int context, const char *fmt, va_list ap)
{
     struct errchain *chain;
     GString *str;
     int n, a, retry = 0;
     a = strlen(fmt) + 50;
     str = g_string_sized_new(a);  /* used for glib1 compatibility */

     /* I hope it is ok to repeatedly use ap like this */
     do {
	  n = g_vsnprintf(str->str, a - 1, fmt, ap);

	  retry = 0;
	  /* await both n==-1 and n > a -1 for not enough free space */
	  if (n > a - 1) {
	       g_string_free(str, TRUE);
	       a = n + 2;
	       str = g_string_sized_new(a);
	       retry = 1;
	  } else if (n == -1) {
	       g_string_free(str, TRUE);
	       a *= 2;
	       str = g_string_sized_new(a);
	       retry = 1;
	  }
     } while (retry);

     /* plug into messagechain */
     chain = error_chain_by_context(context);
     chain->messages = g_list_append(chain->messages, str->str);

     message_log_append(str->str);

     g_string_free(str, FALSE);
}

void error_push_production(int context, const char *fmt, ...)
{
     va_list ap;
     va_start(ap, fmt);
     error_push_production_v(context, fmt, ap);
     va_end(ap);
}

#ifdef DEBUG
void error_push_debug(const char *file, int line, 
		      int context, const char *fmt, ...)
{
     va_list ap;
     va_start(ap, fmt);

     if (debug & GQ_DEBUG_ERROR_LINE) {
	  GString *s = g_string_sized_new(200);
	  g_string_sprintf(s, "%s:%d %s", file, line, fmt);

	  if (debug & GQ_DEBUG_ERROR_TRACE) {
	       g_string_append(s, "\n*** TRACE ***:\n");
	       sprint_trace(s);
	  }

	  /* Is it allowed to change the fmt? */
	  error_push_production_v(context, s->str, ap);
	  g_string_free(s, TRUE);
     } else {
	  error_push_production_v(context, fmt, ap);
     }

     va_end(ap);
}
#endif

/*
 * Check LDAP connection for additional error information and append
 * it to the error context if such information is available.
 */
void push_ldap_addl_error(LDAP *ld, int context)
{
     char *error_msg;

     ldap_get_option(ld, LDAP_OPT_ERROR_STRING, &error_msg);
     if (error_msg != NULL && *error_msg != 0) {
	  error_push(context, _("Additional error: %s"), error_msg);
     }
}

/* returns chain for requested context */
static struct errchain *error_chain_by_context(int q)
{
     GList *I;
     struct errchain *chain;

     for (I = chains ; I ; I = g_list_next(I)) {
	  chain = I->data;
	  if (chain->context == q) return chain;
     }
     fprintf(stderr, _("Oops! errorchain lookup error. Exiting...\n"));
     abort();
     return NULL; /* make sure the compiler shuts up */
}

static void error_free(int context)
{
     struct errchain *chain;

     chain = error_chain_by_context(context);
     g_assert(chain);

     chains = g_list_remove(chains, chain);
     
     free_errchain(chain);
}

void error_clear(int context)
{
     struct errchain *chain = error_chain_by_context(context);
     g_assert(chain);

     g_list_foreach(chain->messages, (GFunc) g_free, NULL);
     g_list_free(chain->messages);
     chain->messages = NULL;
}

void error_flush(int context)
{
     GtkWidget *pixmap, *popupwin, *vbox, *vbox1, *hbox0, *hbox, *vbox2, *msg_label, *okbutton, *align;
     struct errchain *chain;
     GList *msg;

     chain = error_chain_by_context(context);
     g_assert(chain);

     if(chain->messages) {
	  popupwin = gtk_dialog_new();
	  if (chain->transient_for &&
	      GTK_WIDGET_TOPLEVEL(chain->transient_for)) {
	       gtk_window_set_modal(GTK_WINDOW(popupwin), TRUE);
	       gtk_window_set_transient_for(GTK_WINDOW(popupwin),
					    GTK_WINDOW(chain->transient_for));
	  }

	  gtk_widget_realize(popupwin);
	  gtk_window_set_title(GTK_WINDOW(popupwin), chain->title);
	  gtk_window_set_policy(GTK_WINDOW(popupwin), FALSE, FALSE, FALSE);
	  vbox1 = GTK_DIALOG(popupwin)->vbox;

	  gtk_widget_show(vbox1);
	  hbox = gtk_hbox_new(FALSE, 0);
	  gtk_container_border_width(GTK_CONTAINER(hbox),
				     CONTAINER_BORDER_WIDTH);

	  gtk_widget_show(hbox);
	  gtk_box_pack_start(GTK_BOX(vbox1), hbox, FALSE, FALSE, 0);
	  pixmap = gtk_image_new_from_file(PACKAGE_PREFIX "/share/pixmaps/gq/bomb.xpm");
	  gtk_widget_show(pixmap);
	  gtk_box_pack_start(GTK_BOX(hbox), pixmap, TRUE, TRUE, 10);

	  /* align messages with the error icon. One-line messages
             look better that way... */
	  align = gtk_alignment_new(0.0, 0.5, 0.0, 0.0);
	  gtk_widget_show(align);
	  gtk_box_pack_start(GTK_BOX(hbox), align, FALSE, FALSE, 0);

	  vbox = gtk_vbox_new(FALSE, 0);
	  gtk_widget_show(vbox);
	  gtk_container_add(GTK_CONTAINER(align), vbox);

	  /* show messages, freeing them as we go */
	  for(msg = chain->messages ; msg ; msg = g_list_next(msg)) {
	       char *m = msg->data;

	       msg_label = gtk_label_new(m);

	       gtk_label_set_justify(GTK_LABEL(msg_label), GTK_JUSTIFY_LEFT);
	       gtk_label_set_line_wrap(GTK_LABEL(msg_label), TRUE);
	       gtk_misc_set_alignment(GTK_MISC(msg_label), 0, 0.5);
	       gtk_widget_show(msg_label);
	       gtk_box_pack_start(GTK_BOX(vbox), msg_label, FALSE, FALSE, 0);

	       g_free(msg->data);
	  }
	  g_list_free(chain->messages);
	  chain->messages = NULL;

	  vbox2 = GTK_DIALOG(popupwin)->action_area;
	  gtk_widget_show(vbox2);

	  hbox0 = gtk_hbutton_box_new();
	  gtk_container_border_width(GTK_CONTAINER(hbox0), 0);
	  gtk_box_pack_end(GTK_BOX(vbox2), hbox0, TRUE, FALSE, 0);
	  gtk_widget_show(hbox0);

	  okbutton = gtk_button_new_from_stock(GTK_STOCK_OK);
	  g_signal_connect_swapped(okbutton, "clicked",
				    G_CALLBACK(gtk_widget_destroy),
				    GTK_OBJECT(popupwin));
	  g_signal_connect_swapped(popupwin, "key_press_event",
				    G_CALLBACK(close_on_esc),
				    popupwin);
	  GTK_WIDGET_SET_FLAGS(okbutton, GTK_CAN_DEFAULT);
	  gtk_box_pack_end(GTK_BOX(hbox0), okbutton, TRUE, FALSE, 0);
	  gtk_widget_grab_default(okbutton);
	  gtk_widget_show(okbutton);
	  gtk_widget_show(popupwin);
     }

     error_free(context);
}


void error_popup(char *title, char *message, GtkWidget *transient_for)
{
     int context;

     context = error_new_context(title, transient_for);
     error_push(context, message);
     error_flush(context);

}



/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
