"""HTML form handling for web clients.

ClientForm is a Python module for handling HTML forms on the client
side, useful for parsing HTML forms, filling them in and returning the
completed forms to the server.  It has developed from a port of Gisle
Aas' Perl module HTML::Form, from the libwww-perl library, but the
interface is not the same.

The most useful docstring is the one for HTMLForm.

RFC 1866: HTML 2.0
RFC 1867: Form-based File Upload in HTML
RFC 2388: Returning Values from Forms: multipart/form-data
HTML 3.2 Specification, W3C Recommendation 14 January 1997 (for ISINDEX)
HTML 4.01 Specification, W3C Recommendation 24 December 1999


Copyright 2002-2003 John J. Lee <jjl@pobox.com>
Copyright 1998-2000 Gisle Aas.

This code is free software; you can redistribute it and/or modify it
under the terms of the BSD License (see the file COPYING included with
the distribution).

"""

# XXX
# Treat unknown controls as text controls? (this was a recent LWP
#  HTML::Form change)  I guess this is INPUT with no TYPE?  Check LWP
#  source and browser behaviour.
# Support for list item ids.  How to handle missing ids? (How do I deal
#  with duplicate OPTION labels ATM?  Can't remember...)
# Arrange things so can automatically PyPI-register with categories
#  without messing up 1.5.2 compatibility.
# Tests need work.
# Test single and multiple file upload some more on the web.
# Does file upload work when name is missing?  Sourceforge tracker form
#  doesn't like it.  Check standards, and test with Apache.  Test binary
#  upload with Apache.
# Add label support for CHECKBOX and RADIO.
# Better docs.
# Deal with character sets properly.  Not sure what the issues are here.
#  I don't *think* any encoding of control names, filenames or data is
#   necessary -- HTML spec. doesn't require it, and Mozilla Firebird 0.6
#   doesn't seem to do it.
#  Add charset parameter to Content-type headers?  How to find value??
# Get rid of MapBase, AList and MimeWriter.
# I'm not going to fix this unless somebody tells me what real servers
#  that want this encoding actually expect: If enctype is
#  application/x-www-form-urlencoded and there's a FILE control present.
#  Strictly, it should be 'name=data' (see HTML 4.01 spec., section
#  17.13.2), but I send "name=" ATM.  What about multiple file upload??
# Get rid of the two type-switches (for kind and click*).
# Remove single-selection code: can be special case of multi-selection,
#  with a few variations, I think.
# Factor out multiple-selection list code?  May not be easy.  Maybe like
#  this:

#         ListControl
#             ^
#             |       MultipleListControlMixin
#             |             ^
#        SelectControl     /
#             ^           /
#              \         /
#          MultiSelectControl


# Plan
# ----
# Maybe a 0.2.x, cleaned up a bit and with id support for list items?
#  Not sure it's worth it, really.
#   Remove toggle methods.
#   Replace by_label with choice between value / id / label /
#    element contents (see discussion with Gisle about labels on
#    libwww-perl list).
#   ...what else?
# Work on DOMForm.
# XForms?  Don't know if there's a need here.


try: True
except NameError:
    True = 1
    False = 0

try: bool
except NameError:
    def bool(expr):
        if expr: return True
        else: return False

import sys, urllib, urllib2, types, string, mimetools, copy
from urlparse import urljoin
from cStringIO import StringIO
try:
    import UnicodeType
except ImportError:
    UNICODE = False
else:
    UNICODE = True

VERSION = "0.1.13"

CHUNK = 1024  # size of chunks fed to parser, in bytes

# This version of urlencode is from my Python 1.5.2 back-port of the
# Python 2.1 CVS maintenance branch of urllib.  It will accept a sequence
# of pairs instead of a mapping -- the 2.0 version only accepts a mapping.
def urlencode(query,doseq=False,):
    """Encode a sequence of two-element tuples or dictionary into a URL query \
string.

    If any values in the query arg are sequences and doseq is true, each
    sequence element is converted to a separate parameter.

    If the query arg is a sequence of two-element tuples, the order of the
    parameters in the output will match the order of parameters in the
    input.
    """

    if hasattr(query,"items"):
        # mapping objects
        query = query.items()
    else:
        # it's a bother at times that strings and string-like objects are
        # sequences...
        try:
            # non-sequence items should not work with len()
            x = len(query)
            # non-empty strings will fail this
            if len(query) and type(query[0]) != types.TupleType:
                raise TypeError()
            # zero-length sequences of all types will get here and succeed,
            # but that's a minor nit - since the original implementation
            # allowed empty dicts that type of behavior probably should be
            # preserved for consistency
        except TypeError:
            ty,va,tb = sys.exc_info()
            raise TypeError("not a valid non-string sequence or mapping "
                            "object", tb)

    l = []
    if not doseq:
        # preserve old behavior
        for k, v in query:
            k = urllib.quote_plus(str(k))
            v = urllib.quote_plus(str(v))
            l.append(k + '=' + v)
    else:
        for k, v in query:
            k = urllib.quote_plus(str(k))
            if type(v) == types.StringType:
                v = urllib.quote_plus(v)
                l.append(k + '=' + v)
            elif UNICODE and type(v) == types.UnicodeType:
                # is there a reasonable way to convert to ASCII?
                # encode generates a string, but "replace" or "ignore"
                # lose information and "strict" can raise UnicodeError
                v = urllib.quote_plus(v.encode("ASCII","replace"))
                l.append(k + '=' + v)
            else:
                try:
                    # is this a sufficient test for sequence-ness?
                    x = len(v)
                except TypeError:
                    # not a sequence
                    v = urllib.quote_plus(str(v))
                    l.append(k + '=' + v)
                else:
                    # loop over the sequence
                    for elt in v:
                        l.append(k + '=' + urllib.quote_plus(str(elt)))
    return string.join(l, '&')

def startswith(string, initial):
    if len(initial) > len(string): return False
    return string[:len(initial)] == initial

def issequence(x):
    try:
        x[0]
    except (TypeError, KeyError):
        return False
    except IndexError:
        pass
    return True

def isstringlike(x):
    try: x+""
    except: return False
    else: return True


# XXX don't really want to drag this along (MapBase, AList, MimeWriter)

class MapBase:
    """Mapping designed to be easily derived from.

    Subclass it and override __init__, __setitem__, __getitem__, __delitem__
    and keys.  Nothing else should need to be overridden, unlike UserDict.
    This significantly simplifies dictionary-like classes.

    Also different from UserDict in that it has a redonly flag, and can be
    updated (and initialised) with a sequence of pairs (key, value).

    """
    def __init__(self, init=None):
        self._data = {}
        self.readonly = False
        if init is not None: self.update(init)

    def __getitem__(self, key):
        return self._data[key]

    def __setitem__(self, key, item):
        if not self.readonly:
            self._data[key] = item
        else:
            raise TypeError("object doesn't support item assignment")

    def __delitem__(self, key):
        if not self.readonly:
            del self._data[key]
        else:
            raise TypeError("object doesn't support item deletion")

    def keys(self):
        return self._data.keys()

    # now the internal workings, there should be no need to override these:

    def clear(self):
        for k in self.keys():
            del self[k]

    def __repr__(self):
        rep = []
        for k, v in self.items():
            rep.append("%s: %s" % (repr(k), repr(v)))
        return self.__class__.__name__+"{"+(string.join(rep, ", "))+"}"

    def copy(self):
        return copy.copy(self)

    def __cmp__(self, dict):
        # note: return value is *not* boolean
        for k, v in self.items():
            if not (dict.has_key(k) and dict[k] == v):
                return 1  # different
        return 0  # the same

    def __len__(self):
        return len(self.keys())

    def values(self):
        r = []
        for k in self.keys():
            r.append(self[k])
        return r

    def items(self):
        keys = self.keys()
        vals = self.values()
        r = []
        for i in len(self):
            r.append((keys[i], vals[i]))
        return r

    def has_key(self, key):
        return key in self.keys()

    def update(self, map):
        if issequence(map) and not isstringlike(map):
            items = map
        else:
            items = map.items()
        for tup in items:
            if not isinstance(tup, TupleType):
                raise TypeError(
                    "MapBase.update requires a map or a sequence of pairs")
            k, v = tup
            self[k] = v

    def get(self, key, failobj=None):
        if key in self.keys():
            return self[key]
        else:
            return failobj

    def setdefault(self, key, failobj=None):
        if not self.has_key(key):
            self[key] = failobj
        return self[key]


class AList(MapBase):
    """Read-only ordered mapping."""
    def __init__(self, seq=[]):
        self.readonly = True
        self._inverted = False
        self._data = list(seq[:])
        self._keys = []
        self._values = []
        for key, value in seq:
            self._keys.append(key)
            self._values.append(value)

    def set_inverted(self, inverted):
        if (inverted and not self._inverted) or (
            not inverted and self._inverted):
            self._keys, self._values = self._values, self._keys
        if inverted: self._inverted = True
        else: self._inverted = False

    def __getitem__(self, key):
        try:
            i = self._keys.index(key)
        except ValueError:
            raise KeyError(key)
        return self._values[i]

    def __delitem__(self, key):
        try:
            i = self._keys.index[key]
        except ValueError:
            raise KeyError(key)
        del self._values[i]

    def keys(self): return list(self._keys[:])
    def values(self): return list(self._values[:])
    def items(self):
        data = self._data[:]
        if not self._inverted:
            return data
        else:
            newdata = []
            for k, v in data:
                newdata.append((v, k))
            return newdata


