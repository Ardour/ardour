/*
 * Copyright (C) 2023-2024 Adrien Gesta-Fline
 *
 * This file is part of libAAF.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include "aaf/URIParser.h"
#include "aaf/utils.h"

#define debug(...) \
	AAF_LOG (log, NULL, LOG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...) \
	AAF_LOG (log, NULL, LOG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...) \
	AAF_LOG (log, NULL, LOG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

#define IS_LOWALPHA(c) \
	((c >= 'a') && (c <= 'z'))

#define IS_UPALPHA(c) \
	((c >= 'A') && (c <= 'Z'))

#define IS_ALPHA(c) \
	(IS_LOWALPHA (c) || IS_UPALPHA (c))

#define IS_DIGIT(c) \
	(c >= '0' && c <= '9')

#define IS_ALPHANUM(c) \
	(IS_ALPHA (c) || IS_DIGIT (c))

#define IS_HEX(c) \
	(IS_DIGIT (c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))

/*
 * RFC 2396
 * https://datatracker.ietf.org/doc/html/rfc2396#section-2.3
 */
#define IS_MARK(c) \
	(c == '-' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' || c == '\'' || c == '(' || c == ')')

#define IS_UNRESERVED(c) \
	(IS_ALPHANUM (c) || IS_MARK (c))

#define IS_ENCODED(p) \
	((*p == '%') && IS_HEX (*(p + 1)) && IS_HEX (*(p + 2)))

#define SCHEME_SAFE_CHAR(c) \
	(IS_ALPHANUM (c) || c == '+' || c == '.' || c == '-')

#define USERINFO_SAFE_CHAR(p) \
	(IS_ALPHANUM (*p) || IS_ENCODED (p) || *p == ';' || *p == ':' || *p == '&' || *p == '=' || *p == '+' || *p == '$' || *p == ',')

#define WINDOWS_DRIVE_LETTER(p) \
	(IS_ALPHA (*p) && (*(p + 1) == ':' || *(p + 1) == '|') && *(p + 2) == '/')

#define SCHEME_ALLOW_QUERY(uri)                \
	(uri->scheme_t != URI_SCHEME_T_FILE && \
	 !(uri->opts & URI_OPT_IGNORE_QUERY))

#define SCHEME_ALLOW_FRAGMENT(uri)             \
	(uri->scheme_t != URI_SCHEME_T_FILE && \
	 !(uri->opts & URI_OPT_IGNORE_FRAGMENT))

#define URI_SET_STR(str, start, end)                                   \
                                                                       \
	str = malloc (sizeof (char) * (uint32_t) ((end - start) + 1)); \
                                                                       \
	if (!str) {                                                    \
		error ("Out of memory");                               \
		goto err;                                              \
	}                                                              \
                                                                       \
	snprintf (str, (uint32_t) (end - start) + 1, "%s", start);

static char*
uriDecodeString (char* src, char* dst);
static int
uriIsIPv4 (const char* s, size_t size, char** err);
static int
uriIsIPv6 (const char* s, size_t size, char** err);

static int
_uri_parse_scheme (struct uri* uri, const char** pos, const char* end, struct aafLog* log);
static int
_uri_parse_authority (struct uri* uri, const char** pos, const char* end, struct aafLog* log);
static int
_uri_parse_userinfo (struct uri* uri, const char** pos, const char* end, struct aafLog* log);
static int
_uri_parse_hostname (struct uri* uri, const char** pos, const char* end, struct aafLog* log);
static int
_uri_parse_path (struct uri* uri, const char** pos, const char* end, struct aafLog* log);
static int
_uri_parse_query (struct uri* uri, const char** pos, const char* end, struct aafLog* log);
static int
_uri_parse_fragment (struct uri* uri, const char** pos, const char* end, struct aafLog* log);

static void
_uri_scheme2schemeType (struct uri* uri);

static char*
uriDecodeString (char* src, char* dst)
{
	if (src == NULL) {
		return NULL;
	}

	if (dst == NULL) {
		dst = src;
	}

	char* end = src + strlen (src);

	while (*src) {
		if (*src == '%' && src + 2 < end && IS_HEX (*(src + 1)) && IS_HEX (*(src + 2))) {
			char d1 = *(src + 1);
			char d2 = *(src + 2);

			int digit = 0;

			digit = (d1 >= 'A' ? ((d1 & 0xdf) - 'A') + 10 : (d1 - '0'));
			digit <<= 4;
			digit += (d2 >= 'A' ? ((d2 & 0xdf) - 'A') + 10 : (d2 - '0'));

			assert (digit > CHAR_MIN && digit < UCHAR_MAX);

			*dst = (char)digit;

			src += 3;
			dst++;
		} else {
			*dst = *src;
			src++;
			dst++;
		}
	}

	*dst = 0x00;

	return dst;
}

