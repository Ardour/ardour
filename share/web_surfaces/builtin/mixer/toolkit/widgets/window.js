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
function header_action() {
    var that = this.parent;
    switch (that.options.header_action) {
        case "shrink":             that.toggle_shrink(); break;
        case "maximize":           that.toggle_maximize(); break;
        case "maximizehorizontal": that.toggle_maximize_horizontal(); break;
        case "maximizevertical":   that.toggle_maximize_vertical(); break;
        case "minimize":           that.toggle_minimize(); break;
        case "close":              that.destroy(); break;
    }
    /**
     * The user double-clicked on the header.
     * @event TK.Window.headeraction
     * @param {string} action - The function which was executed, e.g. <code>shrink</code>, <code>maximize</code> or <code>close</code>.
     */
    that.fire_event("headeraction", that.options.header_action);
}
function mout(e) {
    if(this.options.auto_active && !this.dragging && !this.resizing)
        TK.remove_class(this.element, "toolkit-active");
}
function mover(e) {
    if(this.options.auto_active)
        TK.add_class(this.element, "toolkit-active");
}
function max_height() {
    // returns the max height of the window
    return (this.options.max_height < 0 ? Number.MAX_SAFE_INTEGER : this.options.max_height);
}
function max_width() {
    // returns the max width of the window
    return (this.options.max_width < 0 ? Number.MAX_SAFE_INTEGER : this.options.max_width);
}
function close(e) {
    /**
     * The user clicked the close button.
     * @event TK.Window.closeclicked
     */
    this.fire_event("closeclicked");
    if (this.options.auto_close)
        this.destroy();
}
function maximize(e) {
    if (this.options.auto_maximize) this.toggle_maximize();
    /**
     * The user clicked the maximize button.
     * @event TK.Window.maximizeclicked
     * @param {Object} maximize - The maximize option.
     */
    this.fire_event("maximizeclicked", this.options.maximize);
}
function maximizevertical(e) {
    if (this.options.auto_maximize) this.toggle_maximize_vertical();
    /**
     * The user clicked the maximize-vertical button.
     * @event TK.Window.maximizeverticalclicked
     * @param {Object} maximize - The maximize option.
     */
    this.fire_event("maximizeverticalclicked", this.options.maximize.y);
}
function maximizehorizontal(e) {
    if (this.options.auto_maximize) this.toggle_maximize_horizontal();
    /**
     * The user clicked the maximize-horizontal button.
     * @event TK.Window.maximizehorizontalclicked
     * @param {Object} maximize - The maximize option.
     */
    this.fire_event("maximizehorizontalclicked", this.options.maximize.x);
}
function minimize(e) {
    if (this.options.auto_minimize) this.toggle_minimize();
    /**
     * The user clicked the minimize button.
     * @event TK.Window.minimizeclicked
     * @param {Object} minimize - The minimize option.
     */
    this.fire_event("minimizeclicked", this.options.minimize);
}
function shrink(e) {
    if (this.options.auto_shrink) this.toggle_shrink();
    /**
     * The user clicked the shrink button.
     * @event TK.Window.shrinkclicked
     * @param {Object} shrink - The shrink option.
     */
    this.fire_event("shrinkclicked", this.options.shrink);
}
function start_resize(el, ev) {
    this.global_cursor("se-resize");
    this.resizing = true;
    TK.add_class(this.element, "toolkit-resizing");
    /**
     * The user starts resizing the window.
     * @event TK.Window.startresize
     * @param {DOMEvent} event - The DOM event.
     */
    this.fire_event("startresize", ev);
}
function stop_resize(el, ev) {
    this.remove_cursor("se-resize");
    this.resizing = false;
    TK.remove_class(this.element, "toolkit-resizing");
    this.trigger_resize_children();
    calculate_dimensions.call(this);
    /**
     * The user stops resizing the window.
     * @event TK.Window.stopresize
     * @param {DOMEvent} event - The DOM event.
     */
    this.fire_event("stopresize", ev);
}
function resizing(el, ev) {
    if (this.options.resizing === "continuous") {
        this.trigger_resize_children();
        calculate_dimensions.call(this);
    }
    /**
     * The user resizes the window.
     * @event TK.Window.resizing
     * @param {DOMEvent} event - The DOM event.
     */
    this.fire_event("resizing", ev);
}
function calculate_dimensions() {
    var x = TK.outer_width(this.element, true);
    var y = TK.outer_height(this.element, true);
    this.dimensions.width  = this.options.width  = x;
    this.dimensions.height = this.options.height = y;
    this.dimensions.x2     = x + this.dimensions.x1;
    this.dimensions.y2     = y + this.dimensions.y1;
}
function calculate_position() {
    var posx  = TK.position_left(this.element);
    var posy  = TK.position_top(this.element);
    var pos1 = this.translate_anchor(this.options.anchor, posx, posy,
                                     this.options.width, this.options.height);
    this.dimensions.x      = this.options.x = pos1.x;
    this.dimensions.y      = this.options.y = pos1.y;
    this.dimensions.x1     = posx;
    this.dimensions.y1     = posy;
    this.dimensions.x2     = posx + this.dimensions.width;
    this.dimensions.y2     = posy + this.dimensions.height;
}
function horiz_max() {
    // returns true if maximized horizontally
    return this.options.maximize.x;
}
function vert_max() {
    // returns if maximized vertically
    return this.options.maximize.y;
}
function start_drag(ev, el) {
    this.global_cursor("move");
    TK.add_class(this.element, "toolkit-dragging");
    // if window is maximized, we have to replace the window according
    // to the position of the mouse
    var x = y = 0;
    if (vert_max.call(this)) {
        var y = (!this.options.fixed ? window.scrollY : 0);
    }
    if (horiz_max.call(this)) {
        var x = ev.clientX - (ev.clientX / TK.width())
                            * this.options.width;
        x += (!this.options.fixed ? window.scrollX : 0);
    }
    var pos = this.translate_anchor(
        this.options.anchor, x, y, this.options.width, this.options.height);
    
    if (horiz_max.call(this)) this.options.x = pos.x;
    if (vert_max.call(this))  this.options.y = pos.y;
    
    this.Drag._xpos += x;
    this.Drag._ypos += y;
    
    /**
     * The user starts dragging the window.
     * @event TK.Window.startdrag
     * @param {DOMEvent} event - The DOM event.
     */
    this.fire_event("startdrag", ev);
}
function stop_drag(ev, el) {
    this.dragging = false;
    calculate_position.call(this);
    this.remove_cursor("move");
    /**
     * The user stops dragging the window.
     * @event TK.Window.stopdrag
     * @param {DOMEvent} event - The DOM event.
     */
    this.fire_event("stopdrag", ev);
}
function dragging(ev, el) {
    if (!this.dragging) {
        this.dragging = true;
        // un-maximize
        if (horiz_max.call(this)) {
            this.set("maximize", {x: false});
        }
        if (vert_max.call(this)) {
            this.set("maximize", {y: false});
        }
    }
    calculate_position.call(this);
    /**
     * The user is dragging the window.
     * @event TK.Window.dragging
     * @param {DOMEvent} event - The DOM event.
     */
    this.fire_event("dragging", ev);
}
function init_position(pos) {
    var O = this.options;
    if (pos) {
        var x0 = O.fixed ? 0 : window.scrollX;
        var y0 = O.fixed ? 0 : window.scrollY;
        var pos1 = this.translate_anchor(
            O.open, x0, y0,
            window.innerWidth - O.width,
            window.innerHeight - O.height);
        var pos2 = this.translate_anchor( O.anchor, pos1.x, pos1.y, O.width, O.height);
        O.x = pos2.x;
        O.y = pos2.y;
    }
    set_dimensions.call(this);
    set_position.call(this);
}
function set_position() {
    var O = this.options;
    var D = this.dimensions;
    var width  = TK.inner_width(this.element);
    var height = TK.inner_height(this.element);
    var pos = this.translate_anchor(O.anchor, O.x, O.y, -width, -height);
    if (horiz_max.call(this)) {
        this.element.style.left = (O.fixed ? 0 : window.scrollX) + "px";
    } else {
        this.element.style.left = pos.x + "px";
    }
    if (vert_max.call(this)) {
        this.element.style.top = (O.fixed ? 0 : window.scrollY) + "px";
    } else {
        this.element.style.top = pos.y + "px";
    }
    D.x      = O.x;
    D.y      = O.y;
    D.x1     = pos.x;
    D.y1     = pos.y;
    D.x2     = pos.x + D.width;
    D.y2     = pos.y + D.height;
    /**
     * The position of the window changed.
     * @event TK.Window.positionchanged
     * @param {Object} event - The {@link TK.Window#dimensions} dimensions object.
     */
    this.fire_event("positionchanged", D);
}
function set_dimensions() {
    var O = this.options;
    var D = this.dimensions;
    if (O.width >= 0) {
        O.width = Math.min(max_width.call(this), Math.max(O.width, O.min_width));
        if (horiz_max.call(this)) {
            TK.outer_width(this.element, true, TK.width());
            D.width = TK.width();
        } else {
            TK.outer_width(this.element, true, O.width);
            D.width = O.width;
        }
    } else {
        D.width = TK.outer_width(this.element);
    }
    if (O.height >= 0) {
        O.height = Math.min(max_height.call(this), Math.max(O.height, O.min_height));
        if (vert_max.call(this)) {
            TK.outer_height(this.element, true, TK.height());
            D.height = TK.height();
        } else {
            TK.outer_height(this.element, true, O.height);
            D.height = O.height;
        }
    } else {
        D.height = TK.outer_height(this.element, true);
    }
    D.x2 = D.x1 + D.width;
    D.y2 = D.y1 + D.height;
    /**
     * The dimensions of the window changed.
     * @event TK.Window.dimensionschanged
     * @param {Object} event - The {@link TK.Window#dimensions} dimensions object.
     */
    this.fire_event("dimensionschanged", this.dimensions);
}
function build_header() {
    build_from_const.call(this, "header");
    if (!this.Drag) {
        this.Drag = new TK.Drag({
            node        : this.element,
            handle      : this.header.element,
            onStartdrag : start_drag.bind(this),
            onStopdrag  : stop_drag.bind(this),
            onDragging  : dragging.bind(this),
            min         : {x: 0 - this.options.width + 20, y: 0},
            max         : {x: TK.width() - 20, y: TK.height() - 20},
        });
        //this.header.add_event("dblclick", header_action.bind(this));
    }
    /**
     * The header changed.
     * @event TK.Window.headerchanged
     */
    this.fire_event("headerchanged");
}
function build_footer() {
    build_from_const.call(this, "footer");
    /**
     * The footer changed.
     * @event TK.Window.footerchanged
     */
    this.fire_event("footerchanged");
}
function build_from_const(element) {
    var E = this[element].element;
    var L = this.options[element];
    var O = this.options;
    var targ;
    while (E.firstChild) E.firstChild.remove();
    if (!L) return;
    for (var i = 0; i < L.length; i++) {
        if (L[i] !== "spacer") {
            this.set("show_" + L[i], true);
            E.appendChild(this[L[i]].element);
            if (L[i] == "size" && !this.Resize && this.size) {
                this.Resize = new TK.Resize({
                    node          : this.element,
                    handle        : this.size.element,
                    min           : {x: O.min_width, y: O.min_height},
                    max           : {x: max_width.call(this), y: max_height.call(this)},
                    onResizestart : start_resize.bind(this),
                    onResizestop  : stop_resize.bind(this),
                    onResizing    : resizing.bind(this),
                    active        : O.resizable
                });
            }
        } else {
            E.appendChild(TK.element("div", "toolkit-spacer"));
        }
    }
}

