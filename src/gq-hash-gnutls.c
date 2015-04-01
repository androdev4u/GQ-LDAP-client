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

#ifdef HAVE_LIBGCRYPT
#include <gcrypt.h>

#include "ldif.h"

GByteArray*
gq_hash_md5(gchar const* data, gsize length) {
	GByteArray* array;
	GString*    b64;
	size_t      hash_len;
	gchar*      buffer; // raw data

	hash_len = gcry_md_get_algo_dlen(GCRY_MD_MD5);
	buffer = g_new0(gchar, hash_len);
	gcry_md_hash_buffer(GCRY_MD_MD5, buffer, data, length);

	b64 = g_string_new("");
	b64_encode(b64, buffer, hash_len);
	g_free(buffer);

	array = g_byte_array_new();
	g_byte_array_append(array, (guchar*)"{MD5}", 5);
	g_byte_array_append(array, (guchar*)b64->str, b64->len);

	g_string_free(b64, TRUE);

	return array;
}
#endif