static int
_uri_parse_scheme (struct uri* uri, const char** pos, const char* end, struct aafLog* log)
{
	const char* p = *pos;

	while (p < end && *p != ':') {
		if (!SCHEME_SAFE_CHAR (*p)) {
			error ("uri scheme contains invalid character : '%c' (0x%02x)", *p, *p);
			goto err;
		}
		p++;
	}

	if (*pos == p) {
		error ("uri is missing scheme");
		goto err;
	}

	URI_SET_STR (uri->scheme, *pos, p);

	/*
	 * RFC 3986 - Generic
	 * https://datatracker.ietf.org/doc/html/rfc3986#section-3.1
	 *
	 * « Although schemes are case- insensitive, the canonical form is lowercase
	 * and documents that specify schemes must do so with lowercase letters.
	 * An implementation should accept uppercase letters as equivalent to lowercase
	 * in scheme names (e.g., allow "HTTP" as well as "http") for the sake of
	 * robustness but should only produce lowercase scheme names for consistency.»
	 */

	char* pp = uri->scheme;

	while (*pp) {
		int charint = tolower (*pp);

		assert (charint > CHAR_MIN && charint < CHAR_MAX);

		*pp = (char)charint;

		pp++;
	}

	_uri_scheme2schemeType (uri);

	*pos = ++p; /* Skips ':' */

	return 1;

err:
	return -1;
}

static int
_uri_parse_authority (struct uri* uri, const char** pos, const char* end, struct aafLog* log)
{
	/*
	 * RFC 3986 - Uniform Resource Identifier (URI): Generic Syntax
	 * https://datatracker.ietf.org/doc/html/rfc3986#section-3.2
	 *
	 * « Many URI schemes include a hierarchical element for a naming
	 * authority so that governance of the name space defined by the
	 * remainder of the URI is delegated to that authority (which may, in
	 * turn, delegate it further).  The generic syntax provides a common
	 * means for distinguishing an authority based on a registered name or
	 * server address, along with optional port and user information.
	 *
	 * The authority component is preceded by a double slash ("//") and is
	 * terminated by the next slash ("/"), question mark ("?"), or number
	 * sign ("#") character, or by the end of the URI.
	 *
	 *  authority   = [ userinfo "@" ] host [ ":" port ]
	 *
	 * URI producers and normalizers should omit the ":" delimiter that
	 * separates host from port if the port component is empty.  Some
	 * schemes do not allow the userinfo and/or port subcomponents.
	 *
	 * If a URI contains an authority component, then the path component
	 * must either be empty or begin with a slash ("/") character.  Non-
	 * validating parsers (those that merely separate a URI reference into
	 * its major components) will often ignore the subcomponent structure of
	 * authority, treating it as an opaque string from the double-slash to
	 * the first terminating delimiter, until such time as the URI is
	 * dereferenced.»
	 */

	if (*(*pos) != '/' ||
	    *((*pos) + 1) != '/') {
		/* uri has no authority */
		if (uri->scheme_t == URI_SCHEME_T_FILE) {
			uri->flags |= URI_T_LOCALHOST;
		}
		// uri->flags |= URI_T_LOCALHOST;
		// uri->flags |= URI_T_HOST_EMPTY;
		return 0;
	}

	*pos += 2;
	const char* p = *pos;

	while (p < end &&
	       *p != '/' &&
	       (!SCHEME_ALLOW_QUERY (uri) || *p != '?') &&
	       (!SCHEME_ALLOW_FRAGMENT (uri) || *p != '#')) {
		p++;
	}

	URI_SET_STR (uri->authority, *pos, p);

	if (*uri->authority == 0x00) {
		uri->flags |= URI_T_LOCALHOST;
		/* TODO: return 0 ? */
	}

	return 1;

err:
	return -1;
}

