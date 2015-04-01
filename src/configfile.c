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

/* $Id: configfile.c 975 2006-09-07 18:44:41Z herzi $ */

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <time.h>

#include <glib.h>
#include <glib/gi18n.h>

#include "common.h"
#include "configfile.h"
#include "gq-keyring.h"
#include "gq-server-list.h"
#include "template.h"
#include "util.h"
#include "filter.h"
#include "encode.h"		/* gq_codeset */
#include "errorchain.h"
#include "debug.h"
#include "ldif.h"  /* for b64_encode */
#include "syntax.h"
#include "gq-xml.h"

struct gq_config *config;
GList *transient_servers = NULL;

const struct tokenlist token_searchargument[] = {
     { SEARCHARG_BEGINS_WITH, "Begins with",	NULL },
     { SEARCHARG_ENDS_WITH,   "Ends with",	NULL },
     { SEARCHARG_CONTAINS,    "Contains",	NULL },
     { SEARCHARG_EQUALS,      "Equals",		NULL },
     { 0, "",					NULL }
};

const struct tokenlist token_bindtype[] = {
     { BINDTYPE_SIMPLE,    "Simple",		NULL },
     { BINDTYPE_KERBEROS,  "Kerberos",		NULL },
     { BINDTYPE_SASL,      "SASL",		NULL },
     { 0, "",					NULL }
};

const struct tokenlist token_ldifformat[] = {
     { LDIF_UMICH,         "UMich",		NULL },
     { LDIF_V1,            "Version1",		NULL },
     { 0, "",					NULL }
};


struct attr_settings *new_attr_settings()
{
     struct attr_settings *a = g_malloc0(sizeof(struct attr_settings));
     a->name = g_strdup("");
     a->defaultDT = -1;
     a->user_friendly = NULL;
     
     return a;
}

void free_attr_settings(struct attr_settings *a)
{
     if (a) {
	  g_free(a->name);
	  g_free(a->user_friendly);
	  g_free(a);
     }
}


gboolean is_default_attr_settings(struct attr_settings *as)
{
     g_assert(as);
     return as && as->defaultDT == -1 && as->user_friendly == NULL;
}


/* transient servers are transient by definition. Thus no _ref/_unref */
void transient_add_server(GqServer *newserver)
{
     transient_servers = g_list_append(transient_servers, newserver);
}

void transient_remove_server(GqServer *server)
{
     transient_servers = g_list_remove(transient_servers, server);
}

/* returns the homedir-path, returned string must be g_free'd */
char *homedir(void)
{
     struct passwd *pwent;
     char *result;
   
     pwent = getpwuid(getuid());
     if(pwent && pwent->pw_dir)
	  result = g_strdup(pwent->pw_dir);
     else
	  result = NULL;
     endpwent();

     return result;
}

/* filename_config returns the name of the config file. The returned
   pointer must g_free'd. */
gchar *filename_config(int context)
{
     gchar *rcpath = NULL;
     char *home;
     char *env = getenv("GQRC");
     
     if (env) {
	  return g_strdup(env);
     } else {
	  home = homedir();
	  if(home == NULL) {
	       error_push(context, _("You have no home directory!"));
	       return(NULL);
	  }

	  /* need add'l "/", thus add some extra chars */
	  rcpath = g_malloc(strlen(home) + strlen(RCFILE) + 3);
	  
	  sprintf(rcpath, "%s/%s", home, RCFILE);
	  g_free(home);
	  return(rcpath);
     }
}


