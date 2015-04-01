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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif /* HAVE_CONFIG_H */

#if defined(HAVE_LIBCRYPTO)
#include <ctype.h>
#include <string.h>
#include <openssl/des.h>
#include <openssl/md4.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include "encode.h"
#include "iconv-helpers.h"
#include "ldif.h"

GByteArray*
gq_hash_crypt(gchar const *data, gsize len)
{
     GString *salt;
     GByteArray *gb = g_byte_array_new();
     unsigned char *password, rand[16], cryptbuf[32];

     password = (guchar*) g_malloc((len + 1) * sizeof(guchar));
     memcpy(password, data, len);
     password[len] = 0;

     salt = g_string_sized_new(32);
     RAND_pseudo_bytes(rand, 8);
     b64_encode(salt, (gchar*)rand, 8);
     /* crypt(3) says [a-zA-Z0-9./] while base64 has [a-zA-Z0-9+/] */
     if(salt->str[0] == '+') salt->str[0] = '.';
     if(salt->str[1] == '+') salt->str[1] = '.';
     salt->str[2] = 0;

     g_byte_array_append(gb, (guchar*)"{CRYPT}", 7);
     des_fcrypt((gchar*)password, salt->str, (gchar*)cryptbuf);

     g_byte_array_append(gb, cryptbuf, strlen((gchar*)cryptbuf));

     g_string_free(salt, TRUE);
     g_free(password);

     return gb;
}

#ifndef HAVE_LIBGCRYPT
/* prefer libgcrypt over libcrypto */
GByteArray*
gq_hash_md5(gchar const* data, gsize len)
{
     char md5_out[MD5_DIGEST_LENGTH];
     GString *b64 = g_string_sized_new(MD5_DIGEST_LENGTH * 4 / 3 + 4);

     GByteArray *gb = g_byte_array_new();

     MD5((guchar*)data, len, (guchar*)md5_out);
     b64_encode(b64, md5_out, MD5_DIGEST_LENGTH);

     g_byte_array_append(gb, (guchar*)"{MD5}", 5);
     g_byte_array_append(gb, (guchar*)b64->str, strlen(b64->str));

     g_string_free(b64, TRUE);

     return gb;
}
#endif

GByteArray*
gq_hash_sha1(gchar const* data, gsize len)
{
     char sha_out[SHA_DIGEST_LENGTH];
     GString *b64 = g_string_sized_new(SHA_DIGEST_LENGTH * 4 / 3 + 4);

     GByteArray *gb = g_byte_array_new();

     SHA1((guchar*)data, len, (guchar*)sha_out);
     b64_encode(b64, sha_out, SHA_DIGEST_LENGTH);

     g_byte_array_append(gb, (guchar*)"{SHA}", 5);
     g_byte_array_append(gb, (guchar*)b64->str, strlen(b64->str));

     g_string_free(b64, TRUE);

     return gb;
}


GByteArray*
gq_hash_ssha1(gchar const* data, gsize len)
{
     unsigned char rand[4];
     unsigned char sha_out[SHA_DIGEST_LENGTH + sizeof(rand)];
     SHA_CTX  SHA1context;

     GString *b64 = g_string_sized_new(SHA_DIGEST_LENGTH * 4 / 3 + 4);
     GByteArray *gb = g_byte_array_new();
     RAND_pseudo_bytes(rand, sizeof(rand));

     SHA1_Init(&SHA1context);
     SHA1_Update(&SHA1context, data, len);
     SHA1_Update(&SHA1context, rand, sizeof(rand));
     SHA1_Final(sha_out, &SHA1context);

     memcpy(sha_out + SHA_DIGEST_LENGTH, rand, sizeof(rand));

     b64_encode(b64, (gchar*)sha_out, sizeof(sha_out));

     g_byte_array_append(gb, (guchar*)"{SSHA}", 6);
     g_byte_array_append(gb, (guchar*)b64->str, strlen(b64->str));

     g_string_free(b64, TRUE);

     return gb;
}

GByteArray*
gq_hash_smd5(gchar const* data, gsize len)
{
     unsigned char rand[4];
     unsigned char md5_out[MD5_DIGEST_LENGTH + sizeof(rand)];
     MD5_CTX  MD5context;

     GString *b64 = g_string_sized_new(MD5_DIGEST_LENGTH * 4 / 3 + 4);
     GByteArray *gb = g_byte_array_new();
     RAND_pseudo_bytes(rand, sizeof(rand));

     MD5_Init(&MD5context);
     MD5_Update(&MD5context, data, len);
     MD5_Update(&MD5context, rand, sizeof(rand));
     MD5_Final(md5_out, &MD5context);

     memcpy(md5_out + MD5_DIGEST_LENGTH, rand, sizeof(rand));

     b64_encode(b64, (gchar*)md5_out, sizeof(md5_out));

     g_byte_array_append(gb, (guchar*)"{SMD5}", 6);
     g_byte_array_append(gb, (guchar*)b64->str, strlen(b64->str));

     g_string_free(b64, TRUE);

     return gb;
}

