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
var document = window.document;

/* this has no global symbol */
function CaptureState(start) {
    this.start = start;
    this.prev = start;
    this.current = start;
}
CaptureState.prototype = {
    /* distance from start */
    distance: function() {
        var v = this.vdistance();
        return Math.sqrt(v[0]*v[0] + v[1]*v[1]);
    },
    set_current: function(ev) {
        this.prev = this.current;
        this.current = ev;
        return true;
    },
    vdistance: function() {
        var start = this.start;
        var current = this.current;
        return [ current.clientX - start.clientX, current.clientY - start.clientY ];
    },
    prev_distance: function() {
        var prev = this.prev;
        var current = this.current;
        return [ current.clientX - prev.clientX, current.clientY - prev.clientY ];
    },
};
/* general api */
function startcapture(state) {
    /* do nothing, let other handlers be called */
    if (this.drag_state) return;
    
    /**
     * Capturing started.
     * 
     * @event TK.DragCapture#startcapture
     * 
     * @param {object} state - An internal state object.
     * @param {DOMEvent} start - The event object of the initial event.
     */

    var v = this.fire_event("startcapture", state, state.start);

    if (v === true) {
        /* we capture this event */
        this.drag_state = state;
        this.set("state", true);
    }

    return v;
}
function movecapture(ev) {
    var d = this.drag_state;
    
    /**
     * A movement was captured.
     * 
     * @event TK.DragCapture#movecapture
     * 
     * @param {DOMEvent} event - The event object of the current move event.
     */
     
    if (!d.set_current(ev) || this.fire_event("movecapture", d) === false) {
        stopcapture.call(this, ev);
        return false;
    }
}
function stopcapture(ev) {
    var s = this.drag_state;
    if (s === null) return;
    
    /**
     * Capturing stopped.
     * 
     * @event TK.DragCapture#stopcapture
     * 
     * @param {object} state - An internal state object.
     * @param {DOMEvent} event - The event object of the current event.
     */
     
    this.fire_event("stopcapture", s, ev);
    this.set("state", false);
    s.destroy();
    this.drag_state = null;
}

/* mouse handling */
function MouseCaptureState(start) {
    this.__mouseup = null;
    this.__mousemove = null;
    CaptureState.call(this, start);
}
MouseCaptureState.prototype = Object.assign(Object.create(CaptureState.prototype), {
    set_current: function(ev) {
        var start = this.start;
        /* If the buttons have changed, we assume that the capture has ended */
        if (!this.is_dragged_by(ev)) return false;
        return CaptureState.prototype.set_current.call(this, ev);
    },
    init: function(widget) {
        this.__mouseup = mouseup.bind(widget);
        this.__mousemove = mousemove.bind(widget);
        document.addEventListener("mousemove", this.__mousemove);
        document.addEventListener("mouseup", this.__mouseup);
    },
    destroy: function() {
        document.removeEventListener("mousemove", this.__mousemove);
        document.removeEventListener("mouseup", this.__mouseup);
        this.__mouseup = null;
        this.__mousemove = null;
    },
    is_dragged_by: function(ev) {
        var start = this.start;
        if (start.buttons !== ev.buttons || start.which !== ev.which) return false;
        return true;
    },
});
function mousedown(ev) {
    var s = new MouseCaptureState(ev);
    var v = startcapture.call(this, s);

    /* ignore this event */
    if (v === void(0)) return;

    ev.stopPropagation();
    ev.preventDefault();

    /* we did capture */
    if (v === true) s.init(this);

    return false;
}
function mousemove(ev) {
    movecapture.call(this, ev);
}
function mouseup(ev) {
    stopcapture.call(this, ev);
}

/* touch handling */

/*
 * Old Safari versions will keep the same Touch objects for the full lifetime
 * and simply update the coordinates, etc. This is a bug, which we work around by
 * cloning the information we need.
 */
function clone_touch(t) {
    return {
        clientX: t.clientX,
        clientY: t.clientY,
        identifier: t.identifier,
    };
}

