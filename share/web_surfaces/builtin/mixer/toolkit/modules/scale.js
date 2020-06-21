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
function get_base(O) {
    return Math.max(Math.min(O.max, O.base), O.min);
}
function vert(O) {
    return O.layout === "left" || O.layout === "right";
}
function fill_interval(range, levels, i, from, to, min_gap, result) {
    var level = levels[i];
    var x, j, pos, last_pos, last;
    var diff;

    var to_pos = range.val2px(to);
    last_pos = range.val2px(from);

    if (Math.abs(to_pos - last_pos) < min_gap) return;

    if (!result) result = {
        values: [],
        positions: [],
    };

    var values = result.values;
    var positions = result.positions;

    if (from > to) level = -level;
    last = from;

    for (j = ((to-from)/level)|0, x = from + level; j > 0; x += level, j--) {
        pos = range.val2px(x);
        diff = Math.abs(last_pos - pos);
        if (Math.abs(to_pos - pos) < min_gap) break;
        if (diff >= min_gap) {
            if (i > 0 && diff >= min_gap * 2) {
                // we have a chance to fit some more labels in
                fill_interval(range, levels, i-1,
                              last, x, min_gap, result);
            }
            values.push(x);
            positions.push(pos);
            last_pos = pos;
            last = x;
        }
    }

    if (i > 0 && Math.abs(last_pos - to_pos) >= min_gap * 2) {
        fill_interval(range, levels, i-1, last, to, min_gap, result);
    }

    return result;
}
// remove collisions from a with b given a minimum gap
function remove_collisions(a, b, min_gap, vert) {
    var pa = a.positions, pb = b.positions;
    var va = a.values;
    var dim;

    min_gap = +min_gap;

    if (typeof vert === "boolean")
        dim = vert ? b.height : b.width;

    if (!(min_gap > 0)) min_gap = 1;

    if (!pb.length) return a;

    var i, j;
    var values = [];
    var positions = [];
    var pos_a, pos_b;
    var size;

    var last_pos = +pb[0],
        last_size = min_gap;

    if (dim) last_size += +dim[0] / 2;

    // If pb is just length 1, it does not matter
    var direction = pb.length > 1 && pb[1] < last_pos ? -1 : 1;

    for (i = 0, j = 0; i < pa.length && j < pb.length;) {
        pos_a = +pa[i];
        pos_b = +pb[j];
        size = min_gap;

        if (dim) size += dim[j] / 2;

        if (Math.abs(pos_a - last_pos) < last_size ||
            Math.abs(pos_a - pos_b) < size) {
            // try next position
            i++;
            continue;
        }

        if (j < pb.length - 1 && (pos_a - pos_b)*direction > 0) {
            // we left the current interval, lets try the next one
            last_pos = pos_b;
            last_size = size;
            j++;
            continue;
        }

        values.push(+va[i]);
        positions.push(pos_a);

        i++;
    }

    return {
        values: values,
        positions: positions,
    };
}
function create_dom_nodes(data, create) {
    var nodes = [];
    var values, positions;
    var i;
    var E = this.element;
    var node;

    data.nodes = nodes;
    values = data.values;
    positions = data.positions;

    for (i = 0; i < values.length; i++) {
        nodes.push(node = create(values[i], positions[i]));
        E.appendChild(node);
    }
}
function create_label(value, position) {
    var O = this.options;
    var elem = document.createElement("SPAN");
    elem.className = "toolkit-label";

    if (vert(O)) {
        elem.style.bottom = position.toFixed(1) + "px";
    } else {
        elem.style.left = position.toFixed(1) + "px";
    }

    TK.set_content(elem, O.labels(value));

    if (get_base(O) === value)
        TK.add_class(elem, "toolkit-base");
    if (O.max === value)
        TK.add_class(elem, "toolkit-max");
    if (O.min === value)
        TK.add_class(elem, "toolkit-min");

    return elem;
}
function create_dot(value, position) {
    var O = this.options;
    var elem = document.createElement("DIV");
    elem.className = "toolkit-dot";
    
    if (O.layout === "left" || O.layout === "right") {
        elem.style.bottom = Math.round(position + 0.5) + "px";
    } else {
        elem.style.left = Math.round(position - 0.5) + "px";
    }
    
    if (get_base(O) === value)
        TK.add_class(elem, "toolkit-base");
    else if (O.max === value)
        TK.add_class(elem, "toolkit-max");
    else if (O.min === value)
        TK.add_class(elem, "toolkit-min");

    return elem;
}
function measure_dimensions(data) {
    var nodes = data.nodes;
    var width = [];
    var height = [];

    for (var i = 0; i < nodes.length; i++) {
        width.push(TK.outer_width(nodes[i]));
        height.push(TK.outer_height(nodes[i]));
    }

    data.width = width;
    data.height = height;
}
function handle_end(O, labels, i) {
    var node = labels.nodes[i];
    var v = labels.values[i];

    if (v === O.min) {
        TK.add_class(node, "toolkit-min");
    } else if (v === O.max) {
        TK.add_class(node, "toolkit-max");
    } else return;
}
function generate_scale(from, to, include_from, show_to) {
    var O = this.options;
    var labels;

    if (O.show_labels || O.show_markers)
        labels = {
            values: [],
            positions: [],
        };

    var dots = {
        values: [],
        positions: [],
    };
    var is_vert = vert(O);
    var tmp;

    if (include_from) {
        tmp = this.val2px(from);

        if (labels) {
            labels.values.push(from);
            labels.positions.push(tmp);
        }

        dots.values.push(from);
        dots.positions.push(tmp);
    }

    var levels = O.levels;

    fill_interval(this, levels, levels.length - 1, from, to, O.gap_dots, dots);

    if (labels) {
        if (O.levels_labels) levels = O.levels_labels;

        fill_interval(this, levels, levels.length - 1, from, to, O.gap_labels, labels);

        tmp = this.val2px(to);

        if (show_to || Math.abs(tmp - this.val2px(from)) >= O.gap_labels) {
            labels.values.push(to);
            labels.positions.push(tmp);

            dots.values.push(to);
            dots.positions.push(tmp);
        }
    } else {
        dots.values.push(to);
        dots.positions.push(this.val2px(to));
    }

    if (O.show_labels) {
        create_dom_nodes.call(this, labels, create_label.bind(this));

        if (labels.values.length && labels.values[0] === get_base(O)) {
            TK.add_class(labels.nodes[0], "toolkit-base");
        }
    }

    var render_cb = function() {
        var markers;

        if (O.show_markers) {
            markers = {
                values: labels.values,
                positions: labels.positions,
            };
            create_dom_nodes.call(this, markers, create_dot.bind(this));
            for (var i = 0; i < markers.nodes.length; i++)
                TK.add_class(markers.nodes[i], "toolkit-marker");
        }

        if (O.show_labels && labels.values.length > 1) {
            handle_end(O, labels, 0);
            handle_end(O, labels, labels.nodes.length-1);
        }

        if (O.avoid_collisions && O.show_labels) {
            dots = remove_collisions(dots, labels, O.gap_dots, is_vert);
        } else if (markers) {
            dots = remove_collisions(dots, markers, O.gap_dots);
        }

        create_dom_nodes.call(this, dots, create_dot.bind(this));
    };

    if (O.show_labels && O.avoid_collisions)
        TK.S.add(function() {
            measure_dimensions(labels);
            TK.S.add(render_cb.bind(this), 3);
        }.bind(this), 2);
    else render_cb.call(this);
}
function mark_markers(labels, dots) {
    var i, j;

    var a = labels.values;
    var b = dots.values;
    var nodes = dots.nodes;

    for (i = j = 0; i < a.length && j < b.length;) {
        if (a[i] < b[j]) i++;
        else if (a[i] > b[j]) j++;
        else {
            TK.add_class(nodes[j], "toolkit-marker");
            i++;
            j++;
        }
    }
}
/**
 * TK.Scale can be used to draw scales. It is used in {@link TK.MeterBase} and
 * {@link TK.Fader}. TK.Scale draws labels and markers based on its parameters
 * and the available space. Scales can be drawn both vertically and horizontally.
 * Scale mixes in {@link TK.Ranged} and inherits all its options.
 *
 * @extends TK.Widget
 * 
 * @mixes TK.Ranged
 * 
 * @class TK.Scale
 *
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {String} [options.layout="right"] - The layout of the TK.Scale. <code>right</code> and
 *   <code>left</code> are vertical layouts with the labels being drawn right and left of the scale,
 *   respectively. <code>top</code> and <code>bottom</code> are horizontal layouts for which the 
 *   labels are drawn on top and below the scale, respectively.
 * @property {Integer} [options.division=1] - Minimal step size of the markers.
 * @property {Array<Number>} [options.levels=[1]] - Array of steps for labels and markers.
 * @property {Number} [options.base=false]] - Base of the scale. If set to <code>false</code> it will
 *   default to the minimum value.
 * @property {Function} [options.labels=TK.FORMAT("%.2f")] - Formatting function for the labels.
 * @property {Integer} [options.gap_dots=4] - Minimum gap in pixels between two adjacent markers.
 * @property {Integer} [options.gap_labels=40] - Minimum gap in pixels between two adjacent labels.
 * @property {Boolean} [options.show_labels=true] - If <code>true</code>, labels are drawn.
 * @property {Boolean} [options.show_max=true] - If <code>true</code>, display a label and a
 *   dot for the 'max' value.
 * @property {Boolean} [options.show_min=true] - If <code>true</code>, display a label and a
 *   dot for the 'min' value.
 * @property {Boolean} [options.show_base=true] - If <code>true</code>, display a label and a
 *   dot for the 'base' value.
 * @property {Array<Number>|Boolean} [options.fixed_dots] - This option can be used to specify fixed positions
 *   for the markers to be drawn at. The values must be sorted in ascending order. <code>false</code> disables
 *   fixed markers.
 * @property {Array<Number>|Boolean} [options.fixed_labels] - This option can be used to specify fixed positions
 *   for the labels to be drawn at. The values must be sorted in ascending order. <code>false</code> disables
 *   fixed labels.
 * @property {Boolean} [options.show_markers=true] - If true, every dot which is located at the same
 *   position as a label has the <code>toolkit-marker</code> class set.
 * @property {Number|Boolean} [options.pointer=false] - The value to set the pointers position to. Set to `false` to hide the pointer.
 * @property {Number|Boolean} [options.bar=false] - The value to set the bars height to. Set to `false` to hide the bar.
 */
