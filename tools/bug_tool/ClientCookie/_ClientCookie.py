"""HTTP cookie handling for web clients, plus some other stuff.

This module originally developed from my port of Gisle Aas' Perl module
HTTP::Cookies, from the libwww-perl library.

Docstrings, comments and debug strings in this code refer to the
attributes of the HTTP cookie system as cookie-attributes, to distinguish
them clearly from Python attributes.

Comments to John J Lee <jjl@pobox.com>.


Copyright 2002-2003 John J Lee <jjl@pobox.com>
Copyright 1997-1999 Gisle Aas (original libwww-perl code)
Copyright 2002-2003 Johnny Lee (original MSIE Perl code)

This code is free software; you can redistribute it and/or modify it under
the terms of the BSD License (see the file COPYING included with the
distribution).

"""

VERSION = "0.4.9"


# Public health warning: anyone who thought 'cookies are simple, aren't they?',
# run away now :-(

import sys, re, urlparse, string, copy, time, struct
try:
    import threading
    _threading = threading; del threading
except ImportError:
    import dummy_threading
    _threading = dummy_threading; del dummy_threading
import httplib  # only for the default HTTP port

MISSING_FILENAME_TEXT = ("a filename was not supplied (nor was the CookieJar "
                         "instance initialised with one)")
DEFAULT_HTTP_PORT = str(httplib.HTTP_PORT)

try: True
except NameError:
    True = 1
    False = 0

try: StopIteration
except NameError:
    class StopIteration(Exception): pass

import ClientCookie
from _HeadersUtil import split_header_words, join_header_words, \
     parse_ns_headers
from _Util import startswith, endswith, iso2time, time2isoz
from _Debug import debug

try: bool
except NameError:
    def bool(expr):
        if expr: return True
        else: return False

try: issubclass(Exception, (Exception,))
except TypeError:
    real_issubclass = issubclass
    from _Util import compat_issubclass
    issubclass = compat_issubclass
    del compat_issubclass

SPACE_DICT = {}
for c in string.whitespace:
    SPACE_DICT[c] = None
del c
def isspace(string):
    for c in string:
        if not SPACE_DICT.has_key(c): return False
    return True

def getheaders(msg, name):
    """Get all values for a header.

    This returns a list of values for headers given more than once; each
    value in the result list is stripped in the same way as the result of
    getheader().  If the header is not given, return an empty list.
    """
    result = []
    current = ''
    have_header = 0
    for s in msg.getallmatchingheaders(name):
        if isspace(s[0]):
            if current:
                current = "%s\n %s" % (current, string.strip(s))
            else:
                current = string.strip(s)
        else:
            if have_header:
                result.append(current)
            current = string.strip(s[string.find(s, ":") + 1:])
            have_header = 1
    if have_header:
        result.append(current)
    return result

def reraise_unmasked_exceptions(unmasked=()):
    # There are a few catch-all except: statements in this module, for
    # catching input that's bad in unexpected ways.
    # This function re-raises some exceptions we don't want to trap.
    if ClientCookie.CLIENTCOOKIE_DEBUG:
        raise
    unmasked = unmasked + (KeyboardInterrupt, SystemExit)
    etype = sys.exc_info()[0]
    if issubclass(etype, unmasked):
        raise


IPV4_RE = re.compile(r"\.\d+$")
def is_HDN(text):
    """Return True if text is a host domain name."""
    # XXX
    # This may well be wrong.  Which RFC is HDN defined in, if any (for
    #  the purposes of RFC 2965)?
    # For the current implementation, what about IPv6?  Remember to look
    #  at other uses of IPV4_RE also, if change this.
    if IPV4_RE.search(text):
        return False
    if text == "":
        return False
    if text[0] == "." or text[-1] == ".":
        return False
    return True

def domain_match(A, B):
    """Return True if domain A domain-matches domain B, according to RFC 2965.

    A and B may be host domain names or IP addresses.

    RFC 2965, section 1:

    Host names can be specified either as an IP address or a HDN string.
    Sometimes we compare one host name with another.  (Such comparisons SHALL
    be case-insensitive.)  Host A's name domain-matches host B's if

         *  their host name strings string-compare equal; or

         * A is a HDN string and has the form NB, where N is a non-empty
            name string, B has the form .B', and B' is a HDN string.  (So,
            x.y.com domain-matches .Y.com but not Y.com.)

    Note that domain-match is not a commutative operation: a.b.c.com
    domain-matches .c.com, but not the reverse.

    """
    # Note that, if A or B are IP addresses, the only relevant part of the
    # definition of the domain-match algorithm is the direct string-compare.
    A = string.lower(A)
    B = string.lower(B)
    if A == B:
        return True
    if not is_HDN(A):
        return False
    i = string.rfind(A, B)
    if i == -1 or i == 0:
        # A does not have form NB, or N is the empty string
        return False
    if not startswith(B, "."):
        return False
    if not is_HDN(B[1:]):
        return False
    return True

def liberal_is_HDN(text):
    """Return True if text is a sort-of-like a host domain name.

    For accepting/blocking domains.

    """
    if IPV4_RE.search(text):
        return False
    return True

def user_domain_match(A, B):
    """For blocking/accepting domains.

    A and B may be host domain names or IP addresses.

    """
    A = string.lower(A)
    B = string.lower(B)
    if not (liberal_is_HDN(A) and liberal_is_HDN(B)):
        if A == B:
            # equal IP addresses
            return True
        return False
    initial_dot = startswith(B, ".")
    if initial_dot and endswith(A, B):
        return True
    if not initial_dot and A == B:
        return True
    return False

cut_port_re = re.compile(r":\d+$")
def request_host(request):
    """Return request-host, as defined by RFC 2965.

    Variation from RFC: returned value is lowercased, for convenient
    comparison.

    """
    url = request.get_full_url()
    host = urlparse.urlparse(url)[1]
    if host == "":
        host = request.headers.get("Host", "")

    # remove port, if present
    host = cut_port_re.sub("", host, 1)
    return string.lower(host)

def eff_request_host(request):
    """Return a tuple (request-host, effective request-host name).

    As defined by RFC 2965, except both are lowercased.

    """
    erhn = req_host = request_host(request)
    if string.find(req_host, ".") == -1 and not IPV4_RE.search(req_host):
        erhn = req_host + ".local"
    return req_host, erhn

def request_path(request):
    """request-URI, as defined by RFC 2965."""
    url = request.get_full_url()
    #scheme, netloc, path, parameters, query, frag = urlparse.urlparse(url)
    req_path = normalize_path(string.join(urlparse.urlparse(url)[2:], ""))
    if not startswith(req_path, "/"):
        # fix bad RFC 2396 absoluteURI
        req_path = "/"+req_path
    return req_path

def request_port(request):
    # ATM (Python 2.3) request.port is always None, and unused by urllib2
    port = request.port
    host = request.get_host()
    if port is None:
        i = string.find(host, ':')
        if i >= 0:
            port = host[i+1:]
            try:
                int(port)
            except ValueError:
                debug("nonnumeric port: '%s'" % port)
                return None
        else:
            port = DEFAULT_HTTP_PORT
    return port

def unescape_path_fn(match):
    x = string.upper(match.group(1))
    if x == "2F" or x == "25":
        return "%%%s" % (x,)
    else:
        # string.atoi deprecated in 2.0, but 1.5.2 int function won't do
        # radix conversion
        return struct.pack("B", string.atoi(x, 16))