static int
_uri_parse_userinfo (struct uri* uri, const char** pos, const char* end, struct aafLog* log)
{
	int hasUserinfo               = 0;
	int userinfoIllegalCharacters = 0;

	const char* p = *pos;

	while (p < end &&
	       /* end of authority */
	       *p != '/' &&
	       (!SCHEME_ALLOW_QUERY (uri) || *p != '?') &&
	       (!SCHEME_ALLOW_FRAGMENT (uri) || *p != '#')) {
		if (*p == '@') {
			hasUserinfo = 1;
			break;
		}

		if (!USERINFO_SAFE_CHAR (p)) {
			userinfoIllegalCharacters++;
		}

		p++;
	}

	if (!hasUserinfo) {
		return 0;
	}

	if (userinfoIllegalCharacters > 0) {
		error ("uri userinfo contains %i invalid char%s", userinfoIllegalCharacters, (userinfoIllegalCharacters > 1) ? "s" : "");
		goto err;
	}

	/* user / pass */

	URI_SET_STR (uri->userinfo, *pos, p);

	*pos = p + 1; // skips '@'

	const char* subpos = NULL;
	p                  = uri->userinfo;

	while (1) {
		if (!*p) {
			if (subpos) {
				URI_SET_STR (uri->pass, subpos, p);
			} else {
				URI_SET_STR (uri->user, uri->userinfo, p);
			}
			break;
		} else if (*p == ':') {
			URI_SET_STR (uri->user, uri->userinfo, p);
			subpos = p + 1;
		}
		p++;
	}

	if (uri->opts & URI_OPT_DECODE_USERINFO && uri->userinfo) {
		uriDecodeString (uri->userinfo, NULL);
	}

	if (uri->opts & URI_OPT_DECODE_USERPASS) {
		if (uri->user)
			uriDecodeString (uri->user, NULL);
		if (uri->pass)
			uriDecodeString (uri->pass, NULL);
	}

	return 1;

err:
	return -1;
}

static int
_uri_parse_hostname (struct uri* uri, const char** pos, const char* end, struct aafLog* log)
{
	const char* p = *pos;

	if (**pos == '[') {
		/*
		 * IPv6 - RFC 2732
		 * https://datatracker.ietf.org/doc/html/rfc2732
		 */
		(*pos)++; // skips '['

		while (p < end &&
		       *p != ']') {
			p++;
		}

		URI_SET_STR (uri->host, *pos, p);

		char* iperr = NULL;
		int   rc    = 0;
		if ((rc = uriIsIPv6 (uri->host, strlen (uri->host), &iperr))) {
			uri->flags |= URI_T_HOST_IPV6;
			if (rc == 2) {
				uri->flags |= URI_T_LOCALHOST;
			}
		} else {
			error ("URI IPv6 Parser error : %s\n", iperr);
			free (iperr);
			goto err;
		}

		p++; // skips ']'
	} else if ((*p == '.' || *p == '?') && (*(p + 1) == '/')) {
		/* windows "//./" and "//?/" guard */
		uri->flags |= URI_T_LOCALHOST;
		return 0;
	} else {
		/*
		 * All other : IPv4, server name, local path
		 */

		while (p < end &&
		       *p != '/' && // if URI contains a path
		       *p != ':' && // if URI has an explicit port
		       (!SCHEME_ALLOW_QUERY (uri) || *p != '?') &&
		       (!SCHEME_ALLOW_FRAGMENT (uri) || *p != '#')) {
			p++;
		}

		// debug( L" >>> %.*s", (int)(p-*pos), p );

		URI_SET_STR (uri->host, *pos, p);
	}

	// if ( !(uri->flags & URI_T_HOST_IPV6 || uri->flags & URI_T_HOST_EMPTY) ) {
	if (!(uri->flags & URI_T_HOST_IPV6) && uri->host != NULL && *uri->host != 0x00) {
		if (uriIsIPv4 (uri->host, strlen (uri->host), NULL)) {
			uri->flags &= ~(unsigned)URI_T_HOST_MASK;
			uri->flags |= URI_T_HOST_IPV4;
			if (strcmp (uri->host, "127.0.0.1") == 0) {
				uri->flags |= URI_T_LOCALHOST;
			}
		} else if (strcmp (uri->host, "localhost") == 0) {
			uri->flags |= URI_T_LOCALHOST;
		} else {
			uri->flags |= URI_T_HOST_REGNAME;
		}

		if (uri->opts & URI_OPT_DECODE_HOSTNAME) {
			uriDecodeString (uri->host, NULL);
		}
	}
	// else if ( uri->host == NULL ) {
	//   if ( uri->scheme_t == URI_SCHEME_T_FILE ) {
	//     uri->flags |= URI_T_LOCALHOST;
	//   }
	// }

	if (*p == ':') {
		/* port */

		*pos = ++p;

		while (p < end &&
		       *p != '/' &&
		       (!SCHEME_ALLOW_QUERY (uri) || *p != '?') &&
		       (!SCHEME_ALLOW_FRAGMENT (uri) || *p != '#')) {
			if (!IS_DIGIT (*p)) {
				error ("URI port contains non-digit char : %c (0x%02x).\n", *p, *p);
				goto err;
			}
			p++;
		}

		uri->port = atoi (*pos);
	}

	*pos = p; // keeps next char, first path '/'

	return 1;

err:
	return -1;
}

