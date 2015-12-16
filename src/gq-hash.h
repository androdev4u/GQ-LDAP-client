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

#ifndef GQ_ENCRYPTION_H
#define GQ_ENCRYPTION_H

/* #include <glib/garray.h> */
#include "util.h"

G_BEGIN_DECLS

/* actually, we do not really need the following FLAG_* values, we
   only use them as tokens in dt_password.c right now */
#define FLAG_ENCODE_CRYPT	0x10
#define FLAG_ENCODE_MD5		0x20
#define FLAG_ENCODE_SHA		0x40
#define FLAG_ENCODE_SMD5	0x30
#define FLAG_ENCODE_SSHA	0x50
#define FLAG_ENCODE_NTHASH	0x60
#define FLAG_ENCODE_LMHASH	0x70

#define ENCODING_MASK           ( FLAG_ENCODE_CRYPT | FLAG_ENCODE_MD5 | FLAG_ENCODE_SHA )

typedef enum {
	GQ_HASH_CLEARTEXT,
	GQ_HASH_CRYPT,
	GQ_HASH_MD5,
	GQ_HASH_SMD5,
	GQ_HASH_SHA,
	GQ_HASH_SSHA,
	GQ_HASH_NTHASH,
	GQ_HASH_LMHASH
} GqHashType;

extern const struct tokenlist cryptmap[];

#if defined(HAVE_LIBCRYPTO) || defined(HAVE_LIBGCRYPT)
GByteArray* gq_hash_md5   (gchar const* data, gsize length);
#endif
#if defined(HAVE_LIBCRYPTO)
GByteArray* gq_hash_crypt (gchar const* data, gsize length);
GByteArray* gq_hash_lmhash(gchar const* data, gsize length);
GByteArray* gq_hash_nthash(gchar const* data, gsize length);
GByteArray* gq_hash_smd5  (gchar const* data, gsize length);
GByteArray* gq_hash_sha1  (gchar const* data, gsize length);
GByteArray* gq_hash_ssha1 (gchar const* data, gsize length);
#endif

/* NOTE: The password-hash generating functions may change the data
   in-place. They are explicitly permitted to do so. */
typedef GByteArray *(CryptFunc)(char *data, unsigned int len);

G_END_DECLS

#endif /* !GQ_ENCRYPTION_H */