def normalize_path_fn(match):
    return "%%%02X" % ord(match.group(1))

unescape_re = re.compile(r"%([0-9a-fA-F][0-9a-fA-F])")
normalize_re = re.compile(r"([\0-\x20\x7f-\xff])")
def normalize_path(path):
    """Normalise URI path so that plain string compare can be used.

    >>> normalize_path("%19\xd3%Fb%2F%25%26")
    '%19%D3%FB%2F%25&'
    >>> 

    In normalised form, all non-printable characters are %-escaped, and all
    printable characters are given literally (not escaped).  All remaining
    %-escaped characters are capitalised.  %25 and %2F are special-cased,
    because they represent the printable characters"%" and "/", which are used
    as escape and URI path separator characters respectively.

    """
    path = unescape_re.sub(unescape_path_fn, path)
    path = normalize_re.sub(normalize_path_fn, path)
    return path

def reach(h):
    """Return reach of host h, as defined by RFC 2965, section 1.

    The reach R of a host name H is defined as follows:

       *  If

          -  H is the host domain name of a host; and,

          -  H has the form A.B; and

          -  A has no embedded (that is, interior) dots; and

          -  B has at least one embedded dot, or B is the string "local".
             then the reach of H is .B.

       *  Otherwise, the reach of H is H.

    >>> reach("www.acme.com")
    '.acme.com'
    >>> reach("acme.com")
    'acme.com'
    >>> reach("acme.local")
    '.local'

    """
    i = string.find(h, ".")
    if i >= 0:
        #a = h[:i]  # this line is only here to show what a is
        b = h[i+1:]
        i = string.find(b, ".")
        if is_HDN(h) and (i >= 0 or b == "local"):
            return "."+b
    return h

def is_third_party(request):
    """

    RFC 2965, section 3.3.6:

        An unverifiable transaction is to a third-party host if its request-
        host U does not domain-match the reach R of the request-host O in the
        origin transaction.

    """
    req_host = string.lower(request_host(request))
    # the origin request's request-host was stuffed into request by
    # _urllib2_support.AbstractHTTPHandler
    if not domain_match(req_host, reach(request.origin_req_host)):
        return True
    else:
        return False


class Cookie:
    """HTTP Cookie.

    This class represents both Netscape and RFC 2965 cookies.

    This is deliberately a very simple class.  It just holds attributes.  It's
    possible to construct Cookie instances that don't comply with the cookie
    standards.  CookieJar.make_cookies is the factory function for Cookie
    objects -- it deals with cookie parsing, supplying defaults, and
    normalising to the representation used in this class.  CookiePolicy is
    responsible for checking them to see whether they should be accepted from
    and returned to the server.

    version: integer;
    name: string (may be None);
    value: string;
    port: string; None indicates no attribute was supplied (eg. "Port", rather
     than eg. "Port=80"); otherwise, a port string (eg. "80") or a port list
     string (eg. "80,8080")
    port_specified: boolean; true if a value was supplied with the Port
     cookie-attribute
    domain: string;
    domain_specified: boolean; true if Domain was explicitly set
    domain_initial_dot: boolean; true if Domain as set in HTTP header by server
     started with a dot (yes, this really is necessary!)
    path: string;
    path_specified: boolean; true if Path was explicitly set
    secure:  boolean; true if should only be returned over secure connection
    expires: integer; seconds since epoch (RFC 2965 cookies should calculate
     this value from the Max-Age attribute)
    discard: boolean, true if this is a session cookie; (if no expires value,
     this should be true)
    comment: string;
    comment_url: string;
    rest: mapping of other attributes

    Note that the port may be present in the headers, but unspecified ("Port"
    rather than"Port=80", for example); if this is the case, port is None.

    """

    def __init__(self, version, name, value,
                 port, port_specified,
                 domain, domain_specified, domain_initial_dot,
                 path, path_specified,
                 secure,
                 expires,
                 discard,
                 comment,
                 comment_url,
                 rest):

        if version is not None: version = int(version)
        if expires is not None: expires = int(expires)
        if port is None and port_specified is True:
            raise ValueError("if port is None, port_specified must be false")

        self.version = version
        self.name = name
        self.value = value
        self.port = port
        self.port_specified = port_specified
        # normalise case, as per RFC 2965 section 3.3.3
        self.domain = string.lower(domain)
        self.domain_specified = domain_specified
        # Sigh.  We need to know whether the domain given in the
        # cookie-attribute had an initial dot, in order to follow RFC 2965
        # (as clarified in draft errata).  Needed for the returned $Domain
        # value.
        self.domain_initial_dot = domain_initial_dot
        self.path = path
        self.path_specified = path_specified
        self.secure = secure
        self.expires = expires
        self.discard = discard
        self.comment = comment
        self.comment_url = comment_url

        self.rest = copy.copy(rest)

    def is_expired(self, now=None):
        if now is None: now = time.time()
        if (self.expires is not None) and (self.expires <= now):
            return True
        return False

    def __str__(self):
        if self.port is None: p = ""
        else: p = ":"+self.port
        limit = self.domain + p + self.path
        if self.name is not None:
            namevalue = "%s=%s" % (self.name, self.value)
        else:
            namevalue = self.value
        return "<Cookie %s for %s>" % (namevalue, limit)

    def __repr__(self):
        args = []
        for name in ["version", "name", "value",
                     "port", "port_specified",
                     "domain", "domain_specified", "domain_initial_dot",
                     "path", "path_specified",
                     "secure", "expires", "discard", "comment", "comment_url"]:
            attr = getattr(self, name)
            args.append("%s=%s" % (name, attr))
        args.append(repr(self.rest))
        return "Cookie(%s)" % string.join(args, ", ")


class CookiePolicy:
    """Defines which cookies get accepted from and returned to server.

    The subclass DefaultCookiePolicy defines the standard rules for Netscape
    and RFC 2965 cookies -- override that if you want a customised policy.

    As well as implementing set_ok and return_ok, implementations of this
    interface must also supply the following attributes, indicating which
    protocols should be used, and how.  These can be read and set at any time,
    though whether that makes complete sense from the protocol point of view is
    doubtful.

    Public attributes:

    netscape: implement netscape protocol
    rfc2965: implement RFC 2965 protocol
    hide_cookie2: don't add Cookie2 header to requests (the presence of
     this header indicates to the server that we understand RFC 2965
     cookies)

    """
    def set_ok(self, cookie, request, unverifiable):
        """Return true if (and only if) cookie should be accepted from server.

        Currently, pre-expired cookies never get this far -- the CookieJar
        class deletes such cookies itself.

        cookie: ClientCookie.Cookie object
        request: object implementing the interface defined by
         CookieJar.extract_cookies.__doc__
        unverifiable: flag indicating whether the transaction is unverifiable,
         as defined by RFC 2965

        """
        raise NotImplementedError()

    def return_ok(self, cookie, request, unverifiable):
        """Return true if (and only if) cookie should be returned to server.

        cookie: ClientCookie.Cookie object
        request: object implementing the interface defined by
         CookieJar.add_cookie_header.__doc__
        unverifiable: flag indicating whether the transaction is unverifiable,
         as defined by RFC 2965

        """
        raise NotImplementedError()

    def domain_return_ok(self, domain, request, unverifiable):
        """Return false if cookies should not be returned, given cookie domain.

        This is here as an optimization, to remove the need for checking every
        cookie with a particular domain (which may involve reading many files).
        The default implementations of domain_return_ok and path_return_ok
        (return True) leave all the work to return_ok.

        If domain_return_ok returns true for the cookie domain, path_return_ok
        is called for the cookie path.  Otherwise, path_return_ok and return_ok
        are never called for that cookie domain.  If path_return_ok returns
        true, return_ok is called with the Cookie object itself for a full
        check.  Otherwise, return_ok is never called for that cookie path.

        Note that domain_return_ok is called for every *cookie* domain, not
        just for the *request* domain.  For example, the function might be
        called with both ".acme.com" and "www.acme.com" if the request domain is
        "www.acme.com".  The same goes for path_return_ok.

        For argument documentation, see the docstring for return_ok.

        """
        return True

    def path_return_ok(self, path, request, unverifiable):
        """Return false if cookies should not be returned, given cookie path.

        See the docstring for domain_return_ok.

        """
        return True