static int
_uri_parse_path (struct uri* uri, const char** pos, const char* end, struct aafLog* log)
{
	int winDrive = 0;

	/* sanitize start of path : ignores all slashes (after already parsed '//' identifying start of authority) */

	while (*(*pos + 1) == '/') {
		(*pos)++;
	}

	if (*(*pos) == '/' && WINDOWS_DRIVE_LETTER (((*pos) + 1))) {
		/*
		 * Windows Drive (c: / c|) - RFC 8089
		 * https://datatracker.ietf.org/doc/html/rfc8089#appendix-E.2.2
		 */

		(*pos)++; /* moves forward last slash before driver letter, so path starts at the letter with no slash before. */
		winDrive = 1;
	}

	const char* p = *pos;

	while (p < end &&
	       (!SCHEME_ALLOW_QUERY (uri) || *p != '?') &&
	       (!SCHEME_ALLOW_FRAGMENT (uri) || *p != '#')) {
		p++;
	}

	// debug( L" >>> (%i) %.*s", (int)(p-*pos), (int)(p-*pos), p );

	URI_SET_STR (uri->path, *pos, p);

	if (winDrive) {
		if (uri->path[1] == '|') {
			/*
			 * https://datatracker.ietf.org/doc/html/rfc8089#appendix-E.2.2
			 * « To update such an old URI, replace the vertical line "|" with a colon ":" »
			 */
			uri->path[1] = ':';
		}
	}

	if (uri->opts & URI_OPT_DECODE_PATH) {
		uriDecodeString (uri->path, NULL);
	}

	*pos = p;

	return 1;

err:
	return -1;
}

static int
_uri_parse_query (struct uri* uri, const char** pos, const char* end, struct aafLog* log)
{
	const char* p = *pos;

	if (!(uri->opts & URI_OPT_IGNORE_QUERY) && **pos == '?') {
		while (p < end && *p != '#') {
			p++;
		}

		(*pos)++; // skips '?'

		URI_SET_STR (uri->query, *pos, p);

		if (uri->opts & URI_OPT_DECODE_QUERY) {
			uriDecodeString (uri->query, NULL);
		}

		*pos = p;
	}

	return 1;

err:
	return -1;
}

static int
_uri_parse_fragment (struct uri* uri, const char** pos, const char* end, struct aafLog* log)
{
	/*
	 * https://datatracker.ietf.org/doc/html/draft-yevstifeyev-ftp-uri-scheme#section-3.2.4.2
	 * « ... fragment identifier are allowed in any URI.
	 *
	 * The number sign ("#") characters (ASCII character 0x23), if used for
	 * the reason other than to delimit the fragment identifier SHALL be
	 * percent-encoded. »
	 *
	 * However, we've seen filenames in 'file' scheme with non encoded '#'.
	 * Plus, it seems impossible for a client to use fragments in a 'file'
	 * scheme URI. So the SCHEME_ALLOW_FRAGMENT() macro will make the parser
	 * treat '#' chars as a normal character, only for 'file' scheme.
	 */

	const char* p = *pos;

	if (!(uri->opts & URI_OPT_IGNORE_FRAGMENT) && **pos == '#') {
		while (p < end) {
			p++;
		}

		(*pos)++; // skips '#'

		URI_SET_STR (uri->fragment, *pos, p);

		if (uri->opts & URI_OPT_DECODE_FRAGMENT) {
			uriDecodeString (uri->fragment, NULL);
		}

		*pos = ++p; // skips '#'
	}

	return 1;

err:
	return -1;
}

