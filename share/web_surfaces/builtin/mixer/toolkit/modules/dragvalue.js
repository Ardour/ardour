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

function start_drag(value) {
    if (!value) return;
    var O = this.options;
    this.start_pos = O.range.call(this).val2px(O.get.call(this));
    this.fire_event("startdrag", this.drag_state.start);
    if (O.events) O.events.call(this).fire_event("startdrag", this.drag_state.start);
}

/* This version integrates movements, instead
 * of using the global change since the beginning */
function movecapture_int(O, range, state) {
    /* O.direction is always 'polar' here */

    /* movement since last event */
    var v = state.prev_distance();
    var RO = range.options;

    if (!v[0] && !v[1]) return;

    var V = O._direction;

    var dist = Math.sqrt(v[0]*v[0] + v[1]*v[1]);

    var c = (V[0]*v[0] - V[1]*v[1]) / dist;

    if (Math.abs(c) > O._cutoff) return;

    if (v[0] * V[1] + v[1] * V[0] < 0) dist = -dist;

    var multi = RO.step || 1;
    var e = state.current;

    if (e.ctrlKey || e.altKey) {
        multi *= RO.shift_down;
    } else if (e.shiftKey) {
        multi *= RO.shift_up;
    }

    dist *= multi;
    var v = this.start_pos + dist;

    var nval = range.px2val(v);
    if (O.limit)
        O.set.call(this, Math.min(RO.max, Math.max(RO.min, nval)));
    else
        O.set.call(this, nval);

    if (!(nval > RO.min) || !(nval < RO.max)) return;

    this.start_pos = v;
}

function movecapture_abs(O, range, state) {
    var dist;
    var RO = range.options
    switch(O.direction) {
    case "vertical":
        dist = -state.vdistance()[1];
        break;
    default:
        TK.warn("Unsupported direction:", O.direction);
    case "horizontal":
        dist = state.vdistance()[0];
        break;
    }
    if (O.reverse)
        dist *= -1;

    var multi = RO.step || 1;
    var e = state.current;

    if (e.ctrlKey && e.shiftKey) {
        multi *= RO.shift_down;
    } else if (e.shiftKey) {
        multi *= RO.shift_up;
    }

    dist *= multi;

    var nval = range.px2val(this.start_pos + dist);
    
    if (O.limit)
        O.set.call(this, Math.min(RO.max, Math.max(RO.min, nval)));
    else
        O.set.call(this, nval);
}

function movecapture(state) {
    var O = this.options;

    if (O.active === false) return false;

    var state = this.drag_state;
    var range = O.range.call(this);

    if (O.direction === "polar") {
        movecapture_int.call(this, O, range, state);
    } else {
        movecapture_abs.call(this, O, range, state);
    }

    this.fire_event("dragging", state.current);
    if (O.events) O.events.call(this).fire_event("dragging", state.current);
}

function stop_drag(state, ev) {
    this.fire_event("stopdrag", ev);
    var O = this.options;
    if (O.events) O.events.call(this).fire_event("stopdrag", ev);
}

