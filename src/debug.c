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

/* $Id: debug.c 947 2006-08-24 23:10:20Z herzi $ */

#include <stdio.h>

#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef HAVE_BACKTRACE
#   include <execinfo.h>
#   include <stdlib.h>
#endif /* HAVE_BACKTRACE */

#include "debug.h"
#include "configfile.h"
#include "common.h"



static int num_mallocs = 0;
static int max_mallocs = 0;

#ifdef DEBUG
int debug = 0;
#endif

void *gq_malloc(size_t size, const char *msg)
{
     void *newmem;
     
     newmem = malloc(size);
#ifdef DEBUG
     if (debug & GQ_DEBUG_MALLOC) {
	  printf("+ gq_malloc: %s (%d bytes @ 0x%x)\n", msg, size,
		 (unsigned int) newmem);
	  
	  num_mallocs++;
	  max_mallocs++;
     }
#endif
     return(newmem);
}


void gq_free(void *mem, const char *msg)
{
#ifdef DEBUG
     if (debug & GQ_DEBUG_MALLOC) {
	  printf("- gq_free: %s (0x%x)\n", msg, (unsigned int) mem);
	  num_mallocs--;
     }
#endif
     free(mem);
}


void report_num_mallocs(void)
{

     printf("==============================================\n");
     printf("Total number of mallocs: %d\n", max_mallocs);
     printf("Number of mallocs left : %d\n", num_mallocs);

     if(num_mallocs)
	  printf("memory allocation mismatch: %d allocations left\n",
		 num_mallocs);

}


#ifdef DEBUG
#ifdef HAVE_MALLINFO
static int memstat_timeout_id = -1;
static int memstat_activity = 0;
static char *memstat_act = "-/|\\";

static gint memstat_timer(gpointer data)
{
    struct mallinfo info = mallinfo();
    printf("\rin_use/alloc %9d/%9d [%c]", info.uordblks, info.arena,
	   memstat_act[memstat_activity++]);

    if (!memstat_act[memstat_activity])
	memstat_activity = 0;

    fflush(stdout);
    return TRUE;
}


void init_memstat_timer() {
    if (memstat_timeout_id == -1) {
	memstat_timeout_id = gtk_timeout_add(500, memstat_timer, NULL);
    }
}
#endif /* HAVE_MALLINFO */

#ifdef HAVE_BACKTRACE

/* taken from the documentation of the GNU libc */
void sprint_trace(GString *str)
{
     void *array[200];
     size_t size;
     char **strings;
     size_t i;
     
     size    = backtrace(array, (sizeof(array) / sizeof(array[0])) );
     strings = backtrace_symbols(array, size);
     
     if (strings) {
	  for (i = 0; i < size; i++) {
	       g_string_append(str, strings[i]);
	       g_string_append(str, "\n");
	  }
	  free(strings);
     }
}

/* taken from the documentation of the GNU libc */
void print_trace(void)
{
     void *array[200];
     size_t size;
     char **strings;
     size_t i;
     
     size    = backtrace(array, (sizeof(array) / sizeof(array[0])) );
     strings = backtrace_symbols(array, size);
     
     if (strings) {
	  fprintf(stderr, "----------------------------------------\n");
	  
	  for (i = 0; i < size; i++) {
	       fprintf(stderr, "%s\n", strings[i]);
	  }
	  free(strings);
     }
}
#else /* BACKTRACE */

void sprint_trace(GString *str)
{
}

void print_trace(void) 
{
}

#endif /* BACKTRACE */

#endif /* DEBUG */






/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