function status_timeout () {
    var O = this.options;
    if (this.__status_to !== false)
        window.clearTimeout(this.__status_to);
    if (!O.hide_status) return;
    if (O.status)
        this.__status_to = window.setTimeout(function () {
            this.set("status", "");
            this.__status_to = false;
        }.bind(this), O.hide_status);
}
    
TK.Window = TK.class({
    /**
     * This widget is a flexible overlay window.
     *
     * @class TK.Window
     * 
     * @extends TK.Container
     * @implments TK.Anchor TK.GlobalCursor
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Number} [options.width=500] - Initial width, can be a CSS length or an integer (pixels).
     * @property {Number} [options.height=200] - Initial height, can be a CSS length or an integer (pixels).
     * @property {Number} [options.x=0] - X position of the window.
     * @property {Number} [options.y=0] - Y position of the window.
     * @property {Number} [options.min_width=64] - Minimum width of the window.
     * @property {Number} [options.max_width=-1] - Maximum width of the window, -1 ~ infinite.
     * @property {Number} [options.min_height=64] - Minimum height of the window.
     * @property {Number} [options.max_height=-1] - Maximum height of the window, -1 ~ infinite.
     * @property {String} [options.anchor="top-left"] - Anchor of the window, can be one out of
     *   `top-left`, `top`, `top-right`, `left`, `center`, `right`, `bottom-left`, `bottom`, `bottom-right`
     * @property {Boolean} [options.modal=false] - If modal window blocks all other elements
     * @property {String} [options.dock=false] - Docking of the window, can be one out of
     *   `top-left`, `top`, `top-right`, `left`, `center`, `right`, `bottom-left`, `bottom`, `bottom-right`
     * @property {Object|Boolean} [options.maximize=false] - Boolean or object with members <code>x</code> and <code>y</code> as boolean to determine the maximized state.
     * @property {Boolean} [options.minimize=false] - Minimize window (does only make sense with a
     *   window manager application to keep track of it)
     * @property {Boolean} [options.shrink=false] - Shrink rolls the window up into the title bar.
     * @property {String|HTMLElement|TK.Container} [options.content=""] - The content of the window.
     *   Can be either a string, a HTMLElement or a {@link TK.Container} to be appended to the content area.
     * @property {String} [options.open="center"] - initial position of the window, can be one out of
     *   `top-left`, `top`, `top-right`, `left`, `center`, `right`, `bottom-left`, `bottom`, `bottom-right`
     * @property {Integer} [options.z_index=10000] - Z index for piling windows. does make more sense
     *   when used together with a window manager
     * @property {String|Array<String>} [options.header=["title", "maximize", "close"]] - Single element or array of
     *   `title`, `icon`, `close`, `minimize`, `shrink`, `maximize`, `maximizevertical`, `maximizehorizontal`, `status`, `resize`, `spacer`.
     * @property {String|Array<String>} [options.footer=false] - Single element or array of
     *   `title`, `icon`, `close`, `minimize`, `shrink`, `maximize`, `maximizevertical`, `maximizehorizontal`, `status`, `resize`, `spacer`.
     * @property {String} [options.title=false] - Window title.
     * @property {String} [options.status=false] Window status.
     * @property {String} [options.icon=false] URL to window icon.
     * @property {Boolean} [options.fixed=true] - Whether the window sticks to the viewport rather than the document
     * @property {Boolean} [options.auto_active=false] - Auto-toggle the active-class when mouseovered
     * @property {Boolean} [options.auto_close=true] - Set whether close destroys the window or not
     * @property {Boolean} [options.auto_maximize=true] - Set whether maximize toggles the window or not
     * @property {Boolean} [options.auto_minimize=true] - Set whether minimize toggles the window or not
     * @property {Boolean} [options.auto_shrink=true] - Set whether shrink toggles the window or not
     * @property {Boolean} [options.draggable=true] - Set whether the window is draggable
     * @property {Boolean} [options.resizable=true] - Set whether the window is resizable
     * @property {String} [options.resizing="continuous"] - Resizing policy, `continuous` or `stop`.
     *   The first one resizes all children continuously while resizing.
     * @property {String} [options.header_action="maximize"] - Action for double clicking the window header, one out of
     *   `close`, `minimize`, `shrink`, `maximize`, `maximizevertical`, `maximizehorizontal`
     * @property {Boolean} [options.active=true] - Active state of the window.
     * @property {Integer} [options.hide_status=0] - If set to !0 status message hides after [n] milliseconds.
     */
     
    /**
     * @member {TK.Drag} TK.Window#Drag - The {TK.Drag} module.
     */
    /**
     * @member {TK.Resize} TK.Window#Resize - The {TK.Resize} module.
     */
    _class: "Window",
    Extends: TK.Container,
    Implements: [TK.Anchor, TK.GlobalCursor],
    _options: Object.assign(Object.create(TK.Container.prototype._options), {
        width: "number",
        height: "number",
        x: "number",
        y: "number",
        min_width: "number",
        max_width: "number",
        min_height: "number",
        max_height: "number",
        anchor: "string",
        modal: "boolean",
        dock: "boolean",
        maximize: "boolean",
        minimize: "boolean",
        shrink: "boolean",
        open: "int",
        z_index: "int",
        header: "array",
        footer: "array",
        title: "string",
        status: "string",
        icon: "string",
        fixed: "boolean",
        auto_active: "boolean",
        auto_close: "boolean",
        auto_maximize: "boolean",
        auto_minimize: "boolean",
        auto_shrink: "boolean",
        draggable: "boolean",
        resizable: "boolean",
        resizing: "int",
        header_action: "int",
        active: "boolean",
        hide_status: "int",
    }),
    options: {
        width:         500,
        height:        200,
        x:             0,
        y:             0,
        min_width:     64,
        max_width:     -1,
        min_height:    64,
        max_height:    -1,
        anchor:        "top-left",
        modal:         false,
        dock:          false,
        maximize:      false,
        minimize:      false,
        shrink:        false,
        content:       "",
        open:          "center",
        z_index:       10000,
        header:        ["title", "maximize", "close"],
        footer:        false,
        title:         false,
        status:        false,
        icon:          false,
        fixed:         true,
        auto_active:   false,
        auto_close:    true,
        auto_maximize: true,
        auto_minimize: true,
        auto_shrink:   true,
        draggable:     true,
        resizable:     true,
        resizing:      "continuous",
        header_action: "maximize",
        active:        true,
        hide_status:   0,
    },
    static_events: {
        mouseenter: mover,
        mouseleave: mout,
    },
    initialize: function (options) {
        this.dimensions = {anchor: "top-left", x: 0, x1: 0, x2: 0, y: 0, y1: 0, y2: 0, width: 0, height: 0};
        TK.Container.prototype.initialize.call(this, options);
        var O = this.options;
        TK.add_class(this.element, "toolkit-window");
        this.__status_to = false;
        init_position.call(this, this.options.open);
        this.set("maximize", this.options.maximize);
        this.set("minimize", this.options.minimize);
    },
    
    /**
     * Appends a new child to the window content area.
     * @method TK.Window#append_child
     * @param {TK.Widget} child - The child widget to add to the windows content area.
     */
    append_child : function(child) {
        child.set("container", this.content.element);
        this.add_child(child);
    },
    
    /**
     * Toggles the overall maximize state of the window.
     * @method TK.Window#toggle_maximize
     * @param {Boolean} maximize - State of maximization. If window is already
     *   maximized in one or both directions it is un-maximized, otherwise maximized.
     */
    toggle_maximize: function () {
        if (!vert_max.call(this) || !horiz_max.call(this))
            this.set("maximize", {x: true, y: true});
        else
            this.set("maximize", {x: false, y: false});
    },
    /**
     * Toggles the vertical maximize state of the window.
     * @method TK.Window#toggle_maximize_vertical
     * @param {Boolean} maximize - The new vertical maximization.
     */
    toggle_maximize_vertical: function () {
        this.set("maximize", {y: !this.options.maximize.y});
    },
    /**
     * Toggles the horizontal maximize state of the window.
     * @method TK.Window#toggle_maximize_horizontal
     * @param {Boolean} maximize - The new horizontal maximization.
     */
    toggle_maximize_horizontal: function () {
        this.set("maximize", {x: !this.options.maximize.x});
    },
    /**
     * Toggles the minimize state of the window.
     * @method TK.Window#toggle_minimize
     * @param {Boolean} minimize - The new minimization.
     */
    toggle_minimize: function () {
        this.set("minimize", !this.options.minimize);
    },
    /**
     * Toggles the shrink state of the window.
     * @method TK.Window#toggle_shrink
     * @param {Boolean} shrink - The new shrink state.
     */
    toggle_shrink: function () {
        this.set("shrink", !this.options.shrink);
    },
    
    resize: function () {
        this.Drag.set("min", {x: 0 - this.options.width + 20, y: 0});
        this.Drag.set("max", {x: TK.width() - 20, y: TK.height() - 20});
        TK.Container.prototype.resize.call(this);
    },
    
    redraw: function () {
        var I = this.invalid;
        var O = this.options;
        var E = this.element;
        
        var setP = false;
        var setD = false;
        
        if (I.maximize) {
            I.maximize = false;
            if (O.shrink) {
                O.shrink = false;
                I.shrink = true;
            }
            TK.toggle_class(this.element, "toolkit-maximized-horizontal", O.maximize.x);
            TK.toggle_class(this.element, "toolkit-maximized-vertical", O.maximize.y);
            setD = true;
        }
        if (I.anchor) {
            I.anchor = false;
            this.dimensions.anchor = O.anchor;
            setP = setD = true;
        }
        if (I.width || I.height) {
            I.width = I.height = false;
            setD = true;
        }
        if (I.x || I.y) {
            I.x = I.y = false;
            setP = true;
        }
        if (I.z_index) {
            I.z_index = false;
            this.element.style.zIndex = O.z_index;
        }
        if (I.header) {
            I.header = false;
            this.set("show_header", !!O.header);
            if (O.header) build_header.call(this);
        }
        if (I.footer) {
            I.footer = false;
            this.set("show_footer", !!O.footer);
            if (O.footer) build_footer.call(this);
        }
        if (I.status) {
            I.status = false;
            status_timeout.call(this);
        }
        if (I.fixed) {
            this.element.style.position = O.fixed ? "fixed" : "absolute";
            setP = true;
        }
        if (I.active) {
            I.active = false;
            TK.toggle_class(this.element, "toolkit-active", O.active);
        }
        if (I.shrink) {
            I.shrink = false;
            this.options.maximize.y = false;
            TK.toggle_class(this.element, "toolkit-shrinked", O.shrink);
        }
        if (I.draggable) {
            I.draggable = false;
            TK.toggle_class(this.element, "toolkit-draggable", O.draggable);
        }
        if (I.resizable) {
            I.resizable = false;
            TK.toggle_class(this.element, "toolkit-resizable", O.resizable);
        }
        if (I.content) {
            I.content = false;
            if (O.content) {
                if (TK.Container.prototype.isPrototypeOf(O.content)) {
                    TK.set_content(this.content.element, "");
                    this.append_child(O.content);
                } else {
                    TK.set_content(this.content.element, O.content);
                }
            }
            setD = true;
            setP = true;
        }
        
        if (setD) set_dimensions.call(this);
        if (setP) set_position.call(this);
        TK.Container.prototype.redraw.call(this);
    },
    
    set: function (key, value) {
        var O = this.options;
        var E = this.element;
        
        if (key == "maximize") {
            if (value === false) value = this.options.maximize = {x: false, y: false};
            else if (value === true) value = this.options.maximize = {x: true, y: true};
            else value = Object.assign(this.options.maximize, value);
        }
        O[key] = value;
        
        switch (key) {
            case "shrink":
                O.maximize.y = false;
                break;
            case "minimize":
                if (value) {
                    if (!this.options.container && E.parentElement)
                        O.container = E.parentElement;
                    E.remove();
                } else if (O.container) {
                    this.set("container", O.container)
                }
                break;
            case "resizable":
                this.Resize.set("active", value);
                break;
            
        }
        return TK.Container.prototype.set.call(this, key, value);
    }
});

