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

/* $Id: debug.h 881 2006-05-04 08:29:02Z herzi $ */

#ifndef GQ_DEBUG_H_INCLUDED
#define GQ_DEBUG_H_INCLUDED

#include <stdlib.h>
#include <glib.h>
#ifdef HAVE_CONFIG_H
# include  <config.h>
#endif /* HAVE_CONFIG_H */

#ifdef DEBUG

extern int debug;

#define GQ_DEBUG_ERROR_LINE	1
#define GQ_DEBUG_MALLOC		2
#define GQ_DEBUG_MEM		4
#define GQ_DEBUG_BROWSER_DND	8
#define GQ_DEBUG_ENCODE		16
#define GQ_DEBUG_SIGNALS	32
#define GQ_DEBUG_ERROR_TRACE	64

#endif

#ifdef DEBUG

#define MALLOC(size, msg)         gq_malloc(size, msg)
#define FREE(mem, msg)            gq_free(mem, msg)

#else

#define MALLOC(size, msg)         malloc(size)
#define FREE(mem, msg)            free(mem)

#endif

#ifdef DEBUG
#  ifdef HAVE_MALLINFO
#    include <malloc.h>
#    include <gtk/gtk.h>

void init_memstat_timer();

#  endif /* HAVE_MALLINFO */
#endif /* DEBUG */

void *gq_malloc(size_t size, const char *msg);
void gq_free(void *mem, const char *msg);
void report_num_mallocs(void);

void print_trace(void);
void sprint_trace(GString *s);

#endif


/* 
   Local Variables:
   c-basic-offset: 5
   End:
 */