# This cut-n-pasted MimeWriter from standard library is here so can add
# to HTTP headers rather than message body when appropriate.  It also uses
# \r\n in place of \n.  This is nasty.
class MimeWriter:

    """Generic MIME writer.

    Methods:

    __init__()
    addheader()
    flushheaders()
    startbody()
    startmultipartbody()
    nextpart()
    lastpart()

    A MIME writer is much more primitive than a MIME parser.  It
    doesn't seek around on the output file, and it doesn't use large
    amounts of buffer space, so you have to write the parts in the
    order they should occur on the output file.  It does buffer the
    headers you add, allowing you to rearrange their order.

    General usage is:

    f = <open the output file>
    w = MimeWriter(f)
    ...call w.addheader(key, value) 0 or more times...

    followed by either:

    f = w.startbody(content_type)
    ...call f.write(data) for body data...

    or:

    w.startmultipartbody(subtype)
    for each part:
        subwriter = w.nextpart()
        ...use the subwriter's methods to create the subpart...
    w.lastpart()

    The subwriter is another MimeWriter instance, and should be
    treated in the same way as the toplevel MimeWriter.  This way,
    writing recursive body parts is easy.

    Warning: don't forget to call lastpart()!

    XXX There should be more state so calls made in the wrong order
    are detected.

    Some special cases:

    - startbody() just returns the file passed to the constructor;
      but don't use this knowledge, as it may be changed.

    - startmultipartbody() actually returns a file as well;
      this can be used to write the initial 'if you can read this your
      mailer is not MIME-aware' message.

    - If you call flushheaders(), the headers accumulated so far are
      written out (and forgotten); this is useful if you don't need a
      body part at all, e.g. for a subpart of type message/rfc822
      that's (mis)used to store some header-like information.

    - Passing a keyword argument 'prefix=<flag>' to addheader(),
      start*body() affects where the header is inserted; 0 means
      append at the end, 1 means insert at the start; default is
      append for addheader(), but insert for start*body(), which use
      it to determine where the Content-type header goes.

    """

    def __init__(self, fp, http_hdrs=None):
        self._http_hdrs = http_hdrs
        self._fp = fp
        self._headers = []
        self._boundary = []
        self._first_part = True

    def addheader(self, key, value, prefix=0,
                  add_to_http_hdrs=0):
        """
        prefix is ignored if add_to_http_hdrs is true.
        """
        lines = string.split(value, "\r\n")
        while lines and not lines[-1]: del lines[-1]
        while lines and not lines[0]: del lines[0]
        if add_to_http_hdrs:
            value = string.join(lines, "")
            self._http_hdrs.append((key, value))
        else:
            for i in range(1, len(lines)):
                lines[i] = "    " + string.strip(lines[i])
            value = string.join(lines, "\r\n") + "\r\n"
            line = key + ": " + value
            if prefix:
                self._headers.insert(0, line)
            else:
                self._headers.append(line)

    def flushheaders(self):
        self._fp.writelines(self._headers)
        self._headers = []

    def startbody(self, ctype=None, plist=[], prefix=1,
                  add_to_http_hdrs=0, content_type=1):
        """
        prefix is ignored if add_to_http_hdrs is true.
        """
        if content_type and ctype:
            for name, value in plist:
                ctype = ctype + ';\r\n %s=\"%s\"' % (name, value)
            self.addheader("Content-type", ctype, prefix=prefix,
                           add_to_http_hdrs=add_to_http_hdrs)
        self.flushheaders()
        if not add_to_http_hdrs: self._fp.write("\r\n")
        self._first_part = True
        return self._fp

    def startmultipartbody(self, subtype, boundary=None, plist=[], prefix=1,
                           add_to_http_hdrs=0, content_type=1):
        boundary = boundary or mimetools.choose_boundary()
        self._boundary.append(boundary)
        return self.startbody("multipart/" + subtype,
                              [("boundary", boundary)] + plist,
                              prefix=prefix,
                              add_to_http_hdrs=add_to_http_hdrs,
                              content_type=content_type)

    def nextpart(self):
        boundary = self._boundary[-1]
        if self._first_part:
            self._first_part = False
        else:
            self._fp.write("\r\n")
        self._fp.write("--" + boundary + "\r\n")
        return self.__class__(self._fp)

    def lastpart(self):
        if self._first_part:
            self.nextpart()
        boundary = self._boundary.pop()
        self._fp.write("\r\n--" + boundary + "--\r\n")


class ControlNotFoundError(ValueError): pass
class ItemNotFoundError(ValueError): pass
class ItemCountError(ValueError): pass

class ParseError(Exception): pass


def ParseResponse(response, select_default=False, ignore_errors=False):
    """Parse HTTP response and return a list of HTMLForm instances.

    The return value of urllib2.urlopen can be conveniently passed to this
    function as the response parameter.

    ClientForm.ParseError is raised on parse errors.

    response: file-like object (supporting read() method) with a method
     geturl(), returning the base URI of the HTTP response
    select_default: for multiple-selection SELECT controls and RADIO controls,
     pick the first item as the default if none are selected in the HTML
    ignore_errors: don't raise ParseError, and carry on regardless if the
     parser gets confused

    Pass a true value for select_default if you want the behaviour specified by
    RFC 1866 (the HTML 2.0 standard), which is to select the first item in a
    RADIO or multiple-selection SELECT control if none were selected in the
    HTML.  Most browsers (including Microsoft Internet Explorer (IE) and
    Netscape Navigator) instead leave all items unselected in these cases.  The
    W3C HTML 4.0 standard leaves this behaviour undefined in the case of
    multiple-selection SELECT controls, but insists that at least one RADIO
    button should be checked at all times, in contradiction to browser
    behaviour.

    Precisely what ignore_errors does isn't well-defined yet, so don't rely too
    much on the current behaviour -- if you want robustness, you're better off
    fixing the HTML before passing it to this function.

    """
    return ParseFile(response, response.geturl(), select_default)

def ParseFile(file, base_uri, select_default=False, ignore_errors=False):
    """Parse HTML and return a list of HTMLForm instances.

    ClientForm.ParseError is raised on parse errors.

    file: file-like object (supporting read() method) containing HTML with zero
     or more forms to be parsed
    base_uri: the base URI of the document

    For the other arguments and further details, see ParseResponse.__doc__.

    """
    fp = _FORM_PARSER_CLASS(ignore_errors)
    while 1:
        data = file.read(CHUNK)
        fp.feed(data)
        if len(data) != CHUNK: break
    forms = []
    for (name, action, method, enctype), attrs, controls in fp.forms:
        if action is None:
            action = base_uri
        else:
            action = urljoin(base_uri, action)
        form = HTMLForm(action, method, enctype, name, attrs)
        for type, name, attr in controls:
            form.new_control(type, name, attr, select_default=select_default)
        forms.append(form)
    for form in forms:
        form.fixup()
    return forms


class _AbstractFormParser:
    """forms attribute contains HTMLForm instances on completion."""
    # pinched (and modified) from Moshe Zadka
    def __init__(self, ignore_errors, entitydefs=None):
        if entitydefs is not None:
            self.entitydefs = entitydefs
        self._ignore_errors = ignore_errors
        self.forms = []
        self._current_form = None
        self._select = None
        self._optgroup = None
        self._option = None
        self._textarea = None

    def error(self, error):
        if not self._ignore_errors: raise error

    def start_form(self, attrs):
        if self._current_form is not None:
            self.error(ParseError("nested FORMs"))
        name = None
        action = None
        enctype = "application/x-www-form-urlencoded"
        method = "GET"
        d = {}
        for key, value in attrs:
            if key == "name":
                name = value
            elif key == "action":
                action = value
            elif key == "method":
                method = string.upper(value)
            elif key == "enctype":
                enctype = string.lower(value)
            else:
                d[key] = value
        controls = []
        self._current_form = (name, action, method, enctype), d, controls

    def end_form(self):
        if self._current_form is None:
            self.error(ParseError("end of FORM before start"))
        self.forms.append(self._current_form)
        self._current_form = None

    def start_select(self, attrs):
        if self._current_form is None:
            self.error(ParseError("start of SELECT before start of FORM"))
        if self._select is not None:
            self.error(ParseError("nested SELECTs"))
        if self._textarea is not None:
            self.error(ParseError("SELECT inside TEXTAREA"))
        d = {}
        for key, val in attrs:
            d[key] = val

        self._select = d

        self._append_select_control({"__select": d})

    def end_select(self):
        if self._current_form is None:
            self.error(ParseError("end of SELECT before start of FORM"))
        if self._select is None:
            self.error(ParseError("end of SELECT before start"))

        if self._option is not None:
            self._end_option()

        self._select = None

    def start_optgroup(self, attrs):
        if self._select is None:
            self.error(ParseError("OPTGROUP outside of SELECT"))
        d = {}
        for key, val in attrs:
            d[key] = val

        self._optgroup = d

    def end_optgroup(self):
        if self._optgroup is None:
            self.error(ParseError("end of OPTGROUP before start"))
        self._optgroup = None

    def _start_option(self, attrs):
        if self._select is None:
            self.error(ParseError("OPTION outside of SELECT"))
        if self._option is not None:
            self._end_option()

        d = {}
        for key, val in attrs:
            d[key] = val

        self._option = {}
        self._option.update(d)
        if (self._optgroup and self._optgroup.has_key("disabled") and
            not self._option.has_key("disabled")):
            self._option["disabled"] = None

    def _end_option(self):
        if self._option is None:
            self.error(ParseError("end of OPTION before start"))

        contents = string.strip(self._option.get("contents", ""))
        #contents = string.strip(self._option["contents"])
        self._option["contents"] = contents
        if not self._option.has_key("value"):
            self._option["value"] = contents
        if not self._option.has_key("label"):
            self._option["label"] = contents
        # stuff dict of SELECT HTML attrs into a special private key
        #  (gets deleted again later)
        self._option["__select"] = self._select
        self._append_select_control(self._option)
        self._option = None

    def _append_select_control(self, attrs):
        controls = self._current_form[2]
        name = self._select.get("name")
        controls.append(("select", name, attrs))

##     def do_option(self, attrs):
##         if self._select is None:
##             self.error(ParseError("OPTION outside of SELECT"))
##         d = {}
##         for key, val in attrs:
##             d[key] = val

