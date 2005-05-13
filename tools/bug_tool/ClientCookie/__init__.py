# Import names so that they can be imported directly from the package, like
# this:
#from ClientCookie import <whatever>

try: True
except NameError:
    True = 1
    False = 0

import sys

# don't edit these here: do eg.
# import ClientCookie; ClientCookie.HTTP_DEBUG = 1
DEBUG_STREAM = sys.stderr
CLIENTCOOKIE_DEBUG = False
REDIRECT_DEBUG = False
HTTP_DEBUG = False

from _ClientCookie import VERSION, __doc__, \
     CookieJar, Cookie, \
     CookiePolicy, DefaultCookiePolicy, \
     lwp_cookie_str
from _MozillaCookieJar import MozillaCookieJar
from _MSIECookieJar import MSIECookieJar
try:
    from urllib2 import AbstractHTTPHandler
except ImportError:
    pass
else:
    from ClientCookie._urllib2_support import \
         HTTPHandler, build_opener, install_opener, urlopen, \
         HTTPRedirectHandler
    from ClientCookie._urllib2_support import \
         OpenerDirector, BaseProcessor, \
         HTTPRequestUpgradeProcessor, \
         HTTPEquivProcessor, SeekableProcessor, HTTPCookieProcessor, \
         HTTPRefererProcessor, HTTPStandardHeadersProcessor, \
         HTTPRefreshProcessor, HTTPErrorProcessor, \
         HTTPResponseDebugProcessor

    import httplib
    if hasattr(httplib, 'HTTPS'):
        from ClientCookie._urllib2_support import HTTPSHandler
    del AbstractHTTPHandler, httplib
from _Util import http2time
str2time = http2time
del http2time

del sys