void load_config(void)
{
     struct stat sfile;
     int load_context;
     char *rcpath;

     load_context = error_new_context(_("Error loading configfile"), NULL);

     rcpath = filename_config(load_context);
     if(!rcpath) {
	  error_flush(load_context);
	  return;
     }

     /* no config file is fine */
     if(stat(rcpath, &sfile) == -1 || !sfile.st_size) {
	  error_flush(load_context);
	  /* If there is no configuration file, we start with the
	     current configuration file version */
	  config->config_version = CURRENT_CONFIG_VERSION;
	  config->asked_version = CURRENT_CONFIG_VERSION;
	  g_free(rcpath);
	  return;
     }

     /* refuse to read config file if world readable (bind passwords) */
     if( ((sfile.st_mode & ~S_IFMT) & (S_IRWXG|S_IRWXO)) != 0 ) {
	  error_push(load_context,
		     _("%s is group and/or world readable or writeable.\n"
		       "This file can contain passwords in cleartext,\n"
		       "and is recommended to have mode 0600.\n\n"
		       "Continuing with default settings...\n"),
		     rcpath);
	  error_flush(load_context);
	  g_free(rcpath);
	  return;
     }

     config = process_rcfile_XML(load_context, rcpath);
/*      printf("config = %08lx\n", config); */
/*      process_rcfile(rcfile) */

     g_free(rcpath);

     if (config == NULL) {
	  error_push(load_context, 
		     _("No valid configuration found. Using default (empty) configuration"));

	  config = new_config();
     }

     error_flush(load_context);
}

struct writeconfig *new_writeconfig()
{
     struct writeconfig *wc = g_malloc0(sizeof(struct writeconfig));
     wc->indent = 0;
     wc->outfile = NULL;
     return wc;
}

void free_writeconfig(struct writeconfig *wc)
{
     if (wc) {
	  if (wc->outfile) fclose(wc->outfile);
	  wc->outfile = NULL;
     }
     g_free(wc);
}

/* just a pretty printer */
void config_write(struct writeconfig *wc, const char *string)
{
     int i;

     for(i = 0; i < wc->indent; i++)
	  fprintf(wc->outfile, "%s", CONFIG_INDENT_STRING);

     fprintf(wc->outfile, "%s", string);

}

static void append_single_attr(const char *key, const char *val,
			       GString *str)
{
     g_string_sprintfa(str, " %s='%s'", key, val);
}

static void start_tag(GString *str, const char *entity, GHashTable *attr)
{
     g_string_sprintfa(str, "<%s", entity);
     if (attr) {
	  g_hash_table_foreach(attr, (GHFunc) append_single_attr, str);
     }
     g_string_sprintfa(str, ">");
}

static void end_tag(GString *str, const char *entity)
{
     g_string_sprintfa(str, "</%s>\n", entity);
}

void config_write_start_tag(struct writeconfig *wc,
			    const char *entity, GHashTable *attr)
{
     GString *outstr = g_string_sized_new(128);

     start_tag(outstr, entity, attr);
     g_string_append(outstr, "\n");

     config_write(wc, outstr->str);
     g_string_free(outstr, TRUE);
}

void config_write_end_tag(struct writeconfig *wc, const char *entity)
{
     GString *outstr = g_string_sized_new(128);

     end_tag(outstr, entity);

     config_write(wc, outstr->str);
     g_string_free(outstr, TRUE);
}


void config_write_bool(struct writeconfig *wc, int value, const char *entity,
		       GHashTable *attr)
{
     GString *outstr = g_string_sized_new(128);

     start_tag(outstr, entity, attr);
     g_string_sprintfa(outstr, "%s", value ? "True" : "False");
     end_tag(outstr, entity);

     config_write(wc, outstr->str);
     g_string_free(outstr, TRUE);
}


void config_write_int(struct writeconfig *wc, int value, const char *entity,
		      GHashTable *attr)
{
     GString *outstr = g_string_sized_new(128);

     start_tag(outstr, entity, attr);
     g_string_sprintfa(outstr, "%d", value);
     end_tag(outstr, entity);

     config_write(wc, outstr->str);
     g_string_free(outstr, TRUE);
}