class DefaultCookiePolicy(CookiePolicy):
    """Implements the standard rules for accepting and returning cookies.

    Both RFC 2965 and Netscape cookies are covered.

    The easiest way to provide your own policy is to override this class and
    call its methods in your overriden implementations before adding your own
    additional checks.

    import ClientCookie
    class MyCookiePolicy(ClientCookie.DefaultCookiePolicy):
        def set_ok(self, cookie, request, unverifiable):
            if not ClientCookie.DefaultCookiePolicy.set_ok(
                self, cookie, request, unverifiable):
                return False
            if i_dont_want_to_store_this_cookie():
                return False
            return True

    In addition to the features required to implement the CookiePolicy
    interface, this class allows you to block and allow domains from setting
    and receiving cookies.  There are also some strictness switches that allow
    you to tighten up the rather loose Netscape protocol rules a little bit (at
    the cost of blocking some benign cookies).

    A domain blacklist and whitelist is provided (both off by default).  Only
    domains not in the blacklist and present in the whitelist (if the whitelist
    is active) participate in cookie setting and returning.  Use the
    blocked_domains constructor argument, and blocked_domains and
    set_blocked_domains methods (and the corresponding argument and methods for
    allowed_domains).  If you set a whitelist, you can turn it off again by
    setting it to None.

    Domains in block or allow lists that do not start with a dot must
    string-compare equal.  For example, "acme.com" matches a blacklist entry of
    "acme.com", but "www.acme.com" does not.  Domains that do start with a dot
    are matched by more specific domains too.  For example, both "www.acme.com"
    and "www.munitions.acme.com" match ".acme.com" (but "acme.com" itself does
    not).  IP addresses are an exception, and must match exactly.  For example,
    if blocked_domains contains "192.168.1.2" and ".168.1.2" 192.168.1.2 is
    blocked, but 193.168.1.2 is not.

    Additional Public Attributes:

    General strictness switches

    strict_domain: don't allow sites to set two-component domains with
     country-code top-level domains like .co.uk, .gov.uk, .co.nz. etc.
     This is far from perfect and isn't guaranteed to work!

    RFC 2965 protocol strictness switches

    strict_rfc2965_unverifiable: follow RFC 2965 rules on unverifiable
     transactions (usually, an unverifiable transaction is one resulting from
     a redirect or an image hosted on another site); if this is false, cookies
     are NEVER blocked on the basis of verifiability

    Netscape protocol strictness switches

    strict_ns_unverifiable: apply RFC 2965 rules on unverifiable transactions
     even to Netscape cookies
    strict_ns_domain: flags indicating how strict to be with domain-matching
     rules for Netscape cookies:
      DomainStrictNoDots: when setting cookies, host prefix must not contain a
       dot (eg. www.foo.bar.com can't set a cookie for .bar.com, because
       www.foo contains a dot)
      DomainStrictNonDomain: cookies that did not explicitly specify a Domain
       cookie-attribute can only be returned to a domain that string-compares
       equal to the domain that set the cookie (eg. rockets.acme.com won't
       be returned cookies from acme.com that had no Domain cookie-attribute)
      DomainRFC2965Match: when setting cookies, require a full RFC 2965
       domain-match
      DomainLiberal and DomainStrict are the most useful combinations of the
       above flags, for convenience
    strict_ns_set_initial_dollar: ignore cookies in Set-Cookie: headers that
     have names starting with '$'
    strict_ns_set_path: don't allow setting cookies whose path doesn't
     path-match request URI

    """

    DomainStrictNoDots = 1
    DomainStrictNonDomain = 2
    DomainRFC2965Match = 4

    DomainLiberal = 0
    DomainStrict = DomainStrictNoDots|DomainStrictNonDomain

    def __init__(self,
                 blocked_domains=None, allowed_domains=None,
                 netscape=True, rfc2965=True,
                 hide_cookie2=False,
                 strict_domain=False,
                 strict_rfc2965_unverifiable=True,
                 strict_ns_unverifiable=False,
                 strict_ns_domain=DomainLiberal,
                 strict_ns_set_initial_dollar=False,
                 strict_ns_set_path=False):
        """
        blocked_domains: sequence of domain names that we never accept cookies
         from, nor return cookies to
        allowed_domains: if not None, this is a sequence of the only domains
         for which we accept and return cookies

        For other arguments, see CookiePolicy.__doc__ and
        DefaultCookiePolicy.__doc__..

        """
        self.netscape = netscape
        self.rfc2965 = rfc2965
        self.hide_cookie2 = hide_cookie2
        self.strict_domain = strict_domain
        self.strict_rfc2965_unverifiable = strict_rfc2965_unverifiable
        self.strict_ns_unverifiable = strict_ns_unverifiable
        self.strict_ns_domain = strict_ns_domain
        self.strict_ns_set_initial_dollar = strict_ns_set_initial_dollar
        self.strict_ns_set_path = strict_ns_set_path

        if blocked_domains is not None:
            self._blocked_domains = tuple(blocked_domains)
        else:
            self._blocked_domains = ()

        if allowed_domains is not None:
            allowed_domains = tuple(allowed_domains)
        self._allowed_domains = allowed_domains

    def blocked_domains(self):
        """Return the sequence of blocked domains (as a tuple)."""
        return self._blocked_domains
    def set_blocked_domains(self, blocked_domains):
        """Set the sequence of blocked domains."""
        self._blocked_domains = tuple(blocked_domains)

    def is_blocked(self, domain):
        for blocked_domain in self._blocked_domains:
            if user_domain_match(domain, blocked_domain):
                return True
        return False

    def allowed_domains(self):
        """Return None, or the sequence of allowed domains (as a tuple)."""
        return self._allowed_domains
    def set_allowed_domains(self, allowed_domains):
        """Set the sequence of allowed domains, or None."""
        if allowed_domains is not None:
            allowed_domains = tuple(allowed_domains)
        self._allowed_domains = allowed_domains

    def is_not_allowed(self, domain):
        if self._allowed_domains is None:
            return False
        for allowed_domain in self._allowed_domains:
            if user_domain_match(domain, allowed_domain):
                return False
        return True

    def set_ok(self, cookie, request, unverifiable):
        """
        If you override set_ok, be sure to call this method.  If it returns
        false, so should your subclass (assuming your subclass wants to be more
        strict about which cookies to accept).

        """
        debug(" - checking cookie %s=%s" % (cookie.name, cookie.value))

        assert cookie.value is not None

        for n in "version", "verifiability", "name", "path", "domain", "port":
            fn_name = "set_ok_"+n
            fn = getattr(self, fn_name)
            if not fn(cookie, request, unverifiable):
                return False
        return True

    def set_ok_version(self, cookie, request, unverifiable):
        if cookie.version is None:
            # Version is always set to 0 by parse_ns_headers if it's a Netscape
            # cookie, so this must be an invalid RFC 2965 cookie.
            debug("   Set-Cookie2 without version attribute (%s=%s)" %
                  (cookie.name, cookie.value))
            return False
        if cookie.version > 0 and not self.rfc2965:
                debug("   RFC 2965 cookies are switched off")
                return False
        elif cookie.version == 0 and not self.netscape:
            debug("   Netscape cookies are switched off")
            return False
        return True

    def set_ok_verifiability(self, cookie, request, unverifiable):
        if unverifiable and is_third_party(request):
            if cookie.version > 0 and self.strict_rfc2965_unverifiable:
                debug("   third-party RFC 2965 cookie during unverifiable "
                      "transaction")
                return False
            elif cookie.version == 0 and self.strict_ns_unverifiable:
                debug("   third-party Netscape cookie during unverifiable "
                      "transaction")
                return False
        return True

    def set_ok_name(self, cookie, request, unverifiable):
        # Try and stop servers setting V0 cookies designed to hack other
        # servers that know both V0 and V1 protocols.
        if (cookie.version == 0 and self.strict_ns_set_initial_dollar and
            (cookie.name is not None) and startswith(cookie.name, "$")):
            debug("   illegal name (starts with '$'): '%s'" % cookie.name)
            return False
        return True

    def set_ok_path(self, cookie, request, unverifiable):
        if cookie.path_specified:
            req_path = request_path(request)
            if ((cookie.version > 0 or
                 (cookie.version == 0 and self.strict_ns_set_path)) and
                not startswith(req_path, cookie.path)):
                debug("   path attribute %s is not a prefix of request "
                      "path %s" % (cookie.path, req_path))
                return False
        return True

    def set_ok_domain(self, cookie, request, unverifiable):
        if self.is_blocked(cookie.domain):
            debug("   domain %s is in user block-list" % cookie.domain)
            return False
        if self.is_not_allowed(cookie.domain):
            debug("   domain %s is not in user allow-list" % cookie.domain)
            return False
        if cookie.domain_specified:
            req_host, erhn = eff_request_host(request)
            domain = cookie.domain
            if self.strict_domain and (string.count(domain, ".") >= 2):
                i = string.rfind(domain, ".")
                j = string.rfind(domain, ".", 0, i)
                if j == 0:  # domain like .foo.bar
                    tld = domain[i+1:]
                    sld = domain[j+1:i]
                    if (string.lower(sld) in [
                        "co", "ac",
                        "com", "edu", "org", "net", "gov", "mil", "int"] and
                        len(tld) == 2):
                        # domain like .co.uk
                        debug("   country-code second level domain %s" %
                              domain)
                        return False
            if startswith(domain, "."):
                undotted_domain = domain[1:]
            else:
                undotted_domain = domain
            embedded_dots = (string.find(undotted_domain, ".") >= 0)
            if not embedded_dots and domain != ".local":
                debug("   non-local domain %s contains no embedded dot" %
                      domain)
                return False
            if cookie.version == 0:
                if (not endswith(erhn, domain) and
                    (not startswith(erhn, ".") and
                     not endswith("."+erhn, domain))):
                    debug("   effective request-host %s (even with added "
                          "initial dot) does not end end with %s" %
                          (erhn, domain))
                    return False
            if (cookie.version > 0 or
                (self.strict_ns_domain & self.DomainRFC2965Match)):
                if not domain_match(erhn, domain):
                    debug("   effective request-host %s does not domain-match "
                          "%s" % (erhn, domain))
                    return False
            if (cookie.version > 0 or
                (self.strict_ns_domain & self.DomainStrictNoDots)):
                host_prefix = req_host[:-len(domain)]
                if (string.find(host_prefix, ".") >= 0 and
                    not IPV4_RE.search(req_host)):
                    debug("   host prefix %s for domain %s contains a dot" %
                          (host_prefix, domain))
                    return False
        return True

    def set_ok_port(self, cookie, request, unverifiable):
        if cookie.port_specified:
            req_port = request_port(request)
            if req_port is None:
                req_port = "80"
            else:
                req_port = str(req_port)
            for p in string.split(cookie.port, ","):
                try:
                    int(p)
                except ValueError:
                    debug("   bad port %s (not numeric)" % p)
                    return False
                if p == req_port:
                    break
            else:
                debug("   request port (%s) not found in %s" %
                      (req_port, cookie.port))
                return False
        return True

    def return_ok(self, cookie, request, unverifiable):
        """
        If you override return_ok, be sure to call this method.  If it returns
        false, so should your subclass.

        """
        # Path has already been checked by path_return_ok, and domain blocking
        # done by domain_return_ok.
        debug(" - checking cookie %s=%s" % (cookie.name, cookie.value))

        for n in "version", "verifiability", "secure", "expires", "port", "domain":
            fn_name = "return_ok_"+n
            fn = getattr(self, fn_name)
            if not fn(cookie, request, unverifiable):
                return False
        return True

    def return_ok_version(self, cookie, request, unverifiable):
        if cookie.version > 0 and not self.rfc2965:
            debug("   RFC 2965 cookies are switched off")
            return False
        elif cookie.version == 0 and not self.netscape:
            debug("   Netscape cookies are switched off")
            return False
        return True

    def return_ok_verifiability(self, cookie, request, unverifiable):
        if unverifiable and is_third_party(request):
            if cookie.version > 0 and self.strict_rfc2965_unverifiable:
                debug("   third-party RFC 2965 cookie during unverifiable "
                      "transaction")
                return False
            elif cookie.version == 0 and self.strict_ns_unverifiable:
                debug("   third-party Netscape cookie during unverifiable "
                      "transaction")
                return False
        return True

    def return_ok_secure(self, cookie, request, unverifiable):
        if cookie.secure and request.get_type() != "https":
            debug("   secure cookie with non-secure request")
            return False
        return True

    def return_ok_expires(self, cookie, request, unverifiable):
        if cookie.is_expired(self._now):
            debug("   cookie expired")
            return False
        return True

    def return_ok_port(self, cookie, request, unverifiable):
        if cookie.port:
            req_port = request_port(request)
            if req_port is None:
                req_port = "80"
            for p in string.split(cookie.port, ","):
                if p == req_port:
                    break
            else:
                debug("   request port %s does not match cookie port %s" %
                      (req_port, cookie.port))
                return False
        return True

    def return_ok_domain(self, cookie, request, unverifiable):
        req_host, erhn = eff_request_host(request)
        domain = cookie.domain

        # strict check of non-domain cookies: Mozilla does this, MSIE5 doesn't
        if (cookie.version == 0 and
            (self.strict_ns_domain & self.DomainStrictNonDomain) and
            not cookie.domain_specified and domain != erhn):
            debug("   cookie with unspecified domain does not string-compare "
                  "equal to request domain")
            return False

        if cookie.version > 0 and not domain_match(erhn, domain):
            debug("   effective request-host name %s does not domain-match "
                  "RFC 2965 cookie domain %s" % (erhn, domain))
            return False
        if cookie.version == 0 and not endswith("."+req_host, domain):
            debug("   request-host %s does not match Netscape cookie domain "
                  "%s" % (req_host, domain))
            return False
        return True

    def domain_return_ok(self, domain, request, unverifiable):
        if self.is_blocked(domain):
            debug("   domain %s is in user block-list" % domain)
            return False
        if self.is_not_allowed(domain):
            debug("   domain %s is not in user allow-list" % domain)
            return False
        return True

    def path_return_ok(self, path, request, unverifiable):
        debug("- checking cookie path=%s" % path)
        req_path = request_path(request)
        if not startswith(req_path, path):
            debug("  %s does not path-match %s" % (req_path, path))
            return False
        return True


