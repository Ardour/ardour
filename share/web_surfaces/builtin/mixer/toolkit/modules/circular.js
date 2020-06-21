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
(function (w, TK) {
function interpret_label(x) {
    if (typeof x === "object") return x;
    if (typeof x === "number") return { pos: x };
    TK.error("Unsupported label type ", x);
}
var __rad = Math.PI / 180;
function _get_coords(deg, inner, outer, pos) {
    deg = +deg;
    inner = +inner;
    outer = +outer;
    pos = +pos;
    deg = deg * __rad;
    return {
        x1: Math.cos(deg) * outer + pos,
        y1: Math.sin(deg) * outer + pos,
        x2: Math.cos(deg) * inner + pos,
        y2: Math.sin(deg) * inner + pos
    }
}
function _get_coords_single(deg, inner, pos) {
    deg = +deg;
    inner = +inner;
    pos = +pos;
    deg = deg * __rad;
    return {
        x: Math.cos(deg) * inner + pos,
        y: Math.sin(deg) * inner + pos
    }
}
var format_path = TK.FORMAT("M %f,%f " +
                            "A %f,%f 0 %d,%d %f,%f " +
                            "L %f,%f " +
                            "A %f,%f 0 %d,%d %f,%f z");
var format_translate = TK.FORMAT("translate(%f, %f)");
var format_translate_rotate = TK.FORMAT("translate(%f %f) rotate(%f %f %f)");
var format_rotate = TK.FORMAT("rotate(%f %f %f)");

function draw_dots() {
    // depends on dots, dot, min, max, size
    var _dots = this._dots;
    var O = this.options;
    var dots = O.dots;
    var dot = O.dot;
    var angle = O.angle;
    TK.empty(_dots);
    for (var i = 0; i < dots.length; i++) {
        var m = dots[i];
        var r = TK.make_svg("rect", {"class": "toolkit-dot"});
        
        var length = m.length === void(0)
                   ? dot.length : m.length;
        var width  = m.width === void(0)
                   ? dot.width : m.width;
        var margin = m.margin === void(0)
                   ? dot.margin : m.margin;
        var pos    = Math.min(O.max, Math.max(O.min, m.pos));
        // TODO: consider adding them all at once
        _dots.appendChild(r);
        if (m["class"]) TK.add_class(r, m["class"]);
        if (m.color) r.style.fill = m.color;
                 
        r.setAttribute("x", O.size - length - margin);
        r.setAttribute("y", O.size / 2 - width / 2);
        
        r.setAttribute("width",  length);
        r.setAttribute("height", width);
        
        r.setAttribute("transform", "rotate("
            + (this.val2coef(this.snap(pos)) * angle) + " "
            + (O.size / 2) + " " + (this.options.size / 2) + ")");
    }
    /**
     * Is fired when dots are (re)drawn.
     * @event TK.Circular#dotsdrawn
     */
    this.fire_event("dotsdrawn");
}
function draw_markers() {
    // depends on size, markers, marker, min, max
    var I = this.invalid;
    var O = this.options;
    var markers = O.markers;
    var marker = O.marker;
    TK.empty(this._markers);
    
    var stroke  = this._get_stroke();
    var outer   = O.size / 2;
    var angle = O.angle;
    
    for (var i = 0; i < markers.length; i++) {
        var m       = markers[i];
        var thick   = m.thickness === void(0)
                    ? marker.thickness : m.thickness;
        var margin  = m.margin === void(0)
                    ? marker.margin : m.margin;
        var inner   = outer - thick;
        var outer_p = outer - margin - stroke / 2;
        var inner_p = inner - margin - stroke / 2;
        var from, to;
        
        if (m.from === void(0))
            from = O.min;
        else
            from = Math.min(O.max, Math.max(O.min, m.from));
        
        if (m.to === void(0))
            to = O.max;
        else
            to = Math.min(O.max, Math.max(O.min, m.to));
        
        var s = TK.make_svg("path", {"class": "toolkit-marker"});
        this._markers.appendChild(s);
        
        if (m["class"]) TK.add_class(s, m["class"]);
        if (m.color) s.style.fill = m.color;
        if (!m.nosnap) {
            from = this.snap(from);
            to = this.snap(to);
        }
        from = this.val2coef(from) * angle;
        to = this.val2coef(to) * angle;
        
        draw_slice.call(this, from, to, inner_p, outer_p, outer, s);
    }
    /**
     * Is fired when markers are (re)drawn.
     * @event TK.Circular#markersdrawn
     */
    this.fire_event("markersdrawn");
}
function draw_labels() {
    // depends on size, labels, label, min, max, start
    var _labels = this._labels;
    var O = this.options;
    var labels = O.labels;
    TK.empty(this._labels);

    if (!labels.length) return;
    
    var outer   = O.size / 2;
    var a = new Array(labels.length);
    var i;

    var l, p, positions = new Array(labels.length);

    for (i = 0; i < labels.length; i++) {
        l = labels[i];
        p = TK.make_svg("text", {"class": "toolkit-label",
                                 style: "dominant-baseline: central;"
        });
        
        if (l["class"]) TK.add_class(p, l["class"]);
        if (l.color) p.style.fill = l.color;

                 
        if (l.label !== void(0))
            p.textContent = l.label;
        else
            p.textContent = O.label.format(l.pos);

        p.setAttribute("text-anchor", "middle");
                 
        _labels.appendChild(p);
        a[i] = p;
    }
    /* FORCE_RELAYOUT */

    TK.S.add(function() {
        var i, p;
        for (i = 0; i < labels.length; i++) {
            l = labels[i];
            p = a[i];

            var margin  = l.margin !== void(0) ? l.margin : O.label.margin;
            var align   = (l.align !== void(0) ? l.align : O.label.align) === "inner";
            var pos     = Math.min(O.max, Math.max(O.min, l.pos));
            var bb      = p.getBBox();
            var angle   = (this.val2coef(this.snap(pos)) * O.angle + O.start) % 360;
            var outer_p = outer - margin;
            var coords  = _get_coords_single(angle, outer_p, outer);
            
            var mx = ((coords.x - outer) / outer_p) * (bb.width + bb.height / 2.5) / (align ? -2 : 2);
            var my = ((coords.y - outer) / outer_p) * bb.height / (align ? -2 : 2);

            positions[i] = format_translate(coords.x + mx, coords.y + my);
        }

        TK.S.add(function() {
            for (i = 0; i < labels.length; i++) {
                p = a[i];
                p.setAttribute("transform", positions[i]);
            }
            /**
             * Is fired when labels are (re)drawn.
             * @event TK.Circular#labelsdrawn
             */
            this.fire_event("labelsdrawn");
        }.bind(this), 1);
    }.bind(this));
}
function draw_slice(a_from, a_to, r_inner, r_outer, pos, slice) {
    a_from = +a_from;
    a_to = +a_to;
    r_inner = +r_inner;
    r_outer = +r_outer;
    pos = +pos;
    // ensure from !== to
    if(a_from % 360 === a_to % 360) a_from += 0.001;
    // ensure from and to in bounds
    while (a_from < 0) a_from += 360;
    while (a_to < 0) a_to += 360;
    if (a_from > 360) a_from %= 360;
    if (a_to > 360) a_to   %= 360;
    // get drawing direction (sweep = clock-wise)
    if (this.options.reverse && a_to <= a_from
    || !this.options.reverse && a_to > a_from)
        var sweep = 1;
    else
        var sweep = 0;
    // get large flag
    if (Math.abs(a_from - a_to) >= 180)
        var large = 1;
    else
        var large = 0;
    // draw this slice
    var from = _get_coords(a_from, r_inner, r_outer, pos);
    var to = _get_coords(a_to, r_inner, r_outer, pos);

    var path = format_path(from.x1, from.y1,
                           r_outer, r_outer, large, sweep, to.x1, to.y1,
                           to.x2, to.y2,
                           r_inner, r_inner, large, !sweep, from.x2, from.y2);
    slice.setAttribute("d", path);
}
TK.Circular = TK.class({
    /**
     * TK.Circular is a SVG group element containing two paths for displaying
     * numerical values in a circular manner. TK.Circular is able to draw labels,
     * dots and markers and can show a hand. TK.Circular e.g. is implemented by
     * {@link TK.Clock} to draw hours, minutes and seconds.
     * 
     * @class TK.Circular
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Number} [options.value=0] - Sets the value on the hand and on the
     *   ring at the same time.
     * @property {Number} [options.value_hand=0] - Sets the value on the hand.
     * @property {Number} [options.value_ring=0] - Sets the value on the ring.
     * @property {Number} [options.size=100] - The diameter of the circle. This
     *   is the base value for all following layout-related parameters. Keeping
     *   it set to 100 offers percentual lenghts. Set the final size of the widget
     *   via CSS.
     * @property {Number} [options.thickness=3] - The thickness of the circle.
     * @property {Number} [options.margin=0] - The margin between base and value circles.
     * @property {Boolean} [options.show_hand=true] - Draw the hand.
     * @property {Object} [options.hand] - Dimensions of the hand.
     * @property {Number} [options.hand.width=2] - Width of the hand.
     * @property {Number} [options.hand.length=30] - Length of the hand.
     * @property {Number} [options.hand.margin=10] - Margin of the hand.
     * @property {Number} [options.start=135] - The starting point in degrees.
     * @property {Number} [options.angle=270] - The maximum degree of the rotation when
     *   <code>options.value === options.max</code>.
     * @property {Number|Boolean} [options.base=false] - If a base value is set in degrees,
     *   circular starts drawing elements from this position.
     * @property {Boolean} [options.show_base=true] - Draw the base ring.
     * @property {Boolean} [options.show_value=true] - Draw the value ring.
     * @property {Number} [options.x=0] - Horizontal displacement of the circle.
     * @property {Number} [options.y=0] - Vertical displacement of the circle.
     * @property {Boolean} [options.show_dots=true] - Show/hide all dots.
     * @property {Object} [options.dot] - This option acts as default values for the individual dots
     *   specified in <code>options.dots</code>.
     * @property {Number} [options.dot.width=2] - Width of the dots.
     * @property {Number} [options.dot.length=2] - Length of the dots.
     * @property {Number} [options.dot.margin=5] - Margin of the dots.
     * @property {Array<Object>} [options.dots=[]] - An array of objects describing where dots should be placed
     *   along the circle. Members are position <code>pos</code> in the value range and optionally
     *   <code>color</code> and <code>class</code> and any of the properties of <code>options.dot</code>.
     * @property {Boolean} [options.show_markers=true] - Show/hide all markers.
     * @property {Object} [options.marker] - This option acts as default values of the individual markers
     *   specified in <code>options.markers</code>.
     * @property {Number} [options.marker.thickness=3] - Thickness of the marker.
     * @property {Number} [options.marker.margin=3] - Margin of the marker.
     * @property {Array<Object>} [options.markers=[]] - An array containing objects which describe where markers
     *   are to be places. Members are the position as <code>from</code> and <code>to</code> and optionally
     *   <code>color</code>, <code>class</code> and any of the properties of <code>options.marker</code>.
     * @property {Boolean} [options.show_labels=true] - Show/hide all labels.
     * @property {Object} [options.label] - This option acts as default values for the individual labels
     *   specified in <code>options.labels</code>.
     * @property {Integer} [options.label.margin=8] - Distance of the label from the circle of diameter
     *   <code>options.size</code>.
     * @property {String} [options.label.align="outer"] - This option controls if labels are positioned
     *   inside or outside of the circle with radius <code>options.size/2 - margin</code>.
     * @property {Function} [options.label.format] - Optional formatting function for the label.
     *   Receives the label value as first argument.
     * @property {Array<Object>} [options.labels=[]] - An array containing objects which describe labels
     *   to be displayed. Either a value or an object whose members are the position <code>pos</code>
     *   insie the value range and optionally <code>color</code>, <code>class</code> and any of the
     *   properties of <code>options.label</code>.
     * 
     * @extends TK.Widget
     * 
     * @mixes TK.Warning
     * 
     * @mixes TK.Ranged
     */
    _class: "Circular",
    Extends: TK.Widget,
    Implements: [TK.Warning, TK.Ranged],
    _options: Object.assign(Object.create(TK.Widget.prototype._options), TK.Ranged.prototype._options, {
        value: "number",
        value_hand: "number",
        value_ring: "number",
        size: "number",
        thickness: "number",
        margin: "number",
        hand: "object",
        start: "number",
        angle: "number",
        base: "number|boolean",
        show_base: "boolean",
        show_value: "boolean",
        show_hand: "boolean",
        x: "number",
        y: "number",
        dot: "object",
        dots: "array",
        marker: "object",
        markers: "array",
        label: "object",
        labels: "array"
    }),
    static_events: {
        set_value: function(value) {
            this.set("value_hand", value);
            this.set("value_ring", value);
        },
        initialized: function() {
            // calculate the stroke here once. this happens before
            // the initial redraw
            TK.S.after_frame(this._get_stroke.bind(this));
            this.set("value", this.options.value);
        },
    },
    options: {
        value:      0,
        value_hand: 0,
        value_ring: 0,
        size:       100,
        thickness:  3,
        margin:     0,
        hand:       {width: 2, length: 30, margin: 10},
        start:      135,
        angle:      270,
        base:       false,
        show_base:  true,
        show_value: true,
        show_hand:  true,
        x:          0,
        y:          0,
        dot:        {width: 2, length: 2, margin: 5},
        dots:       [],
        marker:     {thickness: 3, margin: 0},
        markers:    [],
        label:      {margin: 8, align: "inner", format: function(val){return val;}},
        labels:     []
    },
    
    initialize: function (options) {
        TK.Widget.prototype.initialize.call(this, options);
        var E;
        
        /**
         * @member {SVGImage} TK.Circular#element - The main SVG element.
         *      Has class <code>toolkit-circular</code> 
         */
        this.element = E = TK.make_svg("g", {"class": "toolkit-circular"});
        this.widgetize(E, true, true, true);
        
        /**
         * @member {SVGPath} TK.Circular#_base - The base of the ring.
         *      Has class <code>toolkit-base</code> 
         */
        this._base = TK.make_svg("path", {"class": "toolkit-base"});
        E.appendChild(this._base);
        
        /**
         * @member {SVGPath} TK.Circular#_value - The ring showing the value.
         *      Has class <code>toolkit-value</code> 
         */
        this._value = TK.make_svg("path", {"class": "toolkit-value"});
        E.appendChild(this._value);
        
        /**
         * @member {SVGRect} TK.Circular#_hand - The hand of the knob.
         *      Has class <code>toolkit-hand</code> 
         */
        this._hand = TK.make_svg("rect", {"class": "toolkit-hand"});
        E.appendChild(this._hand);

        if (this.options.labels)
            this.set("labels", this.options.labels);
    },

    resize: function () {
        this.invalid.labels = true;
        this.trigger_draw();
        TK.Widget.prototype.resize.call(this);
    },
    
    redraw: function () {
        TK.Widget.prototype.redraw.call(this);
        var I = this.invalid;
        var O = this.options;
        var E = this.element;
        var outer   = O.size / 2;
        var tmp;

        if (I.validate("x", "y") || I.start || I.size) {
            E.setAttribute("transform", format_translate_rotate(O.x, O.y, O.start, outer, outer));
            this._labels.setAttribute("transform", format_rotate(-O.start, outer, outer));
        }

        if (O.show_labels && (I.validate("show_labels", "labels", "label") ||
                              I.size || I.min || I.max || I.start)) {
            draw_labels.call(this);
        }

        if (O.show_dots && (I.validate("show_dots", "dots", "dot") ||
                            I.min || I.max || I.size)) {
            draw_dots.call(this);
        }

        if (O.show_markers && (I.validate("show_markers", "markers", "marker") ||
                               I.size || I.min || I.max)) {
            draw_markers.call(this);
        }

        var stroke  = this._get_stroke();
        var inner   = outer - O.thickness;
        var outer_p = outer - stroke / 2 - O.margin;
        var inner_p = inner - stroke / 2 - O.margin;
        
        if (I.show_value || I.value_ring) {
            I.show_value = false;
            if (O.show_value) {
                draw_slice.call(this, this.val2coef(this.snap(O.base)) * O.angle, this.val2coef(this.snap(O.value_ring)) * O.angle, inner_p, outer_p, outer,
                                this._value);
            } else {
                this._value.removeAttribute("d");
            }
        }

        if (I.show_base) {
            I.show_base = false;
            if (O.show_base) {
                draw_slice.call(this, 0, O.angle, inner_p, outer_p, outer, this._base);
            } else {
                /* TODO: make this a child element */
                this._base.removeAttribute("d");
            }
        }
        if (I.show_hand) {
            I.show_hand = false;
            if (O.show_hand) {
                this._hand.style.display = "block";
            } else {
                this._hand.style.display = "none";
            }
        }
        if (I.validate("size", "value_hand", "hand", "min", "max", "start")) {
            tmp = this._hand;
            tmp.setAttribute("x", O.size - O.hand.length - O.hand.margin);
            tmp.setAttribute("y", (O.size - O.hand.width) / 2.0);
            tmp.setAttribute("width", O.hand.length);
            tmp.setAttribute("height",O.hand.width);
            tmp.setAttribute("transform",
                             format_rotate(this.val2coef(this.snap(O.value_hand)) * O.angle, O.size / 2, O.size / 2));
        }
    },
    
    destroy: function () {
        this._dots.remove();
        this._markers.remove();
        this._base.remove();
        this._value.remove();
        TK.Widget.prototype.destroy.call(this);
    },
    _get_stroke: function () {
        if (this.hasOwnProperty("_stroke")) return this._stroke;
        var strokeb = parseInt(TK.get_style(this._base, "stroke-width")) || 0;
        var strokev = parseInt(TK.get_style(this._value, "stroke-width")) || 0;
        this._stroke = Math.max(strokeb, strokev);
        return this._stroke;
    },

    /**
     * Adds a label.
     *
     * @method TK.Circular#add_label
     * @param {Object|Number} label - The label. Please refer to the initial options
     *   to learn more about possible values.
     * @returns {Object} label - The interpreted object to build the label from.
     */
    add_label: function(label) {
        var O = this.options;

        if (!O.labels) {
            O.labels = [];
        }

        label = interpret_label(label);
        
        if (label) {
            O.labels.push(label);
            this.invalid.labels = true;
            this.trigger_draw();
            return label;
        }
    },

    /**
     * Removes a label.
     *
     * @method TK.Circular#remove_label
     * @param {Object} label - The label object as returned from `add_label`.
     * @returns {Object} label - The removed label options.
     */
    remove_label: function(label) {
        var O = this.options;

        if (!O.labels) return;

        var i = O.labels.indexOf(label);

        if (i === -1) return;

        O.labels.splice(i);
        this.invalid.labels = true;
        this.trigger_draw();
    },
    
    // GETTERS & SETTERS
    set: function (key, value) {
        switch (key) {
        case "dot":
        case "marker":
        case "label":
            value = Object.assign(this.options[key], value);
            break;
        case "base":
            if (value === false) value = this.options.min;
            break;
        case "value":
            if (value > this.options.max || value < this.options.min)
                this.warning(this.element);
            value = this.snap(value);
            break;
        case "labels":
            if (value)
                for (var i = 0; i < value.length; i++) {
                    value[i] = interpret_label(value[i]);
                }
            break;
        }

        return TK.Widget.prototype.set.call(this, key, value);
    }
});
/**
 * @member {SVGGroup} TK.Circular#_markers - A SVG group containing all markers.
 *      Has class <code>toolkit-markers</code> 
 */
TK.ChildElement(TK.Circular, "markers", {
    //option: "markers",
    //display_check: function(v) { return !!v.length; },
    show: true,
    create: function() {
        return TK.make_svg("g", {"class": "toolkit-markers"});
    },
});
/** 
 * @member {SVGGroup} TK.Circular#_dots - A SVG group containing all dots.
 *      Has class <code>toolkit-dots</code> 
 */
TK.ChildElement(TK.Circular, "dots", {
    //option: "dots",
    //display_check: function(v) { return !!v.length; },
    show: true,
    create: function() {
        return TK.make_svg("g", {"class": "toolkit-dots"});
    },
});
/**
 * @member {SVGGroup} TK.Circular#_labels - A SVG group containing all labels.
 *      Has class <code>toolkit-labels</code> 
 */
TK.ChildElement(TK.Circular, "labels", {
    //option: "labels",
    //display_check: function(v) { return !!v.length; },
    show: true,
    create: function() {
        return TK.make_svg("g", {"class": "toolkit-labels"});
    },
});
})(this, this.TK);