void config_write_string(struct writeconfig *wc,
			 const char *value,
			 const char *entity, GHashTable *attr)
{
     GString *outstr = g_string_sized_new(1024);
     gunichar c;

     start_tag(outstr, entity, attr);

     for(c = g_utf8_get_char(value); c ;
	 value = g_utf8_next_char(value), c = g_utf8_get_char(value)) {
	  switch(c) {
	  case '<':
	       g_string_append(outstr, "&lt;");
	       break;
	  case '>':
	       g_string_append(outstr, "&gt;");
	       break;
	  case '&':
	       g_string_append(outstr, "&amp;");
	       break;
	  case '\'':
	       g_string_append(outstr, "&apos;");
	       break;
	  case '"':
	       g_string_append(outstr, "&quot;");
	       break;
	  default:
	       g_string_append_unichar(outstr, c);
	  }
     }

     end_tag(outstr, entity);
     config_write(wc, outstr->str);

     g_string_free(outstr, TRUE);
}


void config_write_string_ne(struct writeconfig *wc,
			    const char *value,
			    const char *entity, GHashTable *attr)
{
     if(value && strlen(value)) {
	  config_write_string(wc, value, entity, attr);
     }
}

static void attrs_dumper(char *key,
			 struct attr_settings *as,
			 struct writeconfig *wc)
{
     GHashTable *attrs = g_hash_table_new(g_str_hash, g_str_equal);
     g_hash_table_insert(attrs, "name", key);

     config_write_start_tag(wc, "ldap-attribute", attrs);
     wc->indent++;

     if (as->defaultDT != -1) {
	  config_write_int(wc, as->defaultDT, "dt-default", NULL);
     }
     if (as->user_friendly != NULL && strlen(as->user_friendly) > 0) {
	  config_write_string(wc, as->user_friendly, "user-friendly", NULL);
     }

     wc->indent--;
     config_write_end_tag(wc, "ldap-attribute");

     g_hash_table_destroy(attrs);
}

static void
write_server(GQServerList* list, GqServer* server, gpointer user_data) {
	gpointer* wc_and_cfg = user_data;
	struct writeconfig* wc = wc_and_cfg[0];
	struct gq_config * cfg = wc_and_cfg[1];

/*	  GString *pw = g_string_new(); */
	  config_write(wc, "<ldapserver>\n");
	  wc->indent++;

	  config_write_string(wc, server->name, "name", NULL);
	  config_write_string(wc, server->ldaphost, "ldaphost", NULL);
	  config_write_int(wc, server->ldapport, "ldapport", NULL);
	  config_write_string_ne(wc, server->basedn, "basedn", NULL);
	  config_write_string_ne(wc, server->binddn, "binddn", NULL);

	  if (cfg->config_version == 0) {
	       config_write_string_ne(wc, server->bindpw, "bindpw", NULL);
	  } else if(server->bindpw && *server->bindpw) {
	       gq_keyring_save_server_password(server);
	  }

	  if(server->bindtype != DEFAULT_BINDTYPE)
	       config_write_string_ne(wc, detokenize(token_bindtype, server->bindtype), "bindtype", NULL);
	  config_write_string_ne(wc, server->searchattr,
				 "search-attribute", NULL);
	  if(server->maxentries != DEFAULT_MAXENTRIES)
	       config_write_int(wc, server->maxentries, "maxentries", NULL);
	  if(server->cacheconn != DEFAULT_CACHECONN)
	       config_write_bool(wc, server->cacheconn, 
				 "cache-connection", NULL);
	  if(server->enabletls != DEFAULT_ENABLETLS)
	       config_write_bool(wc, server->enabletls, "enable-tls", NULL);
	  if(server->local_cache_timeout != DEFAULT_LOCAL_CACHE_TIMEOUT)
	       config_write_int(wc, server->local_cache_timeout, 
				"local-cache-timeout", NULL);
	  if(server->ask_pw != DEFAULT_ASK_PW)
	       config_write_bool(wc, server->ask_pw, "ask-pw", NULL);
	  if(server->hide_internal != DEFAULT_HIDE_INTERNAL)
	       config_write_bool(wc, server->hide_internal, 
				 "hide-internal", NULL);
	  if(server->show_ref != DEFAULT_SHOW_REF)
	       config_write_bool(wc, server->show_ref, "show-ref", NULL);

	  wc->indent--;
	  config_write(wc, "</ldapserver>\n\n");
}

