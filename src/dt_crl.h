/*
    GQ -- a GTK-based LDAP client
    Copyright (C) 1998-2001 Bert Vermeulen

    This file (dt_crl.h) is
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

/* $Id: dt_crl.h 937 2006-05-25 13:51:47Z herzi $ */

#ifndef DT_CRL_H_INCLUDED
#define DT_CRL_H_INCLUDED

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */
#ifdef HAVE_LIBCRYPTO

#include "syntax.h"
#include "dt_clist.h"

typedef GQDisplayCList      GQDisplayCRL;
typedef GQDisplayCListClass GQDisplayCRLClass;

#define GQ_TYPE_DISPLAY_CRL         (gq_display_crl_get_type())
#define DT_CRL(objpointer) ((dt_cert_handler*)(objpointer))

GType gq_display_crl_get_type(void);

#endif /* HAVE_LIBCRYPTO */

#endif

