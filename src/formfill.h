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

/* $Id: formfill.h 958 2006-09-02 20:27:07Z herzi $ */

#ifndef GQ_FORMFILL_H_INLCUDED
#define GQ_FORMFILL_H_INLCUDED

#include <ldap_schema.h>

#ifdef HAVE_CONFIG_H
# include  <config.h>		/* pull in HAVE_* defines */
#endif /* HAVE_CONFIG_H */
#include "common.h"

typedef enum {
	// FIXME: add DISPLAYTYPE_INVALID = 0,
	DISPLAYTYPE_DN		= 1,
	DISPLAYTYPE_ENTRY	= 2,
	DISPLAYTYPE_TEXT	= 3,
	DISPLAYTYPE_PASSWORD	= 4,
	DISPLAYTYPE_BINARY	= 5,
	DISPLAYTYPE_JPEG	= 6,
	DISPLAYTYPE_OC		= 7,
#ifdef HAVE_LIBCRYPTO
	DISPLAYTYPE_CERT	= 8,
	DISPLAYTYPE_CRL		= 9,
#else
	DISPLAYTYPE_CERT	= DISPLAYTYPE_BINARY,
	DISPLAYTYPE_CRL		= DISPLAYTYPE_BINARY,
#endif
	DISPLAYTYPE_TIME	= 10,
	DISPLAYTYPE_INT		= 11,
	DISPLAYTYPE_NUMSTR	= 12,
	DISPLAYTYPE_DATE	= 13
} GQDisplayType;

#define FLAG_NOT_IN_SCHEMA      0x01
#define FLAG_MUST_IN_SCHEMA	0x02
/* The FLAG_DEL_ME is used to mark form entries not compatible with
   the schema of the object */
#define FLAG_DEL_ME		0x04
/* Used to suppress the "more" button for single valued attributes */
#define FLAG_SINGLE_VALUE	0x08
/* Used to temporarily mark attributes added for extensibleObject entries */
#define FLAG_EXTENSIBLE_OBJECT_ATTR	0x10

/* Used to disable widgets for attributes marked as no_user_mod */
#define FLAG_NO_USER_MOD	0x80

/* forward decls to avoid circular inclusion problems */
struct _display_type_handler;
struct syntax_handler;

struct formfill {
     gchar* attrname;
     GqServer *server;
     int num_inputfields;
     GQDisplayType displaytype;
     GType dt_handler; // GQTypeDisplayClass derivate
     int flags;
     GList *values;
     struct syntax_handler *syntax;

     GtkWidget *event_box;
     GtkWidget *label;
     GtkWidget *vbox;
     GtkWidget *morebutton;
     GList *widgetList;
};

void init_internalAttrs();
gboolean isInternalAttr(const char *attr);

struct formfill *new_formfill(void);
void free_formlist(GList *formlist);
void free_formfill(struct formfill *form);
void free_formfill_values(struct formfill *form);
GList *formlist_append(GList *formlist, struct formfill *form);
GList *formlist_from_entry(int error_context,
			   GqServer *server, 
			   const char *dn, int ocvalues_only);
GList *dup_formlist(GList *formlist);
void dump_formlist(GList *formlist);
struct formfill *lookup_attribute(GList *formlist, char *attr);
struct formfill *lookup_attribute_using_schema(GList *formlist, 
					       const char *attr,
					       struct server_schema *schema,
					       LDAPAttributeType **attrtype);
int find_displaytype(int error_context, GqServer *server, 
		     struct formfill *form);
void set_displaytype(int error_context, GqServer *server, 
		     struct formfill *form);

char *attr_strip(const char *attr);

#endif /* GQ_FORMFILL_H_INLCUDED */

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
