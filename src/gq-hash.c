/* This file is part of GQ
 *
 * AUTHORS
 *     Sven Herzberg  <herzi@gnome-de.org>
 *
 * Copyright (C) 2006  Sven Herzberg <herzi@gnome-de.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "gq-hash.h"

const struct tokenlist cryptmap[] = {
     { 0,                 "Clear", NULL },
#if defined(HAVE_LIBGCRYPT) || defined(HAVE_LIBCRYPTO)
     { FLAG_ENCODE_MD5,    "MD5",    gq_hash_md5 },
#endif
#if defined(HAVE_LIBCRYPTO)
     /* currently, we still need unique token codes, should drop this
        requirement */
     { FLAG_ENCODE_CRYPT,  "Crypt",  gq_hash_crypt   },
     { FLAG_ENCODE_SSHA,   "SSHA",   gq_hash_ssha1   },
     { FLAG_ENCODE_SMD5,   "SMD5",   gq_hash_smd5    },
     { FLAG_ENCODE_SHA,    "SHA",    gq_hash_sha1    },
     { FLAG_ENCODE_NTHASH, "NTHASH", gq_hash_nthash  },
     { FLAG_ENCODE_LMHASH, "LMHASH", gq_hash_lmhash  },
#endif
     { 0,		   "",       NULL },
};