static gboolean save_config_internal(int write_context,
				     struct gq_config *cfg,
				     const char *rcpath)
{
     GList *templatelist, *oclist, *filterlist;
     GqServer *server;
     struct gq_template *template;
     struct gq_filter *filter;
     struct writeconfig *wc;
     char *tmprcpath;
     struct stat sfile;
     int mode = S_IRUSR|S_IWUSR;
     GList *I;
     gboolean ok = FALSE;
     gpointer wc_and_cfg[2] = {
	     NULL,
	     cfg
     };

     if (cfg->config_version > CURRENT_CONFIG_VERSION) {
	  error_push(write_context,
		     _("Configuration file version too high - saving the configuration is not possible"));
	  return FALSE;
     }

     wc = new_writeconfig();
     wc_and_cfg[0] = wc;

     /* write to temp file... */
     tmprcpath = g_malloc(strlen(rcpath) + 10);
     strcpy(tmprcpath, rcpath);
     strcat(tmprcpath, ".new");

     /* check mode of original file. Do not overwrite without write
        permission. */
     if(stat(rcpath, &sfile) == 0) {
	  mode = sfile.st_mode & (S_IRUSR|S_IWUSR);
     }

     wc->outfile = fopen(tmprcpath, "w");
     if(!wc->outfile) {
	  error_push(write_context,
		     _("Unable to open %1$s for writing:\n%2$s\n"), rcpath,
		     strerror(errno));
	  g_free(tmprcpath);
	  return FALSE;
     }
     fchmod(fileno(wc->outfile), mode);

     config_write(wc, "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>\n");
     config_write(wc, "<gq-config>\n\n");

     /* global settings */
     wc->indent++;

     if (cfg->config_version > 0) {
	  config_write_int(wc, cfg->config_version, "config-version", NULL);
	  config_write_int(wc, cfg->config_version, "asked-config-version",
			   NULL);
	  config_write_int(wc, cfg->config_version, "last-asked", NULL);
     }

     config_write_bool(wc, cfg->confirm_mod, "confirm-mod", NULL);
     config_write_string(wc, detokenize(token_searchargument, 
					cfg->search_argument),
			 "search-argument", NULL);
     config_write_bool(wc, cfg->showdn, "show-dn", NULL);
     config_write_bool(wc, cfg->showoc, "show-oc", NULL);
     config_write_bool(wc, cfg->show_rdn_only, "show-rdn-only", NULL);
     config_write_bool(wc, cfg->sort_search, "sort-search-mode", NULL);
     config_write_bool(wc, cfg->sort_browse, "sort-browse-mode", NULL);
     config_write_bool(wc, cfg->browse_use_user_friendly, "browse-use-user-friendly", NULL);

     config_write_bool(wc, cfg->restore_window_sizes,
		       "restore-window-sizes", NULL);
     config_write_bool(wc, cfg->restore_window_positions,
		       "restore-window-positions", NULL);
     config_write_bool(wc, cfg->restore_search_history, 
		       "restore-search-history", NULL);
     config_write_bool(wc, cfg->restore_tabs, 
		       "restore-tabs", NULL);
     config_write_bool(wc, cfg->never_leak_credentials, 
		       "never-leak-credentials", NULL);
     config_write_bool(wc, cfg->do_not_use_ldap_conf, 
		       "do-not-use-ldap-conf", NULL);

     config_write_string(wc, detokenize(token_ldifformat, cfg->ldifformat), 
			 "ldif-format", NULL);
     if(strlen(cfg->schemaserver))
	  config_write_string(wc, cfg->schemaserver, "schema-server", NULL);
     config_write(wc, "\n");

     /* ldapservers */
     gq_server_list_foreach(gq_server_list_get(), write_server, wc_and_cfg);

     /* templates */
     templatelist = cfg->templates;
     while(templatelist) {
	  template = (struct gq_template *) templatelist->data;
	  config_write(wc, "<template>\n");
	  wc->indent++;

	  config_write_string(wc, template->name, "name", NULL);
	  oclist = template->objectclasses;
	  while(oclist) {
	       config_write_string(wc, (char *) oclist->data,
				   "objectclass", NULL);
	       oclist = g_list_next(oclist);
	  }

	  wc->indent--;
	  config_write(wc, "</template>\n\n");

	  templatelist = g_list_next(templatelist);
     }

     /* filters */
     filterlist = cfg->filters;
     while(filterlist) {
	  filter = (struct gq_filter *) filterlist->data;
	  config_write(wc, "<filter>\n");
	  wc->indent++;

	  config_write_string(wc, filter->name, "name", NULL);
	  config_write_string(wc, filter->ldapfilter, "ldapfilter", NULL);
	  if(filter->servername[0])
	       config_write_string(wc, filter->servername, "servername", NULL);
	  if(filter->basedn[0])
	       config_write_string(wc, filter->basedn, "basedn", NULL);

	  wc->indent--;
	  config_write(wc, "</filter>\n\n");

	  filterlist = g_list_next(filterlist);
     }

     config_write(wc, "<defaults>\n");
     wc->indent++;

     if (g_hash_table_size(cfg->attrs) > 0) {
	  config_write_start_tag(wc, "ldap-attributes", NULL);
	  wc->indent++;
	  
	  g_hash_table_foreach(cfg->attrs, (GHFunc) attrs_dumper, wc);
	  
	  wc->indent--;
	  config_write_end_tag(wc, "ldap-attributes");
     }

     wc->indent--;
     config_write(wc, "</defaults>\n\n");

     wc->indent--;
     config_write(wc, "</gq-config>\n");

     free_writeconfig(wc);

     /* rename temp to final config file */

     if (rename(tmprcpath, rcpath) == 0) {
	  ok =TRUE;
     } else {
	  error_push(write_context,
		     _("Could not replace old configuration (%1$s) with the new one (%2$s):\n%3$s\n"), 
		     rcpath, tmprcpath, strerror(errno));
	  ok = FALSE;
     }

     if (ok) {
	  statusbar_msg(_("Configuration saved to '%s'"), rcpath);
     } else {
	  statusbar_msg(_("Saving configuration to '%s' failed."), rcpath);
     }

     g_free(tmprcpath);

     return ok;
}