def lwp_cookie_str(cookie):
    """Return string representation of Cookie in an the LWP cookie file format.

    Actually, the format is slightly extended from that used by LWP's
    (libwww-perl's) HTTP::Cookies, to avoid losing some RFC 2965
    information not recorded by LWP.

    Used by the CookieJar base class for saving cookies to a file.

    """
    h = [(cookie.name, cookie.value),
         ("path", cookie.path),
         ("domain", cookie.domain)]
    if cookie.port is not None: h.append(("port", cookie.port))
    if cookie.path_specified: h.append(("path_spec", None))
    if cookie.port_specified: h.append(("port_spec", None))
    if cookie.domain_initial_dot: h.append(("domain_dot", None))
    if cookie.secure: h.append(("secure", None))
    if cookie.expires: h.append(("expires",
                               time2isoz(float(cookie.expires))))
    if cookie.discard: h.append(("discard", None))
    if cookie.comment: h.append(("comment", cookie.comment))
    if cookie.comment_url: h.append(("commenturl", cookie.comment_url))

    keys = cookie.rest.keys()
    keys.sort()
    for k in keys:
        h.append((k, str(cookie.rest[k])))

    h.append(("version", str(cookie.version)))

    return join_header_words([h])

def vals_sorted_by_key(adict):
    keys = adict.keys()
    keys.sort()
    return map(adict.get, keys)

