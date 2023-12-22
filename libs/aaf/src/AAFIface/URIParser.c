/*
 * Copyright (C) 2023 Adrien Gesta-Fline
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

#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _MSC_VER
#include <BaseTsd.h>
typedef SSIZE_T ssize_t;
#endif

#include "URIParser.h"

#define debug(...)                                                             \
  _dbg(dbg, NULL, DEBUG_SRC_ID_AAF_IFACE, VERB_DEBUG, __VA_ARGS__)

#define warning(...)                                                           \
  _dbg(dbg, NULL, DEBUG_SRC_ID_AAF_IFACE, VERB_WARNING, __VA_ARGS__)

#define error(...)                                                             \
  _dbg(dbg, NULL, DEBUG_SRC_ID_AAF_IFACE, VERB_ERROR, __VA_ARGS__)

#define IS_LOWALPHA(c) ((c >= 'a') && (c <= 'z'))

#define IS_UPALPHA(c) ((c >= 'A') && (c <= 'Z'))

#define IS_ALPHA(c) (IS_LOWALPHA(c) || IS_UPALPHA(c))

#define IS_DIGIT(c) (c >= '0' && c <= '9')

#define IS_ALPHANUM(c) (IS_ALPHA(c) || IS_DIGIT(c))

#define IS_HEX(c)                                                              \
  (IS_DIGIT(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))

/*
 * RFC 2396
 * https://datatracker.ietf.org/doc/html/rfc2396#section-2.3
 */
#define IS_MARK(c)                                                             \
  (c == '-' || c == '_' || c == '.' || c == '!' || c == '~' || c == '*' ||     \
   c == '\'' || c == '(' || c == ')')

#define IS_UNRESERVED(c) (IS_ALPHANUM(c) || IS_MARK(c))

#define IS_ENCODED(p) ((*p == '%') && IS_HEX(*(p + 1)) && IS_HEX(*(p + 2)))

#define SCHEME_SAFE_CHAR(c) (IS_ALPHANUM(c) || c == '+' || c == '.' || c == '-')

#define USERINFO_SAFE_CHAR(p)                                                  \
  (IS_ALPHANUM(*p) || IS_ENCODED(p) || *p == ';' || *p == ':' || *p == '&' ||  \
   *p == '=' || *p == '+' || *p == '$' || *p == ',')

#define WINDOWS_DRIVE_LETTER(p)                                                \
  (IS_ALPHA(*p) && (*(p + 1) == ':' || *(p + 1) == '|') && *(p + 2) == '/')

#define SCHEME_ALLOW_QUERY(uri)                                                \
  (uri->scheme_t != URI_SCHEME_T_FILE && !(uri->opts & URI_OPT_IGNORE_QUERY))

#define SCHEME_ALLOW_FRAGMENT(uri)                                             \
  (uri->scheme_t != URI_SCHEME_T_FILE && !(uri->opts & URI_OPT_IGNORE_FRAGMENT))

#define URI_SET_STR(str, start, end)                                           \
                                                                               \
  str = malloc(sizeof(char) * ((end - start) + 1));                            \
                                                                               \
  if (NULL == str) {                                                           \
    error("URI allocation failed");                                            \
    goto err;                                                                  \
  }                                                                            \
                                                                               \
  snprintf(str, (end - start) + 1, "%s", start);

static int _uri_parse_scheme(struct uri *uri, const char **pos, const char *end,
                             struct dbg *dbg);
static int _uri_parse_authority(struct uri *uri, const char **pos,
                                const char *end, struct dbg *dbg);
static int _uri_parse_userinfo(struct uri *uri, const char **pos,
                               const char *end, struct dbg *dbg);
static int _uri_parse_hostname(struct uri *uri, const char **pos,
                               const char *end, struct dbg *dbg);
static int _uri_parse_path(struct uri *uri, const char **pos, const char *end,
                           struct dbg *dbg);
static int _uri_parse_query(struct uri *uri, const char **pos, const char *end,
                            struct dbg *dbg);
static int _uri_parse_fragment(struct uri *uri, const char **pos,
                               const char *end, struct dbg *dbg);

static void _uri_scheme2schemeType(struct uri *uri);
static int _laaf_util_snprintf_realloc(char **str, size_t *size, size_t offset,
                                       const char *format, ...);

#ifdef BUILD_URI_TEST //  gcc -g -W -Wall ./URIParser.c -D BUILD_URI_TEST
static int _uri_cmp(const struct uri *a, const struct uri *b);
static void _uri_dump_diff(struct uri *a, struct uri *b, int totalDifferencies);
static int _uri_test(const char *uristr, enum uri_option optflags,
                     struct uri expectedRes, int line);
#endif // BUILD_URI_TEST

char *uriDecodeString(char *src, char *dst) {

  int inpos = 0;
  int outpos = 0;

  if (src == NULL) {
    return NULL;
  }

  if (dst == NULL) {
    dst = src;
  }

  while (src[inpos]) {

    if (src[inpos] == '%' && IS_HEX(src[inpos + 1]) && IS_HEX(src[inpos + 2])) {

      int c = 0;

      char hex1 = src[inpos + 1];

      if ((hex1 >= '0') && (hex1 <= '9'))
        c = (hex1 - '0');
      else if ((hex1 >= 'a') && (hex1 <= 'f'))
        c = (hex1 - 'a') + 10;
      else if ((hex1 >= 'A') && (hex1 <= 'F'))
        c = (hex1 - 'A') + 10;

      char hex2 = src[inpos + 2];

      if ((hex2 >= '0') && (hex2 <= '9'))
        c = c * 16 + (hex2 - '0');
      else if ((hex2 >= 'a') && (hex2 <= 'f'))
        c = c * 16 + (hex2 - 'a') + 10;
      else if ((hex2 >= 'A') && (hex2 <= 'F'))
        c = c * 16 + (hex2 - 'A') + 10;

      dst[outpos] = (char)c;
      inpos += 3;
    } else {
      dst[outpos] = src[inpos];
      inpos++;
    }

    outpos++;
  }

  if (inpos > outpos) {
    dst[outpos] = 0x00;
  }

  return dst;
}

