"""Mozilla / Netscape cookie loading / saving.

Copyright 1997-1999 Gisle Aas (libwww-perl)
Copyright 2002-2003 John J Lee <jjl@pobox.com> (The Python port)

This code is free software; you can redistribute it and/or modify it under
the terms of the BSD License (see the file COPYING included with the
distribution).

"""

import sys, re, string, time

import ClientCookie
from _ClientCookie import CookieJar, Cookie, MISSING_FILENAME_TEXT
from _Util import startswith, endswith
from _Debug import debug

try: True
except NameError:
    True = 1
    False = 0

try: issubclass(Exception(), (Exception,))
except TypeError:
    real_issubclass = issubclass
    from _Util import compat_issubclass
    issubclass = compat_issubclass
    del compat_issubclass


class MozillaCookieJar(CookieJar):
    """

    WARNING: you may want to backup your browser's cookies file if you use
    this class to save cookies.  I *think* it works, but there have been
    bugs in the past!

    This class differs from CookieJar only in the format it uses to save and
    load cookies to and from a file.  This class uses the Netscape/Mozilla
    `cookies.txt' format.

    Don't expect cookies saved while the browser is running to be noticed by
    the browser (in fact, Mozilla on unix will overwrite your saved cookies if
    you change them on disk while it's running; on Windows, you probably can't
    save at all while the browser is running).

    Note that the Netscape/Mozilla format will downgrade RFC2965 cookies to
    Netscape cookies on saving.

    In particular, the cookie version and port number information is lost,
    together with information about whether or not Path, Port and Discard were
    specified by the Set-Cookie2 (or Set-Cookie) header, and whether or not the
    domain as set in the HTTP header started with a dot (yes, I'm aware some
    domains in Netscape files start with a dot and some don't -- trust me, you
    really don't want to know any more about this).

    Note that though Mozilla and Netscape use the same format, they use
    slightly different headers.  The class saves cookies using the Netscape
    header by default (Mozilla can cope with that).

    """
    magic_re = "#( Netscape)? HTTP Cookie File"
    header = """\
    # Netscape HTTP Cookie File
    # http://www.netscape.com/newsref/std/cookie_spec.html
    # This is a generated file!  Do not edit.

"""

    def _really_load(self, f, filename, ignore_discard, ignore_expires):
        now = time.time()

        magic = f.readline()
        if not re.search(self.magic_re, magic):
            f.close()
            raise IOError(
                "%s does not look like a Netscape format cookies file" %
                filename)

        try:
            while 1:
                line = f.readline()
                if line == "": break

                # last field may be absent, so keep any trailing tab
                if endswith(line, "\n"): line = line[:-1]

                # skip comments and blank lines XXX what is $ for?
                if (startswith(string.strip(line), "#") or
                    startswith(string.strip(line), "$") or
                    string.strip(line) == ""):
                    continue

                domain, domain_specified, path, secure, expires, name, value = \
                        string.split(line, "\t")
                secure = (secure == "TRUE")
                domain_specified = (domain_specified == "TRUE")
                if name == "": name = None

                initial_dot = startswith(domain, ".")
                assert domain_specified == initial_dot

                discard = False
                if expires == "":
                    expires = None
                    discard = True

                # assume path_specified is false
                c = Cookie(0, name, value,
                           None, False,
                           domain, domain_specified, initial_dot,
                           path, False,
                           secure,
                           expires,
                           discard,
                           None,
                           None,
                           {})
                if not ignore_discard and c.discard:
                    continue
                if not ignore_expires and c.is_expired(now):
                    continue
                self.set_cookie(c)

        except:
            unmasked = (KeyboardInterrupt, SystemExit)
            if ClientCookie.CLIENTCOOKIE_DEBUG:
                unmasked = (Exception,)
            etype = sys.exc_info()[0]
            if issubclass(etype, IOError) or \
                   issubclass(etype, unmasked):
                raise
            raise IOError("invalid Netscape format file %s: %s" %
                          (filename, line))

    def save(self, filename=None, ignore_discard=False, ignore_expires=False):
        if filename is None:
            if self.filename is not None: filename = self.filename
            else: raise ValueError(MISSING_FILENAME_TEXT)

        f = open(filename, "w")
        try:
            f.write(self.header)
            now = time.time()
            debug("Saving Netscape cookies.txt file")
            for cookie in self:
                if not ignore_discard and cookie.discard:
                    debug("   Not saving %s: marked for discard" % cookie.name)
                    continue
                if not ignore_expires and cookie.is_expired(now):
                    debug("   Not saving %s: expired" % cookie.name)
                    continue
                if cookie.secure: secure = "TRUE"
                else: secure = "FALSE"
                if startswith(cookie.domain, "."): initial_dot = "TRUE"
                else: initial_dot = "FALSE"
                if cookie.expires is not None:
                    expires = str(cookie.expires)
                else:
                    expires = ""
                if cookie.name is not None:
                    name = cookie.name
                else:
                    name = ""
                f.write(
                    string.join([cookie.domain, initial_dot, cookie.path,
                                 secure, expires, name, cookie.value], "\t")+
                    "\n")
        finally:
            f.close()
