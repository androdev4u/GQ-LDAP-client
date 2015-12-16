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

/* $Id: errorchain.h 895 2006-05-18 13:54:26Z herzi $ */

#ifndef GQ_ERRORCHAIN_H_INCLUDED
#define GQ_ERRORCHAIN_H_INCLUDED

#include <ldap.h>		/* LDAP */
#include <glib.h>		/* G_GNUC_PRINTF */
#include <gtk/gtkwidget.h>

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

int error_new_context(const char *title, GtkWidget *modalFor);
void error_push_production(int context, const char *msg, ...)
     G_GNUC_PRINTF(2, 3);

#ifdef DEBUG
void error_push_debug(const char *file, int line, int context, const char *msg, ...);
#  define error_push(c, ...) error_push_debug(__FILE__, __LINE__, (c), __VA_ARGS__)

#else
#  define error_push(c, ...) error_push_production((c), __VA_ARGS__)
#endif

/* flush the chain for the given context. Optionally make the error
   window modal/transient for the Widget passed to error_new_context.

   NOTE: Making the error window modal is "required" for errors
   originating from a window that is itself modal. Otherwise it would
   not be possible to "OK" errors as long as the modal window is still
   visible. Users would not understand this.

   The Widget can be any widget below the window to make the error
   modal for.
*/
void error_flush(int context);
void error_popup(char *title, char *message, GtkWidget *transient_for);
void error_clear(int context);

void push_ldap_addl_error(LDAP *ld, int context);

struct errchain {
     int context;
     char *title;
     GList *messages;
     GtkWidget *transient_for;
};

#endif

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