static int _uri_parse_scheme(struct uri *uri, const char **pos, const char *end,
                             struct dbg *dbg) {

  const char *p = *pos;

  while (p < end && *p != ':') {
    if (!SCHEME_SAFE_CHAR(*p)) {
      error("uri scheme contains invalid character : '%c' (0x%02x)", *p, *p);
      goto err;
    }
    p++;
  }

  if (*pos == p) {
    error("uri is missing scheme");
    goto err;
  }

  URI_SET_STR(uri->scheme, *pos, p);

  /*
   * RFC 3986 - Generic
   * https://datatracker.ietf.org/doc/html/rfc3986#section-3.1
   *
   * « Although schemes are case- insensitive, the canonical form is lowercase
   * and documents that specify schemes must do so with lowercase letters.
   * An implementation should accept uppercase letters as equivalent to
   * lowercase in scheme names (e.g., allow "HTTP" as well as "http") for the
   * sake of robustness but should only produce lowercase scheme names for
   * consistency.»
   */

  char *pp = uri->scheme;

  while (*pp) {
    *pp = tolower(*pp);
    pp++;
  }

  _uri_scheme2schemeType(uri);

  *pos = ++p; /* Skips ':' */

  return 1;

err:
  return -1;
}

static int _uri_parse_authority(struct uri *uri, const char **pos,
                                const char *end, struct dbg *dbg) {

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

  if (*(*pos) != '/' || *((*pos) + 1) != '/') {
    /* uri has no authority */
    if (uri->scheme_t == URI_SCHEME_T_FILE) {
      uri->flags |= URI_T_LOCALHOST;
    }
    // uri->flags |= URI_T_LOCALHOST;
    // uri->flags |= URI_T_HOST_EMPTY;
    return 0;
  }

  *pos += 2;
  const char *p = *pos;

  while (p < end && *p != '/' && (!SCHEME_ALLOW_QUERY(uri) || *p != '?') &&
         (!SCHEME_ALLOW_FRAGMENT(uri) || *p != '#')) {
    p++;
  }

  URI_SET_STR(uri->authority, *pos, p);

  if (*uri->authority == 0x00) {
    uri->flags |= URI_T_LOCALHOST;
    /* TODO: return 0 ? */
  }

  return 1;

err:
  return -1;
}

static int _uri_parse_userinfo(struct uri *uri, const char **pos,
                               const char *end, struct dbg *dbg) {

  int hasUserinfo = 0;
  int userinfoIllegalCharacters = 0;

  const char *p = *pos;

  while (p < end &&
         /* end of authority */
         *p != '/' && (!SCHEME_ALLOW_QUERY(uri) || *p != '?') &&
         (!SCHEME_ALLOW_FRAGMENT(uri) || *p != '#')) {

    if (*p == '@') {
      hasUserinfo = 1;
      break;
    }

    if (!USERINFO_SAFE_CHAR(p)) {
      userinfoIllegalCharacters++;
    }

    p++;
  }

  if (!hasUserinfo) {
    return 0;
  }

  if (userinfoIllegalCharacters > 0) {
    error("uri userinfo contains %i invalid char%s", userinfoIllegalCharacters,
          (userinfoIllegalCharacters > 1) ? "s" : "");
    goto err;
  }

  /* user / pass */

  URI_SET_STR(uri->userinfo, *pos, p);

  *pos = p + 1; // skips '@'

  const char *subpos = NULL;
  p = uri->userinfo;

  while (1) {
    if (!*p) {
      if (subpos) {
        URI_SET_STR(uri->pass, subpos, p);
      } else {
        URI_SET_STR(uri->user, uri->userinfo, p);
      }
      break;
    } else if (*p == ':') {
      URI_SET_STR(uri->user, uri->userinfo, p);
      subpos = p + 1;
    }
    p++;
  }

  if (uri->opts & URI_OPT_DECODE_USERINFO && uri->userinfo) {
    uriDecodeString(uri->userinfo, NULL);
  }

  if (uri->opts & URI_OPT_DECODE_USERPASS) {
    if (uri->user)
      uriDecodeString(uri->user, NULL);
    if (uri->pass)
      uriDecodeString(uri->pass, NULL);
  }

  return 1;

err:
  return -1;
}

