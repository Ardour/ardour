"""Integration with Python standard library module urllib2.

Also includes a redirection bugfix, support for parsing HTML HEAD blocks for
the META HTTP-EQUIV tag contents, and following Refresh header redirects.

Copyright 2002-2003 John J Lee <jjl@pobox.com>

This code is free software; you can redistribute it and/or modify it under
the terms of the BSD License (see the file COPYING included with the
distribution).

"""

import copy, time

import ClientCookie
from _ClientCookie import CookieJar, request_host
from _Util import isstringlike
from _Debug import _debug

try: True
except NameError:
    True = 1
    False = 0

CHUNK = 1024  # size of chunks fed to HTML HEAD parser, in bytes

try:
    from urllib2 import AbstractHTTPHandler
except ImportError:
    pass
else:
    import urlparse, urllib2, urllib, httplib, htmllib, formatter, string
    from urllib2 import URLError, HTTPError
    import types, string, socket
    from cStringIO import StringIO
    from _Util import seek_wrapper
    try:
        import threading
        _threading = threading; del threading
    except ImportError:
        import dummy_threading
        _threading = dummy_threading; del dummy_threading

    # This fixes a bug in urllib2 as of Python 2.1.3 and 2.2.2
    #  (http://www.python.org/sf/549151)
    # 2.2.3 is broken here (my fault!), 2.3 is fixed.
    class HTTPRedirectHandler(urllib2.BaseHandler):
        # maximum number of redirections before assuming we're in a loop
        max_redirections = 10

        # Implementation notes:

        # To avoid the server sending us into an infinite loop, the request
        # object needs to track what URLs we have already seen.  Do this by
        # adding a handler-specific attribute to the Request object.  The value
        # of the dict is used to count the number of times the same url has
        # been visited.  This is needed because this isn't necessarily a loop:
        # there is more than one way to redirect (Refresh, 302, 303, 307).

        # Another handler-specific Request attribute, original_url, is used to
        # remember the URL of the original request so that it is possible to
        # decide whether or not RFC 2965 cookies should be turned on during
        # redirect.

        # Always unhandled redirection codes:
        # 300 Multiple Choices: should not handle this here.
        # 304 Not Modified: no need to handle here: only of interest to caches
        #     that do conditional GETs
        # 305 Use Proxy: probably not worth dealing with here
        # 306 Unused: what was this for in the previous versions of protocol??

        def redirect_request(self, newurl, req, fp, code, msg, headers):
            """Return a Request or None in response to a redirect.

            This is called by the http_error_30x methods when a redirection
            response is received.  If a redirection should take place, return a
            new Request to allow http_error_30x to perform the redirect;
            otherwise, return None to indicate that an HTTPError should be
            raised.

            """
            if code in (301, 302, 303) or (code == 307 and not req.has_data()):
                # Strictly (according to RFC 2616), 301 or 302 in response to
                # a POST MUST NOT cause a redirection without confirmation
                # from the user (of urllib2, in this case).  In practice,
                # essentially all clients do redirect in this case, so we do
                # the same.
                return Request(newurl, headers=req.headers)
            else:
                raise HTTPError(req.get_full_url(), code, msg, headers, fp)

        def http_error_302(self, req, fp, code, msg, headers):
            if headers.has_key('location'):
                newurl = headers['location']
            elif headers.has_key('uri'):
                newurl = headers['uri']
            else:
                return
            newurl = urlparse.urljoin(req.get_full_url(), newurl)

            # XXX Probably want to forget about the state of the current
            # request, although that might interact poorly with other
            # handlers that also use handler-specific request attributes
            new = self.redirect_request(newurl, req, fp, code, msg, headers)
            if new is None:
                return

            # remember where we started from
            if hasattr(req, "original_url"):
                new.original_url = req.original_url
            else:
                new.original_url = req.get_full_url()

            # loop detection
            # .error_302_dict[(url, code)] is number of times url
            # previously visited as a result of a redirection with this
            # code (error_30x_dict would be a better name).
            new.origin_req_host = req.origin_req_host
            if not hasattr(req, 'error_302_dict'):
                new.error_302_dict = req.error_302_dict = {(newurl, code): 1}
            else:
                ed = new.error_302_dict = req.error_302_dict
                nr_visits = ed.get((newurl, code), 0)
                # Refreshes generate fake 302s, so we can hit the same URL as
                # a result of the same redirection code twice without
                # necessarily being in a loop!  So, allow two visits to each
                # URL as a result of each redirection code.
                if len(ed) < self.max_redirections and nr_visits < 2:
                    ed[(newurl, code)] = nr_visits + 1
                else:
                    raise HTTPError(req.get_full_url(), code,
                                    self.inf_msg + msg, headers, fp)

            if ClientCookie.REDIRECT_DEBUG:
                _debug("redirecting to %s", newurl)

            # Don't close the fp until we are sure that we won't use it
            # with HTTPError.  
            fp.read()
            fp.close()

            return self.parent.open(new)

        http_error_301 = http_error_303 = http_error_307 = http_error_302

        inf_msg = "The HTTP server returned a redirect error that would " \
                  "lead to an infinite loop.\n" \
                  "The last 30x error message was:\n"


    class Request(urllib2.Request):
        def __init__(self, url, data=None, headers={}):
            urllib2.Request.__init__(self, url, data, headers)
            self.unredirected_hdrs = {}

        def add_unredirected_header(self, key, val):
            # these headers do not persist from one request to the next in a chain
            # of requests
            self.unredirected_hdrs[string.capitalize(key)] = val

        def has_key(self, header_name):
            if (self.headers.has_key(header_name) or
                self.unredirected_hdrs.has_key(header_name)):
                return True
            return False

        def get(self, header_name, failobj=None):
            if self.headers.has_key(header_name):
                return self.headers[header_name]
            if self.unredirected_headers.has_key(header_name):
                return self.unredirected_headers[header_name]
            return failobj


    class BaseProcessor:
        processor_order = 500

        def add_parent(self, parent):
            self.parent = parent
        def close(self):
            self.parent = None
        def __lt__(self, other):
            if not hasattr(other, "processor_order"):
                return True
            return self.processor_order < other.processor_order

    class HTTPRequestUpgradeProcessor(BaseProcessor):
        # upgrade Request to class with support for headers that don't get
        # redirected
        processor_order = 0  # before anything else

        def http_request(self, request):
            if not hasattr(request, "add_unredirected_header"):
                request = Request(request._Request__original, request.data,
                                  request.headers)
            return request

        https_request = http_request

    class HTTPEquivProcessor(BaseProcessor):
        """Append META HTTP-EQUIV headers to regular HTTP headers."""
        def http_response(self, request, response):
            if not hasattr(response, "seek"):
                response = seek_wrapper(response)
            # grab HTTP-EQUIV headers and add them to the true HTTP headers
            headers = response.info()
            for hdr, val in parse_head(response):
                headers[hdr] = val
            response.seek(0)
            return response

        https_response = http_response

    # XXX ATM this only takes notice of http responses -- probably
    #   should be independent of protocol scheme (http, ftp, etc.)
    class SeekableProcessor(BaseProcessor):
        """Make responses seekable."""

        def http_response(self, request, response):
            if not hasattr(response, "seek"):
                return seek_wrapper(response)
            return response

        https_response = http_response

    # XXX if this gets added to urllib2, unverifiable would end up as an
    #   attribute on Request.
    class HTTPCookieProcessor(BaseProcessor):
        """Handle HTTP cookies."""
        def __init__(self, cookies=None):
            if cookies is None:
                cookies = CookieJar()
            self.cookies = cookies

        def _unverifiable(self, request):
            if hasattr(request, "error_302_dict") and request.error_302_dict:
                redirect = True
            else:
                redirect = False
            if (redirect or
                (hasattr(request, "unverifiable") and request.unverifiable)):
                unverifiable = True
            else:
                unverifiable = False
            return unverifiable

        def http_request(self, request):
            unverifiable = self._unverifiable(request)
            if not unverifiable:
                # Stuff request-host of this origin transaction into Request
                # object, because we need to know it to know whether cookies
                # should be in operation during derived requests (redirects,
                # specifically -- including refreshes).
                request.origin_req_host = request_host(request)
            self.cookies.add_cookie_header(request, unverifiable)
            return request

        def http_response(self, request, response): 
            unverifiable = self._unverifiable(request)
            self.cookies.extract_cookies(response, request, unverifiable)
            return response

        https_request = http_request
        https_response = http_response

    class HTTPRefererProcessor(BaseProcessor):
        """Add Referer header to requests.

        This only makes sense if you use each RefererProcessor for a single
        chain of requests only (so, for example, if you use a single
        HTTPRefererProcessor to fetch a series of URLs extracted from a single
        page, this will break).

        """
        def __init__(self):
            self.referer = None

        def http_request(self, request):
            if ((self.referer is not None) and
                not request.has_key("Referer")):
                request.add_unredirected_header("Referer", self.referer)
            return request

        def http_response(self, request, response):
            self.referer = response.geturl()
            return response

        https_request = http_request
        https_response = http_response

    class HTTPStandardHeadersProcessor(BaseProcessor):
        def http_request(self, request):
            host = request.get_host()
            if not host:
                raise URLError('no host given')

            if request.has_data():  # POST
                data = request.get_data()
                if not request.has_key('Content-type'):
                    request.add_unredirected_header(
                        'Content-type',
                        'application/x-www-form-urlencoded')
                if not request.has_key('Content-length'):
                    request.add_unredirected_header(
                        'Content-length', '%d' % len(data))

            scheme, sel = urllib.splittype(request.get_selector())
            sel_host, sel_path = urllib.splithost(sel)
            if not request.has_key('Host'):
                request.add_unredirected_header('Host', sel_host or host)
            for name, value in self.parent.addheaders:
                name = string.capitalize(name)
                if not request.has_key(name):
                    request.add_unredirected_header(name, value)

            return request

        https_request = http_request

    class HTTPResponseDebugProcessor(BaseProcessor):
        processor_order = 900  # before redirections, after everything else

        def http_response(self, request, response):
            if not hasattr(response, "seek"):
                response = seek_wrapper(response)
            _debug(response.read())
            _debug("*****************************************************")
            response.seek(0)
            return response

        https_response = http_response

    class HTTPRefreshProcessor(BaseProcessor):
        """Perform HTTP Refresh redirections.

        Note that if a non-200 HTTP code has occurred (for example, a 30x
        redirect), this processor will do nothing.

        By default, only zero-time Refresh headers are redirected.  Use the
        max_time constructor argument to allow Refresh with longer pauses.
        Use the honor_time argument to control whether the requested pause
        is honoured (with a time.sleep()) or skipped in favour of immediate
        redirection.

        """
        processor_order = 1000

        def __init__(self, max_time=0, honor_time=True):
            self.max_time = max_time
            self.honor_time = honor_time

        def http_response(self, request, response):
            code, msg, hdrs = response.code, response.msg, response.info()

            if code == 200 and hdrs.has_key("refresh"):
                refresh = hdrs["refresh"]
                i = string.find(refresh, ";")
                if i != -1:
                    pause, newurl_spec = refresh[:i], refresh[i+1:]
                    i = string.find(newurl_spec, "=")
                    if i != -1:
                        pause = int(pause)
                        if pause <= self.max_time:
                            if pause != 0 and self.honor_time:
                                time.sleep(pause)
                            newurl = newurl_spec[i+1:]
                            # fake a 302 response
                            hdrs["location"] = newurl
                            response = self.parent.error(
                                'http', request, response, 302, msg, hdrs)

            return response

        https_response = http_response

    class HTTPErrorProcessor(BaseProcessor):
        """Process non-200 HTTP error responses.

        This just passes the job on to the Handler.<proto>_error_<code>
        methods, via the OpenerDirector.error method.

        """
        processor_order = 1000

        def http_response(self, request, response):
            code, msg, hdrs = response.code, response.msg, response.info()

            if code != 200:
                response = self.parent.error(
                    'http', request, response, code, msg, hdrs)

            return response

        https_response = http_response


    class OpenerDirector(urllib2.OpenerDirector):
        # XXX might be useful to have remove_processor, too (say you want to
        #   set a new RefererProcessor, but keep the old CookieProcessor --
        #   could always just create everything anew, though (using old
        #   CookieJar object to create CookieProcessor)
        def __init__(self):
            urllib2.OpenerDirector.__init__(self)
            #self.processors = []
            self.process_response = {}
            self.process_request = {}

        def add_handler(self, handler):
            # XXX
            # tidy me
            # the same handler could be added twice without detection
            added = 0
            for meth in dir(handler.__class__):
                if meth[-5:] == '_open':
                    protocol = meth[:-5]
                    if self.handle_open.has_key(protocol):
                        self.handle_open[protocol].append(handler)
                        self.handle_open[protocol].sort()
                    else:
                        self.handle_open[protocol] = [handler]
                    added = 1
                    continue
                i = string.find(meth, '_')
                j = string.find(meth[i+1:], '_') + i + 1
                if j != -1 and meth[i+1:j] == 'error':
                    proto = meth[:i]
                    kind = meth[j+1:]
                    try:
                        kind = int(kind)
                    except ValueError:
                        pass
                    dict = self.handle_error.get(proto, {})
                    if dict.has_key(kind):
                        dict[kind].append(handler)
                        dict[kind].sort()
                    else:
                        dict[kind] = [handler]
                    self.handle_error[proto] = dict
                    added = 1
                    continue
                if meth[-9:] == "_response":
                    protocol = meth[:-9]
                    if self.process_response.has_key(protocol):
                        self.process_response[protocol].append(handler)
                        self.process_response[protocol].sort()
                    else:
                        self.process_response[protocol] = [handler]
                    added = True
                    continue
                elif meth[-8:] == "_request":
                    protocol = meth[:-8]
                    if self.process_request.has_key(protocol):
                        self.process_request[protocol].append(handler)
                        self.process_request[protocol].sort()
                    else:
                        self.process_request[protocol] = [handler]
                    added = True
                    continue
            if added:
                self.handlers.append(handler)
                self.handlers.sort()
                handler.add_parent(self)

