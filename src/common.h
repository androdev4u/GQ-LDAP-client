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

#ifndef GQ_COMMON_H_INCLUDED
#define GQ_COMMON_H_INCLUDED

#include <lber.h>

#include <glib.h>
#include <gtk/gtk.h>

#include "gq-server.h"
#include "gq-tree-widget.h"

#define g_free_and_dup(target, source) \
	{ g_free(target); (target) = g_strdup(source); }

#ifndef HAVE_LDAP_MEMFREE
#    define ldap_memfree(x)	free(x)
#endif

#endif