##         self._option = {}
##         self._option.update(d)
##         if (self._optgroup and self._optgroup.has_key("disabled") and
##             not self._option.has_key("disabled")):
##             self._option["disabled"] = None

    def start_textarea(self, attrs):
        if self._current_form is None:
            self.error(ParseError("start of TEXTAREA before start of FORM"))
        if self._textarea is not None:
            self.error(ParseError("nested TEXTAREAs"))
        if self._select is not None:
            self.error(ParseError("TEXTAREA inside SELECT"))
        d = {}
        for key, val in attrs:
            d[key] = val

        self._textarea = d

    def end_textarea(self):
        if self._current_form is None:
            self.error(ParseError("end of TEXTAREA before start of FORM"))
        if self._textarea is None:
            self.error(ParseError("end of TEXTAREA before start"))
        controls = self._current_form[2]
        name = self._textarea.get("name")
        controls.append(("textarea", name, self._textarea))
        self._textarea = None

    def handle_data(self, data):
        if self._option is not None:
            # self._option is a dictionary of the OPTION element's HTML
            # attributes, but it has two special keys, one of which is the
            # special "contents" key contains text between OPTION tags (the
            # other is the "__select" key: see the end_option method)
            map = self._option
            key = "contents"
        elif self._textarea is not None:
            map = self._textarea
            key = "value"
        else:
            return

        if not map.has_key(key):
            map[key] = data
        else:
            map[key] = map[key] + data

##     def handle_data(self, data):
##         if self._option is not None:
##             contents = string.strip(data)
##             controls = self._current_form[2]
##             if not self._option.has_key("value"):
##                 self._option["value"] = contents
##             if not self._option.has_key("label"):
##                 self._option["label"] = contents
##             # self._option is a dictionary of the OPTION element's HTML
##             # attributes, but it has two special keys:
##             #  1. special "contents" key contains text between OPTION tags
##             self._option["contents"] = contents
##             #  2. stuff dict of SELECT HTML attrs into a special private key
##             #     (gets deleted again later)
##             self._option["__select"] = self._select
##             self._append_select_control(self._option)
##             self._option = None
##         elif self._textarea is not None:
##             #self._textarea["value"] = data
##             if self._textarea.get("value") is None:
##                 self._textarea["value"] = data
##             else:
##                 self._textarea["value"] = self._textarea["value"] + data

    def do_button(self, attrs):
        if self._current_form is None:
            self.error(ParseError("start of BUTTON before start of FORM"))
        d = {}
        d["type"] = "submit"  # default
        for key, val in attrs:
            d[key] = val
        controls = self._current_form[2]

        type = d["type"]
        name = d.get("name")
        # we don't want to lose information, so use a type string that
        # doesn't clash with INPUT TYPE={SUBMIT,RESET,BUTTON}
        # eg. type for BUTTON/RESET is "resetbutton"
        #     (type for INPUT/RESET is "reset")
        type = type+"button"
        controls.append((type, name, d))

    def do_input(self, attrs):
        if self._current_form is None:
            self.error(ParseError("start of INPUT before start of FORM"))
        d = {}
        d["type"] = "text"  # default
        for key, val in attrs:
            d[key] = val
        controls = self._current_form[2]

        type = d["type"]
        name = d.get("name")
        controls.append((type, name, d))

    def do_isindex(self, attrs):
        if self._current_form is None:
            self.error(ParseError("start of ISINDEX before start of FORM"))
        d = {}
        for key, val in attrs:
            d[key] = val
        controls = self._current_form[2]

        # isindex doesn't have type or name HTML attributes
        controls.append(("isindex", None, d))

# use HTMLParser if we have it (it does XHTML), htmllib otherwise
try:
    import HTMLParser
except ImportError:
    import htmllib, formatter
    class _FormParser(_AbstractFormParser, htmllib.HTMLParser):
        # This is still here for compatibility with Python 1.5.2.
        # It doesn't do the right thing with XHTML.
        def __init__(self, ignore_errors, entitydefs=None):
            htmllib.HTMLParser.__init__(self, formatter.NullFormatter())
            _AbstractFormParser.__init__(self, ignore_errors, entitydefs)

        def do_option(self, attrs):
            _AbstractFormParser._start_option(self, attrs)

    _FORM_PARSER_CLASS = _FormParser
else:
    class _XHTMLCompatibleFormParser(_AbstractFormParser, HTMLParser.HTMLParser):
        # thanks to Michael Howitz for this!
        def __init__(self, ignore_errors, entitydefs=None):
            HTMLParser.HTMLParser.__init__(self)
            _AbstractFormParser.__init__(self, ignore_errors, entitydefs)

        def start_option(self, attrs):
            _AbstractFormParser._start_option(self, attrs)

        def end_option(self):
            _AbstractFormParser._end_option(self)

        def handle_starttag(self, tag, attrs):
            try:
                method = getattr(self, 'start_' + tag)
            except AttributeError:
                try:
                    method = getattr(self, 'do_' + tag)
                except AttributeError:
                    pass # unknown tag
                else:
                    method(attrs)
            else:
                method(attrs)

        def handle_endtag(self, tag):
            try:
                method = getattr(self, 'end_' + tag)
            except AttributeError:
                pass # unknown tag
            else:
                method()

        # handle_charref, handle_entityref and default entitydefs are taken
        # from sgmllib
        def handle_charref(self, name):
            try:
                n = int(name)
            except ValueError:
                self.unknown_charref(name)
                return
            if not 0 <= n <= 255:
                self.unknown_charref(name)
                return
            self.handle_data(chr(n))

        # Definition of entities -- derived classes may override
        entitydefs = \
                {'lt': '<', 'gt': '>', 'amp': '&', 'quot': '"', 'apos': '\''}

        def handle_entityref(self, name):
            table = self.entitydefs
            if name in table:
                self.handle_data(table[name])
            else:
                self.unknown_entityref(name)
                return

        # These methods would have passed through the ref intact if I'd thought
        # of it earlier, but since the old parser silently swallows unknown
        # refs, so does this new parser.
        def unknown_entityref(self, ref): pass
        def unknown_charref(self, ref): pass

    _FORM_PARSER_CLASS = _XHTMLCompatibleFormParser


class Control:
    """An HTML form control.

    An HTMLForm contains a sequence of Controls.  HTMLForm delegates lots of
    things to Control objects, and most of Control's methods are, in effect,
    documented by the HTMLForm docstrings.

    The Controls in an HTMLForm can be got at via the HTMLForm.find_control
    method or the HTMLForm.controls attribute.

    Control instances are usually constructed using the ParseFile /
    ParseResponse functions, so you can probably ignore the rest of this
    paragraph.  A Control is only properly initialised after the fixup method
    has been called.  In fact, this is only strictly necessary for ListControl
    instances.  This is necessary because ListControls are built up from
    ListControls each containing only a single item, and their initial value(s)
    can only be known after the sequence is complete.

    The types and values that are acceptable for assignment to the value
    attribute are defined by subclasses.

    If the disabled attribute is true, this represents the state typically
    represented by browsers by `greying out' a control.  If the disabled
    attribute is true, the Control will raise AttributeError if an attempt is
    made to change its value.  In addition, the control will not be considered
    `successful' as defined by the W3C HTML 4 standard -- ie. it will
    contribute no data to the return value of the HTMLForm.click* methods.  To
    enable a control, set the disabled attribute to a false value.

    If the readonly attribute is true, the Control will raise AttributeError if
    an attempt is made to change its value.  To make a control writable, set
    the readonly attribute to a false value.

    All controls have the disabled and readonly attributes, not only those that
    may have the HTML attributes of the same names.

    On assignment to the value attribute, the following exceptions are raised:
    TypeError, AttributeError (if the value attribute should not be assigned
    to, because the control is disabled, for example) and ValueError.

    If the name or value attributes are None, or the value is an empty list, or
    if the control is disabled, the control is not successful.

    Public attributes:

    type: string describing type of control (see the keys of the
     HTMLForm.type2class dictionary for the allowable values) (readonly)
    name: name of control (readonly)
    value: current value of control (subclasses may allow a single value, a
     sequence of values, or either)
    disabled: disabled state
    readonly: readonly state
    id: value of id HTML attribute

    """
    def __init__(self, type, name, attrs):
        """
        type: string describing type of control (see the keys of the
         HTMLForm.type2class dictionary for the allowable values)
        name: control name
        attrs: HTML attributes of control's HTML element

        """
        raise NotImplementedError()

    def add_to_form(self, form):
        form.controls.append(self)

    def fixup(self):
        pass

    def __getattr__(self, name): raise NotImplementedError()
    def __setattr__(self, name, value): raise NotImplementedError()

    def pairs(self):
        """Return list of (key, value) pairs suitable for passing to urlencode.
        """
        raise NotImplementedError()

    def _write_mime_data(self, mw):
        """Write data for this control to a MimeWriter."""
        # called by HTMLForm
        for name, value in self.pairs():
            mw2 = mw.nextpart()
            mw2.addheader("Content-disposition",
                          'form-data; name="%s"' % name, 1)
            f = mw2.startbody(prefix=0)
            f.write(value)

    def __str__(self):
        raise NotImplementedError()


#---------------------------------------------------
class ScalarControl(Control):
    """Control whose value is not restricted to one of a prescribed set.

    Some ScalarControls don't accept any value attribute.  Otherwise, takes a
    single value, which must be string-like.

    Additional read-only public attribute:

    attrs: dictionary mapping the names of original HTML attributes of the
     control to their values

    """
    def __init__(self, type, name, attrs):
        self.__dict__["type"] = string.lower(type)
        self.__dict__["name"] = name
        self._value = attrs.get("value")
        self.disabled = attrs.has_key("disabled")
        self.readonly = attrs.has_key("readonly")
        self.id = attrs.get("id")

        self.attrs = attrs.copy()

        self._clicked = False

    def __getattr__(self, name):
        if name == "value":
            return self.__dict__["_value"]
        else:
            raise AttributeError("%s instance has no attribute '%s'" %
                                 (self.__class__.__name__, name))

    def __setattr__(self, name, value):
        if name == "value":
            if not isstringlike(value):
                raise TypeError("must assign a string")
            elif self.readonly:
                raise AttributeError("control '%s' is readonly" % self.name)
            elif self.disabled:
                raise AttributeError("control '%s' is disabled" % self.name)
            self.__dict__["_value"] = value
        elif name in ("name", "type"):
            raise AttributeError("%s attribute is readonly" % name)
        else:
            self.__dict__[name] = value

    def pairs(self):
        name = self.name
        value = self.value
        if name is None or value is None or self.disabled:
            return []
        return [(name, value)]

    def __str__(self):
        name = self.name
        value = self.value
        if name is None: name = "<None>"
        if value is None: value = "<None>"

        infos = []
        if self.disabled: infos.append("disabled")
        if self.readonly: infos.append("readonly")
        info = string.join(infos, ", ")
        if info: info = " (%s)" % info

        return "<%s(%s=%s)%s>" % (self.__class__.__name__, name, value, info)


