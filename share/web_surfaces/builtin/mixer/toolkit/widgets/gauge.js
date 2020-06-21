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
function _get_coords_single(deg, inner, pos) {
    deg = deg * Math.PI / 180;
    return {
        x: Math.cos(deg) * inner + pos,
        y: Math.sin(deg) * inner + pos
    }
}
var format_translate = TK.FORMAT("translate(%f, %f)");
var format_viewbox = TK.FORMAT("0 0 %d %d");
/**
 * TK.Gauge draws a single {@link TK.Circular} into a SVG image. It inherits
 * all options of {@link TK.Circular}.
 *
 * @class TK.Gauge
 * 
 * @extends TK.Widget
 *
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {Number} [options.x=0] - Displacement of the {@link TK.Circular}
 *   in horizontal direction. This allows drawing gauges which are only
 *   represented by a segment of a circle.
 * @property {Number} [options.y=0] - Displacement of the {@link TK.Circular}
 *   in vertical direction.
 * @property {Object} [options.title] - Optional gauge title.
 * @property {Number} [options.title.pos] - Position inside of the circle in
 *   degrees.
 * @property {String} [options.title.title] - Title string.
 * @property {Number} [options.title.margin] - Margin of the title string.
 * @property {String} [options.title.align] - Alignment of the title, either
 *   <code>inner</code> or <code>outer</code>.
 */
TK.Gauge = TK.class({
    _class: "Gauge",
    Extends: TK.Widget,
    _options: Object.assign(Object.create(TK.Circular.prototype._options), {
        width:  "number",
        height: "number",
        title: "object",
    }),
    options: Object.assign({}, TK.Circular.prototype.options, {
        width:  120, // width of the element
        height: 120, // height of the svg
        size:   120,
        title: {pos: 90, margin: 0, align: "inner", title:""}
    }),
    initialize: function (options) {
        if (typeof options.title === "string")
            options.title = {title: options.title};
        TK.Widget.prototype.initialize.call(this, options);
        var O = this.options;
        var E, S;
        if (!(E = this.element)) this.element = E = TK.element("div");
        
        /**
         * @member {SVGImage} TK.Gauge#svg - The main SVG image.
         */
        this.svg = S = TK.make_svg("svg");
        
        /**
         * @member {HTMLDivElement} TK.Gauge#element - The main DIV container.
         *   Has class <code>toolkit-gauge</code>.
         */
        TK.add_class(E, "toolkit-gauge");
        this.widgetize(E, true, true, true);
        
        /**
         * @member {SVGText} TK.Gauge#_title - The title of the gauge.
         *   Has class <code>toolkit-title</code>.
         */
        this._title = TK.make_svg("text", {"class": "toolkit-title"});
        S.appendChild(this._title);

        var co = TK.object_and(O, TK.Circular.prototype._options);
        co = TK.object_sub(co, TK.Widget.prototype._options);
        co.container = S;
        
        /**
         * @member {TK.Circular} TK.Gauge#circular - The {@link TK.Circular} module.
         */
        this.circular = new TK.Circular(co);
        this.add_child(this.circular);
        this.widgetize(this.element);
        E.appendChild(S);
    },
    resize: function() {
        TK.Widget.prototype.resize.call(this);
        this.invalid.title = true;
        this.trigger_draw();
    },
    redraw: function() {
        var I = this.invalid, O = this.options;
        var S = this.svg;

        TK.Widget.prototype.redraw.call(this);

        if (I.validate("width", "height")) {
            S.setAttribute("viewBox", format_viewbox(O.width, O.height));
        }

        if (I.validate("title", "size", "x", "y")) {
            var _title = this._title;
            _title.textContent = O.title.title;

            if (O.title.title) {
                TK.S.add(function() {
                    var t = O.title;
                    var outer   = O.size / 2;
                    var margin  = t.margin;
                    var align   = t.align === "inner";
                    var bb      = _title.getBoundingClientRect();
                    var angle   = t.pos % 360;
                    var outer_p = outer - margin;
                    var coords  = _get_coords_single(angle, outer_p, outer);

                    var mx = ((coords.x - outer) / outer_p)
                           * (bb.width + bb.height / 2.5) / (align ? -2 : 2);
                    var my = ((coords.y - outer) / outer_p)
                           * bb.height / (align ? -2 : 2);
                    
                    mx += O.x;
                    my += O.y;
                           
                    TK.S.add(function() {
                        _title.setAttribute("transform", format_translate(coords.x + mx, coords.y + my));
                        _title.setAttribute("text-anchor", "middle");
                    }.bind(this), 1);
                    /**
                     * Is fired when the title changed.
                     * 
                     * @event TK.Gauge#titledrawn
                     */
                    this.fire_event("titledrawn");
                }.bind(this));
            }
        }
    },
    
    // GETTERS & SETTERS
    set: function (key, value) {
        if (key === "title") {
            if (typeof value === "string") value = {title: value};
            value = Object.assign(this.options.title, value);
        }
        // TK.Circular does the snapping
        if (!TK.Widget.prototype._options[key] && TK.Circular.prototype._options[key])
            value = this.circular.set(key, value);
        return TK.Widget.prototype.set.call(this, key, value);
    }
});
})(this, this.TK);