class MappingIterator:
    """Iterates over nested mapping, depth-first, in sorted order by key."""
    def __init__(self, mapping):
        self._s = [(vals_sorted_by_key(mapping), 0, None)]  # LIFO stack

    def __iter__(self): return self

    def next(self):
        # this is hairy because of lack of generators
        while 1:
            try:
                vals, i, prev_item = self._s.pop()
            except IndexError:
                raise StopIteration()
            if i < len(vals):
                item = vals[i]
                i = i + 1
                self._s.append((vals, i, prev_item))
                try:
                    item.items
                except AttributeError:
                    # non-mapping
                    break
                else:
                    # mapping
                    self._s.append((vals_sorted_by_key(item), 0, item))
                    continue
        return item


# Used as second parameter to dict.get method, to distinguish absent
# dict key from one with a None value.
class Absent: pass

class CookieJar:
    """Collection of HTTP cookies.

    The major methods are extract_cookies and add_cookie_header; these are all
    you are likely to need.  In fact, you probably don't even need to know
    about this class: use the cookie-aware extensions to the urllib2 callables
    provided by this module: urlopen in particular (and perhaps also
    build_opener, install_opener, HTTPCookieProcessor, HTTPRefererProcessor,
    HTTPRefreshHandler, HTTPEquivProcessor, SeekableProcessor, etc.).

    CookieJar supports the iterator protocol.  Iteration also works in 1.5.2:

    for cookie in cookiejar:
        # do something with cookie

    Methods:

    CookieJar(filename=None, delayload=False, policy=None)
    add_cookie_header(request, unverifiable=False)
    extract_cookies(response, request, unverifiable=False)
    make_cookies(response, request)
    set_cookie_if_ok(cookie, request, unverifiable=False)
    set_cookie(cookie)
    save(filename=None, ignore_discard=False, ignore_expires=False)
    load(filename=None, ignore_discard=False, ignore_expires=False)
    revert(filename=None, ignore_discard=False, ignore_expires=False)
    clear(domain=None, path=None, key=None)
    clear_session_cookies()
    clear_expired_cookies()
    as_string(skip_discard=False)  (str(cookies) also works)


    Public attributes

    filename: filename for loading and saving cookies
    policy: CookiePolicy object

    Public readable attributes

    delayload: request that cookies are lazily loaded from disk; this is only
     a hint since this only affects performance, not behaviour (unless the
     cookies on disk are changing); a CookieJar object may ignore it (in fact,
     only MSIECookieJar lazily loads cookies at the moment)
    cookies: a three-level dictionary [domain][path][key] containing Cookie
     instances; you almost certainly don't need to use this

    """

    non_word_re = re.compile(r"\W")
    quote_re = re.compile(r"([\"\\])")
    strict_domain_re = re.compile(r"\.?[^.]*")
    domain_re = re.compile(r"[^.]*")
    dots_re = re.compile(r"^\.+")

    magic_re = r"^\#LWP-Cookies-(\d+\.\d+)"

    def __init__(self, filename=None, delayload=False, policy=None):
        """
        See CookieJar.__doc__ for argument documentation.

        Cookies are NOT loaded from the named file until either the load or
        revert method is called.

        """
        self.filename = filename
        self.delayload = delayload

        if policy is None:
            policy = DefaultCookiePolicy()
        self.policy = policy

        self._cookies_lock = _threading.RLock()
        self.cookies = {}

        # for __getitem__ iteration in pre-2.2 Pythons
        self._prev_getitem_index = 0

    def _cookies_for_domain(self, domain, request, unverifiable):
        """Return a list of cookies to be returned to server."""
        debug("Checking %s for cookies to return" % domain)
        if not self.policy.domain_return_ok(domain, request, unverifiable):
            return []

        cookies_by_path = self.cookies.get(domain)
        if cookies_by_path is None:
            return []

        cookies = []
        for path in cookies_by_path.keys():
            if not self.policy.path_return_ok(path, request, unverifiable):
                continue
            for name, cookie in cookies_by_path[path].items():
                if not self.policy.return_ok(cookie, request, unverifiable):
                    debug("   not returning cookie")
                    continue
                debug("   it's a match")
                cookies.append(cookie)

        return cookies

    def _cookie_attrs(self, cookies):
        """Return a list of cookie-attributes to be returned to server.

        like ['foo="bar"; $Path="/"', ...]

        The $Version attribute is also added when appropriate (currently only
        once per request).

        """
        # add cookies in order of most specific (ie. longest) path first
        def decreasing_size(a, b): return cmp(len(b.path), len(a.path))
        cookies.sort(decreasing_size)

        version_set = False

        attrs = []
        for cookie in cookies:
            # set version of Cookie header
            # XXX
            # What should it be if multiple matching Set-Cookie headers have
            #  different versions themselves?
            # Answer: there is no answer; was supposed to be settled by
            #  RFC 2965 errata, but that may never appear...
            version = cookie.version
            if not version_set:
                version_set = True
                if version > 0:
                    attrs.append("$Version=%s" % version)

            # quote cookie value if necessary
            # (not for Netscape protocol, which already has any quotes
            #  intact, due to the poorly-specified Netscape Cookie: syntax)
            if self.non_word_re.search(cookie.value) and version > 0:
                value = self.quote_re.sub(r"\\\1", cookie.value)
            else:
                value = cookie.value

            # add cookie-attributes to be returned in Cookie header
            if cookie.name is None:
                attrs.append(value)
            else:
                attrs.append("%s=%s" % (cookie.name, value))
            if version > 0:
                if cookie.path_specified:
                    attrs.append('$Path="%s"' % cookie.path)
                if startswith(cookie.domain, "."):
                    domain = cookie.domain
                    if (not cookie.domain_initial_dot and
                        startswith(domain, ".")):
                        domain = domain[1:]
                    attrs.append('$Domain="%s"' % domain)
                if cookie.port is not None:
                    p = "$Port"
                    if cookie.port_specified:
                        p = p + ('="%s"' % cookie.port)
                    attrs.append(p)

        return attrs

    def add_cookie_header(self, request, unverifiable=False):
        """Add correct Cookie: header to request (urllib2.Request object).

        The Cookie2 header is also added unless policy.hide_cookie2 is true.

        The request object (usually a urllib2.Request instance) must support
        the methods get_full_url, get_host, get_type and add_header, as
        documented by urllib2, and the attributes headers (a mapping containing
        the request's HTTP headers) and port (the port number).

        If unverifiable is true, it will be assumed that the transaction is
        unverifiable as defined by RFC 2965, and appropriate action will be
        taken.

        """
        debug("add_cookie_header")
        self._cookies_lock.acquire()

        self.policy._now = self._now = int(time.time())

        req_host, erhn = eff_request_host(request)
        strict_non_domain = \
               self.policy.strict_ns_domain & self.policy.DomainStrictNonDomain

        cookies = []

        domain = erhn
        # First check origin server effective host name for an exact match.
        cookies.extend(self._cookies_for_domain(domain, request, unverifiable))
        # Then, start with effective request-host with initial dot prepended
        # (for Netscape cookies with explicitly-set Domain cookie-attributes)
        # -- eg. .foo.bar.baz.com and check all possible derived domain strings
        # (.bar.baz.com, bar.baz.com, .baz.com) for cookies.
        # This isn't too finicky about which domains to check, because we have
        # to cover both V0 and V1 cookies, and policy.return_ok will check the
        # domain in any case.
        if not IPV4_RE.search(req_host):
            # IP addresses must string-compare equal in order to domain-match
            # (IP address case will have been checked above as erhn == req_host
            # in that case).
            if domain != ".local":
                domain = "."+domain
            while string.find(domain, ".") >= 0:
                cookies.extend(self._cookies_for_domain(
                    domain, request, unverifiable))
                if strict_non_domain:
                    domain = self.strict_domain_re.sub("", domain, 1)
                else:
                    # strip either initial dot only, or initial component only
                    # .www.foo.com --> www.foo.com
                    # www.foo.com --> .foo.com
                    if startswith(domain, "."):
                        domain = domain[1:]
                        # we've already done the erhn
                        if domain == erhn:
                            domain = self.domain_re.sub("", domain, 1)
                    else:
                        domain = self.domain_re.sub("", domain, 1)

        attrs = self._cookie_attrs(cookies)
        if attrs:
            request.add_header("Cookie", string.join(attrs, "; "))

        # if necessary, advertise that we know RFC 2965
        if self.policy.rfc2965 and not self.policy.hide_cookie2:
            for cookie in cookies:
                if cookie.version != 1:
                    request.add_header("Cookie2", '$Version="1"')
                    break

        self._cookies_lock.release()

        self.clear_expired_cookies()

    def _normalized_cookie_tuples(self, attrs_set):
        """Return list of tuples containing normalised cookie information.

        attrs_set is the list of lists of key,value pairs extracted from
        the Set-Cookie or Set-Cookie2 headers.

        Tuples are name, value, standard, rest, where name and value are the
        cookie name and value, standard is a dictionary containing the standard
        cookie-attributes (discard, secure, version, expires or max-age,
        domain, path and port) and rest is a dictionary containing the rest of
        the cookie-attributes.

        """
        cookie_tuples = []

        boolean_attrs = "discard", "secure"
        value_attrs = ("version",
                       "expires", "max-age",
                       "domain", "path", "port",
                       "comment", "commenturl")

        for cookie_attrs in attrs_set:
            name, value = cookie_attrs[0]

            # Build dictionary of standard cookie-attributes (standard) and
            # dictionary of other cookie-attributes (rest).

            # Note: expiry time is normalised to seconds since epoch.  V0
            # cookies should have the Expires cookie-attribute, and V1 cookies
            # should have Max-Age, but since V1 includes RFC 2109 cookies (and
            # since V0 cookies may be a mish-mash of Netscape and RFC 2109), we
            # accept either (but prefer Max-Age).
            max_age_set = False

            bad_cookie = False

            standard = {}
            rest = {}
            for k, v in cookie_attrs[1:]:
                lc = string.lower(k)
                # don't lose case distinction for unknown fields
                if lc in value_attrs or lc in boolean_attrs:
                    k = lc
                if k in boolean_attrs and v is None:
                    # boolean cookie-attribute is present, but has no value
                    # (like "discard", rather than "port=80")
                    v = True
                if standard.has_key(k):
                    # only first value is significant
                    continue
                if k == "domain":
                    if v is None:
                        debug("   missing value for domain attribute")
                        bad_cookie = True
                        break
                    # RFC 2965 section 3.3.3
                    v = string.lower(v)
                if k == "expires":
                    if max_age_set:
                        # Prefer max-age to expires (like Mozilla)
                        continue
                    if v is None:
                        debug("   missing or invalid value for expires "
                              "attribute: treating as session cookie")
                        continue
                if k == "max-age":
                    max_age_set = True
                    try:
                        v = int(v)
                    except ValueError:
                        debug("   missing or invalid (non-numeric) value for "
                              "max-age attribute")
                        bad_cookie = True
                        break
                    # convert RFC 2965 Max-Age to seconds since epoch
                    # XXX Strictly you're supposed to follow RFC 2616
                    #   age-calculation rules.  Remember that zero Max-Age is a
                    #   is a request to discard (old and new) cookie, though.
                    k = "expires"
                    v = self._now + v
                if (k in value_attrs) or (k in boolean_attrs):
                    if (v is None and
                        k not in ["port", "comment", "commenturl"]):
                        debug("   missing value for %s attribute" % k)
                        bad_cookie = True
                        break
                    standard[k] = v
                else:
                    rest[k] = v

            if bad_cookie:
                continue

            cookie_tuples.append((name, value, standard, rest))

        return cookie_tuples

    def _cookie_from_cookie_tuple(self, tup, request):
        # standard is dict of standard cookie-attributes, rest is dict of the
        # rest of them
        name, value, standard, rest = tup

        domain = standard.get("domain", Absent)
        path = standard.get("path", Absent)
        port = standard.get("port", Absent)
        expires = standard.get("expires", Absent)

        # set the easy defaults
        version = standard.get("version", None)
        if version is not None: version = int(version)
        secure = standard.get("secure", False)
        # (discard is also set if expires is Absent)
        discard = standard.get("discard", False)
        comment = standard.get("comment", None)
        comment_url = standard.get("commenturl", None)

        # set default path
        if path is not Absent and path != "":
            path_specified = True
            path = normalize_path(path)
        else:
            path_specified = False
            path = request_path(request)
            i = string.rfind(path, "/")
            if i != -1:
                if version == 0:
                    # Netscape spec parts company from reality here
                    path = path[:i]
                else:
                    path = path[:i+1]
            if len(path) == 0: path = "/"

        # set default domain
        domain_specified = domain is not Absent
        # but first we have to remember whether it starts with a dot
        domain_initial_dot = False
        if domain_specified:
            domain_initial_dot = bool(startswith(domain, "."))
        if domain is Absent:
            req_host, erhn = eff_request_host(request)
            domain = erhn
        elif not startswith(domain, "."):
            domain = "."+domain

        # set default port
        port_specified = False
        if port is not Absent:
            if port is None:
                # Port attr present, but has no value: default to request port.
                # Cookie should then only be sent back on that port.
                port = request_port(request)
            else:
                port_specified = True
                port = re.sub(r"\s+", "", port)
        else:
            # No port attr present.  Cookie can be sent back on any port.
            port = None

        # set default expires and discard
        if expires is Absent:
            expires = None
            discard = True
        elif expires <= self._now:
            # Expiry date in past is request to delete cookie.  This can't be
            # in DefaultCookiePolicy, because can't delete cookies there.
            try:
                del self.cookies[domain][path][name]
            except KeyError:
                pass
            else:
                debug("Expiring cookie, domain='%s', path='%s', name='%s'" %
                      (domain, path, name))
            return None

        return Cookie(version,
                      name, value,
                      port, port_specified,
                      domain, domain_specified, domain_initial_dot,
                      path, path_specified,
                      secure,
                      expires,
                      discard,
                      comment,
                      comment_url,
                      rest)

    def _cookies_from_attrs_set(self, attrs_set, request):
        cookie_tuples = self._normalized_cookie_tuples(attrs_set)
        cookies = []
        for tup in cookie_tuples:
            cookie = self._cookie_from_cookie_tuple(tup, request)
            if cookie: cookies.append(cookie)
        return cookies

    def make_cookies(self, response, request):
        """Return sequence of Cookie objects extracted from response object.

        See extract_cookies.__doc__ for the interfaces required of the
        response and request arguments.

        """
        # get cookie-attributes for RFC 2965 and Netscape protocols
        headers = response.info()
        rfc2965_hdrs = getheaders(headers, "Set-Cookie2")
        ns_hdrs = getheaders(headers, "Set-Cookie")

        rfc2965 = self.policy.rfc2965
        netscape = self.policy.netscape

        if ((not rfc2965_hdrs and not ns_hdrs) or
            (not ns_hdrs and not rfc2965) or
            (not rfc2965_hdrs and not netscape) or
            (not netscape and not rfc2965)):
            return []  # no relevant cookie headers: quick exit

        try:
            cookies = self._cookies_from_attrs_set(
                split_header_words(rfc2965_hdrs), request)
        except:
            reraise_unmasked_exceptions()
            cookies = []

        if ns_hdrs and netscape:
            try:
                ns_cookies = self._cookies_from_attrs_set(
                    parse_ns_headers(ns_hdrs), request)
            except:
                reraise_unmasked_exceptions()
                ns_cookies = []

            # Look for Netscape cookies (from Set-Cookie headers) that match
            # corresponding RFC 2965 cookies (from Set-Cookie2 headers).
            # For each match, keep the RFC 2965 cookie and ignore the Netscape
            # cookie (RFC 2965 section 9.1).  Actually, RFC 2109 cookies are
            # bundled in with the Netscape cookies for this purpose, which is
            # reasonable behaviour.
            if rfc2965:
                lookup = {}
                for cookie in cookies:
                    lookup[(cookie.domain, cookie.path, cookie.name)] = None

                def no_matching_rfc2965(ns_cookie, lookup=lookup):
                    key = ns_cookie.domain, ns_cookie.path, ns_cookie.name
                    return not lookup.has_key(key)
                ns_cookies = filter(no_matching_rfc2965, ns_cookies)

            if ns_cookies:
                cookies.extend(ns_cookies)

        return cookies

    def set_cookie_if_ok(self, cookie, request, unverifiable=False):
        """Set a cookie if policy says it's OK to do so.

        cookie: ClientCookie.Cookie instance
        request: see extract_cookies.__doc__ for the required interface
        unverifiable: see extract_cookies.__doc__

        """
        self._cookies_lock.acquire()
        self.policy._now = self._now = int(time.time())

        if self.policy.set_ok(cookie, request, unverifiable):
            self.set_cookie(cookie)

        self._cookies_lock.release()

    def set_cookie(self, cookie):
        """Set a cookie, without checking whether or not it should be set.

        cookie: ClientCookie.Cookie instance
        """
        c = self.cookies
        self._cookies_lock.acquire()
        try:
            if not c.has_key(cookie.domain): c[cookie.domain] = {}
            c2 = c[cookie.domain]
            if not c2.has_key(cookie.path): c2[cookie.path] = {}
            c3 = c2[cookie.path]
            c3[cookie.name] = cookie
        finally:
            self._cookies_lock.release()

    def extract_cookies(self, response, request, unverifiable=False):
        """Extract cookies from response, where allowable given the request.

        Look for allowable Set-Cookie: and Set-Cookie2: headers in the response
        object passed as argument.  Any of these headers that are found are
        used to update the state of the object (subject to the policy.set_ok
        method's approval).

        The response object (usually be the result of a call to
        ClientCookie.urlopen, or similar) should support an info method, which
        returns a mimetools.Message object (in fact, the 'mimetools.Message
        object' may be any object that provides a getallmatchingheaders
        method).

        The request object (usually a urllib2.Request instance) must support
        the methods get_full_url and get_host, as documented by urllib2, and
        the attributes headers (a mapping containing the request's HTTP
        headers) and port (the port number).  The request is used to set
        default values for cookie-attributes as well as for checking that the
        cookie is OK to be set.

        If unverifiable is true, it will be assumed that the transaction is
        unverifiable as defined by RFC 2965, and appropriate action will be
        taken.

        """
        debug("extract_cookies: %s" % response.info())
        self._cookies_lock.acquire()
        self.policy._now = self._now = int(time.time())

        for cookie in self.make_cookies(response, request):
            if self.policy.set_ok(cookie, request, unverifiable):
                debug(" setting cookie: "+str(cookie))
                self.set_cookie(cookie)
        self._cookies_lock.release()

    def save(self, filename=None, ignore_discard=False, ignore_expires=False):
        """Save cookies to a file.

        filename: name of file in which to save cookies
        ignore_discard: save even cookies set to be discarded
        ignore_expires: save even cookies that have expired

        The file is overwritten if it already exists, thus wiping all its
        cookies.  Saved cookies can be restored later using the load or revert
        methods.  If filename is not specified, self.filename is used; if
        self.filename is None, ValueError is raised.

        The CookieJar base class saves a sequence of "Set-Cookie3" lines.
        "Set-Cookie3" is the format used by the libwww-perl libary, not known
        to be compatible with any browser.  The MozillaCookieJar subclass can
        be used to save in a format compatible with the Netscape/Mozilla
        browsers.

        """
        if filename is None:
            if self.filename is not None: filename = self.filename
            else: raise ValueError(MISSING_FILENAME_TEXT)

        f = open(filename, "w")
        try:
            # There really isn't an LWP Cookies 2.0 format, but this indicates
            # that there is extra information in here (domain_dot and
            # port_spec) while still being compatible with libwww-perl, I hope.
            f.write("#LWP-Cookies-2.0\n")
            f.write(self.as_lwp_str(not ignore_discard, not ignore_expires))
        finally:
            f.close()

    def load(self, filename=None, ignore_discard=False, ignore_expires=False):
        """Load cookies from a file.

        Old cookies are kept unless overwritten by newly loaded ones.

        Cookies in the file will be loaded even if they have expired or are
        marked to be discarded.

        If filename is not specified, self.filename is used; if self.filename
        is None, ValueError is raised.  The named file must be in the format
        understood by the class, or IOError will be raised.  This format will
        be identical to that written by the save method, unless the load format
        is not sufficiently well understood (as is the case for MSIECookieJar).

        Note for subclassers: overridden versions of this method should not
        alter the object's state other than by calling self.set_cookie.

        """
        if filename is None:
            if self.filename is not None: filename = self.filename
            else: raise ValueError(MISSING_FILENAME_TEXT)

        f = open(filename)
        try:
            self._really_load(f, filename, ignore_discard, ignore_expires)
        finally:
            f.close()

    def _really_load(self, f, filename, ignore_discard, ignore_expires):
        magic = f.readline()
        if not re.search(self.magic_re, magic):
            msg = "%s does not seem to contain cookies" % filename
            raise IOError(msg)

        now = time.time()

        header = "Set-Cookie3:"
        boolean_attrs = ("port_spec", "path_spec", "domain_dot",
                         "secure", "discard")
        value_attrs = ("version",
                       "port", "path", "domain",
                       "expires",
                       "comment", "commenturl")

        try:
            while 1:
                line = f.readline()
                if line == "": break
                if not startswith(line, header):
                    continue
                line = string.strip(line[len(header):])

                for data in split_header_words([line]):
                    name, value = data[0]
                    # name and value are an exception here, since a plain "foo"
                    # (with no "=", unlike "bar=foo") means a cookie with no
                    # name and value "foo".  With all other cookie-attributes,
                    # the situation is reversed: "foo" means an attribute named
                    # "foo" with no value!
                    if value is None:
                        name, value = value, name
                    standard = {}
                    rest = {}
                    for k in boolean_attrs:
                        standard[k] = False
                    for k, v in data[1:]:
                        if k is not None:
                            lc = string.lower(k)
                        else:
                            lc = None
                        # don't lose case distinction for unknown fields
                        if (lc in value_attrs) or (lc in boolean_attrs):
                            k = lc
                        if k in boolean_attrs:
                            if v is None: v = True
                            standard[k] = v
                        elif k in value_attrs:
                            standard[k] = v
                        else:
                            rest[k] = v

                    h = standard.get
                    expires = h("expires")
                    discard = h("discard")
                    if expires is not None:
                        expires = iso2time(expires)
                    if expires is None:
                        discard = True
                    domain = h("domain")
                    domain_specified = startswith(domain, ".")
                    c = Cookie(h("version"), name, value,
                               h("port"), h("port_spec"),
                               domain, domain_specified, h("domain_dot"),
                               h("path"), h("path_spec"),
                               h("secure"),
                               expires,
                               discard,
                               h("comment"),
                               h("commenturl"),
                               rest)
                    if not ignore_discard and c.discard:
                        continue
                    if not ignore_expires and c.is_expired(now):
                        continue
                    self.set_cookie(c)
        except:
            reraise_unmasked_exceptions((IOError,))
            raise IOError("invalid Set-Cookie3 format file %s" % filename)

    def revert(self, filename=None,
               ignore_discard=False, ignore_expires=False):
        """Clear all cookies and reload cookies from a saved file.

        Raises IOError if reversion is not successful; the object's state will
        not be altered if this happens.

        """
        if filename is None:
            if self.filename is not None: filename = self.filename
            else: raise ValueError(MISSING_FILENAME_TEXT)

        self._cookies_lock.acquire()

        old_state = copy.deepcopy(self.cookies)
        self.cookies = {}
        try:
            self.load(filename, ignore_discard, ignore_expires)
        except IOError:
            self.cookies = old_state
            raise

        self._cookies_lock.release()

    def clear(self, domain=None, path=None, name=None):
        """Clear some cookies.

        Invoking this method without arguments will clear all cookies.  If
        given a single argument, only cookies belonging to that domain will be
        removed.  If given two arguments, cookies belonging to the specified
        path within that domain are removed.  If given three arguments, then
        the cookie with the specified name, path and domain is removed.

        Raises KeyError if no matching cookie exists.

        """
        if name is not None:
            if (domain is None) or (path is None):
                raise ValueError(
                    "domain and path must be given to remove a cookie by name")
                del self.cookies[domain][path][name]
        elif path is not None:
            if domain is None:
                raise ValueError(
                    "domain must be given to remove cookies by path")
            del self.cookies[domain][path]
        elif domain is not None:
            del self.cookies[domain]
        else:
            self.cookies = {}

    def clear_session_cookies(self):
        """Discard all session cookies.

        Discards all cookies held by object which had either no Max-Age or
        Expires cookie-attribute or an explicit Discard cookie-attribute, or
        which otherwise have ended up with a true discard attribute.  For
        interactive browsers, the end of a session usually corresponds to
        closing the browser window.

        Note that the save method won't save session cookies anyway, unless you
        ask otherwise by passing a true ignore_discard argument.

        """
        self._cookies_lock.acquire()
        for cookie in self:
            if cookie.discard:
                del self.cookies[cookie.domain][cookie.path][cookie.name]
        self._cookies_lock.release()

    def clear_expired_cookies(self):
        """Discard all expired cookies.

        You probably don't need to call this method: expired cookies are never
        sent back to the server (provided you're using DefaultCookiePolicy),
        this method is called by CookieJar itself every so often, and the save
        method won't save expired cookies anyway (unless you ask otherwise by
        passing a true ignore_expires argument).

        """
        self._cookies_lock.acquire()
        now = time.time()
        for cookie in self:
            if cookie.is_expired(now):
                del self.cookies[cookie.domain][cookie.path][cookie.name]
        self._cookies_lock.release()

    def __getitem__(self, i):
        if i == 0:
            self._getitem_iterator = self.__iter__()
        elif self._prev_getitem_index != i-1: raise IndexError(
            "CookieJar.__getitem__ only supports sequential iteration")
        self._prev_getitem_index = i
        try:
            return self._getitem_iterator.next()
        except StopIteration:
            raise IndexError()

    def __iter__(self):
        return MappingIterator(self.cookies)

    def __len__(self):
        """Return number of contained cookies."""
        i = 0
        for cookie in self: i = i + 1
        return i

    def __repr__(self):
        r = []
        for cookie in self: r.append(repr(cookie))
        return "<%s[%s]>" % (self.__class__, string.join(r, ", "))

    def __str__(self):
        r = []
        for cookie in self: r.append(str(cookie))
        return "<%s[%s]>" % (self.__class__, string.join(r, ", "))

    def as_lwp_str(self, skip_discard=False, skip_expired=False):
        """Return cookies as a string of "\n"-separated "Set-Cookie3" headers.

        If skip_discard is true, it will not return lines for cookies with the
        Discard cookie-attribute.

        """
        now = time.time()
        r = []
        for cookie in self:
            if skip_discard and cookie.discard:
                continue
            if skip_expired and cookie.is_expired(now):
                continue
            r.append("Set-Cookie3: %s" % lwp_cookie_str(cookie))
        return string.join(r+[""], "\n")
