/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (dt_cert.c) is
    Copyright (C) 2002 by Peter Stamfest and Bert Vermeulen

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

/* $Id: dt_cert.c 937 2006-05-25 13:51:47Z herzi $ */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#ifdef HAVE_LIBCRYPTO

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

#include <openssl/bio.h>
#include <openssl/x509.h>
#include <openssl/err.h>
#include <openssl/asn1.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>
#include <openssl/buffer.h>

#include "common.h"
#include "util.h"
#include "formfill.h"
#include "dt_cert.h"

static void dt_cert_fill_clist(struct formfill *form,
			       GtkWidget *hbox,
			       GtkWidget *data_widget,
			       GByteArray *internal,
			       GtkWidget *clist);

static void dt_cert_fill_details(struct formfill *form,
				 GtkWidget *data_widget,
				 GtkWidget *text,
				 GByteArray *internal,
				 GtkWidget *clist);

static void dt_cert_fill_clist(struct formfill *form,
			       GtkWidget *hbox,
			       GtkWidget *data_widget,
			       GByteArray *internal,
			       GtkWidget *clist)
{
     if (internal) {
	  if (internal->len > 0) {
	       BIO *databio;
	       X509 *x = NULL;
	       gboolean changed = FALSE;

	       int rc;
	       databio = BIO_new(BIO_s_mem());

	       rc = BIO_write(databio, internal->data, internal->len);

	       x = d2i_X509_bio(databio, NULL);
	       if (!x) {
		    /* try PEM */
		    BIO_reset(databio);
		    rc = BIO_write(databio, internal->data, internal->len);

		    x = PEM_read_bio_X509(databio, NULL, NULL, NULL);

		    if (x) {
			 single_warning_popup(_("Converted data from PEM to DER encoding"));
			 changed = TRUE;
		    }
	       }
	       if (!x) {
		    /* try PKCS12 - but without a password this is not
                       really useful.... hmmmm*/
		    PKCS12 *p12 = NULL;

		    BIO_reset(databio);
		    rc = BIO_write(databio, internal->data, internal->len);

		    p12 = d2i_PKCS12_bio(databio, NULL);
		    
		    if (p12) {
			 PKCS12_parse(p12, NULL, NULL, &x, NULL);
			 PKCS12_free(p12);
		    }

		    if (x) {
			 single_warning_popup(_("Converted data from PKCS12 to DER encoding"));
			 changed = TRUE;
		    }
	       }

	       if (x && changed) {
		    BIO *bufbio = BIO_new(BIO_s_mem());
		    BUF_MEM *bm;
		    GByteArray *gb;
		    
		    /* convert data into DER form! */
		    i2d_X509_bio(bufbio, x);
		    BIO_get_mem_ptr(bufbio, &bm);
		    
		    gb = g_byte_array_new();
		    g_byte_array_append(gb, (guchar*)bm->data, bm->length);
		    
		    gtk_object_set_data_full(GTK_OBJECT(data_widget), "data", 
					     gb,
					     (GtkDestroyNotify) free_internal_data);

		    BIO_free(bufbio);
		    internal = gb;
	       }

	       if (x) {
		    X509_NAME *n = X509_get_subject_name(x);
		    BIO *bufbio = BIO_new(BIO_s_mem());
		    BUF_MEM *bm;
		    char *cols[3] = { NULL, NULL, NULL };

		    X509_NAME_print(bufbio, n, 0);
		    BIO_write(bufbio, "", 1); /* write NUL byte */

		    BIO_get_mem_ptr(bufbio, &bm);

		    cols[0] = _("Subject");
		    cols[1] = bm->data;
		    gtk_clist_append(GTK_CLIST(clist), cols);

		    BIO_reset(bufbio);
		    n = X509_get_issuer_name(x);

		    X509_NAME_print(bufbio, n, 0);
		    BIO_write(bufbio, "", 1); /* write NUL byte */

		    BIO_get_mem_ptr(bufbio, &bm);

		    cols[0] = _("Issuer");
		    cols[1] = bm->data;
		    gtk_clist_append(GTK_CLIST(clist), cols);

		    BIO_reset(bufbio);
		    ASN1_TIME_print(bufbio, X509_get_notBefore(x));
		    BIO_get_mem_ptr(bufbio, &bm);

		    cols[0] = _("Not Before");
		    cols[1] = bm->data;
		    gtk_clist_append(GTK_CLIST(clist), cols);

		    BIO_reset(bufbio);
		    ASN1_TIME_print(bufbio, X509_get_notAfter(x));
		    BIO_get_mem_ptr(bufbio, &bm);

		    cols[0] = _("Not After");
		    cols[1] = bm->data;
		    gtk_clist_append(GTK_CLIST(clist), cols);

		    BIO_reset(bufbio);
		    /* the OpenSSL guys seem to originally have
		       introduced X509_get_serialNumber in a non
		       standard way. Then they have deleted it
		       entirely... so we do it ourselves... */
		    BIO_printf(bufbio, "%ld", 
			       ASN1_INTEGER_get(x->cert_info->serialNumber));
		    BIO_get_mem_ptr(bufbio, &bm);

		    cols[0] = _("Serial#");
		    cols[1] = bm->data;
		    gtk_clist_append(GTK_CLIST(clist), cols);

		    /* Version:
		     * Version 0x00 actually means Version 1 */
		    BIO_reset(bufbio);
		    BIO_printf(bufbio, "%ld", X509_get_version(x) + 1);
		    BIO_get_mem_ptr(bufbio, &bm);

		    cols[0] = _("Version");
		    cols[1] = bm->data;
		    gtk_clist_append(GTK_CLIST(clist), cols);

		    gtk_object_set_data_full(GTK_OBJECT(data_widget), "x509", 
					     x,
					     (GtkDestroyNotify) X509_free);

		    BIO_free(bufbio);
	       } else {
		    BIO *err;
		    err = BIO_new(BIO_s_mem());
  		    ERR_print_errors(err);

		    /* FIXME: report error */

		    BIO_free(err);
	       }

	       BIO_free(databio);
	  }
  	  if (hbox) gtk_widget_set_usize(GTK_WIDGET(hbox), -1, 60);
     }
}