#---------------------------------------------------
class TextControl(ScalarControl):
    """Textual input control.

    Covers:

    INPUT/TEXT
    INPUT/PASSWORD
    INPUT/FILE
    INPUT/HIDDEN
    TEXTAREA

    """
    def __init__(self, type, name, attrs):
        ScalarControl.__init__(self, type, name, attrs)
        if self.type == "hidden": self.readonly = True
        if self._value is None:
            self._value = ""


#---------------------------------------------------
class FileControl(ScalarControl):
    """File upload with INPUT TYPE=FILE.

    The value attribute of a FileControl is always None.

    Additional public method: add_file

    """
    def __init__(self, type, name, attrs):
        ScalarControl.__init__(self, type, name, attrs)
        self._value = None
        self._upload_data = []

    def __setattr__(self, name, value):
        if name in ("value", "name", "type"):
            raise AttributeError("%s attribute is readonly" % name)
        else:
            self.__dict__[name] = value

    def add_file(self, file_object, content_type=None, filename=None):
        if not hasattr(file_object, "read"):
            raise TypeError("file-like object must have read method")
        if content_type is not None and not isstringlike(content_type):
            raise TypeError("content type must be None or string-like")
        if filename is not None and not isstringlike(filename):
            raise TypeError("filename must be None or string-like")
        if content_type is None:
            content_type = "application/octet-stream"
        self._upload_data.append((file_object, content_type, filename))

    def pairs(self):
        # XXX should it be successful even if unnamed?
        if self.name is None or self.disabled:
            return []
        return [(self.name, "")]

    def _write_mime_data(self, mw):
        # called by HTMLForm
        if len(self._upload_data) == 1:
            # single file
            file_object, content_type, filename = self._upload_data[0]
            mw2 = mw.nextpart()
            fn_part = filename and ('; filename="%s"' % filename) or ''
            disp = 'form-data; name="%s"%s' % (self.name, fn_part)
            mw2.addheader("Content-disposition", disp, prefix=1)
            fh = mw2.startbody(content_type, prefix=0)
            fh.write(file_object.read())
        elif len(self._upload_data) != 0:
            # multiple files
            mw2 = mw.nextpart()
            disp = 'form-data; name="%s"' % self.name
            mw2.addheader("Content-disposition", disp, prefix=1)
            fh = mw2.startmultipartbody("mixed", prefix=0)
            for file_object, content_type, filename in self._upload_data:
                mw3 = mw2.nextpart()
                fn_part = filename and ('; filename="%s"' % filename) or ''
                disp = 'file%s' % fn_part
                mw3.addheader("Content-disposition", disp, prefix=1)
                fh2 = mw3.startbody(content_type, prefix=0)
                fh2.write(file_object.read())
            mw2.lastpart()

    def __str__(self):
        name = self.name
        if name is None: name = "<None>"

        if not self._upload_data:
            value = "<No files added>"
        else:
            value = []
            for file, ctype, filename in self._upload_data:
                if filename is None:
                    value.append("<Unnamed file>")
                else:
                    value.append(filename)
            value = string.join(value, ", ")

        info = []
        if self.disabled: info.append("disabled")
        if self.readonly: info.append("readonly")
        info = string.join(info, ", ")
        if info: info = " (%s)" % info

        return "<%s(%s=%s)%s>" % (self.__class__.__name__, name, value, info)


#---------------------------------------------------
class IsindexControl(ScalarControl):
    """ISINDEX control.

    ISINDEX is the odd-one-out of HTML form controls.  In fact, it isn't really
    part of regular HTML forms at all, and predates it.  You're only allowed
    one ISINDEX per HTML document.  ISINDEX and regular form submission are
    mutually exclusive -- either submit a form, or the ISINDEX.

    Having said this, since ISINDEX controls may appear in forms (which is
    probably bad HTML), ParseFile / ParseResponse will include them in the
    HTMLForm instances it returns.  You can set the ISINDEX's value, as with
    any other control (but note that ISINDEX controls have no name, so you'll
    need to use the type argument of set_value!).  When you submit the form,
    the ISINDEX will not be successful (ie., no data will get returned to the
    server as a result of its presence), unless you click on the ISINDEX
    control, in which case the ISINDEX gets submitted instead of the form:

    form.set_value("my isindex value", type="isindex")
    urllib2.urlopen(form.click(type="isindex"))

    ISINDEX elements outside of FORMs are ignored.  If you want to submit one
    by hand, do it like so:

    url = urlparse.urljoin(page_uri, "?"+urllib.quote_plus("my isindex value"))
    result = urllib2.urlopen(url)

    """
    def __init__(self, type, name, attrs):
        ScalarControl.__init__(self, type, name, attrs)
        if self._value is None:
            self._value = ""

    def pairs(self):
        return []

    def _click(self, form, coord, return_type):
        # Relative URL for ISINDEX submission: instead of "foo=bar+baz",
        # want "bar+baz".
        # This doesn't seem to be specified in HTML 4.01 spec. (ISINDEX is
        # deprecated in 4.01, but it should still say how to submit it).
        # Submission of ISINDEX is explained in the HTML 3.2 spec, though.
        url = urljoin(form.action, "?"+urllib.quote_plus(self.value))
        req_data = url, None, []

        if return_type == "pairs":
            return []
        elif return_type == "request_data":
            return req_data
        else:
            return urllib2.Request(url)

    def __str__(self):
        value = self.value
        if value is None: value = "<None>"

        infos = []
        if self.disabled: infos.append("disabled")
        if self.readonly: infos.append("readonly")
        info = string.join(infos, ", ")
        if info: info = " (%s)" % info

        return "<%s(%s)%s>" % (self.__class__.__name__, value, info)


#---------------------------------------------------
class IgnoreControl(ScalarControl):
    """Control that we're not interested in.

    Covers:

    INPUT/RESET
    BUTTON/RESET
    INPUT/BUTTON
    BUTTON/BUTTON

    These controls are always unsuccessful, in the terminology of HTML 4 (ie.
    they never require any information to be returned to the server).

    BUTTON/BUTTON is used to generate events for script embedded in HTML.

    The value attribute of IgnoreControl is always None.

    """
    def __init__(self, type, name, attrs):
        ScalarControl.__init__(self, type, name, attrs)
        self._value = None

    def __setattr__(self, name, value):
        if name == "value":
            raise AttributeError(
                "control '%s' is ignored, hence read-only" % self.name)
        elif name in ("name", "type"):
            raise AttributeError("%s attribute is readonly" % name)
        else:
            self.__dict__[name] = value


