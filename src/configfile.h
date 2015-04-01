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

/* $Id: configfile.h 958 2006-09-02 20:27:07Z herzi $ */

#ifndef GQ_CONFIGFILE_H_INCLUDED
#define GQ_CONFIGFILE_H_INCLUDED

#include <stdio.h>
#include <glib.h>

#include "common.h"
#include "gq-tab-search.h"
#include "util.h"

#define CURRENT_CONFIG_VERSION	3

#define CONFIG_INDENT_STRING   "    "

#define RCFILE           ".gq"
#define STATEFILE        ".gq-state"

/* bitwise flags used in keywordlist.flags */
#define NEEDS_CLOSE   1
#define NEEDS_DATA    2


#define BINDTYPE_SIMPLE        0
#define BINDTYPE_KERBEROS      1
#define BINDTYPE_SASL          2

#define MAX_ENTITY_LEN        64   /* not using XML attributes anyway */
#define MAX_DATA_LEN         128
#define DEFAULT_MAXENTRIES   200
#define DEFAULT_SEARCHATTR   "cn"
#define DEFAULT_BINDTYPE     BINDTYPE_SIMPLE
#define DEFAULT_LDIFFORMAT   LDIF_UMICH
#define DEFAULT_CACHECONN      1
#define DEFAULT_ENABLETLS      0
#define DEFAULT_LOCAL_CACHE_TIMEOUT -1
#define DEFAULT_LOCAL_CACHE_SIZE (512*1024)
#define DEFAULT_ASK_PW	       1
#define DEFAULT_HIDE_INTERNAL  1
#define DEFAULT_SHOW_REF       0

#define DEFAULT_BROWSE_USE_USER_FRIENDLY	1

#define DEFAULT_RESTORE_SIZES		1
#define DEFAULT_RESTORE_POSITIONS	0
#define DEFAULT_RESTORE_SEARCHES	1
#define DEFAULT_RESTORE_TABS		0

/* SECURITY: The default is to NOT blindly reuse LDAP credentials for
   referrals */
#define DEFAULT_NEVER_LEAK_CREDENTIALS	1
#define DEFAULT_DO_NOT_USE_LDAP_CONF	0

/* The following do not _really_ belong in here right now... */
/* LDAP Timeout in seconds */
#define DEFAULT_LDAP_TIMEOUT	30

#ifdef LDAP_OPT_NETWORK_TIMEOUT
#define DEFAULT_NETWORK_TIMEOUT	15
#endif


/* this holds all configuration data */
struct gq_config {
     /* persistent */
     long config_version; /* 0 for old-style configuration file format
			     1 introduces password encoding */
     long asked_version; /* holds the highest config file version for
			    which gq has already asked if it should
			    upgrade to. This is to avoid that on every
			    start of gq the used gets asked if he
			    wants to upgrade to a newer configfile
			    version */
     long last_asked;    /* timestamp of lask asking for upgrade to a
			    newer config file. */

     GList *templates;
     GList *filters;
     GqSearchType search_argument;
     int ldifformat;
     int confirm_mod; /* not used yet */
     int showdn;
     int showoc;	/* obsolete */
     int sort_search;
     int sort_browse;
     int show_rdn_only;
     char *schemaserver;
     int restore_window_sizes;
     int restore_window_positions;
     int restore_search_history;
     int restore_tabs;
     int browse_use_user_friendly;

     int never_leak_credentials;
     int do_not_use_ldap_conf;		/* turn off parsing of ldap.conf and
					   .ldaprc */

     GHashTable *attrs;	/* attribute specific data */
};

struct keywordlist {
     const char attribute[64];
     const int token;
     const int flags;
};


/* handy for writing config file */
struct writeconfig {
     FILE *outfile;
     int indent;
};

/* A structure holding LDAP attribute specific, configurable settings */
struct attr_settings {
     char *name;
     int defaultDT;		/* default displaytype: -1: unset */
     char *user_friendly;	/* user-friendly name */
};


struct writeconfig *new_writeconfig();
void free_writeconfig(struct writeconfig *wc);

struct attr_settings *new_attr_settings();
void free_attr_settings(struct attr_settings *);
/* returns TRUE if the attr_settings object contains only default
   values. This indicates that the object could be removed
   altogether */
gboolean is_default_attr_settings(struct attr_settings *);

struct attr_settings *lookup_attr_settings(const char *attrname);
const char *human_readable_attrname(const char *attrname);

void config_write_start_tag(struct writeconfig *wc,
			    const char *entity, GHashTable *attr);
void config_write_end_tag(struct writeconfig *wc, const char *entity);

void config_write(struct writeconfig *wc, const char *string);
void config_write_bool(struct writeconfig *wc,
		       int value, const char *entity, GHashTable *attr);
void config_write_int(struct writeconfig *wc,
		      int value, const char *entity, GHashTable *attr);
void config_write_string(struct writeconfig *wc,
			 const char *value,
			 const char *entity, GHashTable *attr);
void config_write_string_ne(struct writeconfig *wc,
			    const char *value,
			    const char *entity, GHashTable *attr);


char *homedir(void);

/* filename_config returns the name of the config file. The returned
   pointer must g_free'd. */
gchar *filename_config(int context);

void load_config(void);
gboolean save_config(GtkWidget *transient_for);
gboolean save_config_ext(int error_context);
void init_config(void);

struct gq_config *new_config(void);
void free_config(struct gq_config *cfg);

void transient_add_server(GqServer *newserver);
void transient_remove_server(GqServer *server);

extern struct gq_config *config;
extern GList *transient_servers;

extern const struct tokenlist token_bindtype[];
extern const struct tokenlist token_ldifformat[];
extern const struct tokenlist token_searchargument[];

#endif

/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