##         def add_processor(self, processor):
##             added = False
##             for meth in dir(processor):
##                 if meth[-9:] == "_response":
##                     protocol = meth[:-9]
##                     if self.process_response.has_key(protocol):
##                         self.process_response[protocol].append(processor)
##                         self.process_response[protocol].sort()
##                     else:
##                         self.process_response[protocol] = [processor]
##                     added = True
##                     continue
##                 elif meth[-8:] == "_request":
##                     protocol = meth[:-8]
##                     if self.process_request.has_key(protocol):
##                         self.process_request[protocol].append(processor)
##                         self.process_request[protocol].sort()
##                     else:
##                         self.process_request[protocol] = [processor]
##                     added = True
##                     continue
##             if added:
##                 self.processors.append(processor)
##                 # XXX base class sorts .handlers, but I have no idea why
##                 #self.processors.sort()
##                 processor.add_parent(self)

        def _request(self, url_or_req, data):
            if isstringlike(url_or_req):
                req = Request(url_or_req, data)
            else:
                # already a urllib2.Request instance
                req = url_or_req
                if data is not None:
                    req.add_data(data)
            return req

        def open(self, fullurl, data=None):
            req = self._request(fullurl, data)
            type = req.get_type()

            # pre-process request
            # XXX should we allow a Processor to change the type (URL
            #   scheme) of the request?
            meth_name = type+"_request"
            for processor in self.process_request.get(type, []):
                meth = getattr(processor, meth_name)
                req = meth(req)

            response = urllib2.OpenerDirector.open(self, req, data)

            # post-process response
            meth_name = type+"_response"
            for processor in self.process_response.get(type, []):
                meth = getattr(processor, meth_name)
                response = meth(req, response)

            return response