gboolean save_config(GtkWidget *transient_for)
{
     int write_context = error_new_context(_("Error writing configfile"),
					   transient_for);

     gboolean ok = save_config_ext(write_context);

     error_flush(write_context);

     return ok;
}

gboolean save_config_ext(int write_context)
{
     char *rcpath;
     gboolean ok;

     rcpath = filename_config(write_context);
     if(!rcpath) {
	  return FALSE;
     }

     ok = save_config_internal(write_context, config, rcpath);

     g_free(rcpath);

     return ok;
}

struct gq_config *new_config(void)
{
     struct gq_config *cfg = g_malloc0(sizeof(struct gq_config));

     cfg->config_version = 0;
     cfg->asked_version = 0;
     cfg->last_asked = 0;

     cfg->templates = NULL;
     cfg->filters   = NULL;

     cfg->confirm_mod = 0;
     cfg->search_argument = SEARCHARG_BEGINS_WITH;
     cfg->showdn = TRUE;
     cfg->showoc = TRUE;
     cfg->show_rdn_only = TRUE;
     cfg->sort_search = 0;
     cfg->sort_browse = 0;
     cfg->ldifformat = DEFAULT_LDIFFORMAT;
     cfg->schemaserver = g_strdup(""); /*  [0] = '\0'; */
     cfg->attrs = g_hash_table_new(g_str_hash, g_str_equal);

     cfg->restore_window_sizes = DEFAULT_RESTORE_SIZES;
     cfg->restore_window_positions = DEFAULT_RESTORE_POSITIONS;
     cfg->restore_search_history = DEFAULT_RESTORE_SEARCHES;
     cfg->restore_tabs = DEFAULT_RESTORE_TABS;