function angle_diff(a, b) {
    // returns an unsigned difference between two angles
    var d = (Math.abs(a - b) + 360) % 360;
    return d > 180 ? 360 - d : d;
}
TK.DragValue = TK.class({
    /**
     * TK.DragValue enables dragging an element and setting a
     * value according to the dragged distance. TK.DragValue is for example
     * used in {@link TK.Knob} and {@link TK.ValueButton}.
     *
     * @class TK.DragValue
     *
     * @param {Object} [options={ }] - An object containing initial options.
     *
     * @property {Element} options.node - The DOM node used for dragging.
     *   All DOM events are registered with this Element.
     * @property {Function} [options.range] - A function returning a
     *  {@link TK.Range} object for
     *  calculating the value. Returns its parent (usually having
     *  {@link TK.Ranged}-features) by default.
     * @property {Function} [options.events] - Returns an element firing the
     *   events <code>startdrag</code>, <code>dragging</code> and <code>stopdrag</code>.
     *   By default it returns <code>this.parent</code>.
     * @property {Element|boolean} [options.classes=false] - While dragging, the class
     *   <code>toolkit-dragging</code> will be added to this element. If set to <code>false</code>
     *   the class will be set on <code>options.node</code>.
     * @property {Function} [options.get] - Callback function returning the value to drag.
     *   By default it returns <code>this.parent.options.value</code>.
     * @property {Function} [options.set] - Callback function for setting the value.
     *   By default it calls <code>this.parent.userset("value", [value]);</code>.
     * @property {String} [options.direction="polar"] - Direction for changing the value.
     *   Can be <code>polar</code>, <code>vertical</code> or <code>horizontal</code>.
     * @property {Boolean} [options.active=true] - If false, dragging is deactivated.
     * @property {Boolean} [options.cursor=false] - If true, a {@link TK.GlobalCursor} is set while dragging.
     * @property {Number} [options.blind_angle=20] - If options.direction is <code>polar</code>,
     *   this is the angle of separation between positive and negative value changes
     * @property {Number} [options.rotation=45] - Defines the angle of the center of the positive value
     *   changes. 0 means straight upward. For instance, a value of 45 leads to increasing value when
     *   moving towards top and right.
     * @property {Boolean} [options.reverse=false] - If true, the difference of pointer travel is inverted.
     * @property {Boolean} [options.limit=false] - Limit the returned value to min and max of the range.
     *
     * @extends TK.Module
     *
     * @mixes TK.GlobalCursor
     */
    _class: "DragValue",
    Extends: TK.DragCapture,
    Implements: TK.GlobalCursor,
    _options: {
        get: "function",
        set: "function",
        range: "function",
        events: "function",
        classes: "object|boolean",
        direction: "string",
        active: "boolean",
        cursor: "boolean",
        blind_angle: "number",
        rotation: "number",
        reverse: "boolean",
        limit: "boolean",
    },
    options: {
        range:     function () { return this.parent; },
        classes:   false,
        get:       function () { return this.parent.options.value; },
        set:       function (v) { this.parent.userset("value", v); },
        events:    function () { return this.parent; },
        direction: "polar",
        active:    true,
        cursor:    false,
        blind_angle: 20,
        rotation:  45,
        reverse:   false,
        limit: false,
    },
    /**
     * Is fired while a user is dragging.
     *
     * @event TK.DragValue#dragging
     *
     * @param {DOMEvent} event - The native DOM event.
     */
    /**
     * Is fired when a user starts dragging.
     *
     * @event TK.DragValue#startdrag
     *
     * @param {DOMEvent} event - The native DOM event.
     */
    /**
     * Is fired when a user stops dragging.
     *
     * @event TK.DragValue#stopdrag
     *
     * @param {DOMEvent} event - The native DOM event.
     */
    static_events: {
        set_state: start_drag,
        stopcapture: stop_drag,
        startcapture: function() {
            if (this.options.active) return true;
        },
        set_rotation: function(v) {
            v *= Math.PI / 180;
            this.set("_direction", [ -Math.sin(v), Math.cos(v) ]);
        },
        set_blind_angle: function(v) {
            v *= Math.PI / 360;
            this.set("_cutoff", Math.cos(v));
        },
        movecapture: movecapture,
        startdrag: function(ev) {
            TK.S.add(function() {
                var O = this.options;
                TK.add_class(O.classes || O.node, "toolkit-dragging");
                if (O.cursor) {
                    if (O.direction === "vertical") {
                        this.global_cursor("row-resize");
                    } else {
                        this.global_cursor("col-resize");
                    }
                }
            }.bind(this), 1);
        },
        stopdrag: function() {
            TK.S.add(function() {
                var O = this.options;
                TK.remove_class(O.classes || O.node, "toolkit-dragging");

                if (O.cursor) {
                    if (O.direction === "vertical") {
                        this.remove_cursor("row-resize");
                    } else {
                        this.remove_cursor("col-resize");
                    }
                }
            }.bind(this), 1);
        },
    },
    initialize: function (widget, options) {
        TK.DragCapture.prototype.initialize.call(this, widget, options);
        this.start_pos = 0;
        var O = this.options;
        this.set("rotation", O.rotation);
        this.set("blind_angle", O.blind_angle);
    },
});
})(this, this.TK);