/**
 * @member {TK.Icon} TK.Window#icon - A {@link TK.Icon} widget to display the window icon.
 */
TK.ChildWidget(TK.Window, "icon", {
    create: TK.Icon,
    map_options: { icon : "icon" },
    toggle_class: true,
});
/**
 * @member {TK.Label} TK.Window#title - A {@link TK.Label} to display the window title.
 */
TK.ChildWidget(TK.Window, "title", {
    create: TK.Label,
    default_options: { "class" : "toolkit-title" },
    map_options: { title : "label" },
    toggle_class: true,
});
/**
 * @member {TK.Label} TK.Window#status - A {@link TK.Label} to display the window status.
 */
TK.ChildWidget(TK.Window, "status", {
    create: TK.Label,
    default_options: { "class" : "toolkit-status" },
    map_options: { status : "label" },
    toggle_class: true,
});
/**
 * @member {TK.Button} TK.Window#close - The close button.
 */
/**
 * @member {TK.Button} TK.Window#minimize - The minimize button.
 */
/**
 * @member {TK.Button} TK.Window#maximize - The maximize button.
 */
/**
 * @member {TK.Button} TK.Window#maximizevertical - The maximizevertical button.
 */
/**
 * @member {TK.Button} TK.Window#maximizehorizontal - The maximizehorizontal button.
 */