TK.Scale = TK.class({
    _class: "Scale",
    
    Extends: TK.Widget,
    Implements: [TK.Ranged],
    _options: Object.assign(Object.create(TK.Widget.prototype._options), TK.Ranged.prototype._options, {
        layout: "string",
        division: "number",
        levels: "array",
        levels_labels: "array",
        base: "number",
        labels: "function",
        gap_dots: "number",
        gap_labels: "number",
        show_labels: "boolean",
        show_min: "boolean",
        show_max: "boolean",
        show_base: "boolean",
        fixed_dots: "boolean|array",
        fixed_labels: "boolean|array",
        avoid_collisions: "boolean",
        show_markers: "boolean",
        bar: "boolean|number",
        pointer: "boolean|number",
    }),
    options: {
        layout:           "right",
        division:         1,
        levels:           [1],
        base:             false,
        labels:           TK.FORMAT("%.2f"),
        avoid_collisions: false,
        gap_dots:         4,
        gap_labels:       40,
        show_labels:      true,
        show_min:         true,
        show_max:         true,
        show_base:        true,
        show_markers:     true,
        fixed_dots:       false,
        fixed_labels:     false,
        bar:              false,
        pointer:          false,
    },
    
    initialize: function (options) {
        var E;
        TK.Widget.prototype.initialize.call(this, options);
        /**
         * @member {HTMLDivElement} TK.Scale#element - The main DIV element. Has class <code>toolkit-scale</code> 
         */
        if (!(E = this.element)) this.element = E = TK.element("div");
        TK.add_class(E, "toolkit-scale");
        this.element = this.widgetize(E, true, true, true);
    },

    redraw: function () {
        TK.Widget.prototype.redraw.call(this);

        var I = this.invalid;
        var O = this.options;
        var E = this.element;

        if (I.layout) {
            I.layout = false;
            TK.remove_class(E, "toolkit-vertical", "toolkit-horizontal", "toolkit-top",
                            "toolkit-bottom", "toolkit-right", "toolkit-left");
            switch (O.layout) {
            case "left":
                TK.add_class(E, "toolkit-vertical", "toolkit-left");
                break;
            case "right":
                TK.add_class(E, "toolkit-vertical", "toolkit-right");
                break;
            case "top":
                TK.add_class(E, "toolkit-horizontal", "toolkit-top");
                break;
            case "bottom":
                TK.add_class(E, "toolkit-horizontal", "toolkit-bottom");
                break;
            default:
                TK.warn("Unsupported layout setting:", O.layout);
            }
        }

        if (I.reverse) {
          /* NOTE: reverse will be validated below */
          TK.toggle_class(E, "toolkit-reverse", O.reverse);
        }

        if (I.validate("base", "show_base", "gap_labels", "min", "show_min", "division", "max", "show_markers",
                       "fixed_dots", "fixed_labels", "levels", "basis", "scale", "reverse", "show_labels")) {
            TK.empty(E);

            if (O.fixed_dots && O.fixed_labels) {
                var labels;

                if (O.show_labels) {
                    labels = {
                        values: O.fixed_labels,
                        positions: O.fixed_labels.map(this.val2px, this),
                    };
                    create_dom_nodes.call(this, labels, create_label.bind(this));
                }

                var dots = {
                    values: O.fixed_dots,
                    positions: O.fixed_dots.map(this.val2px, this),
                };
                create_dom_nodes.call(this, dots, create_dot.bind(this));

                if (O.show_markers && labels) {
                    mark_markers(labels, dots);
                }
            } else {
                var base = get_base(O);

                if (base !== O.max) generate_scale.call(this, base, O.max, true, O.show_max);
                if (base !== O.min) generate_scale.call(this, base, O.min, base === O.max, O.show_min);
            }
            if (this._bar)
                this.element.appendChild(this._bar);
            if (this._pointer)
                this.element.appendChild(this._pointer);
        }
    },
    
    resize: function () {
        TK.Widget.prototype.resize.call(this);
        var O = this.options;
        this.set("basis", vert(O) ? TK.inner_height(this.element)
                                  : TK.inner_width(this.element) );
    },
    
    // GETTER & SETTER
    set: function (key, value) {
        TK.Widget.prototype.set.call(this, key, value);
        switch (key) {
            case "division":
            case "levels":
            case "labels":
            case "gap_dots":
            case "gap_labels":
            case "show_labels":
                /**
                 * Gets fired when an option the rendering depends on was changed
                 * 
                 * @event TK.Scale#scalechanged
                 * 
                 * @param {string} key - The name of the option which changed the {@link TK.Scale}.
                 * @param {mixed} value - The value of the option.
                 */
                this.fire_event("scalechanged", key, value)
                break;
        }
    }
});