#---------------------------------------------------
class ListControl(Control):
    """Control representing a sequence of items.

    The value attribute of a ListControl represents the selected list items in
    the control.

    ListControl implements both list controls that take a single value and
    those that take multiple values.

    ListControls accept sequence values only.  Some controls only accept
    sequences of length 0 or 1 (RADIO, and single-selection SELECT).
    In those cases, ItemCountError is raised if len(sequence) > 1.  CHECKBOXes
    and multiple-selection SELECTs (those having the "multiple" HTML attribute)
    accept sequences of any length.

    Note the following mistake:

    control.value = some_value
    assert control.value == some_value    # not necessarily true

    The reason for this is that the value attribute always gives the list items
    in the order they were listed in the HTML.

    ListControl items can also be referred to by their labels instead of names.
    Use the by_label argument, and the set_value_by_label, get_value_by_label
    methods.

    XXX RadioControl and CheckboxControl don't implement by_label yet.

    Note that, rather confusingly, though SELECT controls are represented in
    HTML by SELECT elements (which contain OPTION elements, representing
    individual list items), CHECKBOXes and RADIOs are not represented by *any*
    element.  Instead, those controls are represented by a collection of INPUT
    elements.  For example, this is a SELECT control, named "control1":

    <select name="control1">
     <option>foo</option>
     <option value="1">bar</option>
    </select>

    and this is a CHECKBOX control, named "control2":

    <input type="checkbox" name="control2" value="foo" id="cbe1">
    <input type="checkbox" name="control2" value="bar" id="cbe2">

    The id attribute of a CHECKBOX or RADIO ListControl is always that of its
    first element (for example, "cbe1" above).


    Additional read-only public attribute: multiple.


    ListControls are built up by the parser from their component items by
    creating one ListControl per item, consolidating them into a single master
    ListControl held by the HTMLForm:

    -User calls form.new_control(...)
    -Form creates Control, and calls control.add_to_form(self).
    -Control looks for a Control with the same name and type in the form, and
    if it finds one, merges itself with that control by calling
    control.merge_control(self).  The first Control added to the form, of a
    particular name and type, is the only one that survives in the form.
    -Form calls control.fixup for all its controls.  ListControls in the form
    know they can now safely pick their default values.

    To create a ListControl without an HTMLForm, use:

    control.merge_control(new_control)

    """
    def __init__(self, type, name, attrs={}, select_default=False,
                 called_as_base_class=False):
        """
        select_default: for RADIO and multiple-selection SELECT controls, pick
         the first item as the default if no 'selected' HTML attribute is
         present

        """
        if not called_as_base_class:
            raise NotImplementedError()

        self.__dict__["type"] = string.lower(type)
        self.__dict__["name"] = name
        self._value = attrs.get("value")
        self.disabled = False
        self.readonly = False
        self.id = attrs.get("id")

        self._attrs = attrs.copy()
        # As Controls are merged in with .merge_control(), self._attrs will
        # refer to each Control in turn -- always the most recently merged
        # control.  Each merged-in Control instance corresponds to a single
        # list item: see ListControl.__doc__.
        if attrs:
            self._attrs_list = [self._attrs]  # extended by .merge_control()
            self._disabled_list = [self._attrs.has_key("disabled")]  # ditto
        else:
            self._attrs_list = []  # extended by .merge_control()
            self._disabled_list = []  # ditto

        self._select_default = select_default
        self._clicked = False
        # Some list controls can have their default set only after all items
        # are known.  If so, self._value_is_set is false, and the self.fixup
        # method, called after all items have been added, sets the default.
        self._value_is_set = False

    def _value_from_label(self, label):
        raise NotImplementedError("control '%s' does not yet support "
                                  "by_label" % self.name)

    def toggle(self, name, by_label=False):
        return self._set_selected_state(name, 2, by_label)
    def set(self, selected, name, by_label=False):
        action = int(bool(selected))
        return self._set_selected_state(name, action, by_label)

    def _set_selected_state(self, name, action, by_label):
        """
        name: item name
        action:
         0: clear
         1: set
         2: toggle

        """
        if not isstringlike(name):
            raise TypeError("item name must be string-like")
        if self.disabled:
            raise AttributeError("control '%s' is disabled" % self.name)
        if self.readonly:
            raise AttributeError("control '%s' is readonly" % self.name)
        if by_label:
            name = self._value_from_label(name)
        try:
            i = self._menu.index(name)
        except ValueError:
            raise ItemNotFoundError("no item named '%s'" % name)

        if self.multiple:
            if action == 2:
                action = not self._selected[i]
            if action and self._disabled_list[i]:
                raise AttributeError("item '%s' is disabled" % name)
            self._selected[i] = bool(action)
        else:
            if action == 2:
                if self._selected == name:
                    action = 0
                else:
                    action = 1
            if action == 0 and self._selected == name:
                self._selected = None
            elif action == 1:
                if self._disabled_list[i]:
                    raise AttributeError("item '%s' is disabled" % name)
                self._selected = name

    def toggle_single(self, by_label=False):
        self._set_single_selected_state(2, by_label)
    def set_single(self, selected, by_label=False):
        action = int(bool(selected))
        self._set_single_selected_state(action, by_label)

    def _set_single_selected_state(self, action, by_label):
        if len(self._menu) != 1:
            raise ItemCountError("'%s' is not a single-item control" %
                                 self.name)

        name = self._menu[0]
        if by_label:
            name = self._value_from_label(name)
        self._set_selected_state(name, action, by_label)

    def get_item_disabled(self, name, by_label=False):
        """Get disabled state of named list item in a ListControl."""
        if by_label:
            name = self._value_from_label(name)
        try:
            i = self._menu.index(name)
        except ValueError:
            raise ItemNotFoundError()
        else:
            return self._disabled_list[i]

    def set_item_disabled(self, disabled, name, by_label=False):
        """Set disabled state of named list item in a ListControl.

        disabled: boolean disabled state

        """
        if by_label:
            name = self._value_from_label(name)
        try:
            i = self._menu.index(name)
        except ValueError:
            raise ItemNotFoundError()
        else:
            self._disabled_list[i] = bool(disabled)

    def set_all_items_disabled(self, disabled):
        """Set disabled state of all list items in a ListControl.

        disabled: boolean disabled state

        """
        for i in range(len(self._disabled_list)):
            self._disabled_list[i] = bool(disabled)

    def get_item_attrs(self, name, by_label=False):
        """Return dictionary of HTML attributes for a single ListControl item.

        The HTML element types that describe list items are: OPTION for SELECT
        controls, INPUT for the rest.  These elements have HTML attributes that
        you may occasionally want to know about -- for example, the "alt" HTML
        attribute gives a text string describing the item (graphical browsers
        usually display this as a tooltip).

        The returned dictionary maps HTML attribute names to values.  The names
        and values are taken from the original HTML.

        Note that for SELECT controls, the returned dictionary contains a
        special key "contents" -- see SelectControl.__doc__.

        """
        if by_label:
            name = self._value_from_label(name)
        try:
            i = self._menu.index(name)
        except ValueError:
            raise ItemNotFoundError()
        return self._attrs_list[i]

    def add_to_form(self, form):
        try:
            control = form.find_control(self.name, self.type)
        except ControlNotFoundError:
            Control.add_to_form(self, form)
        else:
            control.merge_control(self)

    def merge_control(self, control):
        assert bool(control.multiple) == bool(self.multiple)
        assert isinstance(control, self.__class__)
        self._menu.extend(control._menu)
        self._attrs_list.extend(control._attrs_list)
        self._disabled_list.extend(control._disabled_list)
        if control.multiple:
            self._selected.extend(control._selected)
        else:
            if control._value_is_set:
                self._selected = control._selected
        if control._value_is_set:
            self._value_is_set = True

    def fixup(self):
        """
        ListControls are built up from component list items (which are also
        ListControls) during parsing.  This method should be called after all
        items have been added.  See ListControl.__doc__ for the reason this is
        required.

        """
        # Need to set default selection where no item was indicated as being
        # selected by the HTML:

        # CHECKBOX:
        #  Nothing should be selected.
        # SELECT/single, SELECT/multiple and RADIO:
        #  RFC 1866 (HTML 2.0): says first item should be selected.
        #  W3C HTML 4.01 Specification: says that client behaviour is
        #   undefined in this case.  For RADIO, exactly one must be selected,
        #   though which one is undefined.
        #  Both Netscape and Microsoft Internet Explorer (IE) choose first
        #   item for SELECT/single.  However, both IE5 and Mozilla (both 1.0
        #   and Firebird 0.6) leave all items unselected for RADIO and
        #   SELECT/multiple.

        # Since both Netscape and IE all choose the first item for
        # SELECT/single, we do the same.  OTOH, both Netscape and IE
        # leave SELECT/multiple with nothing selected, in violation of RFC 1866
        # (but not in violation of the W3C HTML 4 standard); the same is true
        # of RADIO (which *is* in violation of the HTML 4 standard).  We follow
        # RFC 1866 if the select_default attribute is set, and Netscape and IE
        # otherwise.  RFC 1866 and HTML 4 are always violated insofar as you
        # can deselect all items in a RadioControl.

        raise NotImplementedError()

    def __getattr__(self, name):
        if name == "value":
            menu = self._menu
            if self.multiple:
                values = []
                for i in range(len(menu)):
                    if self._selected[i]: values.append(menu[i])
                return values
            else:
                if self._selected is None: return []
                else: return [self._selected]
        else:
            raise AttributeError("%s instance has no attribute '%s'" %
                                 (self.__class__.__name__, name))

    def __setattr__(self, name, value):
        if name == "value":
            if self.disabled:
                raise AttributeError("control '%s' is disabled" % self.name)
            if self.readonly:
                raise AttributeError("control '%s' is readonly" % self.name)
            self._set_value(value)
        elif name in ("name", "type", "multiple"):
            raise AttributeError("%s attribute is readonly" % name)
        else:
            self.__dict__[name] = value

    def _set_value(self, value):
        if self.multiple:
            self._multiple_set_value(value)
        else:
            self._single_set_value(value)

    def _single_set_value(self, value):
        if value is None or isstringlike(value):
            raise TypeError("ListControl, must set a sequence")
        nr = len(value)
        if not (0 <= nr <= 1):
            raise ItemCountError("single selection list, must set sequence of "
                                 "length 0 or 1")

        if nr == 0:
            self._selected = None
        else:
            value = value[0]
            try:
                i = self._menu.index(value)
            except ValueError:
                raise ItemNotFoundError("no item named '%s'" %
                                        repr(value))
            if self._disabled_list[i]:
                raise AttributeError("item '%s' is disabled" % value)
            self._selected = value

    def _multiple_set_value(self, value):
        if value is None or isstringlike(value):
            raise TypeError("ListControl, must set a sequence")

        selected = [False]*len(self._selected)
        menu = self._menu
        disabled_list = self._disabled_list

        for v in value:
            found = False
            for i in range(len(menu)):
                item_name = menu[i]
                if v == item_name:
                    if disabled_list[i]:
                        raise AttributeError("item '%s' is disabled" % value)
                    selected[i] = True
                    found = True
                    break
            if not found:
                raise ItemNotFoundError("no item named '%s'" % repr(v))
        self._selected = selected

    def set_value_by_label(self, value):
        raise NotImplementedError("control '%s' does not yet support "
                                  "by_label" % self.name)
    def get_value_by_label(self):
        raise NotImplementedError("control '%s' does not yet support "
                                  "by_label" % self.name)

    def possible_items(self, by_label=False):
        if by_label:
            raise NotImplementedError(
                "control '%s' does not yet support by_label" % self.name)
        return copy.copy(self._menu)

    def pairs(self):
        if self.disabled:
            return []

        if not self.multiple:
            name = self.name
            value = self._selected
            if name is None or value is None:
                return []
            return [(name, value)]
        else:
            control_name = self.name  # usually the name HTML attribute
            pairs = []
            for i in range(len(self._menu)):
                item_name = self._menu[i]  # usually the value HTML attribute
                if self._selected[i]:
                    pairs.append((control_name, item_name))
            return pairs

    def _item_str(self, i):
        item_name = self._menu[i]
        if self.multiple:
            if self._selected[i]:
                item_name = "*"+item_name
        else:
            if self._selected == item_name:
                item_name = "*"+item_name
        if self._disabled_list[i]:
            item_name = "(%s)" % item_name
        return item_name

    def __str__(self):
        name = self.name
        if name is None: name = "<None>"

        display = []
        for i in range(len(self._menu)):
            s = self._item_str(i)
            display.append(s)

        infos = []
        if self.disabled: infos.append("disabled")
        if self.readonly: infos.append("readonly")
        info = string.join(infos, ", ")
        if info: info = " (%s)" % info

        return "<%s(%s=[%s])%s>" % (self.__class__.__name__,
                                    name, string.join(display, ", "), info)


class RadioControl(ListControl):
    """
    Covers:

    INPUT/RADIO

    """
    def __init__(self, type, name, attrs, select_default=False):
        ListControl.__init__(self, type, name, attrs, select_default,
                             called_as_base_class=True)
        self.__dict__["multiple"] = False
        value = attrs.get("value", "on")
        self._menu = [value]
        checked = attrs.has_key("checked")
        if checked:
            self._value_is_set = True
            self._selected = value
        else:
            self._selected = None

    def fixup(self):
        if not self._value_is_set:
            # no item explicitly selected
            assert self._selected is None
            if self._select_default:
                self._selected = self._menu[0]
            self._value_is_set = True