##         def close(self):
##             urllib2.OpenerDirector.close(self)
##             for processor in self.processors:
##                 processor.close()
##             self.processors = []


    # Note the absence of redirect and header-adding code here
    # (AbstractHTTPHandler), and the lack of other clutter that would be
    # here without Processors.
    class AbstractHTTPHandler(urllib2.BaseHandler):
        def do_open(self, http_class, req):
            host = req.get_host()
            if not host:
                raise URLError('no host given')

            h = http_class(host) # will parse host:port
            if ClientCookie.HTTP_DEBUG:
                h.set_debuglevel(1)

            if req.has_data():
                h.putrequest('POST', req.get_selector())
            else:
                h.putrequest('GET', req.get_selector())

            for k, v in req.headers.items():
                h.putheader(k, v)
            for k, v in req.unredirected_hdrs.items():
                h.putheader(k, v)

            # httplib will attempt to connect() here.  be prepared
            # to convert a socket error to a URLError.
            try:
                h.endheaders()
            except socket.error, err:
                raise URLError(err)
            if req.has_data():
                h.send(req.get_data())

            code, msg, hdrs = h.getreply()
            fp = h.getfile()

            response = urllib.addinfourl(fp, hdrs, req.get_full_url())
            response.code = code
            response.msg = msg

            return response


    # XXX would self.reset() work, instead of raising this exception?
    class EndOfHeadError(Exception): pass
    class HeadParser(htmllib.HTMLParser):
        # only these elements are allowed in or before HEAD of document
        head_elems = ("html", "head",
                      "title", "base",
                      "script", "style", "meta", "link", "object")
        def __init__(self):
            htmllib.HTMLParser.__init__(self, formatter.NullFormatter())
            self.http_equiv = []

        def start_meta(self, attrs):
            http_equiv = content = None
            for key, value in attrs:
                if key == "http-equiv":
                    http_equiv = value
                elif key == "content":
                    content = value
            if http_equiv is not None:
                self.http_equiv.append((http_equiv, content))

        def handle_starttag(self, tag, method, attrs):
            if tag in self.head_elems:
                method(attrs)
            else:
                raise EndOfHeadError()

        def handle_endtag(self, tag, method):
            if tag in self.head_elems:
                method()
            else:
                raise EndOfHeadError()

        def end_head(self):
            raise EndOfHeadError()

    def parse_head(file):
        """Return a list of key, value pairs."""
        hp = HeadParser()
        while 1:
            data = file.read(CHUNK)
            try:
                hp.feed(data)
            except EndOfHeadError:
                break
            if len(data) != CHUNK:
                # this should only happen if there is no HTML body, or if
                # CHUNK is big
                break
        return hp.http_equiv


    class HTTPHandler(AbstractHTTPHandler):
        def http_open(self, req):
            return self.do_open(httplib.HTTP, req)

    if hasattr(httplib, 'HTTPS'):
        class HTTPSHandler(AbstractHTTPHandler):
            def https_open(self, req):
                return self.do_open(httplib.HTTPS, req)


    def build_opener(*handlers):
        """Create an opener object from a list of handlers and processors.

        The opener will use several default handlers and processors, including
        support for HTTP and FTP.  If there is a ProxyHandler, it must be at the
        front of the list of handlers.  (Yuck.  This is fixed in 2.3.)

        If any of the handlers passed as arguments are subclasses of the
        default handlers, the default handlers will not be used.
        """
        opener = OpenerDirector()
        default_classes = [
            # handlers
            urllib2.ProxyHandler,
            urllib2.UnknownHandler,
            HTTPHandler,  # from this module (derived from new AbstractHTTPHandler)
            urllib2.HTTPDefaultErrorHandler,
            HTTPRedirectHandler,  # from this module (bugfixed)
            urllib2.FTPHandler,
            urllib2.FileHandler,
            # processors
            HTTPRequestUpgradeProcessor,
            #HTTPEquivProcessor,
            #SeekableProcessor,
            HTTPCookieProcessor,
            #HTTPRefererProcessor,
            HTTPStandardHeadersProcessor,
            #HTTPRefreshProcessor,
            HTTPErrorProcessor
            ]
        if hasattr(httplib, 'HTTPS'):
            default_classes.append(HTTPSHandler)
        skip = []
        for klass in default_classes:
            for check in handlers:
                if type(check) == types.ClassType:
                    if issubclass(check, klass):
                        skip.append(klass)
                elif type(check) == types.InstanceType:
                    if isinstance(check, klass):
                        skip.append(klass)
        for klass in skip:
            default_classes.remove(klass)

        to_add = []
        for klass in default_classes:
            to_add.append(klass())
        for h in handlers:
            if type(h) == types.ClassType:
                h = h()
            to_add.append(h)

        for instance in to_add:
            opener.add_handler(instance)
##             # yuck
##             if hasattr(instance, "processor_order"):
##                 opener.add_processor(instance)
##             else:
##                 opener.add_handler(instance)

        return opener


    _opener = None
    urlopen_lock = _threading.Lock()
    def urlopen(url, data=None):
        global _opener
        if _opener is None:
            urlopen_lock.acquire()
            try:
                if _opener is None:
                    _opener = build_opener()
            finally:
                urlopen_lock.release()
        return _opener.open(url, data)

    def install_opener(opener):
        global _opener
        _opener = opener
