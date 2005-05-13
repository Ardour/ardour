"""Python backwards-compat., date/time routines, seekable file object wrapper.

 Copyright 2002-2003 John J Lee <jjl@pobox.com>

This code is free software; you can redistribute it and/or modify it under
the terms of the BSD License (see the file COPYING included with the
distribution).

"""

try: True
except NameError:
    True = 1
    False = 0

import re, string, time
from types import TupleType
from StringIO import StringIO

try:
    from exceptions import StopIteration
except ImportError:
    from ClientCookie._ClientCookie import StopIteration

def startswith(string, initial):
    if len(initial) > len(string): return False
    return string[:len(initial)] == initial

def endswith(string, final):
    if len(final) > len(string): return False
    return string[-len(final):] == final

def compat_issubclass(obj, tuple_or_class):
    # for 2.1 and below
    if type(tuple_or_class) == TupleType:
        for klass in tuple_or_class:
            if issubclass(obj, klass):
                return True
        return False
    return issubclass(obj, tuple_or_class)

def isstringlike(x):
    try: x+""
    except: return False
    else: return True


try:
    from calendar import timegm
    timegm((2045, 1, 1, 22, 23, 32))  # overflows in 2.1
except:
    # Number of days per month (except for February in leap years)
    mdays = [0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31]

    # Return 1 for leap years, 0 for non-leap years
    def isleap(year):
	return year % 4 == 0 and (year % 100 <> 0 or year % 400 == 0)

    # Return number of leap years in range [y1, y2)
    # Assume y1 <= y2 and no funny (non-leap century) years
    def leapdays(y1, y2):
	return (y2+3)/4 - (y1+3)/4

    EPOCH = 1970
    def timegm(tuple):
        """Unrelated but handy function to calculate Unix timestamp from GMT."""
        year, month, day, hour, minute, second = tuple[:6]
        assert year >= EPOCH
        assert 1 <= month <= 12
        days = 365*(year-EPOCH) + leapdays(EPOCH, year)
        for i in range(1, month):
            days = days + mdays[i]
        if month > 2 and isleap(year):
            days = days + 1
        days = days + day - 1
        hours = days*24 + hour
        minutes = hours*60 + minute
        seconds = minutes*60L + second
        return seconds


# Date/time conversion routines for formats used by the HTTP protocol.

EPOCH = 1970
def my_timegm(tt):
    year, month, mday, hour, min, sec = tt[:6]
    if ((year >= EPOCH) and (1 <= month <= 12) and (1 <= mday <= 31) and
        (0 <= hour <= 24) and (0 <= min <= 59) and (0 <= sec <= 61)):
        return timegm(tt)
    else:
        return None

