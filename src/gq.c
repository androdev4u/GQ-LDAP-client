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
#include <string.h>
#include <signal.h>
#include <unistd.h>

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef DEBUG
#  include "debug.h"
#  ifdef HAVE_MCHECK
#    include <mcheck.h>
#  endif /* HAVE_MCHECK */
#endif /* DEBUG */

#include "gq-server-list.h"
#include "mainwin.h"
#include "configfile.h"
#include "syntax.h"
#include "encode.h"
#include "debug.h"
#include "state.h"

static void sigpipehandler(int sig);

static const GOptionEntry entries[] = {
#ifdef DEBUG
	{ "debug", 'd', 0, G_OPTION_ARG_INT, &debug, N_("Enable debugging"), N_("LEVEL") },
#endif
	{ NULL, 0, 0, 0, NULL, NULL, NULL }
};

int
main(int argc, char **argv) {
	GOptionContext* ctxt = NULL;
	GError* error = NULL;
     void *p;

/*      return main_test(argc, argv); */


#if ENABLE_NLS
     setlocale(LC_ALL, "");
     textdomain(PACKAGE);

#  if HAVE_LANGINFO_CODESET
     {
	  char *codeset = nl_langinfo(CODESET);
	  if (codeset) {
	       /* PS: rule out ANSI* explicitly - I hope this is
		  generically enough */
	       if (strncmp(codeset, "ANSI", 4) != 0) {
		    /* nl_langinfo uses a static buffer - thus duplicate the
		       codeset to use before it gets overwritten */
		    gq_codeset = g_strdup(codeset);
	       }
	  }
     }
#  endif /* HAVE_LANGINFO_CODESET */
#endif

#if ENABLE_NLS
	bindtextdomain(PACKAGE, LOCALEDIR);
	bind_textdomain_codeset(PACKAGE, "UTF-8");
#endif

	ctxt = g_option_context_new(_("- an LDAP client"));
	g_option_context_add_main_entries(ctxt, entries, PACKAGE);
	g_option_context_add_group(ctxt, gtk_get_option_group(TRUE));

	if(!g_option_context_parse(ctxt, &argc, &argv, &error)) {
		if(error) {
			g_print(_("Invalid option: %s\n"), error->message);
			g_error_free(error);
			error = NULL;
		}
		else {
			g_print(_("Invalid option.\n"));
		}
		g_print(_("Use \"%s --help\" for a list of options\n"), argv[0]);
		g_option_context_free(ctxt);
		g_clear_error(&error);
		return 1;
	}
	else if(error) {
		g_print(_("Invalid option: %s\n"), error->message);
		g_error_free(error);
		error = NULL;
		g_print(_("Use \"%s --help\" for a list of options\n"), argv[0]);
	}
	g_option_context_free(ctxt);
	ctxt = NULL;

#ifdef DEBUG
# ifdef HAVE_MCHECK
	if (debug & GQ_DEBUG_MEM) {
		mcheck(NULL);
	}
# endif /* HAVE_MCHECK */
#endif /* DEBUG */

     init_syntaxes();
     init_internalAttrs();

     init_config();
     init_state();

     if (config->do_not_use_ldap_conf) {
	  g_setenv("LDAPNOINIT", "1", TRUE);
     }

     create_mainwin(&mainwin);

#ifdef DEBUG
#  ifdef HAVE_MALLINFO
     if (debug & GQ_DEBUG_MEM) {
	  init_memstat_timer();
     }
#  endif /* HAVE_MALLINFO */
#endif /* DEBUG */

     /* Avoids getting killed if an LDAP server we are connected to
        goes down (restarted?) I do not like this solution, as it is
        not very clean IMHO (though it has been proposed on the
        OpenLDAP mailing list) */

     p = signal(SIGPIPE, sigpipehandler);

#ifdef DEBUG
     if (debug & GQ_DEBUG_SIGNALS) {
	  fprintf(stderr, "SIGPIPE handler installation: %s\n",
		  p == SIG_ERR ? "failed" : "ok");
     }
#endif /* DEBUG */

     gtk_main ();

     free_config(config);
     config = NULL;

     save_state();

     gtk_exit(0);

#ifdef DEBUG
     if (debug & GQ_DEBUG_MALLOC) {
	  report_num_mallocs();
     }
#endif

     return(0);
}

static void
increase_down_per_server(GQServerList* list, GqServer* server, gpointer user_data) {
	server->server_down++;
}

static void sigpipehandler(int sig)
{
#ifdef DEBUG
     if (debug & GQ_DEBUG_SIGNALS) {
	  fprintf(stderr, "Caught SIGPIPE\n");
     }
#endif /* DEBUG */

     gq_server_list_foreach(gq_server_list_get(), increase_down_per_server, NULL);

     /* reinstall signal handler [make sure we work on BSD] */
     signal(SIGPIPE, sigpipehandler);
}

/*
   Local Variables:
   c-basic-offset: 5
   End:
 */