class CheckboxControl(ListControl):
    """
    Covers:

    INPUT/CHECKBOX

    """
    def __init__(self, type, name, attrs, select_default=False):
        ListControl.__init__(self, type, name, attrs, select_default,
                             called_as_base_class=True)
        self.__dict__["multiple"] = True
        value = attrs.get("value", "on")
        self._menu = [value]
        checked = attrs.has_key("checked")
        self._selected = [checked]
        self._value_is_set = True

    def fixup(self):
        # If no items were explicitly checked in HTML, that's how we must
        # leave it, so we have nothing to do here.
        assert self._value_is_set


class SelectControl(ListControl):
    """
    Covers:

    SELECT (and OPTION)

    SELECT control values and labels are subject to some messy defaulting
    rules.  For example, if the HTML repreentation of the control is:

    <SELECT name=year>
      <OPTION value=0 label="2002">current year</OPTION>
      <OPTION value=1>2001</OPTION>
      <OPTION>2000</OPTION>
    </SELECT>

    The items, in order, have labels "2002", "2001" and "2000", whereas their
    values are "0", "1" and "2000" respectively.  Note that the value of the
    last OPTION in this example defaults to its contents, as specified by RFC
    1866, as do the labels of the second and third OPTIONs.

    The purpose of these methods is that the OPTION labels are sometimes much
    more meaningful, than are the OPTION values, which can make for more
    maintainable code.

    Additional read-only public attribute: attrs

    The attrs attribute is a dictionary of the original HTML attributes of the
    SELECT element.  Other ListControls do not have this attribute, because in
    other cases the control as a whole does not correspond to any single HTML
    element.  The get_item_attrs method may be used as usual to get at the
    HTML attributes of the HTML elements corresponding to individual list items
    (for SELECT controls, these are OPTION elements).

    Another special case is that the attributes dictionaries returned by
    get_item_attrs have a special key "contents" which does not correspond to
    any real HTML attribute, but rather contains the contents of the OPTION
    element:

    <OPTION>this bit</OPTION>

    """
    # HTML attributes here are treated slightly from other list controls:
    # -The SELECT HTML attributes dictionary is stuffed into the OPTION
    #  HTML attributes dictionary under the "__select" key.
    # -The content of each OPTION element is stored under the special
    #  "contents" key of the dictionary.
    # After all this, the dictionary is passed to the SelectControl constructor
    # as the attrs argument, as usual.  However:
    # -The first SelectControl constructed when building up a SELECT control
    #  has a constructor attrs argument containing only the __select key -- so
    #  this SelectControl represents an empty SELECT control.
    # -Subsequent SelectControls have both OPTION HTML-attribute in attrs and
    #  the __select dictionary containing the SELECT HTML-attributes.
    def __init__(self, type, name, attrs, select_default=False):
        # fish out the SELECT HTML attributes from the OPTION HTML attributes
        # dictionary
        self.attrs = attrs["__select"].copy()
        attrs = attrs.copy()
        del attrs["__select"]

        ListControl.__init__(self, type, name, attrs, select_default,
                             called_as_base_class=True)

        self._label_map = None
        self.disabled = self.attrs.has_key("disabled")
        self.id = self.attrs.get("id")

        self._menu = []
        self._selected = []
        self._value_is_set = False
        if self.attrs.has_key("multiple"):
            self.__dict__["multiple"] = True
            self._selected = []
        else:
            self.__dict__["multiple"] = False
            self._selected = None

        if attrs:  # OPTION item data was provided
            value = attrs["value"]
            self._menu.append(value)
            selected = attrs.has_key("selected")
            if selected:
                self._value_is_set = True
            if self.attrs.has_key("multiple"):
                self._selected.append(selected)
            elif selected:
                self._selected = value

    def _build_select_label_map(self):
        """Return an ordered mapping of labels to values.

        For example, if the HTML repreentation of the control is as given in
        SelectControl.__doc__,  this function will return a mapping like:

        {"2002": "0", "2001": "1", "2000": "2000"}

        """
        alist = []
        for val in self._menu:
            attrs = self.get_item_attrs(val)
            alist.append((attrs["label"], val))
        return AList(alist)

    def _value_from_label(self, label):
        try:
            return self._label_map[label]
        except KeyError:
            raise ItemNotFoundError("no item has label '%s'" % label)

    def fixup(self):
        if not self._value_is_set:
            # No item explicitly selected.
            if len(self._menu) > 0:
                if self.multiple:
                    if self._select_default:
                        self._selected[0] = True
                else:
                    assert self._selected is None
                    self._selected = self._menu[0]
            self._value_is_set = True
        self._label_map = self._build_select_label_map()

    def possible_items(self, by_label=False):
        if not by_label:
            return copy.copy(self._menu)
        else:
            self._label_map.set_inverted(True)
            try:
                r = map(lambda v, self=self: self._label_map[v], self._menu)
            finally:
                self._label_map.set_inverted(False)
            return r

    def set_value_by_label(self, value):
        if isstringlike(value):
            raise TypeError("ListControl, must set a sequence, not a string")
        if self.disabled:
            raise AttributeError("control '%s' is disabled" % self.name)
        if self.readonly:
            raise AttributeError("control '%s' is readonly" % self.name)

        try:
            value = map(lambda v, self=self: self._label_map[v], value)
        except KeyError, e:
            raise ItemNotFoundError("no item has label '%s'" % e.args[0])
        self._set_value(value)

    def get_value_by_label(self):
        menu = self._menu
        self._label_map.set_inverted(True)
        try:
            if self.multiple:
                values = []
                for i in range(len(menu)):
                    if self._selected[i]:
                        values.append(self._label_map[menu[i]])
                return values
            else:
                return [self._label_map[self._selected]]
        finally:
            self._label_map.set_inverted(False)


#---------------------------------------------------
class SubmitControl(ScalarControl):
    """
    Covers:

    INPUT/SUBMIT
    BUTTON/SUBMIT

    """
    def __init__(self, type, name, attrs):
        ScalarControl.__init__(self, type, name, attrs)
        # IE5 defaults SUBMIT value to "Submit Query"; Firebird 0.6 leaves it
        # blank, Konqueror 3.1 defaults to "Submit".  HTML spec. doesn't seem
        # to define this.
        if self.value is None: self.value = ""
        self.readonly = True

    def _click(self, form, coord, return_type):
        self._clicked = coord
        r = form._switch_click(return_type)
        self._clicked = False
        return r

    def pairs(self):
        if not self._clicked:
            return []
        return ScalarControl.pairs(self)


#---------------------------------------------------
class ImageControl(SubmitControl):
    """
    Covers:

    INPUT/IMAGE

    The value attribute of an ImageControl is always None.  Coordinates are
    specified using one of the HTMLForm.click* methods.

    """
    def __init__(self, type, name, attrs):
        ScalarControl.__init__(self, type, name, attrs)
        self.__dict__["value"] = None

    def __setattr__(self, name, value):
        if name in ("value", "name", "type"):
            raise AttributeError("%s attribute is readonly" % name)
        else:
            self.__dict__[name] = value

    def pairs(self):
        clicked = self._clicked
        if self.disabled or not clicked:
            return []
        name = self.name
        if name is None: return []
        return [("%s.x" % name, str(clicked[0])),
                ("%s.y" % name, str(clicked[1]))]


# aliases, just to make str(control) and str(form) clearer
class PasswordControl(TextControl): pass
class HiddenControl(TextControl): pass
class TextareaControl(TextControl): pass
class SubmitButtonControl(SubmitControl): pass


def is_listcontrol(control): return isinstance(control, ListControl)


