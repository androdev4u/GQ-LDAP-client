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

/* $Id: syntax.c 955 2006-08-31 19:15:21Z herzi $ */

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>

#include <lber.h>
#include <ldap.h>

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

#include "common.h"
#include "util.h"
#include "input.h"
#include "formfill.h"
#include "ldif.h"
#include "encode.h"
#include "syntax.h"
#include "dt_binary.h"
#include "dt_entry.h"
#include "dt_jpeg.h"
#include "dt_password.h"
#include "dt_text.h"
#include "dt_oc.h"
#include "dt_cert.h"
#include "dt_crl.h"
#include "dt_time.h"
#include "dt_int.h"
#include "dt_numstr.h"
#include "dt_date.h"

/* Syntaxes we recognize and may handle specially. This is a rather
   inefficient way to store oids (we search by string-comparison, but
   only the last few characters ever change....) - we should use some
   form of hashing.. */

static int octetString_attr(const char *attr, const char *syntax_oid);
static int oid_attr(const char *attr, const char *syntax_oid);
static int int_attr(const char *attr, const char *syntax_oid);

static struct syntax_handler syntaxes[] = {
     { "1.3.6.1.4.1.1466.115.121.1.1",
       "ACI Item",
       DISPLAYTYPE_BINARY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.2",
       "Access Point",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.3",
       "Attribute Type Description",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.4",
       "Audio",
       DISPLAYTYPE_BINARY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.5",
       "Binary",
       DISPLAYTYPE_BINARY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.6",
       "Bit String",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.7",
       "Boolean",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.8",
       "Certificate",
       DISPLAYTYPE_CERT,
       NULL,
       1,
     },
     { "1.3.6.1.4.1.1466.115.121.1.9",
       "Certificate List",
       DISPLAYTYPE_CRL,
       NULL,
       1,
     },
     { "1.3.6.1.4.1.1466.115.121.1.10",
       "Certificate Pair",
       DISPLAYTYPE_BINARY,
       NULL,
       1,
     },
     { "1.3.6.1.4.1.1466.115.121.1.11",
       "Country String",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.12",
       "DN",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.13",
       "Data Quality Syntax",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.14",
       "Delivery Method",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.15",
       "Directory String",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.16",
       "DIT Content Rule Description",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.17",
       "DIT Structure Rule Description",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.18",
       "DL Submit Permission",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.19",
       "DSA Quality Syntax",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.20",
       "DSE Type",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.21",
       "Enhanced Guide",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.22",
       "Facsimile Telephone Number",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.23",
       "Fax",
       DISPLAYTYPE_BINARY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.24",
       "Generalized Time",
       DISPLAYTYPE_TIME,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.25",
       "Guide",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.26",
       "IA5 String",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.27",
       "INTEGER",
       DISPLAYTYPE_INT,
       int_attr,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.28",
       "JPEG",
       DISPLAYTYPE_JPEG,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.29",
       "Master And Shadow Access Points",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.30",
       "Matching Rule Description",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.31",
       "Matching Rule Use Description",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.32",
       "Mail Preference",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.33",
       "MHS OR Address",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.34",
       "Name And Optional UID",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.35",
       "Name Form Description",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.36",
       "Numeric String",
       DISPLAYTYPE_NUMSTR,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.37",
       "Object Class Description",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.38",
       "OID",
       DISPLAYTYPE_ENTRY,
       oid_attr,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.39",
       "Other Mailbox",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },

     /* Shaky(!) */
     { "1.3.6.1.4.1.1466.115.121.1.40",
       "Octet String",
       DISPLAYTYPE_BINARY,
       octetString_attr,
       0,
     },

