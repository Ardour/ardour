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
function draw_lines(a, mode, last) {
    var labels = new Array(a.length);
    var coords = new Array(a.length);
    var i, label, obj;

    for (i = 0; i < a.length; i++) {
        obj = a[i];
        if (obj.label) {
            label = TK.make_svg("text");
            label.textContent = obj.label;
            label.style["dominant-baseline"] = "central";
            TK.add_class(label, "toolkit-grid-label");
            TK.add_class(label, mode ? "toolkit-horizontal" : "toolkit-vertical");
            if (obj["class"]) TK.add_class(label, obj["class"]);

            this.element.appendChild(label);
            labels[i] = label;
        }
    }

    var w  = this.range_x.options.basis;
    var h  = this.range_y.options.basis;


    TK.S.add(function() {
        /* FORCE_RELAYOUT */

        for (i = 0; i < a.length; i++) {
            obj = a[i];
            label = labels[i];
            if (!label) continue;
            var bb;
            try {
                bb = label.getBBox();
            } catch(e) {
                // if we are hidden, this may throw
                // we should force redraw at some later point, but
                // its hard to do. the grid should really be deactivated
                // by an option.
                continue;
            }
            var tw = bb.width;
            var th = bb.height;
            var p  = TK.get_style(label, "padding").split(" ");
            if (p.length < 2)
                p[1] = p[2] = p[3] = p[0];
            if (p.length < 3) {
                p[2] = p[0];
                p[3] = p[1];
            }
            if (p.length < 4)
                p[3] = p[1];
            var pt = parseInt(p[0]) || 0;
            var pr = parseInt(p[1]) || 0;
            var pb = parseInt(p[2]) || 0;
            var pl = parseInt(p[3]) || 0;
            var x, y;
            if (mode) {
                y = Math.max(th / 2, Math.min(h - th / 2 - pt, this.range_y.val2px(obj.pos)));
                if (y > last) continue;
                x = w - tw - pl;
                coords[i] = {
                    x : x,
                    y : y,
                    m : tw + pl + pr,
                };
                last = y - th;
            } else {
                x = Math.max(pl, Math.min(w - tw - pl, this.range_x.val2px(obj.pos) - tw / 2));
                if (x < last) continue;
                y = h-th/2-pt;
                coords[i] = {
                    x : x,
                    y : y,
                    m : th + pt + pb,
                };
                last = x + tw;
            }
        }

        TK.S.add(function() {
            for (i = 0; i < a.length; i++) {
                label = labels[i];
                if (label) {
                    obj = coords[i];
                    if (obj) {
                        label.setAttribute("x", obj.x);
                        label.setAttribute("y", obj.y);
                    } else {
                        if (label.parentElement == this.element)
                            this.element.removeChild(label);
                    }
                }
            }

            for (i = 0; i < a.length; i++) {
                obj = a[i];
                label = coords[i];
                var m;
                if (label) m = label.m;
                else m = 0;

                if ((mode && obj.pos === this.range_y.options.min)
                || ( mode && obj.pos === this.range_y.options.max)
                || (!mode && obj.pos === this.range_x.options.min)
                || (!mode && obj.pos === this.range_x.options.max))
                    continue;
                var line = TK.make_svg("path");
                TK.add_class(line, "toolkit-grid-line");
                TK.add_class(line, mode ? "toolkit-horizontal" : "toolkit-vertical");
                if (obj["class"]) TK.add_class(line, obj["class"]);
                if (obj.color) line.setAttribute("style", "stroke:" + obj.color);
                if (mode) {
                    // line from left to right
                    line.setAttribute("d", "M0 " + Math.round(this.range_y.val2px(obj.pos))
                        + ".5 L"  + (this.range_x.options.basis - m) + " "
                        + Math.round(this.range_y.val2px(obj.pos)) + ".5");
                } else {
                    // line from top to bottom
                    line.setAttribute("d", "M" + Math.round(this.range_x.val2px(obj.pos))
                        + ".5 0 L"  + Math.round(this.range_x.val2px(obj.pos))
                        + ".5 " + (this.range_y.options.basis - m));
                }
                this.element.appendChild(line);
            }
        }.bind(this), 1);
    }.bind(this));
}
TK.Grid = TK.class({
    /**
     * TK.Grid creates a couple of lines and labels in a SVG
     * image on the x and y axis. It is used in e.g. {@link TK.Graph} and
     * {@link TK.FrequencyResponse} to draw markers and values. TK.Grid needs a
     * parent SVG image do draw into. The base element of a TK.Grid is a
     * SVG group containing all the labels and lines.
     *
     * @class TK.Grid
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Array<Object>} [options.grid_x=[]] - Array for vertical grid line definitions with the members:
     * @property {Number} [options.grid_x.pos] - The value where to draw grid line and correspon ding label.
     * @property {String} [options.grid_x.color] - A valid CSS color string to colorize the elements.
     * @property {String} [options.grid_x.class] - A class name for the elements.
     * @property {String} [options.grid_x.label] - A label string.
     * @property {Array<Object>} [options.grid_y=[]] - Array for horizontal grid lines with the members:
     * @property {Number} [options.grid_y.pos] - The value where to draw grid line and corresponding label.
     * @property {String} [options.grid_y.color] - A valid CSS color string to colorize the elements.
     * @property {String} [options.grid_y.class] - A class name for the elements.
     * @property {String} [options.grid_y.label] - A label string.
     * @property {Function|Object} [options.range_x={}] - A function returning
     *   a {@link TK.Range} instance for vertical grid lines or an object
     *   containing options. for a new {@link Range}.
     * @property {Function|Object} [options.range_y={}] - A function returning
     *   a {@link TK.Range} instance for horizontal grid lines or an object
     *   containing options. for a new {@link Range}.
     * @property {Number} [options.width=0] - Width of the grid.
     * @property {Number} [options.height=0] - Height of the grid.
     * 
     * @extends TK.Widget
     * 
     * @mixes TK.Ranges
     */
    _class: "Grid",
    Extends: TK.Widget,
    Implements: TK.Ranges,
    _options: Object.assign(Object.create(TK.Widget.prototype._options), {
        grid_x: "array",
        grid_y: "array",
        range_x: "object",
        range_y: "object",
        width: "number",
        height: "number",
    }),
    options: {
        grid_x:  [],
        grid_y:  [],
        range_x: {}, 
        range_y: {},
        width:   0,
        height:  0
    },
    initialize: function (options) {
        TK.Widget.prototype.initialize.call(this, options);
        /**
         * @member {SVGGroup} TK.Grid#element - The main SVG group containing all grid elements. Has class <code>toolkit-grid</code>.
         */
        this.element = this.widgetize(
                       TK.make_svg("g", {"class": "toolkit-grid"}), true, true, true);
        /**
         * @member {TK.Range} TK.Grid#range_x - The range for the x axis. 
         */
        /**
         * @member {TK.Range} TK.Grid#range_y - The range for the y axis.
         */
        this.add_range(this.options.range_x, "range_x");
        this.add_range(this.options.range_y, "range_y");
        if (this.options.width)
            this.set("width", this.options.width);
        if (this.options.height)
            this.set("height", this.options.width);
        this.invalidate_ranges = function (key, value) {
            this.invalid.range_x = true;
            this.invalid.range_y = true;
            this.trigger_draw();
        }.bind(this);
        this.range_x.add_event("set", this.invalidate_ranges);
        this.range_y.add_event("set", this.invalidate_ranges);
    },
    
    redraw: function () {
        var I = this.invalid, O = this.options;
        if (I.validate("grid_x", "grid_y", "range_x", "range_y")) {
            TK.empty(this.element);

            draw_lines.call(this, O.grid_x, false, 0);
            draw_lines.call(this, O.grid_y, true, this.range_y.options.basis);
        }
        TK.Widget.prototype.redraw.call(this);
    },
    destroy: function () {
        this.range_x.remove_event("set", this.invalidate_ranges);
        this.range_y.remove_event("set", this.invalidate_ranges);
        TK.Widget.prototype.destroy.call(this);
    },
    // GETTER & SETTER
    set: function (key, value) {
        this.options[key] = value;
        switch (key) {
            case "grid_x":
            case "grid_y":
                /**
                 * Is fired when the grid has changed.
                 * 
                 * @event TK.Grid#gridchanged
                 * 
                 * @param {Array} grid_x - The grid elements for x axis.
                 * @param {Array} grid_y - The grid elements for y axis.
                 */
                this.fire_event("gridchanged", this.options.grid_x, this.options.grid_y);
                break;
            case "width":
                this.range_x.set("basis", value);
                break;
            case "height":
                this.range_y.set("basis", value);
                break;
        }
        TK.Widget.prototype.set.call(this, key, value);
    }
});
})(this, this.TK);