class HTMLForm:
    """Represents a single HTML <form> ... </form> element.

    A form consists of a sequence of controls that usually have names, and
    which can take on various values.  The values of the various types of
    controls represent variously: text, zero-, one- or many-of-many choices,
    and files to be uploaded.

    Forms can be filled in with data to be returned to the server, and then
    submitted, using the click method to generate a request object suitable for
    passing to urllib2.urlopen (or the click_request_data or click_pairs
    methods if you're not using urllib2).

    import ClientForm
    forms = ClientForm.ParseFile(html, base_uri)
    form = forms[0]

    form["query"] = "Python"
    form.set("lots", "nr_results")

    response = urllib2.urlopen(form.click())

    Usually, HTMLForm instances are not created directly.  Instead, the
    ParseFile or ParseResponse factory functions are used.  If you do construct
    HTMLForm objects yourself, however, note that an HTMLForm instance is only
    properly initialised after the fixup method has been called (ParseFile and
    ParseResponse do this for you).  See ListControl.__doc__ for the reason
    this is required.

    Indexing a form (form["control_name"]) returns the named Control's value
    attribute.  Assignment to a form index (form["control_name"] = something)
    is equivalent to assignment to the named Control's value attribute.  If you
    need to be more specific than just supplying the control's name, use the
    set_value and get_value methods.

    ListControl values are lists of item names.  The list item's name is the
    value of the corresponding HTML element's "value" attribute.

    Example:

      <INPUT type="CHECKBOX" name="cheeses" value="leicester"></INPUT>
      <INPUT type="CHECKBOX" name="cheeses" value="cheddar"></INPUT>

    defines a CHECKBOX control with name "cheeses" which has two items, named
    "leicester" and "cheddar".

    Another example:

      <SELECT name="more_cheeses">
        <OPTION>1</OPTION>
        <OPTION value="2" label="CHEDDAR">cheddar</OPTION>
      </SELECT>

    defines a SELECT control with name "more_cheeses" which has two items,
    named "1" and "2".

    To set, clear or toggle individual list items, use the set and toggle
    methods.  To set the whole value, do as for any other control:use indexing
    or the set_/get_value methods.

    Example:

    # select *only* the item named "cheddar"
    form["cheeses"] = ["cheddar"]
    # select "cheddar", leave other items unaffected
    form.set("cheddar", "cheeses")

    Some controls (RADIO and SELECT without the multiple attribute) can only
    have zero or one items selected at a time.  Some controls (CHECKBOX and
    SELECT with the multiple attribute) can have multiple items selected at a
    time.  To set the whole value of a multiple-selection ListControl, assign a
    sequence to a form index:

    form["cheeses"] = ["cheddar", "leicester"]

    To check whether a control has an item, or whether an item is selected,
    respectively:

    "cheddar" in form.possible_items("cheeses")
    "cheddar" in form["cheeses"]  # (or "cheddar" in form.get_value("cheeses"))

    Note that some items may be disabled (see below).

    Note the following mistake:

    form[control_name] = control_value
    assert form[control_name] == control_value  # not necessarily true

    The reason for this is that form[control_name] always gives the list items
    in the order they were listed in the HTML.

    List items (hence list values, too) can be referred to in terms of list
    item labels rather than list item names.  Currently, this is only possible
    for SELECT controls (this is a bug).  To use this feature, use the by_label
    arguments to the various HTMLForm methods.  Note that it is *item* names
    (hence ListControl values also), not *control* names, that can be referred
    to by label.

    The question of default values of OPTION contents, labels and values is
    somewhat complicated: see SelectControl.__doc__ and
    ListControl.get_item_attrs.__doc__ if you think you need to know.

    Controls can be disabled or readonly.  In either case, the control's value
    cannot be changed until you clear those flags (using the methods on
    HTMLForm).  Disabled is the state typically represented by browsers by
    `greying out' a control.  Disabled controls are not `successful' -- they
    don't cause data to get returned to the server.  Readonly controls usually
    appear in browsers as read-only text boxes.  Readonly controls are
    successful.  List items can also be disabled.  Attempts to select disabled
    items (with form[name] = value, or using the ListControl.set method, for
    example) fail.  Attempts to clear disabled items are allowed.

    If a lot of controls are readonly, it can be useful to do this:

    form.set_all_readonly(False)

    When you want to do several things with a single control, or want to do
    less common things, like changing which controls and items are disabled,
    you can get at a particular control:

    control = form.find_control("cheeses")
    control.set_item_disabled(False, "gruyere")
    control.set("gruyere")

    Most methods on HTMLForm just delegate to the contained controls, so see
    the docstrings of the various Control classes for further documentation.
    Most of these delegating methods take name, type, kind, id and nr arguments
    to specify the control to be operated on: see
    HTMLForm.find_control.__doc__.

    ControlNotFoundError (subclass of ValueError) is raised if the specified
    control can't be found.  This includes occasions where a non-ListControl
    is found, but the method (set, for example) requires a ListControl.
    ItemNotFoundError (subclass of ValueError) is raised if a list item can't
    be found.  ItemCountError (subclass of ValueError) is raised if an attempt
    is made to select more than one item and the control doesn't allow that, or
    set/get_single are called and the control contains more than one item.
    AttributeError is raised if a control or item is readonly or disabled and
    an attempt is made to alter its value.

    XXX CheckBoxControl and RadioControl don't yet support item access by label

    Security note: Remember that any passwords you store in HTMLForm instances
    will be saved to disk in the clear if you pickle them (directly or
    indirectly).  The simplest solution to this is to avoid pickling HTMLForm
    objects.  You could also pickle before filling in any password, or just set
    the password to "" before pickling.


    Public attributes:

    action: full (absolute URI) form action
    method: "GET" or "POST"
    enctype: form transfer encoding MIME type
    name: name of form (None if no name was specified)
    attrs: dictionary mapping original HTML form attributes to their values

    controls: list of Control instances; do not alter this list
     (instead, call form.new_control to make a Control and add it to the
     form, or control.add_to_form if you already have a Control instance)



    Methods for form filling:
    -------------------------

    Most of the these methods have very similar arguments.  See
    HTMLForm.find_control.__doc__ for details of the name, type, kind and nr
    arguments.  See above for a description of by_label.

    def find_control(self,
                     name=None, type=None, kind=None, id=None, predicate=None,
                     nr=None)

    get_value(name=None, type=None, kind=None, id=None, nr=None,
              by_label=False)
    set_value(value,
              name=None, type=None, kind=None, id=None, nr=None,
              by_label=False)

    set_all_readonly(readonly)


    Methods applying only to ListControls:

    possible_items(name=None, type=None, kind=None, id=None, nr=None,
                   by_label=False)

    set(selected, item_name,
        name=None, type=None, kind=None, id=None, nr=None,
        by_label=False)
    toggle(item_name,
           name=None, type=None, id=None, nr=None,
           by_label=False)

    set_single(selected,
               name=None, type=None, kind=None, id=None, nr=None,
               by_label=False)
    toggle_single(name=None, type=None, kind=None, id=None, nr=None,
                  by_label=False)


    Method applying only to FileControls:

    add_file(file_object,
             content_type="application/octet-stream", filename=None,
             name=None, id=None, nr=None)


    Methods applying only to clickable controls:

    click(name=None, type=None, id=None, nr=0, coord=(1,1))
    click_request_data(name=None, type=None, id=None, nr=0, coord=(1,1))
    click_pairs(name=None, type=None, id=None, nr=0, coord=(1,1))

    """

    type2class = {
        "text": TextControl,
        "password": PasswordControl,
        "hidden": HiddenControl,
        "textarea": TextareaControl,

        "isindex": IsindexControl,

        "file": FileControl,

        "button": IgnoreControl,
        "buttonbutton": IgnoreControl,
        "reset": IgnoreControl,
        "resetbutton": IgnoreControl,

        "submit": SubmitControl,
        "submitbutton": SubmitButtonControl,
        "image": ImageControl,

        "radio": RadioControl,
        "checkbox": CheckboxControl,
        "select": SelectControl,
        }

#---------------------------------------------------
# Initialisation.  Use ParseResponse / ParseFile instead.

    def __init__(self, action, method="GET",
                 enctype="application/x-www-form-urlencoded",
                 name=None, attrs=None):
        """
        In the usual case, use ParseResponse (or ParseFile) to create new
        HTMLForm objects.

        action: full (absolute URI) form action
        method: "GET" or "POST"
        enctype: form transfer encoding MIME type
        name: name of form
        attrs: dictionary mapping original HTML form attributes to their values

        """
        self.action = action
        self.method = method
        self.enctype = enctype
        self.name = name
        if attrs is not None:
            self.attrs = attrs.copy()
        else:
            self.attrs = {}
        self.controls = []

    def new_control(self, type, name, attrs,
                    ignore_unknown=False, select_default=False):
        """Adds a new control to the form.

        This is usually called by ParseFile and ParseResponse.  Don't call it
        youself unless you're building your own Control instances.

        Note that controls representing lists of items are built up from
        controls holding only a single list item.  See ListControl.__doc__ for
        further information.

        type: type of control (see Control.__doc__ for a list)
        attrs: HTML attributes of control
        ignore_unknown: if true, use a dummy Control instance for controls of
         unknown type; otherwise, raise ValueError
        select_default: for RADIO and multiple-selection SELECT controls, pick
         the first item as the default if no 'selected' HTML attribute is
         present (this defaulting happens when the HTMLForm.fixup method is
         called)

        """
        type = string.lower(type)
        klass = self.type2class.get(type)
        if klass is None:
            if ignore_unknown:
                klass = IgnoreControl
            else:
                raise ValueError("Unknown control type '%s'" % type)

        a = attrs.copy()
        if issubclass(klass, ListControl):
            control = klass(type, name, a, select_default)
        else:
            control = klass(type, name, a)
        control.add_to_form(self)

    def fixup(self):
        """Normalise form after all controls have been added.

        This is usually called by ParseFile and ParseResponse.  Don't call it
        youself unless you're building your own Control instances.

        This method should only be called once, after all controls have been
        added to the form.

        """
        for control in self.controls:
            control.fixup()

#---------------------------------------------------
    def __str__(self):
        header = "%s %s %s" % (self.method, self.action, self.enctype)
        rep = [header]
        for control in self.controls:
            rep.append("  %s" % str(control))
        return "<%s>" % string.join(rep, "\n")

#---------------------------------------------------
# Form-filling methods.

    def __getitem__(self, name):
        return self.find_control(name).value
    def __setitem__(self, name, value):
        control = self.find_control(name)
        try:
            control.value = value
        except AttributeError, e:
            raise ValueError(str(e))

    def get_value(self,
                  name=None, type=None, kind=None, id=None, nr=None,
                  by_label=False):
        """Return value of control.

        If only name and value arguments are supplied, equivalent to

        form[name]

        """
        c = self.find_control(name, type, kind, id, nr=nr)
        if by_label:
            try:
                meth = c.get_value_by_label
            except AttributeError:
                raise NotImplementedError(
                    "control '%s' does not yet support by_label" % c.name)
            else:
                return meth()
        else:
            return c.value
    def set_value(self, value,
                  name=None, type=None, kind=None, id=None, nr=None,
                  by_label=False):
        """Set value of control.

        If only name and value arguments are supplied, equivalent to

        form[name] = value

        """
        c = self.find_control(name, type, kind, id, nr=nr)
        if by_label:
            try:
                meth = c.set_value_by_label
            except AttributeError:
                raise NotImplementedError(
                    "control '%s' does not yet support by_label" % c.name)
            else:
                meth(value)
        else:
            c.value = value

    def set_all_readonly(self, readonly):
        for control in self.controls:
            control.readonly = bool(readonly)


#---------------------------------------------------
# Form-filling methods applying only to ListControls.

    def possible_items(self,
                       name=None, type=None, kind=None, id=None, nr=None,
                       by_label=False):
        """Return a list of all values that the specified control can take."""
        c = self._find_list_control(name, type, kind, id, nr)
        return c.possible_items(by_label)

    def set(self, selected, item_name,
            name=None, type=None, kind=None, id=None, nr=None,
            by_label=False):
        """Select / deselect named list item.

        selected: boolean selected state

        """
        self._find_list_control(name, type, kind, id, nr).set(
            selected, item_name, by_label)
    def toggle(self, item_name,
               name=None, type=None, kind=None, id=None, nr=None,
               by_label=False):
        """Toggle selected state of named list item."""
        self._find_list_control(name, type, kind, id, nr).toggle(
            item_name, by_label)

    def set_single(self, selected,
                   name=None, type=None, kind=None, id=None, nr=None,
                   by_label=False):
        """Select / deselect list item in a control having only one item.

        If the control has multiple list items, ItemCountError is raised.

        This is just a convenience method, so you don't need to know the item's
        name -- the item name in these single-item controls is usually
        something meaningless like "1" or "on".

        For example, if a checkbox has a single item named "on", the following
        two calls are equivalent:

        control.toggle("on")
        control.toggle_single()

        """
        self._find_list_control(name, type, kind, id, nr).set_single(
            selected, by_label)
    def toggle_single(self, name=None, type=None, kind=None, id=None, nr=None,
                      by_label=False):
        """Toggle selected state of list item in control having only one item.

        The rest is as for HTMLForm.set_single.__doc__.

        """
        self._find_list_control(name, type, kind, id, nr).toggle_single(
            by_label)

