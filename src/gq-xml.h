/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2003 Bert Vermeulen
    Copyright (C) 2002-2003 by Peter Stamfest

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

/* $Id: gq-xml.h 960 2006-09-02 20:42:46Z herzi $ */

#ifndef GQ_GQ_XML_H_INCLUDED
#define GQ_GQ_XML_H_INCLUDED

#include "configfile.h" 

struct gq_config *process_rcfile_XML(int error_context, 
				     const char *filename);

#endif