/**
 * @member {TK.Button} TK.Window#shrink - The shrink button.
 */

var bfactory = function (name) {
    TK.ChildWidget(TK.Window, name, {
        create: TK.Button,
        default_options: {
            "class" : "toolkit-" + name,
            "icon" :  "window" + name,
        },
        static_events: {
            "click" : function (e) { (eval(name)).call(this.parent, e); },
            "mousedown" : function (e) { e.stopPropagation(); },
        },
    });
}
var b = ["close", "minimize", "maximize", "maximizevertical", "maximizehorizontal", "shrink"];

b.map(bfactory);
/**
 * @member {TK.Icon} TK.Window#size - A {@link TK.Icon} acting as handle for window resize.
 */
TK.ChildWidget(TK.Window, "size", {
    create: TK.Icon,
    default_options: { "icon" : "windowresize", "class" : "toolkit-size" },
});
/**
 * @member {TK.Container} TK.Window#content - A {@link TK.Container} for the window content.
 */
TK.ChildWidget(TK.Window, "content", {
    create: TK.Container,
    toggle_class: true,
    show: true,
    default_options: { "class" : "toolkit-content" },
});
/**
 * @member {TK.Container} TK.Window#header - The top header bar.
 */
TK.ChildWidget(TK.Window, "header", {
    create: TK.Container,
    toggle_class: true,
    show: true,
    default_options: { "class" : "toolkit-header" },
    static_events: {
        "dblclick" : header_action,
    },
    append: function () { build_header.call(this); this.element.appendChild(this.header.element); },
});
/**
 * @member {TK.Container} TK.Window#footer - The bottom footer bar.
 */
TK.ChildWidget(TK.Window, "footer", {
    create: TK.Container,
    toggle_class: true,
    show: false,
    default_options: { "class" : "toolkit-footer" },
    append : function () { build_footer.call(this); this.element.appendChild(this.footer.element); },
});

})(this, this.TK);