days = ["Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun"]
months = ["Jan", "Feb", "Mar", "Apr", "May", "Jun",
          "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
months_lower = []
for month in months: months_lower.append(string.lower(month))


def time2isoz(t=None):
    """Return a string representing time in seconds since epoch, t.

    If the function is called without an argument, it will use the current
    time.

    The format of the returned string is like "YYYY-MM-DD hh:mm:ssZ",
    representing Universal Time (UTC, aka GMT).  An example of this format is:

    1994-11-24 08:49:37Z

    """
    if t is None: t = time.time()
    year, mon, mday, hour, min, sec = time.gmtime(t)[:6]
    return "%04d-%02d-%02d %02d:%02d:%02dZ" % (
        year, mon, mday, hour, min, sec)

def time2netscape(t=None):
    """Return a string representing time in seconds since epoch, t.

    If the function is called without an argument, it will use the current
    time.

    The format of the returned string is like this:

    Wdy, DD-Mon-YYYY HH:MM:SS GMT

    """
    if t is None: t = time.time()
    year, mon, mday, hour, min, sec, wday = time.gmtime(t)[:7]
    return "%s %02d-%s-%04d %02d:%02d:%02d GMT" % (
        days[wday], mday, months[mon-1], year, hour, min, sec)


UTC_ZONES = {"GMT": None, "UTC": None, "UT": None, "Z": None}

timezone_re = re.compile(r"^([-+])?(\d\d?):?(\d\d)?$")
def offset_from_tz_string(tz):
    offset = None
    if UTC_ZONES.has_key(tz):
        offset = 0
    else:
        m = timezone_re.search(tz)
        if m:
            offset = 3600 * int(m.group(2))
            if m.group(3):
                offset = offset + 60 * int(m.group(3))
            if m.group(1) == '-':
                offset = -offset
    return offset

def _str2time(day, mon, yr, hr, min, sec, tz):
    # translate month name to number
    # month numbers start with 1 (January)
    try:
        mon = months_lower.index(string.lower(mon))+1
    except ValueError:
        # maybe it's already a number
        try:
            imon = int(mon)
        except ValueError:
            return None
        if 1 <= imon <= 12:
            mon = imon
        else:
            return None

    # make sure clock elements are defined
    if hr is None: hr = 0
    if min is None: min = 0
    if sec is None: sec = 0

    yr = int(yr)
    day = int(day)
    hr = int(hr)
    min = int(min)
    sec = int(sec)

    if yr < 1000:
	# find "obvious" year
	cur_yr = time.localtime(time.time())[0]
	m = cur_yr % 100
	tmp = yr
	yr = yr + cur_yr - m
	m = m - tmp
        if abs(m) > 50:
            if m > 0: yr = yr + 100
            else: yr = yr - 100

    # convert UTC time tuple to seconds since epoch (not timezone-adjusted)
    t = my_timegm((yr, mon, day, hr, min, sec, tz))

    if t is not None:
        # adjust time using timezone string, to get absolute time since epoch
        if tz is None:
            tz = "UTC"
        tz = string.upper(tz)
        offset = offset_from_tz_string(tz)
        if offset is None:
            return None
        t = t - offset

    return t


strict_re = re.compile(r"^[SMTWF][a-z][a-z], (\d\d) ([JFMASOND][a-z][a-z]) (\d\d\d\d) (\d\d):(\d\d):(\d\d) GMT$")
wkday_re = re.compile(
    r"^(?:Sun|Mon|Tue|Wed|Thu|Fri|Sat)[a-z]*,?\s*", re.I)
loose_http_re = re.compile(
    r"""^
    (\d\d?)            # day
       (?:\s+|[-\/])
    (\w+)              # month
        (?:\s+|[-\/])
    (\d+)              # year
    (?:
	  (?:\s+|:)    # separator before clock
       (\d\d?):(\d\d)  # hour:min
       (?::(\d\d))?    # optional seconds
    )?                 # optional clock
       \s*
    ([-+]?\d{2,4}|(?![APap][Mm]\b)[A-Za-z]+)? # timezone
       \s*
    (?:\(\w+\))?       # ASCII representation of timezone in parens.
       \s*$""", re.X)
def http2time(text):
    """Returns time in seconds since epoch of time represented by a string.

    Return value is an integer.

    None is returned if the format of str is unrecognized, the time is outside
    the representable range, or the timezone string is not recognized.  The
    time formats recognized are the same as for parse_date.  If the string
    contains no timezone, UTC is assumed.

    The timezone in the string may be numerical (like "-0800" or "+0100") or a
    string timezone (like "UTC", "GMT", "BST" or "EST").  Currently, only the
    timezone strings equivalent to UTC (zero offset) are known to the function.

    The function loosely parses the following formats:

    Wed, 09 Feb 1994 22:23:32 GMT       -- HTTP format
    Tuesday, 08-Feb-94 14:15:29 GMT     -- old rfc850 HTTP format
    Tuesday, 08-Feb-1994 14:15:29 GMT   -- broken rfc850 HTTP format
    09 Feb 1994 22:23:32 GMT            -- HTTP format (no weekday)
    08-Feb-94 14:15:29 GMT              -- rfc850 format (no weekday)
    08-Feb-1994 14:15:29 GMT            -- broken rfc850 format (no weekday)

    The parser ignores leading and trailing whitespace.  The time may be
    absent.

    If the year is given with only 2 digits, then parse_date will select the
    century that makes the year closest to the current date.

    """
    # fast exit for strictly conforming string
    m = strict_re.search(text)
    if m:
        g = m.groups()
        mon = months_lower.index(string.lower(g[1])) + 1
        tt = (int(g[2]), mon, int(g[0]),
              int(g[3]), int(g[4]), float(g[5]))
        return my_timegm(tt)

    # No, we need some messy parsing...

    # clean up
    text = string.lstrip(text)
    text = wkday_re.sub("", text, 1)  # Useless weekday

    # tz is time zone specifier string
    day, mon, yr, hr, min, sec, tz = [None]*7

    # loose regexp parse
    m = loose_http_re.search(text)
    if m is not None:
        day, mon, yr, hr, min, sec, tz = m.groups()
    else:
        return None  # bad format

    return _str2time(day, mon, yr, hr, min, sec, tz)


iso_re = re.compile(
    """^
    (\d{4})              # year
       [-\/]?
    (\d\d?)              # numerical month
       [-\/]?
    (\d\d?)              # day
   (?:
         (?:\s+|[-:Tt])  # separator before clock
      (\d\d?):?(\d\d)    # hour:min
      (?::?(\d\d(?:\.\d*)?))?  # optional seconds (and fractional)
   )?                    # optional clock
      \s*
   ([-+]?\d\d?:?(:?\d\d)?
    |Z|z)?               # timezone  (Z is "zero meridian", i.e. GMT)
      \s*$""", re.X)
def iso2time(text):
    """
    As for httpstr2time, but parses the ISO 8601 formats:

    1994-02-03 14:15:29 -0100    -- ISO 8601 format
    1994-02-03 14:15:29          -- zone is optional
    1994-02-03                   -- only date
    1994-02-03T14:15:29          -- Use T as separator
    19940203T141529Z             -- ISO 8601 compact format
    19940203                     -- only date

    """
    # clean up
    text = string.lstrip(text)

    # tz is time zone specifier string
    day, mon, yr, hr, min, sec, tz = [None]*7

    # loose regexp parse
    m = iso_re.search(text)
    if m is not None:
        # XXX there's an extra bit of the timezone I'm ignoring here: is
        #   this the right thing to do?
        yr, mon, day, hr, min, sec, tz, _ = m.groups()
    else:
        return None  # bad format

    return _str2time(day, mon, yr, hr, min, sec, tz)



# XXX Andrew Dalke kindly sent me a similar class in response to my request on
# comp.lang.python, which I then proceeded to lose.  I wrote this class
# instead, but I think he's released his code publicly since, could pinch the
# tests from it, at least...
class seek_wrapper:
    """Adds a seek method to a file object.

    This is only designed for seeking on readonly file-like objects.

    Wrapped file-like object must have a read method.  The readline method is
    only supported if that method is present on the wrapped object.  The
    readlines method is always supported.  xreadlines and iteration are
    supported only for Python 2.2 and above.

    Public attribute: wrapped (the wrapped file object).

    WARNING: All other attributes of the wrapped object (ie. those that are not
    one of wrapped, read, readline, readlines, xreadlines, __iter__ and next)
    are passed through unaltered, which may or may not make sense for your
    particular file object.

    """
    # General strategy is to check that cache is full enough, then delegate
    # everything to the cache (self._cache, which is a StringIO.StringIO
    # instance.  Seems to be some cStringIO.StringIO problem on 1.5.2 -- I
    # get a StringOobject, with no readlines method.

    # Invariant: the end of the cache is always at the same place as the
    # end of the wrapped file:
    # self.wrapped.tell() == len(self._cache.getvalue())

    def __init__(self, wrapped):
        self.wrapped = wrapped
        self.__have_readline = hasattr(self.wrapped, "readline")
        self.__cache = StringIO()

    def __getattr__(self, name): return getattr(self.wrapped, name)

    def seek(self, offset, whence=0):
        # make sure we have read all data up to the point we are seeking to
        pos = self.__cache.tell()
        if whence == 0:  # absolute
            to_read = offset - pos
        elif whence == 1:  # relative to current position
            to_read = offset
        elif whence == 2:  # relative to end of *wrapped* file
            # since we don't know yet where the end of that file is, we must
            # read everything
            to_read = None
        if to_read >= 0 or to_read is None:
            if to_read is None:
                self.__cache.write(self.wrapped.read())
            else:
                self.__cache.write(self.wrapped.read(to_read))
            self.__cache.seek(pos)

        return self.__cache.seek(offset, whence)

    def read(self, size=-1):
        pos = self.__cache.tell()

        self.__cache.seek(pos)

        end = len(self.__cache.getvalue())
        available = end - pos

        # enough data already cached?
        if size <= available and size != -1:
            return self.__cache.read(size)

        # no, so read sufficient data from wrapped file and cache it
        to_read = size - available
        assert to_read > 0 or size == -1
        self.__cache.seek(0, 2)
        if size == -1:
            self.__cache.write(self.wrapped.read())
        else:
            self.__cache.write(self.wrapped.read(to_read))
        self.__cache.seek(pos)

        return self.__cache.read(size)

    def readline(self, size=-1):
        if not self.__have_readline:
            raise NotImplementedError("no readline method on wrapped object")

        # line we're about to read might not be complete in the cache, so
        # read another line first
        pos = self.__cache.tell()
        self.__cache.seek(0, 2)
        self.__cache.write(self.wrapped.readline())
        self.__cache.seek(pos)

        data = self.__cache.readline()
        if size != -1:
            r = data[:size]
            self.__cache.seek(pos+size)
        else:
            r = data
        return r

    def readlines(self, sizehint=-1):
        pos = self.__cache.tell()
        self.__cache.seek(0, 2)
        self.__cache.write(self.wrapped.read())
        self.__cache.seek(pos)
        try:
            return self.__cache.readlines(sizehint)
        except TypeError:  # 1.5.2 hack
            return self.__cache.readlines()

    def __iter__(self): return self
    def next(self):
        line = self.readline()
        if line == "": raise StopIteration
        return line

    xreadlines = __iter__

    def __repr__(self):
        return ("<%s at %s whose wrapped object = %s>" %
                (self.__class__.__name__, `id(self)`, `self.wrapped`))

    def close(self):
        self.read = None
        self.readline = None
        self.readlines = None
        self.seek = None
        if self.wrapped: self.wrapped.close()
        self.wrapped = None