function TouchCaptureState(start) {
    CaptureState.call(this, start);
    var touch = start.changedTouches.item(0);
    touch = clone_touch(touch);
    this.stouch = touch;
    this.ptouch = touch;
    this.ctouch = touch;
}
TouchCaptureState.prototype = Object.assign(Object.create(CaptureState.prototype), {
    find_touch: function(ev) {
        var id = this.stouch.identifier;
        var touches = ev.changedTouches;
        var touch;

        for (var i = 0; i < touches.length; i++) {
            touch = touches.item(i);
            if (touch.identifier === id) return touch;
        }

        return null;
    },
    set_current: function(ev) {
        var touch = clone_touch(this.find_touch(ev));
        this.ptouch = this.ctouch;
        this.ctouch = touch;
        return CaptureState.prototype.set_current.call(this, ev);
    },
    vdistance: function() {
        var start = this.stouch;
        var current = this.ctouch;
        return [ current.clientX - start.clientX, current.clientY - start.clientY ];
    },
    prev_distance: function() {
        var prev = this.ptouch;
        var current = this.ctouch;
        return [ current.clientX - prev.clientX, current.clientY - prev.clientY ];
    },
    destroy: function() {
    },
    is_dragged_by: function(ev) {
        return this.find_touch(ev) !== null;
    },
});
function touchstart(ev) {
    /* if cancelable is false, this is an async touchstart, which happens
     * during scrolling */
    if (!ev.cancelable) return;

    /* the startcapture event handler has return false. we do not handle this
     * pointer */
    var v = startcapture.call(this, new TouchCaptureState(ev));

    if (v === void(0)) return;

    ev.preventDefault();
    ev.stopPropagation();
    return false;
}
function touchmove(ev) {
    if (!this.drag_state) return;
    /* we are scrolling, ignore the event */
    if (!ev.cancelable) return;
    /* if we cannot find the right touch, some other touchpoint
     * triggered this event and we do not care about that */
    if (!this.drag_state.find_touch(ev)) return;
    /* if movecapture returns false, the capture has ended */
    if (movecapture.call(this, ev) !== false) {
        ev.preventDefault();
        ev.stopPropagation();
        return false;
    }
}
function touchend(ev) {
    var s;
    if (!ev.cancelable) return;
    s = this.drag_state;
    /* either we are not dragging or it is another touch point */
    if (!s || !s.find_touch(ev)) return;
    stopcapture.call(this, ev);
    ev.stopPropagation();
    ev.preventDefault();
    return false;
}
function touchcancel(ev) {
    return touchend.call(this, ev);
}
var dummy = function() {};

function get_parents(e) {
    var ret = [];
    if (Array.isArray(e)) e.map(function(e) { e = e.parentNode; if (e) ret.push(e); });
    else if (e = e.parentNode) ret.push(e);
    return ret;
}

var static_events = {
    set_node: function(value) {
        this.delegate_events(value);
    },
    contextmenu: function() { return false; },
    delegated: [
        function(element, old_element) {
            /* cancel the current capture */
            if (old_element) stopcapture.call(this);
        },
        function(elem, old) {
            /* NOTE: this works around a bug in chrome (#673102) */
            if (old) TK.remove_event_listener(get_parents(old), "touchstart", dummy);
            if (elem) TK.add_event_listener(get_parents(elem), "touchstart", dummy);
        }
    ],
    touchstart: touchstart,
    touchmove: touchmove,
    touchend: touchend,
    touchcancel: touchcancel,
    mousedown: mousedown,
};

TK.DragCapture = TK.class({
    
    /**
     * TK.DragCapture is a low-level class for tracking drag events on
     *   both, touch and mouse events. It can be used for implementing drag'n'drop
     *   functionality as well as dragging the value of e.g. {@link TK.Fader} or
     *   {@link TK.Knob}. {@link TK.DragValue} derives from TK.DragCapture.
     * 
     * @extends TK.Module
     *
     * @param {Object} widget - The parent widget making use of DragValue.
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {HTMLElement} [options.node] - The DOM element receiving the drag events. If not set the widgets element is used.
     * 
     * @class TK.DragCapture
     */
     
    Extends: TK.Module,
    _class: "DragCapture",
    _options: {
        node: "object",
        state: "boolean", /* internal, undocumented */
    },
    options: {
        state: false,
    },
    static_events: static_events,
    initialize: function(widget, O) {
        TK.Module.prototype.initialize.call(this, widget, O);
        this.drag_state = null;
        if (O.node === void(0)) O.node = widget.element;
        this.set("node", O.node);
    },
    destroy: function() {
        TK.Base.prototype.destroy.call(this);
        stopcapture.call(this);
    },
    cancel_drag: stopcapture,
    dragging: function() {
        return this.options.state;
    },
    state: function() {
        return this.drag_state;
    },
    is_dragged_by: function(ev) {
        return this.drag_state !== null && this.drag_state.is_dragged_by(ev);
    },
});
})(this, this.TK);