     cfg->browse_use_user_friendly = DEFAULT_BROWSE_USE_USER_FRIENDLY;


     cfg->never_leak_credentials = DEFAULT_NEVER_LEAK_CREDENTIALS;
     cfg->do_not_use_ldap_conf = DEFAULT_DO_NOT_USE_LDAP_CONF;

     return cfg;
}

void init_config(void)
{
     GqServer *default_server;
     gboolean dosave = FALSE;
     GQServerList* list = gq_server_list_get();

     config = new_config();

     load_config();

     if (config->config_version > CURRENT_CONFIG_VERSION) {
	  /* incompatible configuration file version (version too high!) */

	  single_warning_popup(_("Incompatible configuration file version\n (version of configuration file is too high).\nTrying the best, but changing the configuration is not possible."));
     }

     if (config->config_version < CURRENT_CONFIG_VERSION
	 && (config->asked_version < CURRENT_CONFIG_VERSION ||
	     (config->asked_version == CURRENT_CONFIG_VERSION &&
	      (time(NULL) - config->last_asked) > 31*86400))) {
	  int rc = question_popup(_("Upgrade configuration?"),
				  _("Do you want to upgrade to the latest configuration file version?\nIf you say no you may not be able to use all functionalities.\nIf you say yes you may not be able to use your configuration with older versions of gq.\n"));

	  config->asked_version = CURRENT_CONFIG_VERSION;
	  config->last_asked = time(NULL);

	  if (rc) {
	       config->config_version = CURRENT_CONFIG_VERSION;
	       dosave = TRUE;
	  }
     }

     if(!gq_server_list_n_servers(list)) {
	  /* no ldapserver defined in configfile */
	  default_server = gq_server_new();

	  g_free(default_server->name);
	  g_free(default_server->ldaphost);
	  g_free(default_server->searchattr);
	  default_server->name = g_strdup("localhost");
	  default_server->ldaphost = g_strdup("localhost");
	  default_server->searchattr = g_strdup("cn");
	  default_server->ldapport = 389;

	  gq_server_list_add(list, default_server);
     }

     /* actually do the upgrade if requested */
     if (dosave) save_config(NULL);
}

static gboolean attr_remove(gpointer key,
			    gpointer value,
			    gpointer user_data)
{
     g_free(key);
     free_attr_settings((struct attr_settings *)value);
     return 1;
}

struct attr_settings *lookup_attr_settings(const char *attrname)
{
     struct attr_settings *as;
     char *tmp, *t;

     if (!config->attrs) return NULL;
     
     tmp = g_strdup(attrname);
     for (t = tmp ; *t ; t++) {
	  *t = tolower(*t);
     }

     as = g_hash_table_lookup(config->attrs, tmp);

     g_free(tmp);

     return as;
}

const char *human_readable_attrname(const char *attrname)
{
     struct attr_settings *as = lookup_attr_settings(attrname);
     return (as && as->user_friendly && strlen(as->user_friendly) > 0) ? as->user_friendly : attrname;
}

void free_config(struct gq_config *cfg) {
     GqServer *server;
     GList *I;

     g_assert(cfg);

     if (cfg->schemaserver) g_free(cfg->schemaserver);
     cfg->schemaserver = NULL;

     /* free malloc'd ldapservers */
     g_object_unref(gq_server_list_get());

     /* free templates */
     g_list_foreach(cfg->templates, (GFunc) free_template, NULL);
     g_list_free(cfg->templates);

     /* free filters */
     g_list_foreach(cfg->filters, (GFunc) free_filter, NULL);
     g_list_free(cfg->filters);

     /* free ldap attribute settings hash */
     g_hash_table_foreach_remove(cfg->attrs, attr_remove, NULL);
     g_hash_table_destroy(cfg->attrs);
     cfg->attrs = NULL;

     /* free object itself */
     g_free(cfg);
}

/*
   Local Variables:
   c-basic-offset: 5
   End:
 */