static int _uri_parse_hostname(struct uri *uri, const char **pos,
                               const char *end, struct dbg *dbg) {

  const char *p = *pos;

  if (**pos == '[') {
    /*
     * IPv6 - RFC 2732
     * https://datatracker.ietf.org/doc/html/rfc2732
     */
    (*pos)++; // skips '['

    while (p < end && *p != ']') {
      p++;
    }

    URI_SET_STR(uri->host, *pos, p);

    char *iperr = NULL;
    int rc = 0;
    if ((rc = uriIsIPv6(uri->host, strlen(uri->host), &iperr))) {
      uri->flags |= URI_T_HOST_IPV6;
      if (rc == 2) {
        uri->flags |= URI_T_LOCALHOST;
      }
    } else {
      error("URI IPv6 Parser error : %s\n", iperr);
      free(iperr);
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

    while (p < end && *p != '/' && // if URI contains a path
           *p != ':' &&            // if URI has an explicit port
           (!SCHEME_ALLOW_QUERY(uri) || *p != '?') &&
           (!SCHEME_ALLOW_FRAGMENT(uri) || *p != '#')) {
      p++;
    }

    // debug( " >>> %.*s", (int)(p-*pos), p );

    URI_SET_STR(uri->host, *pos, p);
  }

  // if ( !(uri->flags & URI_T_HOST_IPV6 || uri->flags & URI_T_HOST_EMPTY) ) {
  if (!(uri->flags & URI_T_HOST_IPV6) && uri->host != NULL &&
      *uri->host != 0x00) {

    if (uriIsIPv4(uri->host, strlen(uri->host), NULL)) {
      uri->flags &= ~URI_T_HOST_MASK;
      uri->flags |= URI_T_HOST_IPV4;
      if (strcmp(uri->host, "127.0.0.1") == 0) {
        uri->flags |= URI_T_LOCALHOST;
      }
    } else if (strcmp(uri->host, "localhost") == 0) {
      uri->flags |= URI_T_LOCALHOST;
    } else {
      uri->flags |= URI_T_HOST_REGNAME;
    }

    if (uri->opts & URI_OPT_DECODE_HOSTNAME) {
      uriDecodeString(uri->host, NULL);
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

    while (p < end && *p != '/' && (!SCHEME_ALLOW_QUERY(uri) || *p != '?') &&
           (!SCHEME_ALLOW_FRAGMENT(uri) || *p != '#')) {
      if (!IS_DIGIT(*p)) {
        error("URI port contains non-digit char : %c (0x%02x).\n", *p, *p);
        goto err;
      }
      p++;
    }

    uri->port = atoi(*pos);
  }

  *pos = p; // keeps next char, first path '/'

  return 1;

err:
  return -1;
}

static int _uri_parse_path(struct uri *uri, const char **pos, const char *end,
                           struct dbg *dbg) {

  int winDrive = 0;

  /* sanitize start of path : ignores all slashes (after already parsed '//'
   * identifying start of authority) */

  while (*(*pos + 1) == '/') {
    (*pos)++;
  }

  if (*(*pos) == '/' && WINDOWS_DRIVE_LETTER(((*pos) + 1))) {
    /*
     * Windows Drive (c: / c|) - RFC 8089
     * https://datatracker.ietf.org/doc/html/rfc8089#appendix-E.2.2
     */

    (*pos)++; /* moves forward last slash before driver letter, so path starts
                 at the letter with no slash before. */
    winDrive = 1;
  }

  const char *p = *pos;

  while (p < end && (!SCHEME_ALLOW_QUERY(uri) || *p != '?') &&
         (!SCHEME_ALLOW_FRAGMENT(uri) || *p != '#')) {
    p++;
  }

  // debug( " >>> (%i) %.*s", (int)(p-*pos), (int)(p-*pos), p );

  URI_SET_STR(uri->path, *pos, p);

  if (winDrive) {
    if (uri->path[1] == '|') {
      /*
       * https://datatracker.ietf.org/doc/html/rfc8089#appendix-E.2.2
       * « To update such an old URI, replace the vertical line "|" with a colon
       * ":" »
       */
      uri->path[1] = ':';
    }
  }

  if (uri->opts & URI_OPT_DECODE_PATH) {
    uriDecodeString(uri->path, NULL);
  }

  *pos = p;

  return 1;

err:
  return -1;
}

static int _uri_parse_query(struct uri *uri, const char **pos, const char *end,
                            struct dbg *dbg) {

  const char *p = *pos;

  if (!(uri->opts & URI_OPT_IGNORE_QUERY) && **pos == '?') {

    while (p < end && *p != '#') {
      p++;
    }

    (*pos)++; // skips '?'

    URI_SET_STR(uri->query, *pos, p);

    if (uri->opts & URI_OPT_DECODE_QUERY) {
      uriDecodeString(uri->query, NULL);
    }

    *pos = p;
  }

  return 1;

err:
  return -1;
}

static int _uri_parse_fragment(struct uri *uri, const char **pos,
                               const char *end, struct dbg *dbg) {

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

  const char *p = *pos;

  if (!(uri->opts & URI_OPT_IGNORE_FRAGMENT) && **pos == '#') {

    while (p < end) {
      p++;
    }

    (*pos)++; // skips '#'

    URI_SET_STR(uri->fragment, *pos, p);

    if (uri->opts & URI_OPT_DECODE_FRAGMENT) {
      uriDecodeString(uri->fragment, NULL);
    }

    *pos = ++p; // skips '#'
  }

  return 1;

err:
  return -1;
}

struct uri *uriParse(const char *uristr, enum uri_option optflags,
                     struct dbg *dbg) {

  if (uristr == NULL) {
    return NULL;
  }

  struct uri *uri = calloc(1, sizeof(struct uri));

  if (uri == NULL) {
    return NULL;
  }

  size_t urilen = strlen(uristr);

  if (urilen >= MAX_URI_LENGTH) {
    error("uri is too long");
    goto err;
  }

  uri->opts = optflags;

  const char *pos = uristr;
  const char *end = pos + urilen;

  _uri_parse_scheme(uri, &pos, end, dbg);

  if (_uri_parse_authority(uri, &pos, end, dbg)) {
    _uri_parse_userinfo(uri, &pos, end, dbg);
    _uri_parse_hostname(uri, &pos, end, dbg);
  }

  _uri_parse_path(uri, &pos, end, dbg);

  if (SCHEME_ALLOW_QUERY(uri)) {
    _uri_parse_query(uri, &pos, end, dbg);
  }

  if (SCHEME_ALLOW_FRAGMENT(uri)) {
    _uri_parse_fragment(uri, &pos, end, dbg);
  }

  goto end;

err:
  uriFree(uri);
  uri = NULL;

end:

  return uri;
}

void uriFree(struct uri *uri) {
  if (uri == NULL) {
    return;
  }
  if (NULL != uri->scheme) {
    free(uri->scheme);
  }
  if (NULL != uri->userinfo) {
    free(uri->userinfo);
  }
  if (NULL != uri->authority) {
    free(uri->authority);
  }
  if (NULL != uri->user) {
    free(uri->user);
  }
  if (NULL != uri->pass) {
    free(uri->pass);
  }
  if (NULL != uri->host) {
    free(uri->host);
  }
  if (NULL != uri->path) {
    free(uri->path);
  }
  if (NULL != uri->query) {
    free(uri->query);
  }
  if (NULL != uri->fragment) {
    free(uri->fragment);
  }

  free(uri);
}

int uriIsIPv4(const char *s, int size, char **err) {

  int octets = 0;
  const char *currentOctetStart = s;

  char prev = 0;

  for (int i = 0; i <= size; i++) {

    if (prev == 0) {

      if (IS_DIGIT(*(s + i))) {
        currentOctetStart = (s + i);
        prev = 'd';
        continue;
      }

      if (*(s + i) == '.') {
        if (err) {
          _laaf_util_snprintf_realloc(
              err, NULL, 0,
              "IPV4 parser error : can't start with a single '.'");
        }
        return 0;
      }
    }

    if (prev == 'p') {

      if (IS_DIGIT(*(s + i))) {
        currentOctetStart = (s + i);
        prev = 'd';
        continue;
      }

      if (*(s + i) == '.') {
        if (err) {
          _laaf_util_snprintf_realloc(
              err, NULL, 0, "IPV4 parser error : can't have successive '.'");
        }
        return 0;
      }
    }

    if (prev == 'd') {

      if (IS_DIGIT(*(s + i))) {
        prev = 'd';
        continue;
      }

      if (i == size || *(s + i) == '.') { // period
        int octet = atoi(currentOctetStart);
        if (octet > 255) {
          if (err) {
            _laaf_util_snprintf_realloc(
                err, NULL, 0, "IPV4 parser error : octet %i is too high : %.*s",
                (octets), (int)((s + i) - currentOctetStart),
                currentOctetStart);
          }
          return 0;
        }

        if (i + 1 == size) {
          if (err) {
            _laaf_util_snprintf_realloc(
                err, NULL, 0,
                "IPV4 parser error : can't end with a single '.'");
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
      _laaf_util_snprintf_realloc(
          err, NULL, 0, "IPV4 parser error : illegal char '%c' (0x%02x)",
          *(s + i), *(s + i));
    }
    return 0;
  }

  if (octets > 4) {
    if (err) {
      _laaf_util_snprintf_realloc(err, NULL, 0,
                                  "IPV4 parser error : too many octets");
    }
    return 0;
  }
  if (octets < 4) {
    if (err) {
      _laaf_util_snprintf_realloc(err, NULL, 0,
                                  "IPV4 parser error : not enough octets");
    }
    return 0;
  }

  return 1;
}

int uriIsIPv6(const char *s, int size, char **err) {

  int segmentCount = 0;
  int emptySegmentCount = 0;
  int curSegmentLength = 0;
  int ipv4portion = 0;

  int loopback = 0;

  const char *curSegmentStart = s;

  char prev = 0;

  for (int i = 0; i <= size; i++) {

    if (prev == 0) {

      if (IS_HEX(*(s + i))) {
        segmentCount++;
        curSegmentStart = s + i;
        curSegmentLength++;
        prev = 'h'; // hex

        if (loopback >= 0) {
          if (!IS_DIGIT(*(s + i))) {
            loopback = -1;
          } else {
            loopback += (*(s + i) - '0'); // atoi(*(s+i));
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
          _laaf_util_snprintf_realloc(err, NULL, 0,
                                      "can't start with a single ':'");
        }
        return 0;
      }
    }

    if (prev == 'h') { /* hex */

      if (IS_HEX(*(s + i))) {

        curSegmentLength++;

        if (loopback >= 0) {
          if (!IS_DIGIT(*(s + i))) {
            loopback = -1;
          } else {
            loopback += (*(s + i) - '0');
          }
        }

        continue;
      }

      if (*(s + i) == '.') { /* period */
        int octet = atoi(curSegmentStart);
        if (octet > 255) {
          if (err) {
            _laaf_util_snprintf_realloc(
                err, NULL, 0, "ipv4 portion octet %i is too high : %.*s",
                (ipv4portion), curSegmentLength, curSegmentStart);
          }
          return 0;
        }
        // debug( "%i", octet );
        prev = 'p';
        ipv4portion++;
        continue;
      }

      if (i == size || *(s + i) == ':') {
        if (curSegmentLength > 4) {
          if (err) {
            _laaf_util_snprintf_realloc(
                err, NULL, 0, "segment %i is too long : %.*s",
                (segmentCount - 1), curSegmentLength, curSegmentStart);
          }
          return 0;
        }
        /* here we can parse segment */
        curSegmentStart = NULL;
        curSegmentLength = 0;

        if (i < size && *(s + (i + 1)) == ':') {
          emptySegmentCount++;
          prev = 'e', /* empty "::" */
              i++;
        } else if (i + 1 == size) {
          if (err) {
            _laaf_util_snprintf_realloc(err, NULL, 0,
                                        "can't end with a single ':'");
          }
          return 0;
        } else {
          prev = 'c'; /* colon ":" */
        }
        continue;
      }
    }

    if (prev == 'e' || prev == 'c') { /* empty or colon */

      if (IS_HEX(*(s + i))) {
        segmentCount++;
        curSegmentStart = s + i;
        curSegmentLength++;
        prev = 'h'; /* hex */

        if (loopback >= 0) {
          if (!IS_DIGIT(*(s + i))) {
            loopback = -1;
          } else {
            loopback += (*(s + i) - '0');
          }
        }

        continue;
      }

      if (*(s + i) == ':') {
        if (err) {
          _laaf_util_snprintf_realloc(
              err, NULL, 0, "can't have more than two successive ':'");
        }
        return 0;
      }
    }

    if (prev == 'p') {

      if (IS_DIGIT(*(s + i))) {
        curSegmentStart = s + i;
        prev = 'd';
        continue;
      }

      if (*(s + i) == '.') {
        if (err) {
          _laaf_util_snprintf_realloc(err, NULL, 0,
                                      "can't have successive '.'");
        }
        return 0;
      }
    }

    if (prev == 'd') {

      if (IS_DIGIT(*(s + i))) {
        prev = 'd';
        continue;
      }

      if (i == size || *(s + i) == '.') { /* period */
        int octet = atoi(curSegmentStart);
        if (octet > 255) {
          if (err) {
            _laaf_util_snprintf_realloc(
                err, NULL, 0, "ipv4 portion octet %i is too high : %.*s",
                (ipv4portion), curSegmentLength, curSegmentStart);
          }
          return 0;
        }

        // debug( "%i", octet );

        if (i + 1 == size) {
          if (err) {
            _laaf_util_snprintf_realloc(err, NULL, 0,
                                        "can't end with a single '.'");
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
      _laaf_util_snprintf_realloc(err, NULL, 0, "illegal char '%c' (0x%02x)",
                                  *(s + i), *(s + i));
    }

    return 0;
  }

  // debug( "segments : %i", segmentCount );
  // debug( "empty segments : %i", emptySegmentCount );
  // debug( "ipv4portion : %i", ipv4portion );

  if (ipv4portion > 4) {
    if (err) {
      _laaf_util_snprintf_realloc(err, NULL, 0,
                                  "too many octets in ipv4 portion");
    }
    return 0;
  }
  if (ipv4portion > 0 && ipv4portion < 4) {
    if (err) {
      _laaf_util_snprintf_realloc(err, NULL, 0,
                                  "not enough octets in ipv4 portion");
    }
    return 0;
  }
  if (emptySegmentCount + (segmentCount / 2) + ipv4portion > 8) {
    if (err) {
      _laaf_util_snprintf_realloc(err, NULL, 0, "too many segments");
    }
    return 0;
  }

  if (emptySegmentCount == 0 && (((ipv4portion / 2) + segmentCount) < 8)) {
    if (err) {
      _laaf_util_snprintf_realloc(err, NULL, 0, "not enough segments");
    }
    return 0;
  }

  // debug( "LOCALHOST >>>>>>> %i", loopback );

  /*
   * 1: valid ipv6 address
   * 2: valid ipv6 address and is loopback
   */

  return (loopback == 1) ? 2 : 1;
}

static void _uri_scheme2schemeType(struct uri *uri) {
  if (strcmp(uri->scheme, "afp") == 0) {
    uri->scheme_t = URI_SCHEME_T_AFP;
  } else if (strcmp(uri->scheme, "cifs") == 0) {
    uri->scheme_t = URI_SCHEME_T_CIFS;
  } else if (strcmp(uri->scheme, "data") == 0) {
    uri->scheme_t = URI_SCHEME_T_DATA;
  } else if (strcmp(uri->scheme, "dns") == 0) {
    uri->scheme_t = URI_SCHEME_T_DNS;
  } else if (strcmp(uri->scheme, "file") == 0) {
    uri->scheme_t = URI_SCHEME_T_FILE;
  } else if (strcmp(uri->scheme, "ftp") == 0) {
    uri->scheme_t = URI_SCHEME_T_FTP;
  } else if (strcmp(uri->scheme, "http") == 0) {
    uri->scheme_t = URI_SCHEME_T_HTTP;
  } else if (strcmp(uri->scheme, "https") == 0) {
    uri->scheme_t = URI_SCHEME_T_HTTPS;
  } else if (strcmp(uri->scheme, "imap") == 0) {
    uri->scheme_t = URI_SCHEME_T_IMAP;
  } else if (strcmp(uri->scheme, "irc") == 0) {
    uri->scheme_t = URI_SCHEME_T_IRC;
  } else if (strcmp(uri->scheme, "mailto") == 0) {
    uri->scheme_t = URI_SCHEME_T_MAILTO;
  } else if (strcmp(uri->scheme, "nfs") == 0) {
    uri->scheme_t = URI_SCHEME_T_NFS;
  } else if (strcmp(uri->scheme, "pop") == 0) {
    uri->scheme_t = URI_SCHEME_T_POP;
  } else if (strcmp(uri->scheme, "rtsp") == 0) {
    uri->scheme_t = URI_SCHEME_T_RTSP;
  } else if (strcmp(uri->scheme, "sftp") == 0) {
    uri->scheme_t = URI_SCHEME_T_SFTP;
  } else if (strcmp(uri->scheme, "sip") == 0) {
    uri->scheme_t = URI_SCHEME_T_SIP;
  } else if (strcmp(uri->scheme, "smb") == 0) {
    uri->scheme_t = URI_SCHEME_T_SMB;
  } else if (strcmp(uri->scheme, "ssh") == 0) {
    uri->scheme_t = URI_SCHEME_T_SSH;
  } else if (strcmp(uri->scheme, "tel") == 0) {
    uri->scheme_t = URI_SCHEME_T_TEL;
  } else if (strcmp(uri->scheme, "telnet") == 0) {
    uri->scheme_t = URI_SCHEME_T_TELNET;
  } else {
    uri->scheme_t = URI_SCHEME_T_UNKNOWN;
  }
}

static int _laaf_util_snprintf_realloc(char **str, size_t *size, size_t offset,
                                       const char *format, ...) {
  size_t tmpsize = 0;

  if (!size) {
    size = &tmpsize;
  }

  int retval, needed;
  va_list ap;

  va_start(ap, format);

  while (0 <= (retval =
                   vsnprintf((*str) + offset, (*size) - offset, format, ap)) &&
         (int64_t)((*size) - offset) < (needed = retval + 1)) {
    va_end(ap);

    *size *= 2;

    if ((int64_t)((*size) - offset) < needed)
      *size = needed;

    char *p = realloc(*str, *size);

    if (p) {
      *str = p;
    } else {
      free(*str);
      *str = NULL;
      *size = 0;
      return -1;
    }

    va_start(ap, format);
  }

  va_end(ap);

  return retval;
}

#ifdef BUILD_URI_TEST

static int _uri_cmp(const struct uri *a, const struct uri *b) {

  int differenciesCount = 0;

  if (a == NULL || b == NULL) {
    return -1;
  }

  // if ( (strcmp((a->scheme) ? a->scheme : "", (b->scheme) ? b->scheme : "") !=
  // 0 ) ) {
  //   differenciesCount++;
  // }
  if ((strcmp((a->userinfo) ? a->userinfo : "",
              (b->userinfo) ? b->userinfo : "") != 0)) {
    differenciesCount++;
  }
  if ((strcmp((a->user) ? a->user : "", (b->user) ? b->user : "") != 0)) {
    differenciesCount++;
  }
  if ((strcmp((a->pass) ? a->pass : "", (b->pass) ? b->pass : "") != 0)) {
    differenciesCount++;
  }
  if ((strcmp((a->host) ? a->host : "", (b->host) ? b->host : "") != 0)) {
    differenciesCount++;
  }
  if ((strcmp((a->path) ? a->path : "", (b->path) ? b->path : "") != 0)) {
    differenciesCount++;
  }
  if ((strcmp((a->query) ? a->query : "", (b->query) ? b->query : "") != 0)) {
    differenciesCount++;
  }
  if ((strcmp((a->fragment) ? a->fragment : "",
              (b->fragment) ? b->fragment : "") != 0)) {
    differenciesCount++;
  }
  if (a->port != b->port) {
    differenciesCount++;
  }
  if (a->scheme_t != b->scheme_t) {
    differenciesCount++;
  }
  if (a->flags != b->flags) {
    differenciesCount++;
  }

  return differenciesCount;
}

static void _uri_dump_diff(struct uri *a, struct uri *b,
                           int totalDifferencies) {

  int differenciesCount = 0;

  if (a == NULL || b == NULL) {
    return;
  }

  // if ( (strcmp((a->scheme) ? a->scheme : "", (b->scheme) ? b->scheme : "") !=
  // 0 ) ) {
  //   printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .scheme :
  //   \"%s\" (expected: \"%s\")\n", (++differenciesCount < totalDifferencies) ?
  //   "\u251c\u2500\u2500\u25fb" : "\u2514\u2500\u2500\u25fb", a->scheme,
  //   b->scheme );
  // }
  if ((strcmp((a->userinfo) ? a->userinfo : "",
              (b->userinfo) ? b->userinfo : "") != 0)) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .userinfo : "
           "\"%s\" (expected: \"%s\")\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->userinfo, b->userinfo);
  }
  if ((strcmp((a->user) ? a->user : "", (b->user) ? b->user : "") != 0)) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .user : "
           "\"%s\" (expected: \"%s\")\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->user, b->user);
  }
  if ((strcmp((a->pass) ? a->pass : "", (b->pass) ? b->pass : "") != 0)) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .pass : "
           "\"%s\" (expected: \"%s\")\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->pass, b->pass);
  }
  if ((strcmp((a->host) ? a->host : "", (b->host) ? b->host : "") != 0)) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .host : "
           "\"%s\" (expected: \"%s\")\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->host, b->host);
  }
  if ((strcmp((a->path) ? a->path : "", (b->path) ? b->path : "") != 0)) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .path : "
           "\"%s\" (expected: \"%s\")\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->path, b->path);
  }
  if ((strcmp((a->query) ? a->query : "", (b->query) ? b->query : "") != 0)) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .query : "
           "\"%s\" (expected: \"%s\")\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->query, b->query);
  }
  if ((strcmp((a->fragment) ? a->fragment : "",
              (b->fragment) ? b->fragment : "") != 0)) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .fragment : "
           "\"%s\" (expected: \"%s\")\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->fragment, b->fragment);
  }

  if (a->port != b->port) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .port : %i "
           "(expected: %i)\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->port, b->port);
  }
  if (a->scheme_t != b->scheme_t) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .scheme_t : "
           "%i (expected: %i)\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->scheme_t, b->scheme_t);
  }
  if (a->flags != b->flags) {
    printf("      \x1b[38;5;242m\u2502\x1b[0m   \x1b[38;5;124m%s .flags : %i "
           "(expected: %i)\n",
           (++differenciesCount < totalDifferencies)
               ? "\u251c\u2500\u2500\u25fb"
               : "\u2514\u2500\u2500\u25fb",
           a->flags, b->flags);
  }
}

