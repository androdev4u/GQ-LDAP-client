/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Parts: Copyright (C) 2002-2003 Peter Stamfest <peter@stamfest.at>

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

/* $Id: browse-export.c 955 2006-08-31 19:15:21Z herzi $ */


#include <glib.h>
#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include <string.h>
#include <errno.h>		/* errno */
#include <stdio.h>		/* FILE */
#include <stdlib.h>		/* free - MUST get rid of malloc/free */

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

#include "common.h"
#include "gq-browser-node-dn.h"
#include "gq-browser-node-reference.h"

#include "search.h"		/* fill_out_search */
#include "template.h"		/* struct gq_template */

#include "browse-dnd.h"		/* copy_entry et al */

#include "configfile.h"		/* config */
#include "errorchain.h"
#include "util.h"
#include "encode.h"

#include "ldif.h"
#include "browse-export.h"

struct export {
     GList *to_export;
     GtkWidget *filesel;
     GtkWidget *transient_for;
};

static struct export *new_export()
{
     struct export *ex = g_malloc0(sizeof(struct export));
     return ex;
}

static void free_export(struct export *ex)
{
     if (ex) {
	  g_list_foreach(ex->to_export, (GFunc) free_dn_on_server, NULL);
	  g_list_free(ex->to_export);
	  ex->to_export = NULL;
	  g_free(ex);
     }
}
    
