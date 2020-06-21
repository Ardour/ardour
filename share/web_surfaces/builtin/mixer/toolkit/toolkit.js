/*
 * This file is part of Toolkit.
 *
 * Toolkit is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * Toolkit is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General
 * Public License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA  02110-1301  USA
 */
"use strict";

/** @namespace TK
 *
 * @description This is the namespace of the Toolkit library.
 * It contains all Toolkit classes and constant.
 * There are also a couple of utility functions which provide
 * compatibility for older browsers.
 */
var TK;

(function(w) {

var has_class, add_class, remove_class, toggle_class;
// IE9
function get_class_name(e) {
  if (HTMLElement.prototype.isPrototypeOf(e)) {
      return e.className;
  } else {
      return e.getAttribute("class") || "";
  }
}
function set_class_name(e, s) {
  if (HTMLElement.prototype.isPrototypeOf(e)) {
      e.className = s;
  } else {
      e.setAttribute("class", s);
  }
}

if ('classList' in document.createElement("_") && 'classList' in make_svg('text')) {
  /**
   * Returns true if the node has the given class.
   * @param {HTMLElement|SVGElement} node - The DOM node.
   * @param {string} name - The class name.
   * @returns {boolean}
   * @function TK.has_class
   */
  has_class = function (e, cls) { return e.classList.contains(cls); }
  /**
   * Adds a CSS class to a DOM node.
   *
   * @param {HTMLElement|SVGElement} node - The DOM node.
   * @param {...*} names - The class names.
   * @function TK.add_class
   */
  add_class = function (e) {
      var i;
      e = e.classList;
      for (i = 1; i < arguments.length; i++) {
          var a = arguments[i].split(" ");
          for (var j = 0; j < a.length; j++)
              e.add(a[j]);
      }
  }
  /**
   * Removes a CSS class from a DOM node.
   * @param {HTMLElement|SVGElement} node - The DOM node.
   * @param {...*} names - The class names.
   * @function TK.remove_class
   */
  remove_class = function (e) {
      var i;
      e = e.classList;
      for (i = 1; i < arguments.length; i++)
          e.remove(arguments[i]);
  }
  /**
   * Toggles a CSS class from a DOM node.
   * @param {HTMLElement|SVGElement} node - The DOM node.
   * @param {string} name - The class name.
   * @function TK.toggle_class
   */
  toggle_class = function (e, cls, cond) {
      /* The second argument to toggle is not implemented in IE,
       * so we never use it */
      if (arguments.length >= 3) {
          if (cond) {
              add_class(e, cls);
          } else {
              remove_class(e, cls);
          }
      } else e.classList.toggle(cls);
  };
} else {
  has_class = function (e, cls) {
    return get_class_name(e).split(" ").indexOf(cls) !== -1;
  };
  add_class = function (e) {
    var i, cls;
    var a = get_class_name(e).split(" ");

    for (i = 1; i < arguments.length; i++) {
        cls = arguments[i];
        if (a.indexOf(cls) === -1) {
          a.push(cls);
        }
    }
    set_class_name(e, a.join(" "));
  };
  remove_class = function(e) {
    var j, cls, i;
    var a = get_class_name(e).split(" ");

    for (j = 1; j < arguments.length; j++) {
        cls = arguments[j];
        i = a.indexOf(cls);

        if (i !== -1) {
          do {
            a.splice(i, 1);
            i = a.indexOf(cls);
          } while (i !== -1);

        }
    }

    set_class_name(e, a.join(" "));
  };
  toggle_class = function(e, cls, cond) {
      if (arguments.length < 3) {
          cond = !has_class(e, cls);
      }
      if (cond) {
          add_class(e, cls);
      } else {
          remove_class(e, cls);
      }
  };
}

var data_store;
var data;

if ('WeakMap' in window) {
    data = function(e) {
        var r;
        if (!data_store) data_store = new window.WeakMap();

        r = data_store[e];

        if (!r) {
            data_store[e] = r = {};
        }

        return r;
    };
} else {
    data_store = [];
    var data_keys = [];
    data = function(e) {
        if (typeof(e) !== "object") throw("Cannot store data for non-objects.");
        var k = data_keys.indexOf(e);
        var r;
        if (k === -1) {
            data_keys.push(e);
            k = data_store.push({}) - 1;
        }
        return data_store[k];
    };
}

var get_style;

if ('getComputedStyle' in window) {
  /**
   * Returns the computed style of a node.
   *
   * @param {HTMLElement|SVGElement} node - The DOM node.
   * @param {string} property - The CSS property name.
   * @returns {string}
   *
   * @function TK.get_style
   */
  get_style = function(e, style) {
    return window.getComputedStyle(e).getPropertyValue(style);
  };
} else {
  get_style = function(e, style) {
    return e.currentStyle[style];
  };
}

var class_regex = /[^A-Za-z0-9_\-]/;
function is_class_name (str) {
    /**
     * Returns true ii a string could be a class name.
     * @param {string} string - The string to test
     * @function TK.is_class_name
     * @returns {boolean}
     */
    return !class_regex.test(str);
}

function get_max_time(string) {
   /**
   * Returns the maximum value (float)  of a comma separated string. It is used
   * to find the longest CSS animation in a set of multiple animations.
   * @param {string} string - The comma separated string.
   * @function TK.get_max_time
   * @returns {number}
   * @example
   * get_max_time(get_style(DOMNode, "animation-duration"));
   */
    var ret = 0, i, tmp, s = string;

    if (typeof(s) === "string") {
        s = s.split(",");
        for (i = 0; i < s.length; i++) {
            tmp = parseFloat(s[i]);

            if (tmp > 0) {
                if (-1 === s[i].search("ms")) tmp *= 1000;
                if (tmp > ret) ret = tmp;
            }
        }
    }

    return ret|0;
}

function get_duration(element) {
   /**
   * Returns the longest animation duration of CSS animations and transitions.
   * @param {HTMLElement} element - The element to evalute the animation duration for.
   * @function TK.get_duration
   * @returns {number}
   */
    return Math.max(get_max_time(get_style(element, "animation-duration"))
                  + get_max_time(get_style(element, "animation-delay")),
                    get_max_time(get_style(element, "transition-duration"))
                  + get_max_time(get_style(element, "transition-delay")));
}

function get_id(id) {
    /**
     * Returns the DOM node with the given ID. Shorthand for document.getElementById.
     * @param {string} id - The ID to search for
     * @function TK.get_id
     * @returns {HTMLElement}
     */
    return document.getElementById(id);
}
function get_class(cls, element) {
    /**
     * Returns all elements as NodeList of a given class name. Optionally limit the list
     * to all children of a specific DOM node. Shorthand for element.getElementsByClassName.
     * @param {string} class - The name of the class
     * @param {DOMNode} element - Limit search to child nodes of this element. Optional.
     * @returns {NodeList}
     * @function TK.get_class
     */
    return (element ? element : document).getElementsByClassName(cls);
}
function get_tag(tag, element) {
    /**
     * Returns all elements as NodeList of a given tag name. Optionally limit the list
     * to all children of a specific DOM node. Shorthand for element.getElementsByTagName.
     * @param {string} tag - The name of the tag
     * @param {DOMNode} element - Limit search to child nodes of this element. Optional.
     * @returns {NodeList}
     * @function TK.get_tag
     */
    return (element ? element : document).getElementsByTagName(tag);
}
function element(tag) {
    /**
     * Returns a newly created HTMLElement.
     * @param {string} tag - The type of the element
     * @param {...object} attributes - Optional mapping of attributes for the new node
     * @param {...string} class - Optional class name for the new node
     * @returns HTMLElement
     * @function TK.element
     */
    var n = document.createElement(tag);
    var i, v, j;
    for (i = 1; i < arguments.length; i++) {
        v = arguments[i];
        if (typeof v === "object") {
            for (var key in v) {
                if (v.hasOwnProperty(key))
                    n.setAttribute(key, v[key]);
            }
        } else if (typeof v === "string") {
            add_class(n, v);
        } else throw("unsupported argument to TK.element");
    }
    return n;
}
function empty(element) {
    /**
     * Removes all child nodes from an HTMLElement.
     * @param {HTMLElement} element - The element to clean up
     * @function TK.empty
     */
    while (element.lastChild) element.removeChild(element.lastChild);
}
function set_text(element, text) {
    /**
     * Sets a string as new exclusive text node of an HTMLElement.
     * @param {HTMLElement} element - The element to clean up
     * @param {string} text - The string to set as text content
     * @function TK.set_text
     */
    if (element.childNodes.length === 1 && typeof element.childNodes[0].data === "string")
        element.childNodes[0].data = text;
    else
        element.textContent = text;
}
function html(string) {
    /**
     * Returns a documentFragment containing the result of a string parsed as HTML.
     * @param {string} html - A string to parse as HTML
     * @returns {HTMLFragment}
     * @function TK.html
     */
    /* NOTE: setting innerHTML on a document fragment is not supported */
    var e = document.createElement("div");
    var f = document.createDocumentFragment();
    e.innerHTML = string;
    while (e.firstChild) f.appendChild(e.firstChild);
    return f;
}
function set_content(element, content) {
    /**
     * Sets the (exclusive) content of an HTMLElement.
     * @param {HTMLElement} element - The element receiving the content
     * @param {String|HTMLElement} content - A string or HTMLElement to set as content
     * @function TK.set_content
     */
    if (is_dom_node(content)) {
        empty(element);
        if (content.parentNode) {
            TK.warn("set_content: possible reuse of a DOM node. cloning\n");
            content = content.cloneNode(true);
        }
        element.appendChild(content);
    } else {
        set_text(element, content + "");
    }
}
function insert_after(newnode, refnode) {
    /**
     * Inserts one HTMLELement after another in the DOM tree.
     * @param {HTMLElement} newnode - The new node to insert into the DOM tree
     * @param {HTMLElement} refnode - The reference element to add the new element after
     * @function TK.insert_after
     */
    if (refnode.parentNode)
        refnode.parentNode.insertBefore(newnode, refnode.nextSibling);
}
function insert_before(newnode, refnode) {
    /**
     * Inserts one HTMLELement before another in the DOM tree.
     * @param {HTMLElement} newnode - The new node to insert into the DOM tree
     * @param {HTMLElement} refnode - The reference element to add the new element before
     * @function TK.insert_before
     */
    if (refnode.parentNode)
        refnode.parentNode.insertBefore(newnode, refnode);
}
function width() {
    /**
     * Returns the width of the viewport.
     * @returns {number}
     * @function TK.width
     */
    return Math.max(document.documentElement.clientWidth || 0, window.innerWidth || 0, document.body.clientWidth || 0);
}
function height() {
    /**
     * Returns the height of the viewport.
     * @returns {number}
     * @function TK.height
     */
    return Math.max(document.documentElement.clientHeight, window.innerHeight || 0, document.body.clientHeight || 0);
}
function scroll_top(element) {
    /**
     * Returns the amount of CSS pixels the document or an optional element is scrolled from top.
     * @param {HTMLElement} element - The element to evaluate. Optional.
     * @returns {number}
     * @functionTK.scroll_top
     */
    if (element)
        return element.scrollTop;
    return Math.max(document.documentElement.scrollTop || 0, window.pageYOffset || 0, document.body.scrollTop || 0);
}
function scroll_left(element) {
    /**
     * Returns the amount of CSS pixels the document or an optional element is scrolled from left.
     * @param {HTMLElement} element - The element to evaluate. Optional.
     * @returns {number}
     * @functionTK.scroll_left
     */
    if (element)
        return element.scrollLeft;
    return Math.max(document.documentElement.scrollLeft, window.pageXOffset || 0, document.body.scrollLeft || 0);
}
function scroll_all_top(element) {
    /**
     * Returns the sum of CSS pixels an element and all of its parents are scrolled from top.
     * @param {HTMLElement} element - The element to evaluate
     * @returns {number}
     * @functionTK.scroll_all_top
     */
    var v = 0;
    while (element = element.parentNode) v += element.scrollTop || 0;
    return v;
}
function scroll_all_left(element) {
    /**
     * Returns the sum of CSS pixels an element and all of its parents are scrolled from left.
     * @param {HTMLElement} element - The element to evaluate
     * @returns {number}
     * @functionTK.scroll_all_left
     */
    var v = 0;
    while (element = element.parentNode) v += element.scrollLeft || 0;
    return v;
}
function position_top(e, rel) {
    /**
     * Returns the position from top of an element in relation to the document
     * or an optional HTMLElement. Scrolling of the parent is taken into account.
     * @param {HTMLElement} element - The element to evaluate
     * @param {HTMLElement} relation - The element to use as reference. Optional.
     * @returns {number}
     * @function TK.position_top
     */
    var top    = parseInt(e.getBoundingClientRect().top);
    var f  = fixed(e) ? 0 : scroll_top();
    return top + f - (rel ? position_top(rel) : 0);
}
function position_left(e, rel) {
    /**
     * Returns the position from the left of an element in relation to the document
     * or an optional HTMLElement. Scrolling of the parent is taken into account.
     * @param {HTMLElement} element - The element to evaluate
     * @param {HTMLElement} relation - The element to use as reference. Optional.
     * @returns {number}
     * @function TK.position_left
     */
    var left   = parseInt(e.getBoundingClientRect().left);
    var f = fixed(e) ? 0 : scroll_left();
    return left + f - (rel ? position_left(rel) : 0);
}
function fixed(e) {
    /**
     * Returns if an element is positioned fixed to the viewport
     * @param {HTMLElement} element - the element to evaluate
     * @returns {boolean}
     * @function TK.fixed
     */
    return getComputedStyle(e).getPropertyValue("position") === "fixed";
}
function outer_width(element, margin, width) {
    /**
     * Gets or sets the outer width of an element as CSS pixels. The box sizing
     * method is taken into account.
     * @param {HTMLElement} element - the element to evaluate / manipulate
     * @param {boolean} margin - Determine if margin is included
     * @param {number} width - If defined the elements outer width is set to this value
     * @returns {number}
     * @functionTK.outer_width
     */
    var m = 0;
    if (margin) {
        var cs = getComputedStyle(element);
        m += parseFloat(cs.getPropertyValue("margin-left"));
        m += parseFloat(cs.getPropertyValue("margin-right"));
    }
    if (width !== void(0)) {
        if (box_sizing(element) === "content-box") {
            var css = css_space(element, "padding", "border");
            width -= css.left + css.right;
        }
        width -= m;
        // TODO: fixme
        if (width < 0) return 0;
        element.style.width = width + "px";
        return width;
    } else {
        var w = element.getBoundingClientRect().width;
        return w + m;
    }
}
function outer_height(element, margin, height) {
    /**
     * Gets or sets the outer height of an element as CSS pixels. The box sizing
     * method is taken into account.
     * @param {HTMLElement} element - the element to evaluate / manipulate
     * @param {boolean} margin - Determine if margin is included
     * @param {number} height - If defined the elements outer height is set to this value
     * @returns {number}
     * @functionTK.outer_height
     */
    var m = 0;
    if (margin) {
        var cs = getComputedStyle(element, null);
        m += parseFloat(cs.getPropertyValue("margin-top"));
        m += parseFloat(cs.getPropertyValue("margin-bottom"));
    }
    if (height !== void(0)) {
        if (box_sizing(element) === "content-box") {
            var css = css_space(element, "padding", "border");
            height -= css.top + css.bottom;
        }
        height -= m;
        // TODO: fixme
        if (height < 0) return 0;
        element.style.height = height + "px";
        return height;
    } else {
        var h = element.getBoundingClientRect().height;
        return h + m;
    }
}
function inner_width(element, width) {
    /**
     * Gets or sets the inner width of an element as CSS pixels. The box sizing
     * method is taken into account.
     * @param {HTMLElement} element - the element to evaluate / manipulate
     * @param {number} width - If defined the elements inner width is set to this value
     * @returns {number}
     * @functionTK.inner_width
     */
    var css = css_space(element, "padding", "border");
    var x = css.left + css.right;
    if (width !== void(0)) {
        if (box_sizing(element) === "border-box")
            width += x;
        // TODO: fixme
        if (width < 0) return 0;
        element.style.width = width + "px";
        return width;
    } else {
        var w = element.getBoundingClientRect().width;
        return w - x;
    }
}
function inner_height(element, height) {
    /**
     * Gets or sets the inner height of an element as CSS pixels. The box sizing
     * method is taken into account.
     * @param {HTMLElement} element - the element to evaluate / manipulate
     * @param {number} height - If defined the elements outer height is set to this value
     * @returns {number}
     * @functionTK.inner_height
     */
    var css = css_space(element, "padding", "border");
    var y = css.top + css.bottom;
    if (height !== void(0)) {
        if (box_sizing(element) === "border-box")
            height += y;
        // TODO: fixme
        if (height < 0) return 0;
        element.style.height = height + "px";
        return height;
    } else {
        var h = element.getBoundingClientRect().height;
        return h - y;
    }
}
function box_sizing(element) {
    /**
     * Returns the box-sizing method of an HTMLElement.
     * @param {HTMLElement} element - The element to evaluate
     * @returns {string}
     * @functionTK.box_sizing
     */
    var cs = getComputedStyle(element, null);
    if (cs.getPropertyValue("box-sizing")) return cs.getPropertyValue("box-sizing");
    if (cs.getPropertyValue("-moz-box-sizing")) return cs.getPropertyValue("-moz-box-sizing");
    if (cs.getPropertyValue("-webkit-box-sizing")) return cs.getPropertyValue("-webkit-box-sizing");
    if (cs.getPropertyValue("-ms-box-sizing")) return cs.getPropertyValue("-ms-box-sizing");
    if (cs.getPropertyValue("-khtml-box-sizing")) return cs.getPropertyValue("-khtml-box-sizing");
}
function css_space(element) {
    /**
     * Returns the overall spacing around an HTMLElement of all given attributes.
     * @param {HTMLElement} element - The element to evaluate
     * @param{...string} The CSS attributes to take into account
     * @returns {object} An object with the members "top", "bottom", "lfet", "right"
     * @function TK.css_space
     * @example
     * TK.css_space(element, "padding", "border");
     */
    var cs = getComputedStyle(element, null);
    var o = {top: 0, right: 0, bottom: 0, left: 0};
    var a;
    var s;
    for (var i = 1; i < arguments.length; i++) {
        a = arguments[i];
        for (var p in o) {
            if (o.hasOwnProperty(p)) {
                s = a + "-" + p;
                if (a === "border") s += "-width";
            }
            o[p] += parseFloat(cs.getPropertyValue(s));
        }
    }
    return o;
}

var number_attributes = [
    "animation-iteration-count",
    "column-count",
    "flex-grow",
    "flex-shrink",
    "opacity",
    "order",
    "z-index"
]
    
function set_styles(elem, styles) {
    /**
     * Set multiple CSS styles onto an HTMLElement.
     * @param {HTMLElement} element - the element to add the styles to
     * @param {object} styles - A mapping containing all styles to add
     * @function TK.set_styles
     * @example
     * TK.set_styles(element, {"width":"100px", "height":"100px"});
     */
    var key, v;
    var s = elem.style;
    for (key in styles) if (styles.hasOwnProperty(key)) {
        v = styles[key];
        if (typeof v !== "number" && !v) {
            delete s[key];
        } else {
            if (typeof v === "number" && number_attributes.indexOf(key) == -1) {
                TK.warn("TK.set_styles: use of implicit px conversion is _deprecated_ and will be removed in the future.");
                v = v.toFixed(3) + "px";
            }
            s[key] = v;
        }
    }
}
function set_style(e, style, value) {
    /**
     * Sets a single CSS style onto an HTMLElement. It is used to autimatically
     * add "px" to numbers and trim them to 3 digits at max. DEPRECATED!
     * @param {HTMLElement} element - The element to set the style to
     * @param {string} style - The CSS attribute to set
     * @param {string|number} value - The value to set the CSS attribute to
     * @function TK.set_style
     */
    if (typeof value === "number") {
        /* By default, numbers are transformed to px. I believe this is a very _dangerous_ default
         * behavior, because it breaks other number like properties _without_ warning.
         * this is now deprecated. */
        TK.warn("TK.set_style: use of implicit px conversion is _deprecated_ and will be removed in the future.");
        value = value.toFixed(3) + "px";
    }
    e.style[style] = value;
}
var _id_cnt = 0;
function unique_id() {
    /**
     * Generate a unique ID string.
     * @returns {string}
     * @function TK.unique_id
     */
    var id;
    do { id = "tk-" + _id_cnt++; } while (document.getElementById(id));
    return id;
};

/**
 * Generates formatting functions from sprintf-style format strings.
 * This is generally faster when the same format string is used many times.
 *
 * @returns {function} A formatting function.
 * @param {string} fmt - The format string.
 * @function TK.FORMAT
 * @example
 * var f = TK.FORMAT("%.2f Hz");
 * @see TK.sprintf
 */
function FORMAT(fmt) {
    var args = [];
    var s = "return ";
    var res;
    var last = 0;
    var argnum = 0;
    var precision;
    var regexp = /%(\.\d+)?([bcdefgosO%])/g;
    var argname;

    while (res = regexp.exec(fmt)) {
        if (argnum) s += "+";
        s += JSON.stringify(fmt.substr(last, regexp.lastIndex - res[0].length - last));
        s += "+";
        argname = "a"+argnum;
        if (args.indexOf(argname) === -1)
            args.push(argname);
        if (argnum+1 < arguments.length) {
            argname = "(" + sprintf(arguments[argnum+1].replace("%", "%s"), argname) + ")";
        }
        switch (res[2].charCodeAt(0)) {
        case 100: // d
            s += "("+argname+" | 0)";
            break;
        case 102: // f
            if (res[1]) { // length qualifier
                precision = parseInt(res[1].substr(1));
                s += "(+"+argname+").toFixed("+precision+")";
            } else {
                s += "(+"+argname+")";
            }
            break;
        case 115: // s
            s += argname;
            break;
        case 37:
            s += "\"%\"";
            argnum--;
            break;
        case 79:
        case 111:
            s += "JSON.stringify("+argname+")";
            break;
        default:
            throw("unknown format:"+res[0]);
            break;
        }
        argnum++;
        last = regexp.lastIndex;
    }

    if (argnum) s += "+";
    s += JSON.stringify(fmt.substr(last));

    return new Function(args, s);
}

/**
 * Formats the arguments according to a given format string.
 * @returns {function} A formatting function.
 * @param {string} fmt - The format string.
 * @param {...*} args - The format arguments.
 * @function TK.sprintf
 * @example
 * TK.sprintf("%d Hz", 440);
 * @see TK.FORMAT
 */
function sprintf(fmt) {
    var arg_len = arguments.length;
    var i, last_fmt;
    var c, arg_num = 1;
    var ret = [];
    var precision, s;
    var has_precision = false;

    for (last_fmt = 0; -1 !== (i = fmt.indexOf("%", last_fmt)); last_fmt = i+1) {
        if (last_fmt < i) {
            ret.push(fmt.substring(last_fmt, i));
        }

        i ++;

        if (has_precision = (fmt.charCodeAt(i) === 46 /* '.' */)) {
            i++;
            precision = parseInt(fmt.substr(i));
            while ((c = fmt.charCodeAt(i)) >= 48 && c <= 57) i++;
        }

        c = fmt.charCodeAt(i);

        if (c === 37) {
            ret.push("%");
            continue;
        }

        s = arguments[arg_num++];

        switch (fmt.charCodeAt(i)) {
        case 102: /* f */
            s = +s;
            if (has_precision) {
                s = s.toFixed(precision);
            }
            break;
        case 100: /* d */
            s = s|0;
            break;
        case 115: /* s */
            break;
        case 79: /* O */
        case 111: /* o */
            s = JSON.stringify(s);
            break;
        default:
            throw("Unsupported format.");
        }

        ret.push(s);

        last_fmt = i+1;
    }

    if (last_fmt < fmt.length) {
        ret.push(fmt.substring(last_fmt, fmt.length));
    }

    return ret.join("");
}

function escapeHTML(text) {
    /**
     * Escape an HTML string to be displayed as text.
     * @param {string} html - The HTML code to escape
     * @returns {string}
     * @function TK.escapeHTML
     */
    var map = {
        '&' : '&amp;',
        '<' : '&lt;',
        '>' : '&gt;',
        '"' : '&quot;',
        "'" : '&#039;'
    };
    return text.replace(/[&<>"']/g, function(m) { return map[m]; });
}

function is_touch() {
    /**
     * Check if a device is touch-enabled.
     * @returns {boolean}
     * @function TK.is_touch
     */
    return 'ontouchstart' in window // works on most browsers
      || 'onmsgesturechange' in window; // works on ie10
}
function os() {
    /**
     * Return the operating system
     * @returns {string}
     * @function TK.os
     */
    var ua = navigator.userAgent.toLowerCase();
    if (ua.indexOf("android") > -1)
        return "Android";
    if (/iPad/i.test(ua) || /iPhone OS 3_1_2/i.test(ua) || /iPhone OS 3_2_2/i.test(ua))
        return "iOS";
    if ((ua.match(/iPhone/i)) || (ua.match(/iPod/i)))
        return "iOS";
    if (navigator.appVersion.indexOf("Win")!=-1)
        return "Windows";
    if (navigator.appVersion.indexOf("Mac")!=-1)
        return "MacOS";
    if (navigator.appVersion.indexOf("X11")!=-1)
        return "UNIX";
    if (navigator.appVersion.indexOf("Linux")!=-1)
        return "Linux";
}
function make_svg(tag, args) {
    /**
     * Creates and returns an SVG child element.
     * @param {string} tag - The element to create as string, e.g. "line" or "g"
     * @param {object} arguments - The attributes to set onto the element
     * @returns {SVGElement}
     */
    var el = document.createElementNS('http://www.w3.org/2000/svg', "svg:" + tag);
    for (var k in args)
        el.setAttribute(k, args[k]);
    return el;
}
function seat_all_svg(parent) {
    /**
     * Searches for all SVG that don't have the class "svg-fixed" and re-positions them
     * in order to avoid blurry lines.
     * @param {HTMLElement} parent - If set only children of parent are searched
     * @function TK.seat_all_svg
     */
    var a = get_tag("svg", parent);
    for (var i = 0; i < a.length; i++) {
        if (!has_class(a[i], "svg-fixed"))
            seat_svg(a[i]);
    }
}
function seat_svg(e) {
    /**
     * Move SVG for some sub-pixel if their position in viewport is not int.
     * @param {SVGElement} svg - The SVG to manipulate
     * @function TK.seat_svg
     */
    if (retrieve(e, "margin-left") === null) {
        store(e, "margin-left", parseFloat(get_style(e, "margin-left")));
    } else {
        e.style.marginLeft = retrieve(e, "margin-left") || 0;
    }
    var l = parseFloat(retrieve(e, "margin-left")) || 0;
    var b = e.getBoundingClientRect();
    var x = b.left % 1;
    if (x) {
        if (x < 0.5) l -= x;
        else l += (1 - x);
    }
    if (e.parentElement && get_style(e.parentElement, "text-align") === "center")
        l += 0.5;
    e.style.marginLeft = l + "px";
    //console.log(l);
    if (retrieve(e, "margin-top") === null) {
        store(e, "margin-top", parseFloat(get_style(e, "margin-top")));
    } else {
        e.style.marginTop = retrieve(e, "margin-top") || 0;
    }
    var t = parseFloat(retrieve(e, "margin-top") || 0);
    var b = e.getBoundingClientRect();
    var y = b.top % 1;
    if (y) {
        if (x < 0.5) t -= y;
        else t += (1 - y);
    }
    //console.log(t);
    e.style.marginTop = t + "px";
}
function delayed_callback(timeout, cb, once) {
    var tid;
    var args;

    var my_cb = function() {
        tid = null;
        cb.apply(this, args);
    };
    return function() {
        args = Array.prototype.slice.call(arguments);

        if (tid)
            window.clearTimeout(tid);
        else if (once) once();
        tid = window.setTimeout(my_cb, timeout);
    };
}

function store(e, key, val) {
    /**
     * Store a piece of data in an object.
     * @param {object} object - The object to store the data
     * @param {string} key - The key to identify the memory
     * @param {*} data - The data to store
     * @function TK.store
     */
    data(e)[key] = val;
}
function retrieve(e, key) {
    /**
     * Retrieve a piece of data from an object.
     * @param {object} object - The object to retrieve the data from
     * @param {string} key - The key to identify the memory
     * @function TK.retrieve
     * @returns {*}
     */
    return data(e)[key];
}
function merge(dst) {
    /**
     * Merge two or more objects. The second and all following objects
     * will be merged into the first one.
     * @param {...object} object - The objects to merge
     * @returns {object}
     * @function TK.merge
     */
    //console.log("merging", src, "into", dst);
    var key, i, src;
    for (i = 1; i < arguments.length; i++) {
        src = arguments[i];
        for (key in src) {
            dst[key] = src[key];
        }
    }
    return dst;
}
function object_and(orig, filter) {
    /**
     * Filter an object via white list.
     * @param {object} origin - The object to filter
     * @param {object} filter - The object containing the white list
     * @returns {object} The filtered result
     * @function TK.object_and
     */
    var ret = {};
    for (var key in orig) {
        if (filter[key]) ret[key] = orig[key];
    }
    return ret;
}
function object_sub(orig, filter) {
    /**
     * Filter an object via black list.
     * @param {object} origin - The object to filter
     * @param {object} filter - The object containing the black list
     * @returns {object} The filtered result
     * @function TK.object_sub
     */
    var ret = {};
    for (var key in orig) {
        if (!filter[key]) ret[key] = orig[key];
    }
    return ret;
}
function to_array(collection) {
    /**
     * Convert any collection (like NodeList) into an array.
     * @param {collection} collection - The collection to convert into an array
     * @returns {array}
     * @functionTK.to_array
     */
    var ret = new Array(collection.length);
    var i;

    for (i = 0; i < ret.length; i++) {
        ret[i] = collection[i];
    }

    return ret;
}
function is_dom_node(o) {
    /* this is broken for SVG */
    return typeof o === "object" && o instanceof Node;
}

// NOTE: IE9 will throw errors when console is used without debugging tools. In general, it
// is better for log/warn to silently fail in case of error. This unfortunately means that
// warnings might be lost, but probably better than having diagnostics and debugging code
// break an application

/**
 * Generates an error to the JavaScript console. This is virtually identical to console.error, however
 * it can safely be used in browsers which do not support it.
 *
 * @param {...*} args
 * @function TK.error
 */
function error() {
    if (!window.console) return;
    try {
        window.console.error.apply(window.console, arguments);
    } catch(e) {}
}

/**
 * Generates a warning to the JavaScript console. This is virtually identical to console.warn, however
 * it can safely be used in browsers which do not support it.
 *
 * @param {...*} args
 * @function TK.warn
 */
function warn() {
    if (!window.console) return;
    try {
        window.console.warn.apply(window.console, arguments);
    } catch(e) {}
}
/**
 * Generates a log message to the JavaScript console. This is virtually identical to console.log, however
 * it can safely be used in browsers which do not support it.
 *
 * @param {...*} args
 * @function TK.log
 */
function log() {
    if (!window.console) return;
    try {
        window.console.log.apply(window.console, arguments);
    } catch(e) {}
}

function print_widget_tree(w, depth) {
  if (!depth) depth = 0;

  var print = function(fmt) {
    var extra = Array.prototype.slice.call(arguments, 1);
    if (depth) fmt = nchars(depth, " ") + fmt;
    var args = [ fmt ];
    log.apply(TK, args.concat(extra));
  };

  var nchars = function(n, c) {
    var ret = new Array(n);

    for (var i = 0; i < n; i++) ret[i] = c;

    return ret.join("");
  };

  var C = w.children;
  var nchildren = C ? C.length : 0;

  var state = [ ];

  state.push(w._drawn ? "show" : "hide");

  if (w.needs_redraw) state.push("redraw");
  if (w.needs_resize) state.push("resize");


  print("%s (%s, children: %o)", w._class, state.join(" "), nchildren);

  if (C) {
    for (var i = 0; i < C.length; i++) print_widget_tree(C[i], depth+1);
  }
}

/* Detection and handling for passive event handler support.
 * The chrome team has threatened to make passive event handlers
 * the default in a future version. To make sure that this does
 * not break our code, we explicitly register 'active' event handlers
 * for most cases.
 */

/* generic code, supports node arrays */
function add_event_listener(e, type, cb, options) {
    if (Array.isArray(e)) {
        for (var i = 0; i < e.length; i++)
            e[i].addEventListener(type, cb, options);
    } else e.addEventListener(type, cb, options);
}
function remove_event_listener(e, type, cb, options) {
    if (Array.isArray(e)) {
        for (var i = 0; i < e.length; i++)
            e[i].removeEventListener(type, cb, options);
    } else e.removeEventListener(type, cb, options);
}

/* Detect if the 'passive' option is supported.
 * This code has been borrowed from mdn */
var passiveSupported = false;

try {
  var options = Object.defineProperty({}, "passive", {
    get: function() {
      passiveSupported = true;
    }
  });

  window.addEventListener("test", null, options);
  window.removeEventListener("test", null);
} catch(err) {}

var active_options, passive_options;

if (passiveSupported) {
  active_options = { passive: false };
  passive_options = { passive: true };
} else {
  active_options = false;
  passive_options = false;
}

function add_active_event_listener(e, type, cb) {
  add_event_listener(e, type, cb, active_options);
}
function remove_active_event_listener(e, type, cb) {
  remove_event_listener(e, type, cb, active_options);
}
function add_passive_event_listener(e, type, cb) {
  add_event_listener(e, type, cb, passive_options);
}
function remove_passive_event_listener(e, type, cb) {
  remove_event_listener(e, type, cb, passive_options);
}

TK = w.toolkit = w.TK = {
    // ELEMENTS
    S: new w.DOMScheduler(),
    is_dom_node: is_dom_node,
    get_id: get_id,
    get_class: get_class,
    get_tag: get_tag,
    element : element,
    empty: empty,
    set_text : set_text,
    set_content : set_content,
    has_class : has_class,
    remove_class : remove_class,
    add_class : add_class,
    toggle_class : toggle_class,
    is_class_name : is_class_name,

    insert_after: insert_after,
    insert_before: insert_before,

    // WINDOW

    width: width,
    height: height,

    // DIMENSIONS

    scroll_top: scroll_top,
    scroll_left: scroll_left,
    scroll_all_top: scroll_all_top,
    scroll_all_left: scroll_all_left,

    position_top: position_top,
    position_left: position_left,

    fixed: fixed,

    outer_width : outer_width,

    outer_height : outer_height,

    inner_width: inner_width,

    inner_height: inner_height,

    box_sizing: box_sizing,

    css_space: css_space,

    // CSS AND CLASSES

    set_styles : set_styles,
    set_style: set_style,
    get_style: get_style,
    get_duration: get_duration,

    // STRINGS

    unique_id: unique_id,

    FORMAT : FORMAT,

    sprintf : sprintf,
    html : html,

    escapeHTML : escapeHTML,

    // OS AND BROWSER CAPABILITIES

    is_touch: is_touch,
    os: os,

    browser: function () {
        /**
         * Returns the name of the browser
         * @returns {string}
         * @function TK.browser
         */
        var ua = navigator.userAgent, tem, M = ua.match(/(opera|chrome|safari|firefox|msie|trident(?=\/))\/?\s*(\d+)/i) || [];
        if (/trident/i.test(M[1])) {
            tem = /\brv[ :]+(\d+)/g.exec(ua) || [];
            return { name : 'IE', version : (tem[1]||'') };
        }
        if (M[1] === 'Chrome') {
            tem = ua.match(/\bOPR\/(\d+)/)
            if (tem!=null)
                return { name : 'Opera', version : tem[1] };
        }
        M = M[2] ? [M[1], M[2]] : [navigator.appName, navigator.appVersion, '-?'];
        if ((tem = ua.match(/version\/(\d+)/i)) !== null) { M.splice(1, 1, tem[1]); }
        return { name : M[0], version : M[1] };
    }(),
    
    supports_transform: function () { return 'transform' in document.createElement("div").style; }(),

    // SVG

    make_svg: make_svg,
    seat_all_svg: seat_all_svg,
    seat_svg: seat_svg,


    // EVENTS

    delayed_callback : delayed_callback,
    add_event_listener: add_active_event_listener,
    remove_event_listener: remove_active_event_listener,
    add_passive_event_listener: add_passive_event_listener,
    remove_passive_event_listener: remove_passive_event_listener,

    // OTHER

    data: data,
    store: store,
    retrieve: retrieve,
    merge: merge,
    object_and: object_and,
    object_sub: object_sub,
    to_array: to_array,
    warn: warn,
    error: error,
    log: log,
    assign_warn: function(a) {
        for (var i = 1; i < arguments.length; i++) {
            var b = arguments[i];
            for (var key in b) if (b.hasOwnProperty(key)) {
                if (a[key] === b[key]) {
                    TK.warn("overwriting identical", key, "(", a[key], ")");
                } else if (a[key]) {
                    TK.warn("overwriting", key, "(", a[key], "vs", b[key], ")");
                }
                a[key] = b[key];
            }
        }

        return a;
    },
    print_widget_tree: print_widget_tree,
};

// POLYFILLS

if (Array.isArray === void(0)) {
    Array.isArray = function(obj) {
        return Object.prototype.toString.call(obj) === '[object Array]';
    }
};

if (Object.assign === void(0)) {
  Object.defineProperty(Object, 'assign', {
    enumerable: false,
    configurable: true,
    writable: true,
    value: function(target) {
      'use strict';
      if (target === void(0) || target === null) {
        throw new TypeError('Cannot convert first argument to object');
      }

      var to = Object(target);
      for (var i = 1; i < arguments.length; i++) {
        var nextSource = arguments[i];
        if (nextSource === void(0) || nextSource === null) {
          continue;
        }
        nextSource = Object(nextSource);

        var keysArray = Object.keys(Object(nextSource));
        for (var nextIndex = 0, len = keysArray.length; nextIndex < len; nextIndex++) {
          var nextKey = keysArray[nextIndex];
          var desc = Object.getOwnPropertyDescriptor(nextSource, nextKey);
          if (desc !== void(0) && desc.enumerable) {
            to[nextKey] = nextSource[nextKey];
          }
        }
      }
      return to;
    }
  });
}

if (!('remove' in Element.prototype)) {
    Element.prototype.remove = function() {
        this.parentNode.removeChild(this);
    };
}
})(this);