struct uri*
laaf_uri_parse (const char* uristr, enum uri_option optflags, struct aafLog* log)
{
	if (uristr == NULL) {
		return NULL;
	}

	struct uri* uri = calloc (1, sizeof (struct uri));

	if (!uri) {
		error ("Out of memory");
		return NULL;
	}

	size_t urilen = strlen (uristr);

	if (urilen >= MAX_URI_LENGTH) {
		error ("uri is too long");
		goto err;
	}

	uri->opts = optflags;

	const char* pos = uristr;
	const char* end = pos + urilen;

	_uri_parse_scheme (uri, &pos, end, log);

	if (_uri_parse_authority (uri, &pos, end, log)) {
		_uri_parse_userinfo (uri, &pos, end, log);
		_uri_parse_hostname (uri, &pos, end, log);
	}

	_uri_parse_path (uri, &pos, end, log);

	if (SCHEME_ALLOW_QUERY (uri)) {
		_uri_parse_query (uri, &pos, end, log);
	}

	if (SCHEME_ALLOW_FRAGMENT (uri)) {
		_uri_parse_fragment (uri, &pos, end, log);
	}

	goto end;

err:
	laaf_uri_free (uri);
	uri = NULL;

end:

	return uri;
}

void
laaf_uri_free (struct uri* uri)
{
	if (!uri) {
		return;
	}

	free (uri->scheme);
	free (uri->userinfo);
	free (uri->authority);
	free (uri->user);
	free (uri->pass);
	free (uri->host);
	free (uri->path);
	free (uri->query);
	free (uri->fragment);

	free (uri);
}

static int
uriIsIPv4 (const char* s, size_t size, char** err)
{
	int         octets            = 0;
	const char* currentOctetStart = s;

	char prev = 0;

	for (size_t i = 0; i <= size; i++) {
		if (prev == 0) {
			if (IS_DIGIT (*(s + i))) {
				currentOctetStart = (s + i);
				prev              = 'd';
				continue;
			}

			if (*(s + i) == '.') {
				if (err) {
					laaf_util_snprintf_realloc (err, NULL, 0, "IPV4 parser error : can't start with a single '.'");
				}
				return 0;
			}
		}

		if (prev == 'p') {
			if (IS_DIGIT (*(s + i))) {
				currentOctetStart = (s + i);
				prev              = 'd';
				continue;
			}

			if (*(s + i) == '.') {
				if (err) {
					laaf_util_snprintf_realloc (err, NULL, 0, "IPV4 parser error : can't have successive '.'");
				}
				return 0;
			}
		}

		if (prev == 'd') {
			if (IS_DIGIT (*(s + i))) {
				prev = 'd';
				continue;
			}

			if (i == size || *(s + i) == '.') { // period
				int octet = atoi (currentOctetStart);
				if (octet > 255) {
					if (err) {
						laaf_util_snprintf_realloc (err, NULL, 0, "IPV4 parser error : octet %i is too high : %.*s", (octets), (int)((s + i) - currentOctetStart), currentOctetStart);
					}
					return 0;
				}

				if (i + 1 == size) {
					if (err) {
						laaf_util_snprintf_realloc (err, NULL, 0, "IPV4 parser error : can't end with a single '.'");
					}
					return 0;
				}

				prev = 'p';
				octets++;
				continue;
			}
		}

		if (i == size) {
			break;
		}

		if (err) {
			laaf_util_snprintf_realloc (err, NULL, 0, "IPV4 parser error : illegal char '%c' (0x%02x)", *(s + i), *(s + i));
		}
		return 0;
	}

	if (octets > 4) {
		if (err) {
			laaf_util_snprintf_realloc (err, NULL, 0, "IPV4 parser error : too many octets");
		}
		return 0;
	}
	if (octets < 4) {
		if (err) {
			laaf_util_snprintf_realloc (err, NULL, 0, "IPV4 parser error : not enough octets");
		}
		return 0;
	}

	return 1;
}

