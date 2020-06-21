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

(function(w, TK){

function Invalid(options) {
    for (var key in options) this[key] = true;
};
Invalid.prototype = {
    validate : function() {
        var i = 0, key;
        var ret = false;
        for (i = 0; i < arguments.length; i++) {
            key = arguments[i];
            if (this.hasOwnProperty(key) && this[key]) {
                this[key] = false;
                ret = true;
            }
        }

        return ret;
    },
    test : function() {
        var i = 0, key;
        for (i = 0; i < arguments.length; i++) {
            key = arguments[i];
            if (this.hasOwnProperty(key) && this[key]) {
                return true;
            }
        }
    }
};
function redraw(fun) {
    if (!this._drawn) return;
    this.needs_redraw = false;
    /**
     * Is fired when a redraw is executed.
     * 
     * @event TK.Widget#redraw
     */
    this.fire_event("redraw");
    fun.call(this);
}
function resize() {
    if (this.is_destructed()) return;
    this.resize();
}
function dblclick (e) {
    /**
     * Is fired after a double click appeared. Set `dblclick` to 0 to
     * disable click event handling.
     * 
     * @event TK.Widget#doubleclick
     * 
     * @param {string} event - The browsers `MouseEvent`.
     * 
     */
    var O = this.options;
    var dbc = O.dblclick;
    if (!dbc) return;
    var d = + new Date();
    if (this.__lastclick + dbc > d) {
        e.lastclick = this.__lastclick;
        this.fire_event("doubleclick", e);
        this.__lastclick = 0;
    } else {
        this.__lastclick = d;
    }
}

TK.Widget = TK.class({
    /**
     * TK.Widget is the base class for all widgets drawing DOM elements. It
     * provides basic functionality like delegating events, setting options and
     * firing some events.
     *
     * @class TK.Widget
     * 
     * @extends TK.Base
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {String} [options.class=""] - A class to add to the class attribute of the main element.
     * @property {HTMLElement} [options.container] - A container the main element shall be added to.
     * @property {String} [options.id=""] - A string to be set as id attribute on the main element.
     * @property {Object} [options.styles=""] - An object containing CSS declarations to be added directly to the main element.
     * @property {Boolean} [options.disabled=false] - Toggles the class <code>toolkit-disabled</code>.
     * @property {HTMLElement} [options.element] - An element to be used as the main element.
     * @property {Boolean} [options.active] - Toggles the class <code>toolkit-inactive</code>.
     * @property {Boolean} [options.needs_resize=true] - Set to true if the resize function shall be called before the next redraw.
     * @property {Boolean} [options.dblclick=400] - Set a time in milliseconds for triggering double click event. If 0, no double click events are fired.
     */
    /**
     * The <code>set</code> event is emitted when an option was set using the {@link TK.Widget#set}
     * method. The arguments are the option name and its new value.
     *
     * Note that this happens both for user interaction and programmatical option changes.
     *
     * @event TK.Widget#set
     */
    /**
     * The <code>redraw</code> event is emitted when a widget is redrawn. This can be used
     * to do additional DOM modifications to a Widget.
     *
     * @event TK.Widget#redraw
     */
    /**
     * The <code>resize</code> event is emitted whenever a widget is being resized. This event can
     * be used to e.g. measure its new size. Note that some widgets do internal adjustments after
     * the <code>resize</code> event. If that is relevant, the {@link TK.Widget#resized} event can
     * be used, instead.
     *
     * @event TK.Widget#resize
     */
    /**
     * The <code>resized</code> event is emitted after each rendering frame, which was triggered by
     * a resize event.
     *
     * @event TK.Widget#resized
     */
    /**
     * The <code>hide</code> event is emitted when a widget is hidden and is not rendered anymore.
     * This happens both with browser visibility changes and also internally when using layout widgets
     * such as {@link TK.Pager}.
     *
     * @event TK.Widget#hide
     */
    /**
     * The <code>show</code> event is emitted when a widget is shown and is being rendered. This is the
     * counterpart to {@link TK.Widget#hide}.
     *
     * @event TK.Widget#show
     */
    Extends : TK.Base,
    _class: "Widget",
    _options: {
        // A CSS class to add to the main element
        class: "string",
        // A DOM element as container to inject the element
        // into
        container: "object",
        // a id to set on the element. If omitted a random
        // string is generated.
        id: "string",
        // If an element was stylized, styles can be applied
        styles: "object",
        disabled: "boolean",
        element: "object",
        active: "boolean",
        needs_resize: "boolean",
        dblclick: "number",
    },
    options: {
        // these options are of less use and only here to show what we need
        disabled:  false,  // Widgets can be disabled by setting this to true
        needs_resize: true,
        dblclick: 0,
    },
    static_events: {
        set_container: function(value) {
            if (value && this.element) {
                value.appendChild(this.element);
            } else if (!value && this.element.parentElement) {
                this.element.parentElement.removeChild(this.element);
            }
        },
        set_dblclick: function (val) {
            if (!this.__delegated) return;
            if (!!val)
                this.__delegated.addEventListener("click", this.__dblclick_cb);
            else
                this.__delegated.removeEventListener("click", this.__dblclick_cb);
        },
        initialized: function () {
            var v = this.options.dblclick;
            if (v > 0)
              this.set("dblclick", v);
        },
    },
    initialize: function (options) {
        // Main actions every widget needs to take
        if (!options) options = {};
        /** @property {HTMLElement} TK.Widget#element - The main element. */
        if (options.element)
            this.element = options.element;
        TK.Base.prototype.initialize.call(this, options);
        this.__classified = null;
        this.__stylized = null;
        this.__delegated = null;
        this.__widgetized = null;
        this.invalid = new Invalid(this.options);
        if (!this.value_time) this.value_time = null;
        this.needs_redraw = false;
        this._redraw = redraw.bind(this, this.redraw);
        this.__resize = resize.bind(this);
        this._schedule_resize = this.schedule_resize.bind(this);
        this._drawn = false;
        this.parent = null;
        this.children = null;
        this.draw_queue = null;
        this.__lastclick = 0;
        this.__dblclick_cb = dblclick.bind(this);
    },

    is_destructed: function() {
        return this.options === null;
    },

    invalidate_all: function() {
        for (var key in this.options) {
            if (!this._options[key]) {
                if (key.charCodeAt(0) !== 95)
                    TK.warn("%O %s: unknown option %s", this, this._class, key);
            } else this.invalid[key] = true;
        }
    },

    assert_none_invalid: function() {
        var warn = [];
        for (var key in this.invalid) {
            if (this.invalid[key] === true) {
                warn.push(key);
            }
        }

        if (warn.length) {
            TK.warn("found", warn.length, "invalid in", this, ":", warn);
        }
    },

    trigger_resize: function() {
        if (!this.options.needs_resize) {
            if (this.is_destructed()) {
                // This object was destroyed but trigger resize was still scheduled for the next frame.
                // FIXME: fix this whole problem properly
                return;
            }

            this.set("needs_resize", true);

            var C = this.children;

            if (!C) return;

            for (var i = 0; i < C.length; i++) {
                C[i].trigger_resize();
            }
        }
    },

    trigger_resize_children: function() {
        var C = this.children;

        if (!C) return;

        for (var i = 0; i < C.length; i++) {
            C[i].trigger_resize();
        }
    },

    schedule_resize: function() {
        TK.S.add(this.__resize, 0);
    },

    resize: function() {
        /**
         * Is fired when a resize is requested.
         * 
         * @event TK.Widget#resize
         */
        this.fire_event("resize");

        if (this._options.resized)
            this.set("resized", true);
        
        /**
         * Is fired after the resize was executed and the DOM is updated.
         * 
         * @event TK.Widget#resized
         */
        if (this.has_event_listeners("resized")) {
            TK.S.after_frame(this.fire_event.bind(this, "resized"));
        }
    },

    trigger_draw: function() {
        if (!this.needs_redraw) {
            this.needs_redraw = true;
            if (this._drawn) TK.S.add(this._redraw, 1);
        }
    },

    trigger_draw_next : function() {
        if (!this.needs_redraw) {
            this.needs_redraw = true;
            if (this._drawn) TK.S.add_next(this._redraw, 1);
        }
    },

    initialized: function () {
        // Main actions every widget needs to take
        /**
         * Is fired when a widget is initialized.
         * 
         * @event TK.Widget#initialized
         */
        this.fire_event("initialized");
        this.trigger_draw();
    },
    draw_once: function(fun) {
        var q = this.draw_queue;

        if (q === null) {
            this.draw_queue = [ fun ];
        } else {
            for (var i = 0; i < q.length; i++) if (q[i] === fun) return;
            q[i] = fun;
        }
        this.trigger_draw();
    },
    redraw: function () {
        var I = this.invalid;
        var O = this.options;
        var E = this.element;

        if (E) {
            if (I.id) {
                I.id = false;
                if (O.id) E.setAttribute("id", O.id);
            }
        }

        E = this.__stylized;

        if (E) {
            if (I.active) {
                I.active = false;
                TK.toggle_class(E, "toolkit-inactive", !O.active);
            }

            if (I.disabled) {
                I.disabled = false;
                TK.toggle_class(E, "toolkit-disabled", O.disabled);
            }

            if (I.styles) {
                I.styles = false;
                if (O.styles) TK.set_styles(E, O.styles);
            }
        }

        if (I.needs_resize) {
            I.needs_resize = false;

            if (O.needs_resize) {
                O.needs_resize = false;

                TK.S.after_frame(this._schedule_resize);
            }
        }

        var q = this.draw_queue;

        this.draw_queue = null;

        if (q) for (var i = 0; i < q.length; i++) {
            q[i].call(this, O);
        }
    },
    destroy: function () {
        /**
         * Is fired when a widget is destroyed.
         * 
         * @event TK.Widget#destroy
         */
        if (this.is_destructed()) {
          TK.warn("destroy called twice on ", this);
          return;
        }
        this.fire_event("destroy");

        this.disable_draw();
        if (this.parent) this.parent.remove_child(this);

        TK.Base.prototype.destroy.call(this);

        this._redraw = null;
        this.__resize = null;
        this._schedule_resize = null;
        this.children = null;
        this.options = null;
        this.parent = null;

        if (this.element) {
            this.element.remove();
            this.element = null;
        }
    },
    delegate: function (element) {
        this.delegate_events(element);
        this.__delegated = element;
        /**
         * Is fired when a widget gets delegated.
         * 
         * @event TK.Widget#initialized
         * 
         * @param {HTMLElement} element - The element all native DOM events are delegated to.
         */
        this.fire_event("delegated", element);
        return element;
    },
    add_class: function (cls) {
        TK.add_class(this.__classified, cls);
    },
    remove_class: function (cls) {
        TK.remove_class(this.__classified, cls);
    },
    has_class: function (cls) {
        return TK.has_class(this.__classified, cls);
    },
    classify: function (element) {
        // Takes a DOM element and adds its CSS functionality to the
        // widget instance
        this.__classified = element;
        if (this.options.class && element)
            TK.add_class(element, this.options.class);
        /**
         * Is fired when a widget is classified.
         * 
         * @event TK.Widget#classified
         * 
         * @param {HTMLElement} element - The element which receives all further class changes.
         */
        this.fire_event("classified", element);
        return element;
    },
    set_style: function (name, value) {
        TK.set_style(this.__stylized, name, value);
    },
    /**
     * Sets a CSS style property in this widgets DOM element.
     *
     * @method TK.Widget#set_style
     */
    set_styles: function (styles) {
        TK.set_styles(this.__stylized, styles);
    },
    /**
     * Returns the computed style of this widgets DOM element.
     *
     * @method TK.Widget#get_style
     */
    get_style: function (name) {
        return TK.get_style(this.__stylized, name);
    },
    stylize: function (element) {
        // Marks a DOM element as receiver for the "styles" options
        this.__stylized = element;
        if (this.options.styles) {
            TK.set_styles(element, this.options.styles);
        }
        /**
         * Is fired when a widget is stylized.
         * 
         * @event TK.Widget#stylized
         * 
         * @param {HTMLElement} element - The element which receives all further style changes.
         */
        this.fire_event("stylized", element);
        return element;
    },
    widgetize: function (element, delegate, classify, stylize) {
        /**
         * Set the DOM elements of this widgets. This method is usually only used internally.
         * Basically it means to add the id from options and set a basic CSS class.
         * If delegate is true, basic events will be delegated from the element to the widget instance
         * if classify is true, CSS functions will be bound to the widget instance.
         *
         * @method TK.Widget#widgetize
         * @emits TK.Widget#widgetize
         */
        var O = this.options;
        
        // classify?
        TK.add_class(element, "toolkit-widget");
        if (typeof O.id !== "string") {
            O.id = element.getAttribute("id");
            if (!O.id) {
                O.id = TK.unique_id()
                element.setAttribute("id", O.id);
            }
        } else element.setAttribute("id", O.id);

        if (O.class) {
            var c = O.class.split(" ");
            for (var i = 0; i < c.length; i++)
                TK.add_class(element, c[i]);
        }
        if (O.container)
            O.container.appendChild(element);
        if (delegate)
            this.delegate(element);
        if (classify)
            this.classify(element);
        if (stylize)
            this.stylize(element);
        this.__widgetized = element;
        /**
         * Is fired when a widget is widgetized.
         * 
         * @event TK.Widget#widgetize
         * 
         * @param {HTMLElement} element - The element which got widgetized.
         */
        this.fire_event("widgetized", element);
        return element;
    },
    
    // GETTER & SETTER
    /**
     * Sets an option.
     *
     * @method TK.Widget#set
     * 
     * @param {string} key - The option name.
     * @param value - The option value.
     */
    set: function (key, value) {
        /* These options are special and need to be handled immediately, in order
         * to preserve correct ordering */
        if (key === "class" && this.__classified) {
            if (this.options.class) TK.remove_class(this.__classified, this.options.class);
            if (value) TK.add_class(this.__classified, value);
        }
        if (this._options[key]) {
            this.invalid[key] = true;
            if (this.value_time && this.value_time[key])
                this.value_time[key] = Date.now();
            this.trigger_draw();
        } else if (key.charCodeAt(0) !== 95) {
            TK.warn("%O: %s.set(%s, %O): unknown option.", this, this._class, key, value);
        }
        TK.Base.prototype.set.call(this, key, value);
        return value;
    },
    track_option: function(key) {
        if (!this.value_time) this.value_time = {};
        this.value_time[key] = Date.now();
    },
    /**
     * Schedules this widget for drawing.
     *
     * @method TK.Widget#enable_draw
     * 
     * @emits TK.Widget#show
     */
    enable_draw: function () {
        if (this._drawn) return;
        this._drawn = true;
        if (this.needs_redraw) {
            TK.S.add(this._redraw, 1);
        }
        /**
         * Is fired when a widget gets enabled for drawing.
         * 
         * @event TK.Widget#show
         */
        this.fire_event("show");
        this.fire_event("visibility", true);
        var C = this.children;
        if (C) for (var i = 0; i < C.length; i++) C[i].enable_draw();
    },
    /**
     * Stop drawing this widget.
     *
     * @method TK.Widget#enable_draw
     * 
     * @emits TK.Widget#hide
     */
    disable_draw: function () {
        if (!this._drawn) return;
        this._drawn = false;
        if (this.needs_redraw) {
            TK.S.remove(this._redraw, 1);
            TK.S.remove_next(this._redraw, 1);
        }
        /**
         * Is fired when a widget is hidden and not rendered anymore.
         * 
         * @event TK.Widget#hide
         */
        /**
         * Is fired when the visibility state changes. The first argument
         * is the visibility state, which is either <code>true</code>
         * or <code>false</code>.
         * 
         * @event TK.Widget#visibility
         */
        this.fire_event("hide");
        this.fire_event("visibility", false);
        var C = this.children;
        if (C) for (var i = 0; i < C.length; i++) C[i].disable_draw();
    },
    /**
     * Make the widget visible. This does not modify the DOM, instead it will only schedule
     * the widget for rendering.
     *
     * @method TK.Widget#show
     */
    show: function () {
        this.enable_draw();
    },
    /**
     * This is an alias for hide, which may be overloaded.
     * See {@link TK.Container} for an example.
     *
     * @method TK.Widget#force_show
     */
    force_show: function() {
        this.enable_draw();
    },
    /**
     * Make the widget hidden. This does not modify the DOM, instead it will stop rendering
     * this widget. Options changed after calling hide will only be rendered (i.e. applied
     * to the DOM) when the widget is made visible again using {@link TK.Widget#show}.
     *
     * @method TK.Widget#hide
     */
    hide: function () {
        this.disable_draw();
    },
    /**
     * This is an alias for hide, which may be overloaded.
     * See {@link TK.Container} for an example.
     *
     * @method TK.Widget#force_hide
     */
    force_hide: function () {
        this.disable_draw();
    },
    show_nodraw: function() { },
    hide_nodraw: function() { },
    /**
     * Returns the current hidden status.
     *
     * @method TK.Widget#hidden
     */
    hidden: function() {
        return !this._drawn;
    },
    is_drawn: function() {
        return this._drawn;
    },
    /**
     * TK.Toggle the hidden status. This is equivalent to calling hide() or show(), depending on
     * the current hidden status of this widget.
     *
     * @method TK.Widget#toggle_hidden
     */
    toggle_hidden: function() {
        if (this.hidden()) this.show();
        else this.hide();
    },
    set_parent: function(parent) {
        if (this.parent) {
            this.parent.remove_child(this);
        }
        this.parent = parent;
    },
    /**
     * Registers a widget as a child widget. This method is used to build up the widget tree. It does not modify the DOM tree.
     *
     * @method TK.Widget#add_child
     * 
     * @param {TK.Widget} child - The child to add.
     * 
     * @see TK.Container#append_child
     */
    add_child: function(child) {
        var C = this.children;
        if (!C) this.children = C = [];
        child.set_parent(this);
        C.push(child);
        if (!this.hidden()) {
            child.enable_draw();
        } else {
            child.disable_draw();
        }
        child.trigger_resize();
    },
    /**
     * Removes a child widget. Note that this method only modifies
     * the widget tree and does not change the DOM.
     *
     * @method TK.Widget#remove_child
     * 
     * @param {TK.Widget} child - The child to remove.
     */
    remove_child : function(child) {
        child.disable_draw();
        child.parent = null;
        var C = this.children;
        if (C === null) return;
        var i = C.indexOf(child);
        if (i !== -1) {
            C.splice(i, 1);
        }
        if (!C.length) this.children = null;
    },
    /**
     * Removes an array of children.
     *
     * @method TK.Widget#remove_children
     * 
     * @param {Array.<TK.Widget>} a - An array of Widgets.
     */
    remove_children : function(a) {
        a.map(this.remove_child, this);
    },
    /**
     * Registers an array of widgets as children.
     *
     * @method TK.Widget#add_children
     * 
     * @param {Array.<TK.Widget>} a - An array of Widgets.
     */
    add_children : function (a) {
        a.map(this.add_child, this);
    },

    /**
     * Returns an array of all visible children.
     *
     * @method TK.Widget#visible_children
     */
    visible_children: function(a) {
        if (!a) a = [];
        var C = this.children;
        if (C) for (var i = 0; i < C.length; i++) {
            a.push(C[i]);
            C[i].visible_children(a);
        }
        return a;
    },

    /**
     * Returns an array of all children.
     *
     * @method TK.Widget#all_children
     */
    all_children: function(a) {
        if (!a) a = [];
        var C = this.children;
        if (C) for (var i = 0; i < C.length; i++) {
            a.push(C[i]);
            C[i].all_children(a);
        }
        return a;
    },
});

TK.Module = TK.class({
    Extends: TK.Base,
    initialize: function(widget, options) {
        this.parent = widget;
        TK.Base.prototype.initialize.call(this, options);
    },
    destroy: function() {
        this.parent = null;
        TK.Base.prototype.destroy.call(this);
    },
});
})(this, this.TK);

/**
 * Generic DOM events. Please refer to
 *   <a href="https://www.w3schools.com/jsref/dom_obj_event.asp">
 *   W3Schools
 *   </a> for further details.
 * 
 * @event TK.Widget##GenericDOMEvents
 */
