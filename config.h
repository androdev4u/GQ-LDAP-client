/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */


#ifndef GQ_CONFIG_H_INCLUDE
#define GQ_CONFIG_H_INCLUDE


/* Define if you want to have Drag and Drop support in gq */
/* #undef BROWSER_DND */

/* Define if you want to enable some debugging features */
#define DEBUG 1

/* Set to the default codeset you want to use */
/* #undef DEFAULT_CODESET */

/* always defined to indicate that i18n is enabled */
#define ENABLE_NLS 1

/* Set to 1.2 or 2 */
#define FOUND_OPENLDAP_VERSION 2

/* The package name for gettext */
#define GETTEXT_PACKAGE "gq"

/* Define to 1 if you have the `backtrace' function. */
#define HAVE_BACKTRACE 1

/* Define to 1 if you have the `backtrace_symbols' function. */
#define HAVE_BACKTRACE_SYMBOLS 1

/* Define to 1 if you have the `bind_textdomain_codeset' function. */
#define HAVE_BIND_TEXTDOMAIN_CODESET 1

/* Define to 1 if you have the `dcgettext' function. */
#define HAVE_DCGETTEXT 1

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* Define to 1 if you have the `g_snprintf' function. */
/* #undef HAVE_G_SNPRINTF */

/* Define if you have iconv available */
#define HAVE_ICONV 1

/* Define to 1 if you have the <iconv.h> header file. */
#define HAVE_ICONV_H 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `iswspace' function. */
#define HAVE_ISWSPACE 1

/* Define if you have Kerberos installed */
/* #undef HAVE_KERBEROS */

/* Define if your <locale.h> file defines LC_MESSAGES. */
#define HAVE_LC_MESSAGES 1

/* Define if you want to enable client side LDAP caching in gq */
/* #undef HAVE_LDAP_CLIENT_CACHE */

/* Define to 1 if you have the `ldap_enable_cache' function. */
/* #undef HAVE_LDAP_ENABLE_CACHE */

/* Define to 1 if you have the <ldap.h> header file. */
#define HAVE_LDAP_H 1

/* Define to 1 if you have the `ldap_initialize' function. */
#define HAVE_LDAP_INITIALIZE 1

/* Define to 1 if you have the `ldap_memfree' function. */
#define HAVE_LDAP_MEMFREE 1

/* Define to 1 if you have the `ldap_rename' function. */
#define HAVE_LDAP_RENAME 1

/* Define to 1 if you have the `ldap_str2dn' function. */
#define HAVE_LDAP_STR2DN 1

/* Define to 1 if you have the `ldap_str2objectclass' function. */
#define HAVE_LDAP_STR2OBJECTCLASS 1

/* Define if you have libcrypto */
#define HAVE_LIBCRYPTO 1

/* Define if you have libgcrypt */
#define HAVE_LIBGCRYPT 1

/* Define if you have a liblber containing ber_alloc et.al. */
#define HAVE_LLBER "-llber"

/* Define to 1 if you have the <locale.h> header file. */
#define HAVE_LOCALE_H 1

/* Define if you have struct mallinfo */
#define HAVE_MALLINFO 1

/* Define if you have mcheck */
#define HAVE_MCHECK 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define if you have OpenLDAP 1.2 */
/* #undef HAVE_OPENLDAP12 */

/* Define if you have OpenLDAP 2.x */
#define HAVE_OPENLDAP2 1

/* Define if you have SASL */
/* #undef HAVE_SASL */

/* Define to 1 if you have the <sasl.h> header file. */
/* #undef HAVE_SASL_H */

/* Define to 1 if you have the <sasl/sasl.h> header file. */
/* #undef HAVE_SASL_SASL_H */

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define if your libc has a global timezone variable */
/* #undef HAVE_TIMEZONE */

/* Define if you want TLS support in gq */
#define HAVE_TLS 1

/* Define if your struct tm contains tm_gmtoff or __tm_gmtoff */
#define HAVE_TM_GMTOFF 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Set to proper naming of ISO 8859-1 encoding for your iconv */
#define ISO8859_1 "ISO-8859-1"

/* allow GQ to use deprecated OpenLDAP API */
/* #undef LDAP_DEPRECATED */

/* the locale directory, for gettext */
#define LOCALEDIR "/usr/local/share/locale"

/* Name of package */
#define PACKAGE "gq"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* The package's locale path for gettext */
#define PACKAGE_LOCALE_DIR "/usr/local/share/locale"

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* the package's prefix */
#define PACKAGE_PREFIX "/usr/local"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to the name of the tm_gmtoff member of struct tm */
#define TM_GMTOFF tm_gmtoff

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Version number of package */
#define VERSION "1.2.3"



/* take care to turn off assertions in case debugging is disabled */
#ifndef DEBUG
#define NDEBUG
#endif

#endif /* GQ_CONFIG_H_INCLUDE */