static void dump_subtree_ok_callback(struct export *ex)
{

     LDAPMessage *res = NULL, *e;
     LDAP *ld = NULL;
     GList *I;
     int num_entries;
     const char *filename;
     FILE *outfile = NULL;
     GString *out = NULL;
     GString *gmessage = NULL;
     size_t written;
     int ctx;
     GqServer *last = NULL;

     out = g_string_sized_new(2048);

     ctx = error_new_context(_("Dump subtree"), ex->transient_for);

     if(g_list_length(ex->to_export) == 0) {
	  error_push(ctx, _("Nothing to dump!"));
	  goto fail;
     }

     set_busycursor();

     /* obtain filename and open file for reading */
     filename = gtk_file_selection_get_filename(GTK_FILE_SELECTION(ex->filesel));

     if( (outfile = fopen(filename, "w")) == NULL) {
	  error_push(ctx, _("Could not open output file '%1$s': %2$s"),
		     filename, strerror(errno));

	  goto fail;
     } else {
	  /* AFAIK, the UMich LDIF format doesn't take comments or a
             version string */
	  if (config->ldifformat != LDIF_UMICH) {
	       g_string_truncate(out, 0);
	       
	       prepend_ldif_header(out, ex->to_export);

	       written = fwrite(out->str, 1, out->len, outfile);
	       if(written != (size_t) out->len) {
		    error_push(ctx, 
			       _("Save to '%3$s' failed: Only %1$d of %2$d bytes written"),
			       written, out->len, filename);
		    goto fail; /* sometimes goto is useful */
	       }
	  }
	  
	  num_entries = 0;
	  gmessage = g_string_sized_new(256);
	  for (I = g_list_first(ex->to_export) ; I ; I = g_list_next(I)) {
	       struct dn_on_server *dos = I->data;

	       LDAPControl ct;
	       LDAPControl *ctrls[2] = { NULL, NULL } ;
	       char *attrs[] = {
		    LDAP_ALL_USER_ATTRIBUTES,
		    "ref",
		    NULL 
	       };
	       int rc;

	       ct.ldctl_oid		= LDAP_CONTROL_MANAGEDSAIT;
	       ct.ldctl_value.bv_val	= NULL;
	       ct.ldctl_value.bv_len	= 0;
	       ct.ldctl_iscritical	= 1;
	       
	       ctrls[0] = &ct;
	  
	       statusbar_msg(_("Search on %s"), (char *) dos->dn);

	       if (last != dos->server) {
		    if (last) {
			 close_connection(last, FALSE);
			 last = NULL;
		    }

		    if( (ld = open_connection(ctx, dos->server)) == NULL) {
			 /* no extra error, open_connection does error
			    reporting itself... */
			 goto fail;
		    }

		    last = dos->server;
	       }

	       rc = ldap_search_ext_s(ld, (char *) dos->dn,
				      dos->flags == LDAP_SCOPE_SUBTREE ? LDAP_SCOPE_SUBTREE : LDAP_SCOPE_BASE, 
				      "(objectClass=*)", 
				      attrs,
				      0, 
				      ctrls,		/* serverctrls */
				      NULL,		/* clientctrls */
				      NULL,		/* timeout */
				      LDAP_NO_LIMIT,	/* sizelimit */
				      &res);

	       if(rc == LDAP_NOT_SUPPORTED) {
		    rc = ldap_search_s(ld, (char *) dos->dn,
				       dos->flags == LDAP_SCOPE_SUBTREE ? LDAP_SCOPE_SUBTREE : LDAP_SCOPE_BASE,
				       "(objectClass=*)",
				       attrs, 0, &res);
	       }

	       if (rc == LDAP_SUCCESS) {
		    for(e = ldap_first_entry(ld, res); e; e = ldap_next_entry(ld, e)) {
			 g_string_truncate(out, 0);
			 ldif_entry_out(out, ld, e, ctx);
			 num_entries++;
			 
			 written = fwrite(out->str, 1, out->len, outfile);
			 if(written != (size_t) out->len) {
			      g_string_sprintf(gmessage,
					       _("%1$d of %2$d bytes written"),
					       written, out->len);
			      error_popup(_("Save failed"), gmessage->str,
					  ex->transient_for);

			      ldap_msgfree(res);
			      goto fail;
			 }

		    }
		    ldap_msgfree(res);
	       } else if (rc == LDAP_SERVER_DOWN) {
		    dos->server->server_down++;
		    
		    error_push(ctx,
			       _("Server '%s' down. Export may be incomplete!"),
			       dos->server->name);
		    push_ldap_addl_error(ld, ctx);
		    goto fail;
	       } else {
		    /* report error */
		    error_push(ctx,
			       _("LDAP error while searching below '%s'."
				 " Export may be incomplete!"),
			       (char *) dos->dn);
		    push_ldap_addl_error(ld, ctx);
		    goto fail;
	       }
	  }

	  statusbar_msg(ngettext("%1$d entry exported to %2$s",
				 "%1$d entries exported to %2$s", num_entries),
			num_entries, filename);
     }

 fail:		/* labels are only good for cleaning up, really */
     if (outfile) fclose(outfile);
     
     set_normalcursor();
     if (out) g_string_free(out, TRUE);
     if (gmessage) g_string_free(gmessage, TRUE);
     if (ld && last) close_connection(last, FALSE);

     gtk_widget_destroy(ex->filesel);

     error_flush(ctx);
}

void export_many(int error_context, GtkWidget *transient_for, GList *to_export)
{
     GtkWidget *filesel;
     struct export *ex = new_export();

     filesel = gtk_file_selection_new(_("Save LDIF"));
     ex->to_export = to_export;
     ex->filesel = filesel;
     ex->transient_for = transient_for;

     gtk_object_set_data_full(GTK_OBJECT(filesel), "export",
			      ex, (GtkDestroyNotify) free_export);

     g_signal_connect_swapped(GTK_FILE_SELECTION(filesel)->ok_button,
			       "clicked",
			       G_CALLBACK(dump_subtree_ok_callback),
			       ex);
     g_signal_connect_swapped(GTK_FILE_SELECTION(filesel)->cancel_button,
			       "clicked",
			       G_CALLBACK(gtk_widget_destroy),
			       GTK_OBJECT(filesel));
     g_signal_connect_swapped(filesel, "key_press_event",
			       G_CALLBACK(close_on_esc),
			       filesel);
     gtk_widget_show(filesel);

}

/*
   Local Variables:
   c-basic-offset: 5
   End:
*/