#---------------------------------------------------
# Form-filling method applying only to FileControls.

    def add_file(self, file_object, content_type=None, filename=None,
                 name=None, id=None, nr=None):
        """Add a file to be uploaded.

        file_object: file-like object (with read method) from which to read
         data to upload
        content_type: MIME content type of data to upload
        filename: filename to pass to server

        If filename is None, no filename is sent to the server.

        If content_type is None, the content type is guessed based on the
        filename and the data from read from the file object.

        XXX
        At the moment, guessed content type is always application/octet-stream.
        Use sndhdr, imghdr modules.  Should also try to guess HTML, XML, and
        plain text.

        """
        self.find_control(name, "file", id=id, nr=nr).add_file(
            file_object, content_type, filename)

#---------------------------------------------------
# Form submission methods, applying only to clickable controls.

    def click(self, name=None, type=None, id=None, nr=0, coord=(1,1)):
        """Return request that would result from clicking on a control.

        The request object is a urllib2.Request instance, which you can pass to
        urllib2.urlopen (or ClientCookie.urlopen).

        Only some control types (INPUT/SUBMIT & BUTTON/SUBMIT buttons and
        IMAGEs) can be clicked.

        Will click on the first clickable control, subject to the name, type
        and nr arguments (as for find_control).  If no name, type, id or number
        is specified and there are no clickable controls, a request will be
        returned for the form in its current, un-clicked, state.

        IndexError is raised if any of name, type, id or nr is specified but no
        matching control is found.  ValueError is raised if the HTMLForm has an
        enctype attribute that is not recognised.

        You can optionally specify a coordinate to click at, which only makes a
        difference if you clicked on an image.

        """
        return self._click(name, type, id, nr, coord, "request")

    def click_request_data(self,
                           name=None, type=None, id=None, nr=0, coord=(1,1)):
        """As for click method, but return a tuple (url, data, headers).

        You can use this data to send a request to the server.  This is useful
        if you're using httplib or urllib rather than urllib2.  Otherwise, use
        the click method.

        # Untested.  Have to subclass to add headers, I think -- so use urllib2
        # instead!
        import urllib
        url, data, hdrs = form.click_request_data()
        r = urllib.urlopen(url, data)

        # Untested.  I don't know of any reason to use httplib -- you can get
        # just as much control with urllib2.
        import httplib, urlparse
        url, data, hdrs = form.click_request_data()
        tup = urlparse(url)
        host, path = tup[1], urlparse.urlunparse((None, None)+tup[2:])
        conn = httplib.HTTPConnection(host)
        if data:
            httplib.request("POST", path, data, hdrs)
        else:
            httplib.request("GET", path, headers=hdrs)
        r = conn.getresponse()

        """
        return self._click(name, type, id, nr, coord, "request_data")

    def click_pairs(self, name=None, type=None, id=None, nr=0, coord=(1,1)):
        """As for click_request_data, but returns a list of (key, value) pairs.

        You can use this list as an argument to ClientForm.urlencode.  This is
        usually only useful if you're using httplib or urllib rather than
        urllib2 or ClientCookie.  It may also be useful if you want to manually
        tweak the keys and/or values, but this should not be necessary.
        Otherwise, use the click method.

        Note that this method is only useful for forms of MIME type
        x-www-form-urlencoded.  In particular, it does not return the
        information required for file upload.  If you need file upload and are
        not using urllib2, use click_request_data.

        Also note that Python 2.0's urllib.urlencode is slightly broken: it
        only accepts a mapping, not a sequence of pairs, as an argument.  This
        messes up any ordering in the argument.  Use ClientForm.urlencode
        instead.

        """
        return self._click(name, type, id, nr, coord, "pairs")

#---------------------------------------------------

    def find_control(self,
                     name=None, type=None, kind=None, id=None, predicate=None,
                     nr=None):
        """Locate some specific control within the form.

        At least one of the name, type, kind, predicate and nr arguments must
        be supplied.  If no matching control is found, ControlNotFoundError is
        raised.

        If name is specified, then the control must have the indicated name.

        If type is specified then the control must have the specified type (in
        addition to the types possible for <input> HTML tags: "text",
        "password", "hidden", "submit", "image", "button", "radio", "checkbox",
        "file" we also have "reset", "buttonbutton", "submitbutton",
        "resetbutton", "textarea", "select" and "isindex").

        If kind is specified, then the control must fall into the specified
        group, each of which satisfies a particular interface.  The types are
        "text", "list", "multilist", "singlelist", "clickable" and "file".

        If id is specified, then the control must have the indicated id.

        If predicate is specified, then the control must match that function.
        The predicate function is passed the control as its single argument,
        and should return a boolean value indicating whether the control
        matched.

        nr, if supplied, is the sequence number of the control (where 0 is the
        first).  Note that control 0 is the first control matching all the
        other arguments (if supplied); it is not necessarily the first control
        in the form.

        """
        if ((name is None) and (type is None) and (kind is None) and
            (id is None) and (predicate is None) and (nr is None)):
            raise ValueError(
                "at least one argument must be supplied to specify control")
        if nr is None: nr = 0

        return self._find_control(name, type, kind, id, predicate, nr)

#---------------------------------------------------
# Private methods.

    def _find_list_control(self,
                           name=None, type=None, kind=None, id=None, nr=None):
        if ((name is None) and (type is None) and (kind is None) and
            (id is None) and (nr is None)):
            raise ValueError(
                "at least one argument must be supplied to specify control")
        if nr is None: nr = 0

        return self._find_control(name, type, kind, id, is_listcontrol, nr)

    def _find_control(self, name, type, kind, id, predicate, nr):
        if (name is not None) and not isstringlike(name):
            raise TypeError("control name must be string-like")
        if (type is not None) and not isstringlike(type):
            raise TypeError("control type must be string-like")
        if (kind is not None) and not isstringlike(kind):
            raise TypeError("control kind must be string-like")
        if (id is not None) and not isstringlike(id):
            raise TypeError("control id must be string-like")
        if (predicate is not None) and not callable(predicate):
            raise TypeError("control predicate must be callable")
        if nr < 0: raise ValueError("control number must be a positive "
                                    "integer")

        orig_nr = nr

        for control in self.controls:
            if name is not None and name != control.name:
                continue
            if type is not None and type != control.type:
                continue
            if (kind is not None and
                not self._is_control_in_kind(control, kind)):
                continue
            if id is not None and id != control.id:
                continue
            if predicate and not predicate(control):
                continue
            if nr:
                nr = nr - 1
                continue
            return control

        description = []
        if name is not None: description.append("name '%s'" % name)
        if type is not None: description.append("type '%s'" % type)
        if kind is not None: description.append("kind '%s'" % kind)
        if id is not None: description.append("id '%s'" % id)
        if predicate is not None:
            description.append("matching predicate %s" % predicate)
        if orig_nr: description.append("nr %d" % orig_nr)
        description = string.join(description, ", ")
        raise ControlNotFoundError("no control with "+description)

    def _is_control_in_kind(self, control, kind):
        # XXX not OO
        if kind  == "list":
            return isinstance(control, ListControl)
        elif kind == "multilist":
            return bool(isinstance(control, ListControl) and control.multiple)
        elif kind == "singlelist":
            return bool(isinstance(control, ListControl) and
                        not control.multiple)
        elif kind == "file":
            return isinstance(control, FileControl)
        elif kind == "text":
            return isinstance(control, TextControl)
        elif kind == "clickable":
            return (isinstance(control, SubmitControl) or
                    isinstance(control, IsindexControl))
        else:
            raise ValueError("no such control kind '%s'" % kind)

    def _click(self, name, type, id, nr, coord, return_type):
        try:
            control = self._find_control(name, type, "clickable", id, None, nr)
        except ControlNotFoundError:
            if ((name is not None) or (type is not None) or (id is not None) or
                (nr != 0)):
                raise
            # no clickable controls, but no control was explicitly requested,
            # so return state without clicking any control
            return self._switch_click(return_type)
        else:
            return control._click(self, coord, return_type)

    def _pairs(self):
        """Return sequence of (key, value) pairs suitable for urlencoding."""
        pairs = []
        for control in self.controls:
            pairs.extend(control.pairs())
        return pairs

    def _request_data(self):
        """Return a tuple (url, data, headers)."""
        method = string.upper(self.method)
        if method == "GET":
            if self.enctype != "application/x-www-form-urlencoded":
                raise ValueError(
                    "unknown GET form encoding type '%s'" % self.enctype)
            uri = "%s?%s" % (self.action, urlencode(self._pairs()))
            return uri, None, []
        elif method == "POST":
            if self.enctype == "application/x-www-form-urlencoded":
                return (self.action, urlencode(self._pairs()),
                        [("Content-type", self.enctype)])
            elif self.enctype == "multipart/form-data":
                data = StringIO()
                http_hdrs = []
                mw = MimeWriter(data, http_hdrs)
                f = mw.startmultipartbody("form-data", add_to_http_hdrs=True,
                                          prefix=0)
                for control in self.controls:
                    control._write_mime_data(mw)
                mw.lastpart()
                return self.action, data.getvalue(), http_hdrs
            else:
                raise ValueError(
                    "unknown POST form encoding type '%s'" % self.enctype)
        else:
            raise ValueError("Unknown method '%s'" % method)

    def _switch_click(self, return_type):
        # This is called by HTMLForm and clickable Controls to hide switching
        # on return_type.
        # XXX
        # not OO
        # duplicated in IsindexControl._click
        if return_type == "pairs":
            return self._pairs()
        elif return_type == "request_data":
            return self._request_data()
        else:
            req_data = self._request_data()
            req = urllib2.Request(req_data[0], req_data[1])
            for key, val in req_data[2]:
                req.add_header(key, val)
            return req