static const char hexdigit[] = "0123456789ABCDEF";

GByteArray*
gq_hash_nthash(gchar const* data, gsize len)
{
     unsigned char md4_out[MD4_DIGEST_LENGTH];
     unsigned int i;
     GByteArray *unicode = g_byte_array_new();
     GByteArray *gb = g_byte_array_new();
     MD4_CTX MD4context;

#if defined(HAVE_ICONV)
     ICONV_CONST char *in;
     guchar *out;
     size_t inlen, outlen;
     iconv_t conv;

     conv = iconv_open("UNICODE", gq_codeset);
     if (conv != (iconv_t) -1) {
	  gchar* outbuf;

	  in = (ICONV_CONST char *) data;
	  inlen = len;
	  outlen = len * 2 + 4;
	  g_byte_array_set_size(unicode, outlen);
	  out = unicode->data;

	  outbuf = (gchar*)out;
	  while(inlen > 0 && outlen > 0) {
	       if(iconv(conv, &in, &inlen, &outbuf, &outlen) != 0) {
		    in++;		/* *** */
		    inlen--;
	       }
	  }
	  iconv_close(conv);
	  out = unicode->data;

	  /* strip leading endian marker (should be ff fe) */
	  g_assert(out[0] == 0xff);
	  g_assert(out[1] == 0xfe);

	  memmove(out, out + 2, len * 2);
	  g_byte_array_set_size(unicode, len * 2);
     } else {
	  for (i = 0 ; i < len ; i++) {
	       g_byte_array_append(unicode, (guchar*)data + i, 1);
	       g_byte_array_append(unicode, (guchar*)"\0", 1);
	  }
     }
#else /* HAVE_ICONV */
     for (i = 0 ; i < len ; i++) {
	  g_byte_array_append(in, data + i, 1);
	  g_byte_array_append(in, "\0", 1);
     }
#endif /* HAVE_ICONV */

     MD4_Init(&MD4context);
     MD4_Update(&MD4context, unicode->data, unicode->len);
     MD4_Final(md4_out, &MD4context);

     for(i = 0 ; i < sizeof(md4_out) ; i++) {
	  char hex[2];
	  hex[0] = hexdigit[md4_out[i] / 16];
	  hex[1] = hexdigit[md4_out[i] % 16];
	  g_byte_array_append(gb, (guchar*)hex, 2);
     }

     return gb;
}

static const char *lmhash_key = "KGS!@#$%";
/* FIXME: silently assumes US-ASCII (or a single-byte encoding to be
   handled by toupper) */

static void lm_make_key(const char *pw, des_cblock *key)
{
     int i;
     char *k = (char *) key;

     k[0] = 0;
     for ( i = 0 ; i < 7 ; i++ ) {
	  k[i]   |= (pw[i] >> i) & 0xff;
	  k[i+1]  = (pw[i] << (7 - i)) & 0xff;
     }

     des_set_odd_parity(key);
}

GByteArray*
gq_hash_lmhash(gchar const* data, gsize len)
{
     unsigned int i;
     char hex[2];
     char plain[15];
     des_key_schedule schedule;
     GByteArray *gb = NULL;
     des_cblock ckey1, ckey2;
     des_cblock bin1,  bin2;

     memset(plain, 0, sizeof(plain));

     for (i = 0 ; i < len && i < 14 ; i++) {
	  plain[i] = toupper(data[i]);
     }

     lm_make_key(plain, &ckey1);
     des_set_key_unchecked(&ckey1, schedule);
     des_ecb_encrypt((des_cblock*)lmhash_key, &bin1, schedule, DES_ENCRYPT);

     lm_make_key(plain + 7, &ckey2);
     des_set_key_unchecked(&ckey2, schedule);
     des_ecb_encrypt((des_cblock*)lmhash_key, &bin2, schedule, DES_ENCRYPT);

     gb = g_byte_array_new();

     for(i = 0 ; i < sizeof(bin1) ; i++) {
	  hex[0] = hexdigit[bin1[i] / 16];
	  hex[1] = hexdigit[bin1[i] % 16];
	  g_byte_array_append(gb, (guchar*)hex, 2);
     }
     for(i = 0 ; i < sizeof(bin2) ; i++) {
	  hex[0] = hexdigit[bin2[i] / 16];
	  hex[1] = hexdigit[bin2[i] % 16];
	  g_byte_array_append(gb, (guchar*)hex, 2);
     }
     return gb;
}

#endif /* HAVE_LIBCRYPTO */