static int _uri_test(const char *uristr, enum uri_option optflags,
                     struct uri expectedRes, int line) {

  struct uri *uri = uriParse(uristr, optflags);

  int differenciesCount = 0;

  if ((differenciesCount = _uri_cmp(uri, &expectedRes)) == 0) {
    printf("\x1b[38;5;242m"); // dark gray
    printf("%05i", line);
    printf("\x1b[0m");

    printf("\x1b[38;5;242m %s \x1b[0m", "\u2502");

    printf("\x1b[38;5;120m"); // green
    printf("[ok] ");
    printf("\x1b[0m");

    printf("\x1b[38;5;242m"); // dark gray
    printf("%s", uristr);
    printf("\x1b[0m");

    printf("\n");
  } else {
    printf("\x1b[38;5;124m"); // red
    printf("%05i", line);
    printf("\x1b[0m");

    printf("\x1b[38;5;242m %s \x1b[0m", "\u2502");

    printf("\x1b[38;5;124m"); // red
    printf("[er] ");
    printf("\x1b[0m");

    printf("\x1b[38;5;242m"); // dark gray
    printf("%s", uristr);
    printf("\x1b[0m");

    printf("\n");

    printf("\x1b[38;5;124m"); // red
    _uri_dump_diff(uri, &expectedRes, differenciesCount);
    printf("\x1b[0m");

    printf("      \x1b[38;5;242m\u2502\x1b[0m\n");
  }

  uriFree(uri);

  return differenciesCount;
}