static void dt_cert_fill_details(struct formfill *form,
				 GtkWidget *data_widget,
				 GtkWidget *text,
				 GByteArray *internal,
				 GtkWidget *clist)
{
     X509 *x = gtk_object_get_data(GTK_OBJECT(data_widget), "x509");
     BIO *data;
     BUF_MEM *bm;

     if (x == NULL) return;

     data = BIO_new(BIO_s_mem());
     X509_print(data, x);
     BIO_get_mem_ptr(data, &bm);

     gtk_text_insert(GTK_TEXT(text), NULL, NULL, NULL,
		     bm->data, bm->length);

     BIO_free(data);
}

/* GType */
G_DEFINE_TYPE(GQDisplayCert, gq_display_cert, GQ_TYPE_DISPLAY_CLIST);

static void
gq_display_cert_init(GQDisplayCert* self) {}

static void
gq_display_cert_class_init(GQDisplayCertClass* self_class) {
	GQTypeDisplayClass * gtd_class = GQ_TYPE_DISPLAY_CLASS(self_class);
	GQDisplayBinaryGenericClass* gdbg_class = GQ_DISPLAY_BINARY_GENERIC_CLASS(self_class);
	GQDisplayCListClass* gdc_class = GQ_DISPLAY_CLIST_CLASS(self_class);

	gtd_class->name = Q_("displaytype|Certificate");
	gtd_class->selectable = TRUE;
	gtd_class->show_in_search_result = FALSE;
	gtd_class->get_widget = dt_clist_get_widget;
	gtd_class->get_data = dt_clist_get_data;
	gtd_class->set_data = dt_generic_binary_set_data; // reused from dt_generic_binary
	gtd_class->buildLDAPMod = bervalLDAPMod;

	gdbg_class->encode = NULL;
	gdbg_class->decode = NULL;
	gdbg_class->get_data_widget = dt_clist_get_data_widget;
	gdbg_class->store_data = dt_clist_store_data;
	gdbg_class->delete_data = dt_clist_delete_data;
	gdbg_class->show_entries = dt_clist_show_entries;

	gdc_class->fill_clist = dt_cert_fill_clist;
	gdc_class->fill_details = dt_cert_fill_details;
}

#endif /* HAVE_LIBCRYPTO */