     { "1.3.6.1.4.1.1466.115.121.1.41",
       "Postal Address",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.42",
       "Protocol Information",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.43",
       "Presentation Address",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.44",
       "Printable String",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.45",
       "Subtree Specification",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.46",
       "Supplier Information",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.47",
       "Supplier Or Consumer",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.48",
       "Supplier And Consumer",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.49",
       "Supported Algorithm",
       DISPLAYTYPE_BINARY,
       NULL,
       1,
     },
     { "1.3.6.1.4.1.1466.115.121.1.50",
       "Telephone Number",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.51",
       "Teletex Terminal Identifier",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.52",
       "Telex Number",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.53",
       "UTC Time",
       DISPLAYTYPE_TIME,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.54",
       "LDAP Syntax Description",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.55",
       "Modify Rights",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.56",
       "LDAP Schema Definition",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.57",
       "LDAP Schema Description",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { "1.3.6.1.4.1.1466.115.121.1.58",
       "Substring Assertion",
       DISPLAYTYPE_ENTRY,
       NULL,
       0,
     },
     { NULL, NULL, DISPLAYTYPE_BINARY, NULL, 0, },
};

static GHashTable *syntax_hash = NULL;

static int octetString_attr(const char *attr, const char *syntax_oid) {
     if(!strcasecmp(attr, "userPassword")) {
	  return DISPLAYTYPE_PASSWORD;
     }
#ifdef HAVE_LIBCRYPTO
     if(!strcasecmp(attr, "nDSPKIPublicKeyCertificate")) {
	  return DISPLAYTYPE_CERT;
     }
#endif
     return DISPLAYTYPE_BINARY;
}

static int oid_attr(const char *attr, const char *syntax_oid) {
#ifdef HAVE_LDAP_STR2OBJECTCLASS
     if(!strcasecmp(attr, "objectClass")) {
	  return DISPLAYTYPE_OC;
     }
#endif
     return DISPLAYTYPE_ENTRY;
}

static int int_attr(const char *attr, const char *syntax_oid) {
     if(!strcasecmp(attr, "shadowLastChange")) {
	  return DISPLAYTYPE_DATE;
     }
     if(!strcasecmp(attr, "shadowExpire")) {
	  return DISPLAYTYPE_DATE;
     }
     return DISPLAYTYPE_INT;
}

/* mapping of display types to handlers */
static GHashTable *dt_handlers = NULL;

/* mapping of handlers to display types - using two hashes like this
   requires only little changes, while a proper implementation would
   take time... and a lot of patching */
static GHashTable *handlers_dt = NULL;

static GList *selectable_dt_list = NULL;

static void
add_syntax(int type, GType class_type) {
	g_return_if_fail(g_type_is_a(class_type, GQ_TYPE_TYPE_DISPLAY));

	if (dt_handlers == NULL) {
		dt_handlers = g_hash_table_new(g_direct_hash, g_direct_equal);
	}
	if (handlers_dt == NULL) {
		handlers_dt = g_hash_table_new(g_direct_hash, g_direct_equal);
	}

	g_hash_table_insert(dt_handlers, GINT_TO_POINTER(type), GINT_TO_POINTER(class_type));
	g_hash_table_insert(handlers_dt, GINT_TO_POINTER(class_type), GINT_TO_POINTER(type));

	if (!G_TYPE_IS_ABSTRACT(class_type)) {
		selectable_dt_list = g_list_append(selectable_dt_list, GINT_TO_POINTER(class_type));
	}
}

GList *get_selectable_displaytypes()
{
     return selectable_dt_list;
}

GType
get_dt_handler(int type) {
     GType h = G_TYPE_INVALID;
     if (dt_handlers) {
	  h = GPOINTER_TO_INT(g_hash_table_lookup(dt_handlers, GINT_TO_POINTER(type)));
     }
     return h;
}

int get_dt_from_handler(GType h) {
     gpointer dt = NULL;

     if (handlers_dt) {
	  dt = g_hash_table_lookup(handlers_dt, GINT_TO_POINTER(h));
     }
     return dt != NULL ? GPOINTER_TO_INT(dt) : -1;
}

/* initialize the displaytype -> dt_handler hash */
void init_syntaxes(void) {
    add_syntax(DISPLAYTYPE_ENTRY,	GQ_TYPE_DISPLAY_ENTRY);
    add_syntax(DISPLAYTYPE_TEXT,	GQ_TYPE_DISPLAY_TEXT);
    add_syntax(DISPLAYTYPE_BINARY,	GQ_TYPE_DISPLAY_BINARY);
    add_syntax(DISPLAYTYPE_PASSWORD,	GQ_TYPE_DISPLAY_PASSWORD);
    add_syntax(DISPLAYTYPE_JPEG,	GQ_TYPE_DISPLAY_JPEG);
#ifdef HAVE_LDAP_STR2OBJECTCLASS
    add_syntax(DISPLAYTYPE_OC,		GQ_TYPE_DISPLAY_OC);
#endif
#ifdef HAVE_LIBCRYPTO
    add_syntax(DISPLAYTYPE_CERT,	GQ_TYPE_DISPLAY_CERT);
    add_syntax(DISPLAYTYPE_CRL,		GQ_TYPE_DISPLAY_CRL);
#endif
    add_syntax(DISPLAYTYPE_TIME,	GQ_TYPE_DISPLAY_TIME);
    add_syntax(DISPLAYTYPE_INT,		GQ_TYPE_DISPLAY_INT);
    add_syntax(DISPLAYTYPE_NUMSTR,	GQ_TYPE_DISPLAY_NUMSTR);
    add_syntax(DISPLAYTYPE_DATE,	GQ_TYPE_DISPLAY_DATE);
}

struct syntax_handler *get_syntax_handler_of_attr(int error_context,
						  GqServer *server,
						  const char *attrname,
						  const char *oidin)
{
     struct syntax_handler *sh;

     const char *oid = oidin ? oidin : find_s_by_at_oid(error_context, server, attrname);

     if (!oid) return NULL;

     if (!syntax_hash) {
	 syntax_hash = g_hash_table_new(g_str_hash, g_str_equal);
	 /* lookup syntax... */
	 for(sh = syntaxes ; sh->syntax_oid ; sh++) {
	     g_hash_table_insert(syntax_hash, GINT_TO_POINTER(sh->syntax_oid), sh);
	 }
     }

     sh = (struct syntax_handler *) g_hash_table_lookup(syntax_hash, oid);
     return sh;
}

int get_display_type_of_attr(int error_context,
			     GqServer *server,
			     const char *attrname)
{
     int dt = -1;
     const char *oid = find_s_by_at_oid(error_context, server, attrname);
     struct syntax_handler *sh;

     /* Those without a syntax (most notably cn and sn) most often are
        printable anyway .... */

     if (!oid) return DISPLAYTYPE_ENTRY;

     sh = get_syntax_handler_of_attr(error_context, server, attrname, oid);

     if (sh) {
	 if (sh->displayTypeFunc) {
	     dt = sh->displayTypeFunc(attrname, oid);
	 }
	 if (dt == -1) {
	     dt = sh->displaytype;
	 }

	 return dt;
     }

     return DISPLAYTYPE_BINARY;
}

int show_in_search(int error_context,
		   GqServer *server, const char *attrname)
{
     GType type;
     struct syntax_handler *sh =
	  get_syntax_handler_of_attr(error_context, server, attrname, NULL);

     if (!sh) return FALSE;

     type = GPOINTER_TO_INT(get_dt_handler(sh->displaytype));
     if (!type) return FALSE;

     return G_TYPE_IS_ABSTRACT(type);
}

GByteArray *identity(const char *val, int len)
{
     GByteArray *gb = g_byte_array_new();
     g_byte_array_set_size(gb, len + 1);
     if (gb) {
	  memcpy(gb->data, val, len);
	  /* uuuhhh this is ugly.... AND dangerous... currently we trust
	     g_byte_array_set_size to not really make [len] inaccessible */
	  gb->data[len] = 0;
	  g_byte_array_set_size(gb, len);

     }
/*       printf("identity %s %d\n", gb->data, len); */
     return gb;
}


/* Set up a LDAPMod structure for the attribute denoted by form for a
   "op" operation (one of the LDAP_MOD_* constants). The values for
   this modification are given in "values" (if values is NULL,
   form->values get used). "values" must be a GList of GByteArray
   objects.

   FIXME: check if returning NULL is safe to LDAP data (ie. check if
   this won't be translated into some kind of ldap_modify call) */

LDAPMod *bervalLDAPMod(struct formfill *form,
		       int op,
		       GList *values)
{
     LDAPMod *mod;
     int cval = 0;

     if (values == NULL) {
	  values = form->values;
     }

     mod = malloc(sizeof(LDAPMod));
     if (mod == NULL) return NULL;

     mod->mod_op = op | LDAP_MOD_BVALUES;

     mod->mod_type = g_malloc(strlen(form->attrname) + 20);
     strcpy(mod->mod_type, form->attrname);

     if (form->syntax && form->syntax->must_binary) {
	  strcat(mod->mod_type, ";binary");
     }

     mod->mod_bvalues = (struct berval **)
	  calloc(g_list_length(values) + 1, sizeof(struct berval *));

     if (mod->mod_bvalues == NULL) {
	  free(mod);
	  return NULL;
     }

     cval = 0;

/*       printf("bervalLDAPMod op=%d attr=%s\n", op, mod->mod_type); */

     while(values) {
	  GByteArray *d = values->data;
	  GByteArray *copy = g_byte_array_new();
	  g_byte_array_append(copy, d->data, d->len);

	  mod->mod_bvalues[cval] = (struct berval*) malloc(sizeof(struct berval));
	  if (mod->mod_bvalues[cval] == NULL) {
	       struct berval **b;
	       for (b = mod->mod_bvalues ; b && *b ; b++) {
		    free(*b);
	       }
	       g_byte_array_free(copy, FALSE);
	       return NULL;
	  }
	  mod->mod_bvalues[cval]->bv_len = copy->len;
	  mod->mod_bvalues[cval]->bv_val = (gchar*)copy->data;

/*	  printf("bervalLDAPMod data=%s (%d)\n", copy->data, copy->len); */
	  g_byte_array_free(copy, FALSE);

	  values = values->next;
	  cval++;
     }
     mod->mod_bvalues[cval] = NULL;

/*       printf("bervalLDAPMod #=%d\n", cval); */

     return mod;
}

/*
   Local Variables:
   c-basic-offset: 5
   End:
 */