static int
uriIsIPv6 (const char* s, size_t size, char** err)
{
	int segmentCount      = 0;
	int emptySegmentCount = 0;
	int curSegmentLength  = 0;
	int ipv4portion       = 0;

	int loopback = 0;

	const char* curSegmentStart = s;

	char prev = 0;

	for (size_t i = 0; i <= size; i++) {
		if (prev == 0) {
			if (IS_HEX (*(s + i))) {
				segmentCount++;
				curSegmentStart = s + i;
				curSegmentLength++;
				prev = 'h'; // hex

				if (loopback >= 0) {
					if (!IS_DIGIT (*(s + i))) {
						loopback = -1;
					} else {
						loopback += (*(s + i) - '0'); //atoi(*(s+i));
					}
				}

				continue;
			}

			if (*(s + i) == ':' && *(s + (i + 1)) == ':') {
				emptySegmentCount++;
				prev = 'e', // empty
				    i++;
				continue;
			}

			if (*(s + i) == ':') {
				if (err) {
					laaf_util_snprintf_realloc (err, NULL, 0, "can't start with a single ':'");
				}
				return 0;
			}
		}

		if (prev == 'h') { /* hex */

			if (IS_HEX (*(s + i))) {
				curSegmentLength++;

				if (loopback >= 0) {
					if (!IS_DIGIT (*(s + i))) {
						loopback = -1;
					} else {
						loopback += (*(s + i) - '0');
					}
				}

				continue;
			}

			if (*(s + i) == '.') { /* period */
				int octet = atoi (curSegmentStart);
				if (octet > 255) {
					if (err) {
						laaf_util_snprintf_realloc (err, NULL, 0, "ipv4 portion octet %i is too high : %.*s", (ipv4portion), curSegmentLength, curSegmentStart);
					}
					return 0;
				}
				// debug( L"%i", octet );
				prev = 'p';
				ipv4portion++;
				continue;
			}

			if (i == size || *(s + i) == ':') {
				if (curSegmentLength > 4) {
					if (err) {
						laaf_util_snprintf_realloc (err, NULL, 0, "segment %i is too long : %.*s", (segmentCount - 1), curSegmentLength, curSegmentStart);
					}
					return 0;
				}
				/* here we can parse segment */
				curSegmentStart  = NULL;
				curSegmentLength = 0;

				if (i < size && *(s + (i + 1)) == ':') {
					emptySegmentCount++;
					prev = 'e', /* empty "::" */
					    i++;
				} else if (i + 1 == size) {
					if (err) {
						laaf_util_snprintf_realloc (err, NULL, 0, "can't end with a single ':'");
					}
					return 0;
				} else {
					prev = 'c'; /* colon ":" */
				}
				continue;
			}
		}

		if (prev == 'e' || prev == 'c') { /* empty or colon */

			if (IS_HEX (*(s + i))) {
				segmentCount++;
				curSegmentStart = s + i;
				curSegmentLength++;
				prev = 'h'; /* hex */

				if (loopback >= 0) {
					if (!IS_DIGIT (*(s + i))) {
						loopback = -1;
					} else {
						loopback += (*(s + i) - '0');
					}
				}

				continue;
			}

			if (*(s + i) == ':') {
				if (err) {
					laaf_util_snprintf_realloc (err, NULL, 0, "can't have more than two successive ':'");
				}
				return 0;
			}
		}

		if (prev == 'p') {
			if (IS_DIGIT (*(s + i))) {
				curSegmentStart = s + i;
				prev            = 'd';
				continue;
			}

			if (*(s + i) == '.') {
				if (err) {
					laaf_util_snprintf_realloc (err, NULL, 0, "can't have successive '.'");
				}
				return 0;
			}
		}

		if (prev == 'd') {
			if (IS_DIGIT (*(s + i)) && *(s + i + 1) != '\0' && i + 1 != size) {
				prev = 'd';
				continue;
			}

			if (i == size || *(s + i) == '.') { /* period */
				int octet = atoi (curSegmentStart);
				if (octet > 255) {
					if (err) {
						laaf_util_snprintf_realloc (err, NULL, 0, "ipv4 portion octet %i is too high : %.*s", (ipv4portion), curSegmentLength, curSegmentStart);
					}
					return 0;
				}

				// debug( L"%i", octet );

				if (i + 1 == size) {
					if (err) {
						laaf_util_snprintf_realloc (err, NULL, 0, "can't end with a single '.'");
					}
					return 0;
				}

				prev = 'p';
				ipv4portion++;
				continue;
			}
		}

		if (i == size) {
			break;
		}

		if (err) {
			laaf_util_snprintf_realloc (err, NULL, 0, "illegal char '%c' (0x%02x)", *(s + i), *(s + i));
		}

		return 0;
	}

	// debug( L"segments : %i", segmentCount );
	// debug( L"empty segments : %i", emptySegmentCount );
	// debug( L"ipv4portion : %i", ipv4portion );

	if (ipv4portion > 4) {
		if (err) {
			laaf_util_snprintf_realloc (err, NULL, 0, "too many octets in ipv4 portion : %i", ipv4portion);
		}
		return 0;
	}
	if (ipv4portion > 0 && ipv4portion < 4) {
		if (err) {
			laaf_util_snprintf_realloc (err, NULL, 0, "not enough octets in ipv4 portion : %i", ipv4portion);
		}
		return 0;
	}
	if (emptySegmentCount + (segmentCount / 2) + ipv4portion > 8) {
		if (err) {
			laaf_util_snprintf_realloc (err, NULL, 0, "too many segments");
		}
		return 0;
	}

	if (emptySegmentCount == 0 && (((ipv4portion / 2) + segmentCount) < 8)) {
		if (err) {
			laaf_util_snprintf_realloc (err, NULL, 0, "not enough segments");
		}
		return 0;
	}

	// debug( L"LOCALHOST >>>>>>> %i", loopback );

	/*
	 * 1: valid ipv6 address
	 * 2: valid ipv6 address and is loopback
	 */

	return (loopback == 1) ? 2 : 1;
}

