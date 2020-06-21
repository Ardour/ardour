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
(function(w, TK) {
var merge = function(dst) {
    var key, i, src;
    for (i = 1; i < arguments.length; i++) {
        src = arguments[i];
        for (key in src) {
            dst[key] = src[key];
        }
    }
    return dst;
};
var mixin = function(dst, src) {
    var fun, key;
    for (key in src) if (!dst[key]) {
        if (key === "constructor" ||
            key === "_class" ||
            key === "Extends" ||
            key === "Implements" ||
            key === "options") continue;
        if (!src.hasOwnProperty(key)) continue;

        fun = src[key];

        dst[key] = fun;
    }

    return dst;
};
function call_handler(self, fun, args) {
    try {
        return fun.apply(self, args);
    } catch (e) {
        TK.warn("event handler", fun, "threw", e);
    }
}
function dispatch_events(self, handlers, args) {
    var v;
    if (Array.isArray(handlers)) {
        for (var i = 0; i < handlers.length; i++) {
            v = call_handler(self, handlers[i], args);
            if (v !== void(0)) return v;
        }
    } else return call_handler(self, handlers, args);
}
function add_event(to, event, fun) {
    var tmp = to[event];

    if (!tmp) {
        to[event] = fun;
    } else if (Array.isArray(tmp)) {
        tmp.push(fun);
    } else {
        to[event] = [ tmp, fun ];
    }
}
function remove_event(from, event, fun) {
    var tmp = from[event];
    if (!tmp) return;
    if (Array.isArray(tmp)) {
        for (var i = 0; i < tmp.length; i++) {
            if (tmp[i] === fun) {
                tmp[i] = tmp[tmp.length-1];
                tmp.length --;
            }
        }
        if (tmp.length === 1) from[event] = tmp[0];
        else if (tmp.length === 0) delete from[event];
    } else if (tmp === fun) {
        delete from[event];
    }
}
function add_static_event(w, event, fun) {
    var p = w.prototype, e;
    if (!p.hasOwnProperty('static_events')) {
        if (p.static_events) {
            p.static_events = e = Object.assign({}, p.static_events);
        } else {
            p.static_events = e = {};
        }
    } else e = p.static_events;
    add_event(e, event, fun);
}
function arrayify(x) {
    if (!Array.isArray(x)) x = [ x ];
    return x;
}
function merge_static_events(a, b) {
    var event;
    if (!a) return b;
    if (!b) return Object.assign({}, a);
    for (event in a) {
        var tmp = a[event];
        if (b.hasOwnProperty(event)) {
            b[event] = arrayify(tmp).concat(arrayify(b[event]));
        } else {
            b[event] = Array.isArray(tmp) ? tmp.slice(0) : tmp;
        }
    }
    return Object.assign({}, a, b);
}
TK.class = function(o) {
    var constructor;
    var methods;
    var tmp, i, c, key;

    if (tmp = o.Extends) {
        if (typeof(tmp) === "function") {
            tmp = tmp.prototype;
        }
        if (typeof(o.options) === "object" &&
            typeof(tmp.options) === "object") {
            o.options = Object.assign(Object.create(tmp.options), o.options);
        }
        if (o.static_events) o.static_events = merge_static_events(tmp.static_events, o.static_events);
        methods = Object.assign(Object.create(tmp), o);
    } else {
        methods = o;
    }

    tmp = o.Implements;
    // mixins
    if (tmp !== void(0)) {
        tmp = arrayify(tmp);
        for (i = 0; i < tmp.length; i++) {
            if (typeof(tmp[i]) === "function") {
                c = tmp[i].prototype;
            } else c = tmp[i];

            if (typeof(c.options) === "object") {
                if (!methods.hasOwnProperty("options")) {
                    methods.options = Object.create(methods.options || null);
                }
                methods.options = merge({}, c.options, methods.options);
            }
            if (c.static_events) {
              if (methods.static_events) {
                methods.static_events = merge_static_events(c.static_events,
                                                            Object.assign({}, methods.static_events));
              } else {
                methods.static_events = c.static_events;
              }
            }

            methods = mixin(methods, c, true);
        }
    }

    var init = methods.initialize;
    var post_init = methods.initialized;

    if (post_init) {
        constructor = function() {
            init.apply(this, arguments);
            post_init.call(this);
        };
    } else constructor = init || (function() {});

    constructor.prototype = methods;
    methods.constructor = constructor;
    return constructor;
};
var __native_events = {
    // mouse
    mouseenter : true,
    mouseleave : true,
    mousedown  : true,
    mouseup    : true,
    mousemove  : true,
    mouseover  : true,

    click      : true,
    dblclick   : true,

    startdrag  : true,
    stopdrag   : true,
    drag       : true,
    dragenter  : true,
    dragleave  : true,
    dragover   : true,
    drop       : true,
    dragend    : true,

    // touch
    touchstart : true,
    touchend   : true,
    touchmove  : true,
    touchenter : true,
    touchleave : true,
    touchcancel: true,

    keydown    : true,
    keypress   : true,
    keyup      : true,
    scroll     : true,
    focus      : true,
    blur       : true,

    // mousewheel
    mousewheel : true,
    DOMMouseScroll : true,
    wheel : true,

    submit     : true,
    contextmenu: true,
};
function is_native_event(type) {
    return __native_events[type];
}

function remove_native_events(element) {
    var type;
    var s = this.static_events;
    var d = this.__events;
    var handler = this.__native_handler;

    for (type in s) if (is_native_event(type))
        TK.remove_event_listener(element, type, handler);

    for (type in d) if (is_native_event(type) && (!s || !s.hasOwnProperty(type)))
        TK.remove_event_listener(element, type, handler);
}
function add_native_events(element) {
    var type;
    var s = this.static_events;
    var d = this.__events;
    var handler = this.__native_handler;

    for (type in s) if (is_native_event(type))
        TK.add_event_listener(element, type, handler);

    for (type in d) if (is_native_event(type) && (!s || !s.hasOwnProperty(type)))
        TK.add_event_listener(element, type, handler);
}
function native_handler(ev) {
    /* FIXME:
     * * mouseover and error are cancelled with true
     * * beforeunload is cancelled with null
     */
    if (this.fire_event(ev.type, ev) === false) return false;
}
function has_event_listeners(event) {
    var ev = this.__events;

    if (ev.hasOwnProperty(event)) return true;

    ev = this.static_events;

    return ev && ev.hasOwnProperty(event);
}
/**
 * This is the base class for all widgets in toolkit.
 * It provides an API for event handling and other basic implementations.
 *
 * @class TK.Base
 */
TK.Base = TK.class({
    initialize : function(options) {
        this.__events = {};
        this.__event_target = null;
        this.__native_handler = native_handler.bind(this);
        this.set_options(options);
        this.fire_event("initialize");
    },
    initialized : function() {
        /**
         * Is fired when an instance is initialized.
         * 
         * @event TK.Base#initialized
         */
        this.fire_event("initialized");
    },
    /**
     * Destroys all event handlers and the options object.
     *
     * @method TK.Base#destroy
     */
    destroy : function() {
        if (this.__event_target) {
            remove_native_events.call(this, this.__event_target, this.__events);
        }

        this.__events = null;
        this.__event_target = null;
        this.__native_handler = null;
        this.options = null;
    },
    /**
     * Merges a new options object into the existing one,
     * including deep copies of objects. If an option key begins with
     * the string "on" it is considered as event handler. In this case
     * the value should be the handler function for the event with
     * the corresponding name without the first "on" characters.
     *
     * @method TK.Base#set_options(options)
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     */
    set_options : function(o) {
        var opt = this.options;
        var key, a, b;
        if (typeof(o) !== "object") {
            delete this.options;
            o = {};
        } else if (typeof(opt) === "object") for (key in o) if (o.hasOwnProperty(key)) {
            a = o[key];
            b = opt[key];
            if (typeof a === "object" && a &&
                Object.getPrototypeOf(Object.getPrototypeOf(a)) === null &&
                typeof b === "object" && b &&
                Object.getPrototypeOf(Object.getPrototypeOf(b)) === null
                ) {
                o[key] = merge({}, b, a);
            }
        }
        if (this.hasOwnProperty("options")) {
            this.options = merge(opt, o);
        } else if (opt) {
            this.options = Object.assign(Object.create(opt), o);
        } else {
            this.options = Object.assign({}, o);
        }
        for (key in this.options) if (key.startsWith("on")) {
            this.add_event(key.substr(2).toLowerCase(), this.options[key]);
            delete this.options[key];
        }
    },
    /**
     * Get the value of an option.
     *
     * @method TK.Base#get
     * 
     * @param {string} key - The option name.
     */
    get: function (key) {
        return this.options[key];
    },
    /**
     * Sets an option. Fires both the events <code>set</code> with arguments <code>key</code>
     * and <code>value</code>; and the event <code>'set_'+key</code> with arguments <code>value</code>
     * and <code>key</code>.
     *
     * @method TK.Base#set
     * 
     * @param {string} key - The name of the option.
     * @param {mixed} value - The value of the option.
     * 
     * @emits TK.Base#set
     * @emits TK.Base#set_[option]
     */
    set: function (key, value) {
        var e;

        this.options[key] = value;
        /**
         * Is fired when an option is set.
         * 
         * @event TK.Base#set
         * 
         * @param {string} name - The name of the option.
         * @param {mixed} value - The value of the option.
         */
        if (this.has_event_listeners("set"))
            this.fire_event("set", key, value);
        /**
         * Is fired when an option is set.
         * 
         * @event TK.Base#set_[option]
         * 
         * @param {mixed} value - The value of the option.
         */
        e = "set_"+key;
        if (this.has_event_listeners(e))
            this.fire_event(e, value, key);

        return value;
    },
    /**
     * Sets an option by user interaction. Emits the <code>userset</code>
     * event. The <code>userset</code> event can be cancelled (if an event handler
     * returns <code>false</code>), in which case the option is not set.
     * Returns <code>true</code> if the option was set, <code>false</code>
     * otherwise. If the option was set, it will emit a <code>useraction</code> event.
     *
     * @method TK.Base#userset
     * 
     * @param {string} key - The name of the option.
     * @param {mixed} value - The value of the option.
     *
     * @emits TK.Base#userset
     * @emits TK.Base#useraction
     */
    userset: function(key, value) {
        if (false === this.fire_event("userset", key, value)) return false;
        value = this.set(key, value);
        this.fire_event("useraction", key, value);
        return true;
    },
    /**
     * Delegates all occuring DOM events of a specific DOM node to the widget.
     * This way the widget fires e.g. a click event if someone clicks on the
     * given DOM node.
     * 
     * @method TK.Base#delegate_events
     * 
     * @param {HTMLElement} element - The element all native events of the widget should be bound to.
     * 
     * @returns {HTMLElement} The element
     * 
     * @emits TK.Base#delegated
     */
    delegate_events: function (element) {
        var old_target = this.__event_target;
        /**
         * Is fired when an element is delegated.
         * 
         * @event TK.Base#delegated
         * 
         * @param {HTMLElement|Array} element - The element which receives all
         *      native DOM events.
         * @param {HTMLElement|Array} old_element - The element which previously
         *      received all native DOM events.
         */
        this.fire_event("delegated", element, old_target);

        if (old_target) remove_native_events.call(this, old_target);
        if (element) add_native_events.call(this, element);

        this.__event_target = element;

        return element;
    },
    /**
     * Register an event handler.
     *
     * @method TK.Base#add_event
     * 
     * @param {string} event - The event descriptor.
     * @param {Function} func - The function to call when the event happens.
     * @param {boolean} prevent - Set to true if the event should prevent the default behavior.
     * @param {boolean} stop - Set to true if the event should stop bubbling up the tree.
     */
    add_event: function (event, func) {
        var ev, tmp;

        if (typeof event !== "string")
            throw new TypeError("Expected string.");

        if (typeof func !== "function")
            throw new TypeError("Expected function.");

        if (arguments.length !== 2)
            throw new Error("Bad number of arguments.");

        if (is_native_event(event) && (ev = this.__event_target) && !this.has_event_listeners(event))
            TK.add_event_listener(ev, event, this.__native_handler);
        ev = this.__events;
        add_event(ev, event, func);
    },
    /**
     * Removes the given function from the event queue.
     * If it is a native DOM event, it removes the DOM event listener
     * as well.
     *
     * @method TK.Base#remove_event
     * 
     * @param {string} event - The event descriptor.
     * @param {Function} fun - The function to remove.
     */
    remove_event: function (event, fun) {
        remove_event(this.__events, event, fun);

        // remove native DOM event listener from __event_target
        if (is_native_event(event) && !this.has_event_listeners(event)) {
            var ev = this.__event_target;
            if (ev) TK.remove_event_listener(ev, event, this.__native_handler);
        }
    },
    /**
     * Fires an event.
     *
     * @method TK.Base#fire_event
     * 
     * @param {string} event - The event descriptor.
     * @param {...*} args - Event arguments.
     */
    fire_event: function (event) {
        var ev;
        var args;
        var v;

        ev = this.__events;

        if (ev !== void(0) && (event in ev)) {
            ev = ev[event];

            args = Array.prototype.slice.call(arguments, 1);

            v = dispatch_events(this, ev, args);
            if (v !== void(0)) return v;
        }

        ev = this.static_events;

        if (ev !== void(0) && (event in ev)) {
            ev = ev[event];

            if (args === void(0)) args = Array.prototype.slice.call(arguments, 1);

            v = dispatch_events(this, ev, args);
            if (v !== void(0)) return v;
        }
    },
    /**
     * Test if the event descriptor has some handler functions in the queue.
     *
     * @method TK.Base#has_event_listeners
     * 
     * @param {string} event - The event desriptor.
     * 
     * @returns {boolean} True if the event has some handler functions in the queue, false if not.
     */
    has_event_listeners: has_event_listeners,
    /**
     * Add multiple event handlers at once, either as dedicated event handlers or a list of event
     * descriptors with a single handler function.
     *
     * @method TK.Base#add_events
     * 
     * @param {Object | Array} events - Object with event descriptors as keys and functions as
     *   values or Array of event descriptors. The latter requires a handler function as the
     *   second argument.
     * @param {Function} func - A function to add as event handler if the first argument is an
     *   array of event desriptors.
     */
    add_events: function (events, func) {
        var i;
        if (Array.isArray(events)) {
            for (i = 0; i < events.length; i++)
                this.add_event(events[i], func);
        } else {
            for (i in events) 
                if (events.hasOwnProperty(i))
                    this.add_event(i, events[i]);
        }
    },
    /**
     * Remove multiple event handlers at once, either as dedicated event handlers or a list of
     * event descriptors with a single handler function.
     *
     * @method TK.Base#remove_events
     * 
     * @param {Object | Array} events - Object with event descriptors as keys and functions as
     *   values or Array of event descriptors. The latter requires the handler function as the
     *   second argument.
     * @param {Function} func - A function to remove from event handler queue if the first
     *   argument is an array of event desriptors.
     */
    remove_events: function (events, func) {
        var i;
        if (Array.isArray(events)) {
            for (i = 0; i < events.length; i++)
                this.remove_event(events[i], func);
        } else {
            for (i in events)
                if (events.hasOwnProperty(i))
                    this.remove_event(i, events[i]);
        }
    },
    /**
     * Fires several events.
     *
     * @method TK.Base#fire_events
     * 
     * @param {Array.<string>} events - A list of event names to fire.
     */
    fire_events: function (events) {
        for (var i in events) {
            if (events.hasOwnProperty(i))
                this.fire_event(i, events[i]);
        }
    }
});
function get_child_options(parent, name, options, config) {
    var ret = {};
    var key, pref = name+".";
    var tmp;

    var inherit_options = !!config.inherit_options;
    var blacklist_options = config.blacklist_options || [];

    if (tmp = config.default_options)
        Object.assign(ret, (typeof(tmp) === "function") ?  tmp.call(parent) : tmp);

    for (key in options) {
        if (key.startsWith(pref)) {
            ret[key.substr(pref.length)] = options[key];
        }

        if (inherit_options && blacklist_options.indexOf(tmp) < 0) {
            if (key in config.create.prototype._options && !(key in TK["Widget"].prototype._options)) {
                ret[key] = options[key];
            }
        }
    }

    var map_options = config.map_options;

    if (map_options) for (key in map_options) {
        if (options[key]) {
            ret[map_options[key]] = options[key];
        }
    }

    return ret;
}
function ChildWidget(widget, name, config) {
    
    /**
     * Defines a {@link TK.Widget} as a child for another widget. This function
     * is used internally to simplify widget definitions. E.g. the {@link TK.Icon} of a
     * {@link TK.Button} is defined as a TK.ChildWidget. TK.ChildWidgets
     * are created/added after the initialization of the parent widget.
     * If not configured explicitly, all options of the child widget can
     * be accessed via <code>TK.Widget.options[config.name + "." + option]</code>
     * on the parent widget.
     * 
     * @param {TK.Widget} widget - The {@link TK.Widget} to add the TK.ChildWidget to.
     * @param {string} name - The identifier of the element inside the parent Element, <code>TK.Widget[config.name]</code>.
     * @param {object} config - The configuration of the child element.
     * 
     * @property {TK.Widget} config.create - A TK.Widget class derivate to be used as child widget.
     * @property {boolean} [config.fixed] - A fixed child widget cannot be removed after initialization.
     * @property {boolean} [config.show=false] - Show/hide a non-fixed child widget on initialization.
     * @property {string} [config.option="show_"+config.name] - A custom option of the parent widget
     *     to determine the visibility of the child element. If this is
     *     <code>null</code>, <code>TK.Widget.options["show_"+  config.name]</code>
     *     is used to toggle its visibility. The child element is visible, if
     *     this options is <code>!== false</code>.
     * @property {function} [config.append] - A function overriding the generic
     *     append mechanism. If not <code>null</code>, this function is
     *     supposed to take care of adding the child widget to the parent
     *     widgets DOM. Otherwise the element of the child widget is added
     *     to the element of the parent widget.
     * @property {boolean} [config.inherit_options=false] - Defines if both widgets share the
     *     same set of options. If <code>true</code>, Setting an option on the
     *     parent widget also sets the same option on the child widget. If <code>false</code>,
     *     the options of the child widget can be accessed via <code>options[config.name + "." + option]</code>
     *     in the parent widget.
     * @property {array} [config.map_options=[<Object>]] - A list of options to be mapped between
     *     parent and child. Keys are the options to be added to the parent, values the options
     *     names in the child widget.
     *     names in the parent to which the childrens options (defined as values) are mapped to.
     *     If one of these options is set
     *     on the parent widget, it also gets set on the child widget. This is
     *     a fine-grained version of <code>config.inherit-options</code>.
     * @property {boolean} [config.userset_ignore=false] - Do not care about the <code>userset</code>
     *     event of the parent widget, only keep track of <code>set</code>.
     * @property {boolean} [config.userset_delegate=false] - Delegates all user interaction from
     *     the child to the parent element. If the user triggers an event on
     *     the child widget, the <code>userset</code> function of the parent
     *     element is called.
     * @property {array} [config.static_events=[]] - An array of static events to be
     *     added to the parent widget. Each entry is a mapping between
     *     the name of the event and the callback function.
     * @property {boolean} [config.toggle_class=false] - Defines if the parent widget
     *     receives the class <code>toolkit-has-[name]</code> as soon as
     *     the child element is shown.
     * @property {array<string>} [config.blacklist_options] - Array containing options names
     *     which are skipped on `inherit_options`.
     * 
     * @class TK.ChildWidget
     * 
     */
     
    var p = widget.prototype;
    var key = config.option || "show_"+name;
    var tmp, m;
    var static_events = { };
    
    if (!config.userset_ignore)
      static_events.userset = (config.inherit_options || config.userset_delegate)
          ? function(key, value) { this.parent.userset(key, value); return false; }
          : function(key, value) { this.parent.userset(name+"."+key, value); return false; };


    if (m = config.static_events)
        Object.assign(static_events, m);

    if (config.create === void(0)) {
      TK.warn("'create' is undefined. Skipping ChildWidget ", name);
      return;
    }

    var child = TK.class({
        Extends: config.create,
        static_events: static_events,
    });


    /* trigger child widget creation after initialization */
    add_static_event(widget, "initialized", function() {
        /* we do not want to trash the class cache */
        if (!this[name])
        {
          this[name] = null;
          this.set(key, this.options[key]);
        }
    });

    /* clean up on destroy */
    add_static_event(widget, "destroy", function() {
        if (this[name]) {
            this[name].destroy();
            this[name] = null;
        }
    });
    
    var fixed = config.fixed;
    var append = config.append;
    var toggle_class = !!config.toggle_class;

    if (append === void(0)) append = true;

    /* child widget creation */
    add_static_event(widget, "set_"+key, function(val) {
        var C = this[name];
        var show = fixed || !!val;
        if (show && !C) {
            var O = get_child_options(this, name, this.options, config);
            if (append === true)
                O.container = this.element;
            var w = new child(O);
            this.add_child(w);
            this[name] = w;
            if (typeof(append) === "function")
                append.call(this);
        } else if (!show && C) {
            C.destroy();
            this[name] = null;
        }
        if (toggle_class) TK.toggle_class(this.element, "toolkit-has-"+name, show);
        this.trigger_resize();
    });
    var set_cb = function(val, key) {
        if (this[name]) this[name].set(key.substr(name.length+1), val);
    };

    for (tmp in child.prototype._options) {
        add_static_event(widget, "set_"+name+"."+tmp, set_cb);
        p._options[name+"."+tmp] = child.prototype._options[tmp];
    }

    /* direct option inherit */
    var blacklist_options = config.blacklist_options || [];
    if (config.inherit_options) {
        set_cb = function(val, key) {
            if (this[name]) this[name].set(key, val);
        };
        for (tmp in child.prototype._options) {
            if (tmp in TK["Widget"].prototype._options) continue;
            if (blacklist_options.indexOf(tmp) > -1) continue;
            add_static_event(widget, "set_"+tmp, set_cb);
            if (!p._options[tmp])
                p._options[tmp] = child.prototype._options[tmp];
        }
    }
    set_cb = function(key) {
        return function(val) {
            if (this[name]) this[name].set(key, val);
        };
    };
    if (m = config.map_options) {
        for (tmp in m) {
            p._options[tmp] = child.prototype._options[m[tmp]];
            if (!p.options[tmp])
                p.options[tmp] = child.prototype.options[m[tmp]];
            add_static_event(widget, "set_"+tmp, set_cb(m[tmp]));
        }
    }
    if (!config.options) {
        if (!p._options[key])
            p._options[key] = "boolean";
        p.options[key] = fixed || !!config.show;
    }
}
TK.add_static_event = add_static_event;
TK.ChildWidget = ChildWidget;

function ChildElement(widget, name, config) {
    /**
     * Creates a HTMLElement as a child for a widget. Is used to simplify
     * widget definitions. E.g. the tiny marker used to display the back-end
     * value is a simple DIV added using TK.ChildElement. The generic element
     * is a DIV added to TK.Widget.element with the class
     * <code>toolkit-[name]</code>. Default creating and adding can be
     * overwritten with custom callback functions.
     * 
     * @param {TK.Widget} widget - The {@link TK.Widget} to add the TK.ChildElement to.
     * @param {string} name - The identifier of the element. It will be prefixed
     *     by an underscore <code>TK.Widget["_" + config.name]</code>.
     * @param {object} config - The configuration of the child element.
     * 
     * @property {boolean} [config.show=false] - Show/hide the child element on initialization.
     * @property {string} [config.option="show_"+config.name] - A custom option of the parent widget
     *     to determine the visibility of the child element. If this is
     *     <code>null</code>, <code>TK.Widget.options["show_"+  config.name]</code>
     *     is used to toggle its visibility. The child element is visible, if
     *     this options is <code>!== false</code>.
     * @property {function} [config.display_check] - A function overriding the 
     *     generic <code>show_option</code> behavior. If set, this function
     *     is called with the value of <code>show_option</code> as argument
     *     as soon as it gets set and is supposed to return a boolean
     *     defining the visibility of the element.
     * @property {function} [config.append] - A function overriding the generic
     *     append mechanism. If not <code>null</code>, this function is
     *     supposed to take care of adding the child element to the parent
     *     widgets DOM.
     * @property {function} [config.create] - A function overriding the generic
     *     creation mechanism. If not <code>null</code>, this function is
     *     supposed to create and return a DOM element to be added to the
     *     parent widget.
     * @property {boolean} [config.toggle_class=false] - Defines if the parent widget
     *     receives the class <code>toolkit-has-[name]</code> as soon as
     *     the child element is shown.
     * @property {array} [config.draw_options] - A list of options of the parent
     *     widget which are supposed to trigger a check if the element has to
     *     be added or removed.
     * @property {function} [config.draw] - A function to be called on redraw.
     * 
     * @class TK.ChildElement
     * 
     */
    var p = widget.prototype;
    var show_option = config.option || ("show_" + name);
    var index = "_"+name;

    var display_check = config.display_check;

    /* This is done to make sure that the object property is created
     * inside of the constructor. Otherwise, if we add the widget later
     * might be turned into a generic mapping.
     */
    add_static_event(widget, "initialize", function() {
        this[index] = null;
    });

    /* trigger child element creation after initialization */
    add_static_event(widget, "initialized", function() {
        this.set(show_option, this.options[show_option]);
    });

    /* clean up on destroy */
    add_static_event(widget, "destroy", function() {
        if (this[index]) {
            this[index].remove();
            this[index] = null;
        }
    });

    var append = config.append;
    var create = config.create;
    var toggle_class = !!config.toggle_class;

    if (create === void(0)) create = function() { return TK.element("div", "toolkit-"+name); }
    if (append === void(0)) append = function() { this.element.appendChild(this[index]); }

    add_static_event(widget, "set_"+show_option, function(value) {
        var C = this[index];
        var show = display_check ? display_check(value) : (value !== false);
        if (show && !C) {
            C = create.call(this);
            this[index] = C;
            append.call(this, this.options);
        } else if (C && !show) {
            this[index] = null;
            C.remove();
        }
        if (toggle_class) TK.toggle_class(this.element, "toolkit-has-"+name, value);
        this.trigger_resize();
    });

    if (config.draw) {
        var m = config.draw_options;

        if (!m) m = [ show_option ];
        else m.push(show_option);

        for (var i = 0; i < m.length; i++) {
            add_static_event(widget, "set_"+m[i], function() {
                if (this.options[show_option] !== false)
                    this.draw_once(config.draw);
            });
        }
    }

    if (p._options[show_option] === void(0)) {
        p._options[show_option] = "boolean";
        p.options[show_option] = !!config.show;
    }
}
TK.ChildElement = ChildElement;
})(this, this.TK||{});
