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
    
function extract_matrix (t) {
    var a = t.indexOf("matrix(");
    if (a < 0) return; 
    t = t.substr(a+7);
    return (t.split(")"))[0].split(",").map(function(v){return parseInt(v.trim())});
}

function xy_from_transform (t) {
    var mx = extract_matrix(t);
    return (!mx || !mx.length) ? [0, 0] : [mx[4], mx[5]];
}

function startdrag(e, drag) {
    this._dragged = 0;
    var O = this.options;
    if (!O.active) return;
    if (e.button !== void(0) && e.button > 0) return;
    this._xstart = this._xlast = e.pageX;
    this._ystart = this._ylast = e.pageY;
    if (O.transform) {
        var xy = xy_from_transform(this._style.transform);
        this._xpos = xy[0];
        this._ypos = xy[1];
    } else {
        this._xpos = O.node.offsetLeft;
        this._ypos = O.node.offsetTop;
    }
    TK.add_class(O.node, "toolkit-dragging");
}
function stopdrag(e, drag) {
    if (!this.options.active) return;
    if (e.button !== void(0) && e.button > 0) return;
    TK.remove_class(this.options.node, "toolkit-dragging");
}
function dragging(e, drag) {
    var O = this.options;
    if (!O.active) return;
    if (e.button !== void(0) && e.button > 0) return;
    this._dragged += (Math.abs(e.pageX - this._xlast)
                    + Math.abs(e.pageY - this._ylast)) / 2;
    if (this._dragged < O.initial) return;
    this._xlast = e.pageX;
    this._ylast = e.pageY;
    var x = this._xpos + e.pageX - this._xstart;
    var y = this._ypos + e.pageY - this._ystart;
    if (O.min.x !== false) x = Math.max(O.min.x, x);
    if (O.max.x !== false) x = Math.min(O.max.x, x);
    if (O.min.y !== false) y = Math.max(O.min.y, y);
    if (O.max.y !== false) y = Math.min(O.max.y, y);
    if (O.transform) {
        var t = this._style.transform;
        var mx = extract_matrix(t);
        mx[4] = x;
        mx[5] = y;
        var nt = t.replace(/matrix\([0-9 \,]*\)/, "matrix(" + mx.join(",") + ")");
        O.node.style.transform = nt;
    } else {
        O.node.style.top = y + "px";
        O.node.style.left = x + "px";
    }
}
function set_handle() {
    var h = this.options.handle;
    if (this.drag)
        this.drag.destroy();
    var range = new TK.Range({});
    this.drag = new TK.DragValue(this, {
        node: h,
        range: function () { return range; },
        get: function() { return 0; },
        set: function(v) { return; },
    });
}
/**
 * TK.Drag enables dragging of elements on the screen positioned absolutely or by CSS transformation.
 * 
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {HTMLElement|SVGElement} options.node - The element to drag.
 * @property {HTMLElement|SVGElement} [options.handle] A DOM node to be used as a handle. If not set, <code>options.node</code> is used.
 * @property {Boolean} [options.active=true] - Enable or disable dragging
 * @property {Object|Boolean} [options.min={x: false, y: false}] - Object containing the minimum positions for x and y. A value of <code>false</code> is interpreted as no minimum.
 * @property {Object|Boolean} [options.max={x: false, y: false}] - Object containing the maximum positions for x and y. A value of <code>false</code> is interpreted as no maximum.
 * @property {Number} [options.initial=2] - Number of pixels the user has to move until dragging starts.
 * @property {Boolean} [options.transform=false] - Use CSS transformations instead of absolute positioning.
 * 
 * @extends TK.Base
 * 
 * @class TK.Drag
 */
TK.Drag = TK.class({
    _class: "Drag",
    Extends: TK.Base,
    _options: {
        node    : "object",
        handle  : "object",
        active  : "boolean",
        min     : "object",
        max     : "object",
        initial : "number",
        transform : "boolean",
    },
    options: {
        node      : null,
        handle    : null,
        active    : true,
        min       : {x: false, y: false},
        max       : {x: false, y: false},
        initial   : 2,
        transform : false,
    },
    /**
     * The user is dragging this item.
     *
     * @event TK.Drag#dragging
     * 
     * @param {DOMEvent} event - The native DOM event.
     */
    /** 
     * The user started dragging this item.
     * 
     * @event TK.Drag#startdrag
     * 
     * @param {DOMEvent} event - The native DOM event.
     */
    /**
     * The user stopped dragging this item.
     * 
     * @event TK.Drag#stopdrag
     * 
     * @param {DOMEvent} event - The native DOM event.
     */
    static_events: {
        startdrag: startdrag,
        dragging: dragging,
        stopdrag: stopdrag,
    },
    initialize: function (options) {
        TK.Base.prototype.initialize.call(this, options);
        this.set("handle", this.options.handle);
        this.set("node", this.options.node);
    },
    // GETTERS & SETTERS
    set: function (key, value) {
        if (key === "node")
            this._style = window.getComputedStyle(value);
        if (key === "handle" && !value)
            value = this.options.node;

        TK.Base.prototype.set.call(this, key, value);

        if (key === "handle")
            set_handle.call(this);
        if (key === "initial" && this.drag)
            this.drag.set("initial", value);
    }
});
})(this, this.TK);