int main(void) {

  int rc = 0;

  // rc += _uri_test( "", URI_OPT_NONE, (struct uri){ .scheme_t =
  // URI_SCHEME_T_UNKNOWN, .host = NULL, .port = 0, .path = NULL, .query = NULL,
  // .fragment = NULL }, __LINE__ );
  rc += _uri_test("https://www.server.com", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = NULL,
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://user:pass@www.server.com", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .userinfo = "user:pass",
                               .user = "user",
                               .pass = "pass",
                               .host = "www.server.com",
                               .port = 0,
                               .path = NULL,
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("HTTPS://www.server.com", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = NULL,
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("hTtPs://www.server.com", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = NULL,
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com:8080", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 8080,
                               .path = NULL,
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com:8080?foo=bar", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 8080,
                               .path = NULL,
                               .query = "foo=bar",
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com:8080#anchor", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 8080,
                               .path = NULL,
                               .query = NULL,
                               .fragment = "anchor",
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com/", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = "/",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com/?foo=bar", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = "/",
                               .query = "foo=bar",
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com/////?foo=bar", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = "/",
                               .query = "foo=bar",
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com///////", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = "/",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com?foo=bar", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = NULL,
                               .query = "foo=bar",
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com#anchor", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = NULL,
                               .query = NULL,
                               .fragment = "anchor",
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test(
      "https://www.server.com/path/to/file.html?foo=bar&foo2=bar2#anchor",
      URI_OPT_NONE,
      (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                   .host = "www.server.com",
                   .port = 0,
                   .path = "/path/to/file.html",
                   .query = "foo=bar&foo2=bar2",
                   .fragment = "anchor",
                   .flags = URI_T_HOST_REGNAME},
      __LINE__);
  rc += _uri_test("https://www.server.com:80/", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 80,
                               .path = "/",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com:/", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = "/",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);
  rc += _uri_test("https://www.server.com:", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "www.server.com",
                               .port = 0,
                               .path = "",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);

  rc += _uri_test("https://[8:3:1:2:1234:5678::]:8080/ipv6", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "8:3:1:2:1234:5678::",
                               .port = 8080,
                               .path = "/ipv6",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_IPV6},
                  __LINE__);
  rc +=
      _uri_test("https://[2001:db8:0:85a3::ac1f:8001]:8080/ipv6", URI_OPT_NONE,
                (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                             .host = "2001:db8:0:85a3::ac1f:8001",
                             .port = 8080,
                             .path = "/ipv6",
                             .query = NULL,
                             .fragment = NULL,
                             .flags = URI_T_HOST_IPV6},
                __LINE__);
  rc += _uri_test(
      "https://user:pass@[2001:db8:3333:4444:5555:6666:1.2.3.4]:8080/ipv6",
      URI_OPT_NONE,
      (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                   .userinfo = "user:pass",
                   .user = "user",
                   .pass = "pass",
                   .host = "2001:db8:3333:4444:5555:6666:1.2.3.4",
                   .port = 8080,
                   .path = "/ipv6",
                   .query = NULL,
                   .fragment = NULL,
                   .flags = URI_T_HOST_IPV6},
      __LINE__);
  rc += _uri_test("https://192.168.0.1:8080/ipv4", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "192.168.0.1",
                               .port = 8080,
                               .path = "/ipv4",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_IPV4},
                  __LINE__);
  rc += _uri_test("https://127.0.0.1:8080/ipv4loopback", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "127.0.0.1",
                               .port = 8080,
                               .path = "/ipv4loopback",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_IPV4 | URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("https://localhost:8080/loopback", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "localhost",
                               .port = 8080,
                               .path = "/loopback",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("https://[0:0:0:0:0:0:0:1]:8080/ipv6loopback", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "0:0:0:0:0:0:0:1",
                               .port = 8080,
                               .path = "/ipv6loopback",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_IPV6 | URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("https://[::0:0:0:1]:8080/ipv6loopback", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "::0:0:0:1",
                               .port = 8080,
                               .path = "/ipv6loopback",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_IPV6 | URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("https://[::0:0000:0:001]:8080/ipv6loopback", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "::0:0000:0:001",
                               .port = 8080,
                               .path = "/ipv6loopback",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_IPV6 | URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("https://[::1]:8080/ipv6loopback", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .host = "::1",
                               .port = 8080,
                               .path = "/ipv6loopback",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_IPV6 | URI_T_LOCALHOST},
                  __LINE__);

  rc += _uri_test("https://user:pass@192.168.0.1:8080/ipv4", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .userinfo = "user:pass",
                               .user = "user",
                               .pass = "pass",
                               .host = "192.168.0.1",
                               .port = 8080,
                               .path = "/ipv4",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_IPV4},
                  __LINE__);

  rc += _uri_test("file://///C:/windows/path", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                               .host = NULL,
                               .port = 0,
                               .path = "C:/windows/path",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("file:C:/windows/path", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                               .host = NULL,
                               .port = 0,
                               .path = "C:/windows/path",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("file:/C:/windows/path", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                               .host = NULL,
                               .port = 0,
                               .path = "C:/windows/path",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("file:///C:/windows/path", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                               .host = NULL,
                               .port = 0,
                               .path = "C:/windows/path",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("file://?/C:/windows/path", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                               .host = NULL,
                               .port = 0,
                               .path = "C:/windows/path",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test("file://./C:/windows/path", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                               .host = NULL,
                               .port = 0,
                               .path = "C:/windows/path",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_LOCALHOST},
                  __LINE__);

  // Examples from AAF files external essences
  rc +=
      _uri_test("file:///C:/Users/username/Downloads/441-16b.wav", URI_OPT_NONE,
                (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                             .host = NULL,
                             .port = 0,
                             .path = "C:/Users/username/Downloads/441-16b.wav",
                             .query = NULL,
                             .fragment = NULL,
                             .flags = URI_T_LOCALHOST},
                __LINE__);
  rc += _uri_test("file://?/E:/ADPAAF/Sequence A Rendu.mxf", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                               .host = NULL,
                               .port = 0,
                               .path = "E:/ADPAAF/Sequence A Rendu.mxf",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test(
      "file:////C:/Users/username/Desktop/TEST2977052.aaf", URI_OPT_NONE,
      (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                   .host = NULL,
                   .port = 0,
                   .path = "C:/Users/username/Desktop/TEST2977052.aaf",
                   .query = NULL,
                   .fragment = NULL,
                   .flags = URI_T_LOCALHOST},
      __LINE__);
  rc += _uri_test("file://localhost/Users/username/Music/fonk_2_3#04.wav",
                  URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                               .host = "localhost",
                               .port = 0,
                               .path = "/Users/username/Music/fonk_2_3#04.wav",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_LOCALHOST},
                  __LINE__);
  rc += _uri_test(
      "file://10.87.230.71/mixage/DR2/Avid MediaFiles/MXF/1/3572607.mxf",
      URI_OPT_NONE,
      (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                   .host = "10.87.230.71",
                   .port = 0,
                   .path = "/mixage/DR2/Avid MediaFiles/MXF/1/3572607.mxf",
                   .query = NULL,
                   .fragment = NULL,
                   .flags = URI_T_HOST_IPV4},
      __LINE__);
  rc += _uri_test(
      "file:///_system/Users/username/pt2MCCzmhsFRHQgdgsTMQX.mxf", URI_OPT_NONE,
      (struct uri){.scheme_t = URI_SCHEME_T_FILE,
                   .host = NULL,
                   .port = 0,
                   .path = "/_system/Users/username/pt2MCCzmhsFRHQgdgsTMQX.mxf",
                   .query = NULL,
                   .fragment = NULL,
                   .flags = URI_T_LOCALHOST},
      __LINE__);

  // URL Percent Decoding
  rc += _uri_test(
      "https://www.server.com/NON_DECODING/"
      "%C2%B0%2B%29%3D%C5%93%21%3A%3B%2C%3F.%2F%C2%A7%C3%B9%2A%24%C2%B5%C2%A3%"
      "7D%5D%E2%80%9C%23%7B%5B%7C%5E%40%5D%3C%3E",
      URI_OPT_NONE,
      (struct uri){
          .scheme_t = URI_SCHEME_T_HTTPS,
          .host = "www.server.com",
          .port = 0,
          .path = "/NON_DECODING/"
                  "%C2%B0%2B%29%3D%C5%93%21%3A%3B%2C%3F.%2F%C2%A7%C3%B9%2A%24%"
                  "C2%B5%C2%A3%7D%5D%E2%80%9C%23%7B%5B%7C%5E%40%5D%3C%3E",
          .query = NULL,
          .fragment = NULL,
          .flags = URI_T_HOST_REGNAME},
      __LINE__);
  rc +=
      _uri_test("https://www.server.com/DECODING/"
                "%C2%B0%2B%29%3D%C5%93%21%3A%3B%2C%3F.%2F%C2%A7%C3%B9%2A%24%C2%"
                "B5%C2%A3%7D%5D%E2%80%9C%23%7B%5B%7C%5E%40%5D%3C%3E",
                URI_OPT_DECODE_ALL,
                (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                             .host = "www.server.com",
                             .port = 0,
                             .path = "/DECODING/°+)=œ!:;,?./§ù*$µ£}]“#{[|^@]<>",
                             .query = NULL,
                             .fragment = NULL,
                             .flags = URI_T_HOST_REGNAME},
                __LINE__);
  rc += _uri_test("https://www.server.com/DECODING_UTF8/"
                  "%E3%82%B5%E3%83%B3%E3%83%97%E3%83%AB%E7%B2%BE%E5%BA%A6%E7%"
                  "B7%A8%E9%9B%86",
                  URI_OPT_DECODE_ALL,
                  (struct uri){.scheme_t = URI_SCHEME_T_HTTPS,
                               .userinfo = NULL,
                               .user = NULL,
                               .pass = NULL,
                               .host = "www.server.com",
                               .port = 0,
                               .path = "/DECODING_UTF8/サンプル精度編集",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = URI_T_HOST_REGNAME},
                  __LINE__);

  // Examples from https://en.wikipedia.org/wiki/Uniform_Resource_Identifier
  rc += _uri_test("tel:+1-816-555-1212", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_TEL,
                               .userinfo = NULL,
                               .user = NULL,
                               .pass = NULL,
                               .host = NULL,
                               .port = 0,
                               .path = "+1-816-555-1212",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = 0},
                  __LINE__);
  rc += _uri_test("mailto:John.Doe@example.com", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_MAILTO,
                               .userinfo = NULL,
                               .user = NULL,
                               .pass = NULL,
                               .host = NULL,
                               .port = 0,
                               .path = "John.Doe@example.com",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = 0},
                  __LINE__);
  rc += _uri_test(
      "urn:oasis:names:specification:docbook:dtd:xml:4.1.2", URI_OPT_NONE,
      (struct uri){.scheme_t = URI_SCHEME_T_UNKNOWN,
                   .userinfo = NULL,
                   .user = NULL,
                   .pass = NULL,
                   .host = NULL,
                   .port = 0,
                   .path = "oasis:names:specification:docbook:dtd:xml:4.1.2",
                   .query = NULL,
                   .fragment = NULL,
                   .flags = 0},
      __LINE__);
  rc += _uri_test("ldap://[2001:db8::7]/c=GB?objectClass?one", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_UNKNOWN,
                               .userinfo = NULL,
                               .user = NULL,
                               .pass = NULL,
                               .host = "2001:db8::7",
                               .port = 0,
                               .path = "/c=GB",
                               .query = "objectClass?one",
                               .fragment = NULL,
                               .flags = URI_T_HOST_IPV6},
                  __LINE__);
  rc += _uri_test("news:comp.infosystems.www.servers.unix", URI_OPT_NONE,
                  (struct uri){.scheme_t = URI_SCHEME_T_UNKNOWN,
                               .userinfo = NULL,
                               .user = NULL,
                               .pass = NULL,
                               .host = NULL,
                               .port = 0,
                               .path = "comp.infosystems.www.servers.unix",
                               .query = NULL,
                               .fragment = NULL,
                               .flags = 0},
                  __LINE__);

  // rc += _uri_test( "xxxxxxxx", URI_OPT_NONE, (struct uri){ .scheme_t =
  // URI_SCHEME_T_UNKNOWN, .userinfo = NULL, .user = NULL, .pass = NULL, .host =
  // NULL, .port = 0, .path = NULL, .query = NULL, .fragment = NULL, .flags = 0
  // }, __LINE__ ); rc += _uri_test( "xxxxxxxx", URI_OPT_NONE, (struct uri){
  // .scheme_t = URI_SCHEME_T_UNKNOWN, .userinfo = NULL, .user = NULL, .pass =
  // NULL, .host = NULL, .port = 0, .path = NULL, .query = NULL, .fragment =
  // NULL, .flags = 0 }, __LINE__ );

  return rc;
}

#endif // BUILD_URI_TEST
