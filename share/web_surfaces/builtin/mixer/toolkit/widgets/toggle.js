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

/* Abstract toggle logic */

function reset_delay_to () {
    window.clearTimeout(this.__delayed_to);
    this.__delayed_to = -1;
    this.remove_class("toolkit-delayed");
}

function toggle(O) {
    if (this.userset("state", !O.state) === false) return;
    this.fire_event("toggled", O.state);
}
function press_start() {
    var O = this.options;
    this.__press_start_time = Date.now();
    if (O.delay && this.__delayed_to < 0) {
        this.__delayed_to = window.setTimeout((function (t) {
            return function () { press_start.call(t); }
        })(this), O.delay);
        this.add_class("toolkit-delayed");
        return;
    }
    this.remove_class("toolkit-delayed");
    if (O.delay && this.__delayed_to >= 0) {
        toggle.call(this, O);
    }
    if (O.press) toggle.call(this, O);
}
function press_end() {
    var O = this.options;
    if (O.delay && this.__delayed_to >= 0) {
        reset_delay_to.call(this);
        return;
    }
    var t = Date.now() - this.__press_start_time;
    if ((O.toggle && (!O.press || t > O.press)) || (!O.toggle && O.press)) {
        toggle.call(this, O);
    }
}
function press_cancel() {
    var O = this.options;
    if (O.delay && this.__delayed_to >= 0) {
        reset_delay_to.call(this);
        return;
    }
    /* this is definitely not a click, its a cancel by leaving the
     * button with mouse or finger while pressing */
    if (O.press) toggle.call(this, O);
}

/* MOUSE handling */
function mouseup(e) {
    this.remove_event("mouseup", mouseup);
    this.remove_event("mouseleave", mouseleave);
    press_end.call(this);
}
function mouseleave(e) {
    this.remove_event("mouseup", mouseup);
    this.remove_event("mouseleave", mouseleave);
    press_cancel.call(this);
}
function mousedown(e) {
    /* only left click please */
    if (e.button) return true;
    press_start.call(this);
    this.add_event("mouseup", mouseup);
    this.add_event("mouseleave", mouseleave);
}

/* TOUCH handling */
function is_current_touch(ev) {
    var id = this.__touch_id;
    var i;
    for (i = 0; i < ev.changedTouches.length; i++) {
        if (ev.changedTouches[i].identifier === id) {
            return true;
        }
    }

    return false;
}

function touchend(e) {
    if (!is_current_touch.call(this, e)) return;
    this.__touch_id = false;
    e.preventDefault();
    press_end.call(this);

    this.remove_event("touchend", touchend);
    this.remove_event("touchcancel", touchleave);
    this.remove_event("touchleave", touchleave);
}
function touchleave(e) {
    if (!is_current_touch.call(this, e)) return;
    this.__touch_id = false;
    e.preventDefault();
    press_cancel.call(this);

    this.remove_event("touchend", touchend);
    this.remove_event("touchcancel", touchleave);
    this.remove_event("touchleave", touchleave);
}
function touchstart(e) {
    if (this.__touch_id !== false) return;
    this.__touch_id = e.targetTouches[0].identifier;
    press_start.call(this);
    this.add_event("touchend", touchend);
    this.add_event("touchcancel", touchleave);
    this.add_event("touchleave", touchleave);
    e.preventDefault();
    e.stopPropagation();
    return false;
}
function contextmenu(e) {
    e.preventDefault();
    e.stopPropagation();
    return false;
}
var class_regex = /[^A-Za-z0-9_\-]/;
function is_class_name (str) {
    return !class_regex.test(str);
}

TK.Toggle = TK.class({
    /**
     * A toggle button. The toggle button can either be pressed (which means that it will
     * switch its state as long as it is pressed) or toggled permanently. Its behavior is
     * controlled by the two options <code>press</code> and <code>toggle</code>.
     *
     * @class TK.Toggle
     * 
     * @extends TK.Button
     *
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Boolean} [options.toggle=true] - If true, the button is toggled on click.
     * @property {Integer} [options.press=0] - Controls press behavior. If <code>options.toggle</code>
     *   is <code>false</code> and this option is <code>0</code>, the toggle button will toggle until
     *   released. If <code>options.toggle</code> is true and this option is a positive integer, it is
     *   interpreted as a milliseconds timeout. When pressing a button longer than this timeout, it will
     *   be toggled until released, otherwise it will be toggled permanently.
     * @property {Integer} [options.delay=0] - Delay all actions for n milliseconds. While actions are
     *   delayed, the widget has class <code>toolkit-delayed</code>. Use to force users to press the button
     *   for a certain amount of time before it actually gets toggled.
     * @property {String|Boolean} [options.icon_active=false] - An optional icon which is only displayed
     *   when the button toggle state is <code>true</code>. Please note that this option only works if `icon` is also set.
     * @property {String|Boolean} [options.label_active=false] - An optional label which is only displayed
     *   when the button toggle state is <code>true</code>. Please note that this option only works if `label` is also set.
     */
    _class: "Toggle",
    Extends: TK.Button,
    _options: Object.assign(Object.create(TK.Button.prototype._options), {
        label_active: "string",
        icon_active: "string",
        press: "int",
        delay: "int",
        toggle: "boolean",
    }),
    options: {
        label_active:  false,
        icon_active:   false,
        icon_inactive: false,
        press:         0,
        delay:         0,
        toggle:        true,
    },
    static_events: {
        mousedown: mousedown,
        touchstart: touchstart,
        contextmenu: contextmenu,
    },
    
    initialize: function (options) {
        TK.Button.prototype.initialize.call(this, options);
        /**
         * @member {HTMLDivElement} TK.Toggle#element - The main DIV container.
         *   Has class <code>toolkit-toggle</code>.
         */
        TK.add_class(this.element, "toolkit-toggle");
        this.__press_start_time = 0;
        this.__touch_id = false;
        this.__delayed_to = -1;
    },
    
    redraw: function () {
        var O = this.options;
        var I = this.invalid;
        if (I.state) {
            var tmp = (O.state && O.label_active) || O.label;
            if (tmp)
                this.label.set("label", tmp || "");
            tmp = (O.state && O.icon_active) || O.icon;
            if (tmp)
                this.icon.set("icon", tmp || "");
        }
        TK.Button.prototype.redraw.call(this);
    },
    /**
     * Toggle the button state.
     *
     * @method TK.Toggle#toggle
     * 
     * @emits TK.Toggle#toggled
     */
    toggle: function () {
        toggle.call(this, this.options);
        /**
         * Is fired when the button was toggled.
         * 
         * @event TK.Toggle#toggled
         * 
         * @param {boolean} state - The state of the {@link TK.Toggle}.
         */
    },
});
})(this, this.TK);