/**
 * @member {HTMLDivElement} TK.Fader#_pointer - The DIV element of the pointer. It can be used to e.g. visualize the value set in the backend.
 */
TK.ChildElement(TK.Scale, "pointer", {
    show: false,
    toggle_class: true,
    option: "pointer",
    draw_options: Object.keys(TK.Ranged.prototype._options).concat([ "pointer", "basis" ]),
    draw: function(O) {
        if (this._pointer) {
            var tmp = this.val2px(this.snap(O.pointer)) + "px";
            if (vert(O)) {
                if (TK.supports_transform)
                    this._pointer.style.transform = "translateY(-"+tmp+")";
                else
                    this._pointer.style.bottom = tmp;
            } else {
                if (TK.supports_transform)
                    this._pointer.style.transform = "translateX("+tmp+")";
                else
                    this._pointer.style.left = tmp;
            }
        }
    },
});

/**
 * @member {HTMLDivElement} TK.Fader#_bar - The DIV element of the bar. It can be used to e.g. visualize the value set in the backend or to draw a simple levelmeter.
 */
TK.ChildElement(TK.Scale, "bar", {
    show: false,
    toggle_class: true,
    option: "bar",
    draw_options: Object.keys(TK.Ranged.prototype._options).concat([ "bar", "basis" ]),
    draw: function(O) {
        if (this._bar) {
            var tmp = this.val2px(this.snap(O.bar)) + "px";
            if (vert(O))
                this._bar.style.height = tmp;
            else
                this._bar.style.width = tmp;
        }
    },
});

})(this, this.TK);