static void
_uri_scheme2schemeType (struct uri* uri)
{
	if (strcmp (uri->scheme, "afp") == 0) {
		uri->scheme_t = URI_SCHEME_T_AFP;
	} else if (strcmp (uri->scheme, "cifs") == 0) {
		uri->scheme_t = URI_SCHEME_T_CIFS;
	} else if (strcmp (uri->scheme, "data") == 0) {
		uri->scheme_t = URI_SCHEME_T_DATA;
	} else if (strcmp (uri->scheme, "dns") == 0) {
		uri->scheme_t = URI_SCHEME_T_DNS;
	} else if (strcmp (uri->scheme, "file") == 0) {
		uri->scheme_t = URI_SCHEME_T_FILE;
	} else if (strcmp (uri->scheme, "ftp") == 0) {
		uri->scheme_t = URI_SCHEME_T_FTP;
	} else if (strcmp (uri->scheme, "http") == 0) {
		uri->scheme_t = URI_SCHEME_T_HTTP;
	} else if (strcmp (uri->scheme, "https") == 0) {
		uri->scheme_t = URI_SCHEME_T_HTTPS;
	} else if (strcmp (uri->scheme, "imap") == 0) {
		uri->scheme_t = URI_SCHEME_T_IMAP;
	} else if (strcmp (uri->scheme, "irc") == 0) {
		uri->scheme_t = URI_SCHEME_T_IRC;
	} else if (strcmp (uri->scheme, "mailto") == 0) {
		uri->scheme_t = URI_SCHEME_T_MAILTO;
	} else if (strcmp (uri->scheme, "nfs") == 0) {
		uri->scheme_t = URI_SCHEME_T_NFS;
	} else if (strcmp (uri->scheme, "pop") == 0) {
		uri->scheme_t = URI_SCHEME_T_POP;
	} else if (strcmp (uri->scheme, "rtsp") == 0) {
		uri->scheme_t = URI_SCHEME_T_RTSP;
	} else if (strcmp (uri->scheme, "sftp") == 0) {
		uri->scheme_t = URI_SCHEME_T_SFTP;
	} else if (strcmp (uri->scheme, "sip") == 0) {
		uri->scheme_t = URI_SCHEME_T_SIP;
	} else if (strcmp (uri->scheme, "smb") == 0) {
		uri->scheme_t = URI_SCHEME_T_SMB;
	} else if (strcmp (uri->scheme, "ssh") == 0) {
		uri->scheme_t = URI_SCHEME_T_SSH;
	} else if (strcmp (uri->scheme, "tel") == 0) {
		uri->scheme_t = URI_SCHEME_T_TEL;
	} else if (strcmp (uri->scheme, "telnet") == 0) {
		uri->scheme_t = URI_SCHEME_T_TELNET;
	} else {
		uri->scheme_t = URI_SCHEME_T_UNKNOWN;
	}
}
