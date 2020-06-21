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
var MODES = [
    "circular",
    "line-horizontal",
    "line-vertical",
    "block-top",
    "block-bottom",
    "block-left",
    "block-right",
    "block",
];
function normalize(v) {
    var n = Math.sqrt(v[0]*v[0] + v[1]*v[1]);
    v[0] /= n;
    v[1] /= n;
}
function scrollwheel(e) {
    var direction;
    e.preventDefault();
    var d = e.wheelDelta !== void(0) && e.wheelDelta ? e.wheelDelta : e.detail;
    if (d > 0) {
        direction = 1;
    } else if (d < 0) {
        direction = -1;
    } else return;

    if (this.__sto) window.clearTimeout(this.__sto);
    this.set("dragging", true);
    TK.add_class(this.element, "toolkit-active");
    this.__sto = window.setTimeout(function () {
        this.set("dragging", false);
        TK.remove_class(this.element, "toolkit-active");
        this.fire_event("zchangeended", this.options.z);
    }.bind(this), 250);
    var s = this.range_z.get("step") * direction;
    if (e.ctrlKey && e.shiftKey)
        s *= this.range_z.get("shift_down");
    else if (e.shiftKey)
        s *= this.range_z.get("shift_up");
    this.userset("z", this.get("z") + s);
    if (!this._zwheel)
        this.fire_event("zchangestarted", this.options.z);
    this._zwheel = true;
}

/* The following functions turn positioning options
 * into somethine we can calculate with */

function ROT(a) {
    return [ +Math.sin(+a), +Math.cos(+a) ];
}

var ZHANDLE_POSITION_circular = {
    "top":          ROT(Math.PI),
    "center":       [1e-10, 1e-10],
    "top-right":    ROT(Math.PI*3/4),
    "right":        ROT(Math.PI/2),
    "bottom-right": ROT(Math.PI/4),
    "bottom":       ROT(0),
    "bottom-left":  ROT(Math.PI*7/4),
    "left":         ROT(Math.PI*3/2),
    "top-left":     ROT(Math.PI*5/4),
};

function get_zhandle_position_movable(O, X) {
    var vec = ZHANDLE_POSITION_circular[O.z_handle];
    var x = (X[0]+X[2])/2;
    var y = (X[1]+X[3])/2;
    var R = (X[2] - X[0] - O.z_handle_size)/2;

    return [
        x + R*vec[0],
        y + R*vec[1]
    ];
}

var Z_HANDLE_SIZE_corner = [ 1, 1, 0, 0 ];
var Z_HANDLE_SIZE_horiz = [ 1, 0, 0, 1 ];
var Z_HANDLE_SIZE_vert = [ 0, 1, 1, 0 ];

function Z_HANDLE_SIZE(pos) {
    switch (pos) {
    default:
        TK.warn("Unsupported z_handle position:", pos);
    case "top-right":
    case "bottom-right":
    case "bottom-left":
    case "top-left":
    case "center":
        return Z_HANDLE_SIZE_corner;
    case "top":
    case "bottom":
        return Z_HANDLE_SIZE_vert;
    case "left":
    case "right":
        return Z_HANDLE_SIZE_horiz;
    }
};

function get_zhandle_size(O, X) {
    var vec = Z_HANDLE_SIZE(O.z_handle);
    var z_handle_size = O.z_handle_size;
    var z_handle_centered = O.z_handle_centered;
    var width = X[2] - X[0];
    var height = X[3] - X[1];

    if (z_handle_centered < 1) {
        width *= z_handle_centered;
        height *= z_handle_centered;
    } else {
        width = z_handle_centered;
        height = z_handle_centered;
    }

    width = vec[0] * z_handle_size + vec[2] * width;
    height = vec[1] * z_handle_size + vec[3] * height;

    if (width < z_handle_size) width = z_handle_size;
    if (height < z_handle_size) height = z_handle_size;

    return [width, height];
}

var Z_HANDLE_POS = {
    "top":          [ 0, -1 ],
    "top-right":    [ 1, -1 ],
    "right":        [ 1, 0 ],
    "bottom-right": [ 1, 1 ],
    "bottom":       [ 0, 1 ],
    "bottom-left":  [ -1, 1 ],
    "left":         [ -1, 0 ],
    "top-left":     [ -1, -1 ],
    "center":       [ 0, 0 ],
};

function get_zhandle_position(O, X, zhandle_size) {
    var x = +(+X[0]+X[2]-+zhandle_size[0])/2;
    var y = +(+X[1]+X[3]-+zhandle_size[1])/2;
    var width = +X[2] - +X[0];
    var height = +X[3] - +X[1];
    var vec = Z_HANDLE_POS[O.z_handle] || Z_HANDLE_POS["top-right"];

    x += +vec[0] * +(width - +zhandle_size[0])/2;
    y += +vec[1] * +(height - +zhandle_size[1])/2;

    return [x, y];
}

function mode_to_handle(mode) {
    if (mode === "block-left" || mode === "block-right" ||
        mode === "block-top" || mode === "block-bottom")
        return "block";
    return mode;
}

var LABEL_ALIGN = {
    "line-vertical": {
        "top":      "middle",
        "bottom":   "middle",
        "left":     "end",
        "top-left": "end",
        "bottom-left":"end",
        "right":    "start",
        "top-right":"start",
        "bottom-right":"start",
        "center" : "middle",
    },
    "line-horizontal": {
        "top":      "middle",
        "bottom":   "middle",
        "left":     "start",
        "top-left": "start",
        "bottom-left":"start",
        "right":    "end",
        "top-right":"end",
        "bottom-right":"end",
        "center" : "middle",
    },
    "circular": {
        "top":      "middle",
        "bottom":   "middle",
        "left":     "end",
        "top-left": "start",
        "bottom-left":"start",
        "right":    "start",
        "top-right":"end",
        "bottom-right":"end",
        "center" : "middle",
    },
    "block": {
        "top":      "middle",
        "bottom":   "middle",
        "left":     "start",
        "top-left": "start",
        "bottom-left":"start",
        "right":    "end",
        "top-right":"end",
        "bottom-right":"end",
        "center" : "middle",
    }
}

function get_label_align(O, pos) {
    return LABEL_ALIGN[mode_to_handle(O.mode)][pos];
}

/* The following arrays contain multipliers, alternating x and y, starting with x.
 * The first pair is a multiplier for the handle width and height
 * The second pair is a multiplier for the label size
 * The third pair is a multiplier for the margin
*/ 

var LABEL_POSITION = {
    "line-vertical": {
        top:                 [ 0, -1, 0, 0, 0, 1 ],
        right:               [ 1, 0, 0, -1/2, 1, 0 ],
        left:                [ -1, 0, 0, -1/2, -1, 0 ],
        bottom:              [ 0, 1, 0, -1, 0, -1 ],
        "bottom-left":       [ -1, 1, 0, -1, -1, -1 ],
        "bottom-right":      [ 1, 1, 0, -1, 1, -1 ],
        "top-right":         [ 1, -1, 0, 0, 0, 1 ],
        "top-left":          [ -1, -1, 0, 0, -1, 1 ],
        center:              [ 0, 0, 0, -1/2, 0, 0 ],
    },
    "line-horizontal": {
        top:                 [ 0, -1, 0, -1, 0, -1 ],
        right:               [ 1, 0, 0, -1/2, 1, 0 ],
        left:                [ -1, 0, 0, -1/2, -1, 0 ],
        bottom:              [ 0, 1, 0, 0, 0, 1 ],
        "bottom-left":       [ -1, 1, 0, 0, 1, 1 ],
        "bottom-right":      [ 1, 1, 0, 0, -1, 1 ],
        "top-right":         [ 1, -1, 0, -1, -1, -1 ],
        "top-left":          [ -1, -1, 0, -1, 1, -1 ],
        center:              [ 0, 0, 0, -1/2, 0, 0 ],
    },
    "circular": {
        top:                 [ 0, -1, 0, -1, 0, -1 ],
        right:               [ 1, 0, 0, -1/2, 1, 0 ],
        left:                [ -1, 0, 0, -1/2, -1, 0 ],
        bottom:              [ 0, 1, 0, 0, 0, 1 ],
        "bottom-left":       [ -1, 1, 0, 0, 0, 1 ],
        "bottom-right":      [ 1, 1, 0, 0, 0, 1 ],
        "top-right":         [ 1, -1, 0, -1, 0, -1 ],
        "top-left":          [ -1, -1, 0, -1, 0, -1 ],
        center:              [ 0, 0, 0, -1/2, 0, 0 ],
    },
    "block": {
        top:                 [ 0, -1, 0, 0, 0, 1 ],
        bottom:              [ 0, 1, 0, -1, 0, -1 ],
        right:               [ 1, 0, 0, -1/2, -1, 0 ],
        left:                [ -1, 0, 0, -1/2, 1, 0 ],
        "bottom-left":       [ -1, 1, 0, -1, 1, -1 ],
        "bottom-right":      [ 1, 1, 0, -1, -1, -1 ],
        "top-right":         [ 1, -1, 0, 0, -1, 1 ],
        "top-left":          [ -1, -1, 0, 0, 1, 1 ],
        center:              [ 0, 0, 0, -1/2, 0, 0 ],
    }
}

function get_label_position(O, X, pos, label_size) {
    /* X: array containing [X0, Y0, X1, Y1] of the handle
     * pos: string describing the position of the label ("top", "bottom-right", ...)
     * label_size: array containing width and height of the label
     */
    var m = O.margin;
    
    // Pivot (x, y) is the center of the handle.
    var x = (X[0]+X[2])/2;
    var y = (X[1]+X[3])/2;
    
    // Size of handle
    var width = +X[2]-+X[0];
    var height = +X[3]-+X[1];
    
    // multipliers
    var vec = LABEL_POSITION[mode_to_handle(O.mode)][pos];

    x += vec[0] * width/2 + vec[2] * label_size[0] + vec[4] * m;
    y += vec[1] * height/2 + vec[3] * label_size[1] + vec[5] * m;
    
    // result is [x, y] of the "real" label position. Please note that
    // the final x position depends on the LABEL_ALIGN value for pos.
    // Y value is the top border of the overall label.
    return [x,y];
}

function remove_zhandle() {
    var E = this._zhandle;
    if (!E) return;
    this._zhandle = null;
    if (this.z_drag.get("node") === E)
        this.z_drag.set("node", null);

    E.remove();
}

function create_zhandle() {
    var E;
    var O = this.options;

    if (this._zhandle) remove_zhandle.call(this);

    E = TK.make_svg(
        O.mode === "circular" ? "circle" : "rect", {
            "class": "toolkit-z-handle",
        }
    );

    this._zhandle = E;
    if (this.z_drag.get("node") !== document)
        this.z_drag.set("node", E);
}

function create_line1() {
    if (this._line1) remove_line1.call(this);
    this._line1 = TK.make_svg("path", {
        "class": "toolkit-line toolkit-line-1"
    });
}
function create_line2() {
    if (this._line2) remove_line2.call(this);
    this._line2 = TK.make_svg("path", {
        "class": "toolkit-line toolkit-line-2"
    });
}
function remove_line1() {
    if (!this._line1) return;
    this._line1.remove();
    this._line1 = null;
}
function remove_line2() {
    if (!this._line2) return;
    this._line2.remove();
    this._line2 = null;
}

/* Prints a line, making sure that an offset of 0.5 px aligns them on
 * pixel boundaries */
var format_line = TK.FORMAT("M %.0f.5 %.0f.5 L %.0f.5 %.0f.5");

/* calculates the actual label positions based on given alignment
 * and dimensions */
function get_label_dimensions(align, X, label_size) {
    switch (align) {
    case "start":
        return [ X[0], X[1], X[0]+label_size[0], X[1]+label_size[1] ];
    case "middle":
        return [ X[0]-label_size[0]/2, X[1], X[0]+label_size[0]/2, X[1]+label_size[1] ];
    case "end":
        return [ X[0]-label_size[0], X[1], X[0], X[1]+label_size[1] ];
    }
}

function redraw_handle(O, X) {
    var _handle = this._handle;

    if (!O.show_handle) {
        if (_handle) remove_handle.call(this);
        return;
    }

    var range_x = this.range_x;
    var range_y = this.range_y;
    var range_z = this.range_z;

    if (!range_x.options.basis || !range_y.options.basis) return;

    var x = range_x.val2px(O.x);
    var y = range_y.val2px(O.y);
    var z = range_z.val2coef(O.z);

    var tmp;

    if (O.mode === "circular") {
        tmp = Math.max(O.min_size, z * O.max_size)/2;
        X[0] = x-tmp;
        X[1] = y-tmp;
        X[2] = x+tmp;
        X[3] = y+tmp;

        _handle.setAttribute("r", tmp.toFixed(2));
        _handle.setAttribute("cx", x.toFixed(2));
        _handle.setAttribute("cy", y.toFixed(2));
    } else if (O.mode === "block") {
        tmp = Math.max(O.min_size, z)/2;
        X[0] = x-tmp;
        X[1] = y-tmp;
        X[2] = x+tmp;
        X[3] = y+tmp;

        _handle.setAttribute("x", Math.round(+X[0]).toFixed(0));
        _handle.setAttribute("y", Math.round(+X[1]).toFixed(0));
        _handle.setAttribute("width", Math.round(+X[2]-X[0]).toFixed(0));
        _handle.setAttribute("height", Math.round(+X[3]-X[1]).toFixed(0));
    } else {
        var x_min = O.x_min !== false ? range_x.val2px(range_x.snap(O.x_min)) : 0;
        var x_max = O.x_max !== false ? range_x.val2px(range_x.snap(O.x_max)) : range_x.options.basis;

        if (x_min > x_max) {
            tmp = x_min;
            x_min = x_max;
            x_max = tmp;
        }

        var y_min = O.y_min !== false ? range_y.val2px(range_y.snap(O.y_min)) : 0;
        var y_max = O.y_max !== false ? range_y.val2px(range_y.snap(O.y_max)) : range_y.options.basis;

        if (y_min > y_max) {
            tmp = y_min;
            y_min = y_max;
            y_max = tmp;
        }

        tmp = O.min_size / 2;

        /* All other modes are drawn as rectangles */
        switch (O.mode) {
        case "line-vertical":
            tmp = Math.max(tmp, z * O.max_size/2);
            X[0] = x-tmp;
            X[1] = y_min;
            X[2] = x+tmp;
            X[3] = y_max;
            break;
        case "line-horizontal":
            // line horizontal
            tmp = Math.max(tmp, z * O.max_size/2);
            X[0] = x_min;
            X[1] = y - tmp;
            X[2] = x_max;
            X[3] = y + tmp;
            break;
        case "block-left":
            // rect lefthand
            X[0] = 0;
            X[1] = y_min;
            X[2] = Math.max(x, tmp);
            X[3] = y_max;
            break;
        case "block-right":
            // rect righthand
            X[0] = x;
            X[1] = y_min;
            X[2] = range_x.options.basis;
            X[3] = y_max;
            if (X[2] - X[0] < tmp) X[0] = X[2] - tmp;
            break;
        case "block-top":
            // rect top
            X[0] = x_min;
            X[1] = 0;
            X[2] = x_max;
            X[3] = Math.max(y, tmp);
            break;
        case "block-bottom":
            // rect bottom
            X[0] = x_min;
            X[1] = y;
            X[2] = x_max;
            X[3] = range_y.options.basis;
            if (X[3] - X[1] < tmp) X[1] = X[3] - tmp;
            break;
        default:
            TK.warn("Unsupported mode:", O.mode);
        }

        /* Draw the rectangle */
        _handle.setAttribute("x", Math.round(+X[0]).toFixed(0));
        _handle.setAttribute("y", Math.round(+X[1]).toFixed(0));
        _handle.setAttribute("width", Math.round(+X[2]-X[0]).toFixed(0));
        _handle.setAttribute("height", Math.round(+X[3]-X[1]).toFixed(0));
    }
}

function redraw_zhandle(O, X) {
    var vec;
    var size;
    var zhandle = this._zhandle;

    if (!O.show_handle || O.z_handle === false) {
        if (zhandle) remove_zhandle.call(this);
        return;
    }

    if (!zhandle.parentNode)
        this.element.appendChild(zhandle);
    
    if (this._handle && O.z_handle_below)
        this.element.appendChild(this._handle);
    
    if (O.mode === "circular") {
        /*
         * position the z_handle on the circle.
         */
        vec = get_zhandle_position_movable(O, X);
        /* width and height are equal here */
        zhandle.setAttribute("cx", vec[0].toFixed(1));
        zhandle.setAttribute("cy", vec[1].toFixed(1));
        zhandle.setAttribute("r",  (O.z_handle_size / 2).toFixed(1));

        this.zhandle_position = vec;
    } else if (O.mode === "block") {
        /*
         * position the z_handle on the box.
         */
        vec = get_zhandle_position_movable(O, X);
        size = O.z_handle_size / 2;
        /* width and height are equal here */
        zhandle.setAttribute("x", vec[0].toFixed(0) - size);
        zhandle.setAttribute("y", vec[1].toFixed(0) - size);
        zhandle.setAttribute("width", O.z_handle_size);
        zhandle.setAttribute("height", O.z_handle_size);

        this.zhandle_position = vec;
    } else {
        // all other handle types (lines/blocks)
        this.zhandle_position = vec = get_zhandle_size(O, X);

        zhandle.setAttribute("width", vec[0].toFixed(0));
        zhandle.setAttribute("height", vec[1].toFixed(0));

        vec = get_zhandle_position(O, X, vec);

        zhandle.setAttribute("x", vec[0].toFixed(0));
        zhandle.setAttribute("y", vec[1].toFixed(0));

        /* adjust to the center of the zhandle */
        this.zhandle_position[0] /= 2;
        this.zhandle_position[1] /= 2;
        this.zhandle_position[0] += vec[0];
        this.zhandle_position[1] += vec[1];
    }

    this.zhandle_position[0] -= (X[0]+X[2])/2;
    this.zhandle_position[1] -= (X[1]+X[3])/2;
    normalize(this.zhandle_position);
}

function prevent_default(e) {
    e.preventDefault();
    return false;
}

function create_label() {
    var E;
    this._label = E = TK.make_svg("text", {
        "class": "toolkit-label"
    });
    TK.add_event_listener(E, "mousewheel", this._scrollwheel);
    TK.add_event_listener(E, "DOMMouseScroll",  this._scrollwheel);
    TK.add_event_listener(E, 'contextmenu', prevent_default);
}

function remove_label() {
    var E = this._label;
    this._label = null;
    E.remove();
    TK.remove_event_listener(E, "mousewheel",      this._scrollwheel);
    TK.remove_event_listener(E, "DOMMouseScroll",  this._scrollwheel);
    TK.remove_event_listener(E, 'contextmenu', prevent_default);

    this.label = [0,0,0,0];
}

function STOP() { return false; };

function create_handle() {
    var O = this.options;
    var E;

    if (this._handle) remove_handle.call(this);
    
    E = TK.make_svg(O.mode === "circular" ? "circle" : "rect",
                    { class: "toolkit-handle" });
    TK.add_event_listener(E, "mousewheel",     this._scrollwheel);
    TK.add_event_listener(E, "DOMMouseScroll", this._scrollwheel);
    TK.add_event_listener(E, 'selectstart', prevent_default);
    TK.add_event_listener(E, 'contextmenu', prevent_default);
    this._handle = E;
    this.element.appendChild(E);
}

function remove_handle() {
    var E = this._handle;
    if (!E) return;
    this._handle = null;
    E.remove();
    TK.remove_event_listener(E, "mousewheel",     this._scrollwheel);
    TK.remove_event_listener(E, "DOMMouseScroll", this._scrollwheel);
    TK.remove_event_listener(E, "selectstart", prevent_default);
    TK.remove_event_listener(E, 'contextmenu', prevent_default);
}

function redraw_label(O, X) {
    if (!O.show_handle || O.label === false) {
        if (this._label) remove_label.call(this);
        return false;
    }

    var a = O.label.call(this, O.title, O.x, O.y, O.z).split("\n");
    var c = this._label.childNodes;

    while (c.length < a.length) {
        this._label.appendChild(TK.make_svg("tspan", {dy:"1.0em"}));
    }
    while (c.length > a.length) {
        this._label.removeChild(this._label.lastChild);
    }
    for (var i = 0; i < a.length; i++) {
        TK.set_text(c[i], a[i]);
    }

    if (!this._label.parentNode) this.element.appendChild(this._label);

    TK.S.add(function() {
        var w = 0;
        for (var i = 0; i < a.length; i++) {
            w = Math.max(w, c[i].getComputedTextLength());
        }

        var bbox;

        try {
            bbox = this._label.getBBox();
        } catch(e) {
            /* _label is not in the DOM yet */
            return;
        }

        TK.S.add(function() {
            var label_size = [ w, bbox.height ];

            var i;
            var pref = O.preferences;
            var area = 0;
            var label_position;
            var text_position;
            var text_anchor;
            var tmp;

            /*
             * Calculate possible positions of the labels and calculate their intersections. Choose
             * that position which has the smallest intersection area with all other handles and labels
             */
            for (i = 0; i < pref.length; i++) {

                /* get alignment */
                var align = get_label_align(O, pref[i]);

                /* get label position */
                var LX = get_label_position(O, X, pref[i], label_size);

                /* calculate the label bounding box using anchor and dimensions */
                var pos = get_label_dimensions(align, LX, label_size);

                tmp = O.intersect(pos, this);

                /* We require at least one square px smaller intersection
                 * to avoid flickering label positions */
                if (area === 0 || tmp.intersect + 1 < area) {
                    area = tmp.intersect;
                    label_position = pos;
                    text_position = LX;
                    text_anchor = align;

                    /* there is no intersections, we are done */
                    if (area === 0) break;
                }
            }

            this.label = label_position;
            tmp = Math.round(text_position[0]) + "px";
            this._label.setAttribute("x", tmp);
            this._label.setAttribute("y", Math.round(text_position[1]) + "px");
            this._label.setAttribute("text-anchor", text_anchor);
            var c = this._label.childNodes;
            for (var i = 0; i < c.length; i++)
                c[i].setAttribute("x", tmp);

            redraw_lines.call(this, O, X);
        }.bind(this), 1);
    }.bind(this));

    return true;
}

function redraw_lines(O, X) {

    if (!O.show_handle) {
        if (this._line1) remove_line1.call(this);
        if (this._line2) remove_line2.call(this);
        return;
    }

    var pos = this.label;
    var range_x = this.range_x;
    var range_y = this.range_y;
    var range_z = this.range_z;

    var x = range_x.val2px(O.x);
    var y = range_y.val2px(O.y);
    var z = range_z.val2px(O.z);
    switch (O.mode) {
        case "circular":
        case "block":
            if (O.show_axis) {
                this._line1.setAttribute("d",
                     format_line(((y >= pos[1] && y <= pos[3]) ? Math.max(X[2], pos[2]) : X[2]) + O.margin, y,
                                 range_x.options.basis, y));
                this._line2.setAttribute("d",
                     format_line(x, ((x >= pos[0] && x <= pos[2]) ? Math.max(X[3], pos[3]) : X[3]) + O.margin,
                                 x, range_y.options.basis));
            } else {
                if (this._line1) remove_line1.call(this);
                if (this._line2) remove_line2.call(this);
            }
            break;
        case "line-vertical":
        case "block-left":
        case "block-right":
            this._line1.setAttribute("d", format_line(x, X[1], x, X[3]));
            if (O.show_axis) {
                this._line2.setAttribute("d", format_line(0, y, range_x.options.basis, y));
            } else if (this._line2) {
                remove_line2.call(this);
            }
            break;
        case "line-horizontal":
        case "block-top":
        case "block-bottom":
            this._line1.setAttribute("d", format_line(X[0], y, X[2], y));
            if (O.show_axis) {
                this._line2.setAttribute("d", format_line(x, 0, x, range_y.options.basis));
            } else if (this._line2) {
                remove_line2.call(this);
            }
            break;
        default:
            TK.warn("Unsupported mode", pref[i]);
    }

    if (this._line1 && !this._line1.parentNode) this.element.appendChild(this._line1);
    if (this._line2 && !this._line2.parentNode) this.element.appendChild(this._line2);
}

function set_main_class(O) {
    var E = this.element;
    var i;

    for (i = 0; i < MODES.length; i++) TK.remove_class(E, "toolkit-"+MODES[i]);

    TK.remove_class(E, "toolkit-line");
    TK.remove_class(E, "toolkit-block");

    switch (O.mode) {
    case "line-vertical":
    case "line-horizontal":
        TK.add_class(E, "toolkit-line");
    case "circular":
        break;
    case "block-left":
    case "block-right":
    case "block-top":
    case "block-bottom":
    case "block":
        TK.add_class(E, "toolkit-block");
        break;
    default:
        TK.warn("Unsupported mode:", O.mode);
        return;
    }

    TK.add_class(E, "toolkit-"+O.mode);
}

function startdrag() {
    this.draw_once(function() {
        var e = this.element;
        var p = e.parentNode;
        TK.add_class(e, "toolkit-active");
        this.set("dragging", true);

        /* TODO: move this into the parent */
        TK.add_class(this.parent.element, "toolkit-dragging");

        this.global_cursor("move");

        if (p.lastChild !== e)
            p.appendChild(e);
    });
}

function enddrag() {
    this.draw_once(function() {
        var e = this.element;
        TK.remove_class(e, "toolkit-active");
        this.set("dragging", false);

        /* TODO: move this into the parent */
        TK.remove_class(this.parent.element, "toolkit-dragging");

        this.remove_cursor("move");
    });
}

/**
 * Class which represents a draggable SVG element, which can be used to represent and change
 * a value inside of a {@link TK.ResponseHandler} and is drawn inside of a chart.
 *
 * @class TK.ResponseHandle
 * 
 * @extends TK.Widget
 *
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {Function|Object} options.range_x - Callback returning a {@link TK.Range}
 *   for the x-axis or an object with options for a {@link TK.Range}. This is usually
 *   the <code>x_range</code> of the parent chart.
 * @property {Function|Object} options.range_y - Callback returning a {@link TK.Range}
 *   for the y-axis or an object with options for a {@link TK.Range}. This is usually
 *   the <code>y_range</code> of the parent chart.
 * @property {Function|Object} options.range_z - Callback returning a {@link TK.Range}
 *   for the z-axis or an object with options for a {@link TK.Range}.
 * @property {String} [options.mode="circular"] - Type of the handle. Can be one out of
 *   <code>circular</code>, <code>line-vertical</code>, <code>line-horizontal</code>,
 *   <code>block-left</code>, <code>block-right</code>, <code>block-top</code> or
 *   <code>block-right</code>.
 * @property {Number} [options.x] - Value of the x-coordinate.
 * @property {Number} [options.y] - Value of the y-coordinate.
 * @property {Number} [options.z] - Value of the z-coordinate.
 * @property {Number} [options.min_size=24] - Minimum size of the handle in px.
 * @property {Number} [options.max_size=100] - Maximum size of the handle in px.
 * @property {Function|Boolean} options.label - Label formatting function. Arguments are
 *   <code>title</code>, <code>x</code>, <code>y</code>, <code>z</code>. If set to <code>false</code>, no label is displayed.
 * @property {Array<String>}  [options.preferences=["left", "top", "right", "bottom"]] - Possible label
 *   positions by order of preference. Depending on the selected <code>mode</code> this can
 *   be a subset of <code>top</code>, <code>top-right</code>, <code>right</code>,
 *   <code>bottom-right</code>, <code>bottom</code>, <code>bottom-left</code>,
 *   <code>left</code>, <code>top-left</code> and <code>center</code>.
 * @property {Number} [options.margin=3] - Margin in px between the handle and the label.
 * @property {Boolean|String} [options.z_handle=false] - If not false, a small handle is drawn at the given position (`top`, `top-left`, `top-right`, `left`, `center`, `right`, `bottom-left`, `bottom`, `bottom-right`), which can
 *   be dragged to change the value of the z-coordinate.
 * @property {Number} [options.z_handle_size=6] - Size in px of the z-handle.
 * @property {Number} [options.z_handle_centered=0.1] - Size of the z-handle in center positions.
 *   If this options is between 0 and 1, it is interpreted as a ratio, otherwise as a px size.
 * @property {Number} [options.z_handle_below=false] - Render the z-handle below the normal handle in the DOM. SVG doesn't know CSS attribute z-index, so this workaround is needed from time to time.
 * @property {Number} [options.x_min] - Minimum value of the x-coordinate.
 * @property {Number} [options.x_max] - Maximum value of the x-coordinate.
 * @property {Number} [options.y_min] - Minimum value of the y-coordinate.
 * @property {Number} [options.y_max] - Maximum value of the y-coordinate.
 * @property {Number} [options.z_min] - Minimum value of the z-coordinate.
 * @property {Number} [options.z_max] - Maximum value of the z-coordinate.
 * @property {Boolean} [options.show_axis=false] - If set to true,  additional lines are drawn at the coordinate values.
 *
 * @mixes TK.Ranges
 * @mixes TK.Warning
 * @mixes TK.GlobalCursor
 */

/**
 * @member {SVGText} TK.ResponseHandle#_label - The label. Has class <code>toolkit-label</code>.
 */
/**
 * @member {SVGPath} TK.ResponseHandle#_line1 - The first line. Has class <code>toolkit-line toolkit-line-1</code>.
 */
/**
 * @member {SVGPath} TK.ResponseHandle#_line2 - The second line. Has class <code>toolkit-line toolkit-line-2</code>.
 */

function set_min(value, key) {
    var name = key.substr(0, 1);
    var O = this.options;
    if (value !== false && O[name] < value) this.set(name, value);
}

function set_max(value, key) {
    var name = key.substr(0, 1);
    var O = this.options;
    if (value !== false && O[name] > value) this.set(name, value);
}
         
/**
 * The <code>useraction</code> event is emitted when a widget gets modified by user interaction.
 * The event is emitted for the options <code>x</code>, <code>y</code> and <code>z</code>.
 *
 * @event TK.ResponseHandle#useraction
 * 
 * @param {string} name - The name of the option which was changed due to the users action.
 * @param {mixed} value - The new value of the option.
 */
TK.ResponseHandle = TK.class({
    _class: "ResponseHandle",
    Extends: TK.Widget,
    Implements: [TK.GlobalCursor, TK.Ranges, TK.Warning],
    _options: Object.assign(Object.create(TK.Widget.prototype._options), TK.Ranges.prototype._options, {
        range_x: "mixed",
        range_y: "mixed",
        range_z: "mixed",
        intersect: "function",
        mode: "string",
        preferences: "array",
        label: "function|boolean",
        x: "number",
        y: "number",
        z: "number",
        min_size: "number",
        max_size: "number",
        margin: "number",
        z_handle: "boolean|string",
        z_handle_size: "number",
        z_handle_centered: "number",
        z_handle_below: "boolean",
        min_drag: "number",
        x_min: "number",
        x_max: "number",
        y_min: "number",
        y_max: "number",
        z_min: "number",
        z_max: "number",
        active: "boolean",
        show_axis: "boolean",
        title: "string",
        hover: "boolean",
        dragging: "boolean",
        show_handle: "boolean"
    }),
    options: {
        range_x:          {},
        range_y:          {},
        range_z:          {},
        intersect:        function () { return { intersect: 0, count: 0 } },
        // NOTE: this is currently not a public API
        // callback function for checking intersections: function (x1, y1, x2, y2, id) {}
        // returns a value describing the amount of intersection with other handle elements.
        // intersections are weighted depending on the intersecting object. E.g. SVG borders have
        // a very high impact while intersecting in comparison with overlapping handle objects
        // that have a low impact on intersection
        mode:             "circular",
        preferences:      ["left", "top", "right", "bottom"],
        label:            TK.FORMAT("%s\n%d Hz\n%.2f dB\nQ: %.2f"),
        x:                0,
        y:                0,
        z:                0,
        min_size:         24,
        max_size:         100,
        margin:           3,
        z_handle:         false,
        z_handle_size:    6,
        z_handle_centered:0.1,
        z_handle_below:   false,
        min_drag:         0,
        // NOTE: not yet a public API
        // amount of pixels the handle has to be dragged before it starts to move
        x_min:            false,
        x_max:            false,
        y_min:            false,
        y_max:            false,
        z_min:            false,
        z_max:            false,
        active:           true,
        show_axis:        false,
        hover:            false,
        dragging:         false,
        show_handle:      true
    },
    static_events: {
        set_show_axis: function(value) {
            var O = this.options;
            if (O.mode === "circular") create_line1.call(this);
            create_line2.call(this);
        },
        set_label: function(value) {
            if (value !== false && !this._label) create_label.call(this);
        },
        set_show_handle: function(value) {
            this.set("mode", this.options.mode);
            this.set("show_axis", this.options.show_axis);
            this.set("label", this.options.label);
        },
        set_mode: function(value) {
            var O = this.options;
            if (!O.show_handle) return;
            create_handle.call(this);
            if (O.z_handle !== false) create_zhandle.call(this);
            if (value !== "circular") create_line1.call(this);
        },
        set_x_min: set_min,
        set_y_min: set_min,
        set_z_min: set_min,
        set_x_max: set_max,
        set_y_max: set_max,
        set_z_max: set_max,
        mouseenter: function() {
            this.set("hover", true);
        },
        mouseleave: function() {
            this.set("hover", false);
        },
        set_active: function(v) {
            if (!v) {
                this.pos_drag.cancel_drag();
                this.z_drag.cancel_drag();
            }
        },
    },

    initialize: function (options) {
        this.label = [0,0,0,0];
        this.handle = [0,0,0,0];
        this._zwheel = false;
        this.__sto = 0;
        TK.Widget.prototype.initialize.call(this, options);
        var O = this.options;
        
        /**
         * @member {TK.Range} TK.ResponseHandle#range_x - The {@link TK.Range} for the x axis.
         */
        /**
         * @member {TK.Range} TK.ResponseHandle#range_y - The {@link TK.Range} for the y axis.
         */
        /**
         * @member {TK.Range} TK.ResponseHandle#range_z - The {@link TK.Range} for the z axis.
         */
        this.add_range(O.range_x, "range_x");
        this.add_range(O.range_y, "range_y");
        this.add_range(O.range_z, "range_z");

        var set_cb = function() {
            this.invalid.x = true;
            this.trigger_draw();
        }.bind(this);

        this.range_x.add_event("set", set_cb);
        this.range_y.add_event("set", set_cb);
        this.range_z.add_event("set", set_cb);

        var E = TK.make_svg("g");
        
        /**
         * @member {SVGGroup} TK.ResponseHandle#element - The main SVG group containing all handle elements. Has class <code>toolkit-response-handle</code>.
         */
        this.element = E;

        this.widgetize(E, true, true);

        TK.add_class(E, "toolkit-response-handle");
        /**
         * @member {SVGCircular} TK.ResponseHandle#_handle - The main handle.
         *      Has class <code>toolkit-handle</code>.
         */
        
        /**
         * @member {SVGCircular} TK.ResponseHandle#_zhandle - The handle for manipulating z axis.
         *      Has class <code>toolkit-z-handle</code>.
         */

        this._scrollwheel = scrollwheel.bind(this);

        this._handle = this._zhandle = this._line1 = this._line2 = this._label = null;

        this.z_drag = new TK.DragCapture(this, {
            node: null,
            onstartcapture: function(state) {
                var self = this.parent;
                var O = self.options;
                if (!O.active) return;
                state.z = self.range_z.val2px(O.z);

                /* the main handle is active,
                 * this is a z gesture */
                var pstate = self.pos_drag.state();
                if (pstate) {
                    var d;
                    var v = [ state.current.clientX - pstate.prev.clientX,
                              state.current.clientY - pstate.prev.clientY ];
                    normalize(v);
                    state.vector = v;
                } else {
                    state.vector = self.zhandle_position;
                }
                /**
                 * Is fired when the user grabs the z-handle. The argument is the
                 * actual z value.
                 * 
                 * @event TK.ResponseHandle#zchangestarted
                 * 
                 * @param {number} z - The z value.
                 */
                self.fire_event("zchangestarted", O.z);
                startdrag.call(self);
                return true;
            },
            onmovecapture: function(state) {
                var self = this.parent;
                var O = self.options;

                var zv = state.vector;
                var v = state.vdistance();

                var d = zv[0] * v[0] + zv[1] * v[1];

                /* ignore small movements */
                if (O.min_drag > 0 && O.min_drag > d) return;

                var range_z = self.range_z;
                var z = range_z.px2val(state.z + d);

                self.userset("z", z);
            },
            onstopcapture: function() {
                var self = this.parent;
                /**
                 * Is fired when the user releases the z-handle. The argument is the
                 * actual z value.
                 * 
                 * @event TK.ResponseHandle#zchangeended
                 * 
                 * @param {number} z - The z value.
                 */
                self.fire_event("zchangeended", self.options.z);
                enddrag.call(self);
            },
        });
        this.pos_drag = new TK.DragCapture(this, {
            node: this.element,
            onstartcapture: function(state) {
                var self = this.parent;
                var O = self.options;
                if (!O.active) return;

                var button = state.current.button;
                var E = self.element;
                var p = E.parentNode;
                var ev = state.current;

                self.z_drag.set("node", document);

                /* right click triggers move to the back */
                if (ev.button === 2) {
                    if (E !== p.firstChild)
                        self.draw_once(function() {
                            var e = this.element;
                            var p = e.parentNode;
                            if (p && e !== p.firstChild) p.insertBefore(e, p.firstChild);
                        });
                    /* cancel everything else, but do not drag */
                    ev.preventDefault();
                    ev.stopPropagation();
                    return false;
                }

                state.x = self.range_x.val2px(O.x);
                state.y = self.range_y.val2px(O.y);
                /**
                 * Is fired when the main handle is grabbed by the user.
                 * The argument is an object with the following members:
                 * <ul>
                 * <li>x: the actual value on the x axis</li>
                 * <li>y: the actual value on the y axis</li>
                 * <li>pos_x: the position in pixels on the x axis</li>
                 * <li>pos_y: the position in pixels on the y axis</li>
                 * </ul>
                 * 
                 * @event TK.ResponseHandle#handlegrabbed
                 * 
                 * @param {Object} positions - An object containing all relevant positions of the pointer.
                 */
                self.fire_event("handlegrabbed", {
                    x:     O.x,
                    y:     O.y,
                    pos_x: state.x,
                    pos_y: state.y
                });
                startdrag.call(self);
                return true;
            },
            onmovecapture: function(state) {
                var self = this.parent;
                var O = self.options;

                /* ignore small movements */
                if (O.min_drag > 0 && O.min_drag > state.distance()) return;

                /* we are changing z right now using a gesture, irgnore this movement */
                if (self.z_drag.dragging()) return;

                var v = state.vdistance();
                var range_x = self.range_x;
                var range_y = self.range_y;
                var x = range_x.px2val(state.x + v[0]);
                var y = range_y.px2val(state.y + v[1]);

                self.userset("x", x);
                self.userset("y", y);
            },
            onstopcapture: function() {
                /**
                 * Is fired when the user releases the main handle.
                 * The argument is an object with the following members:
                 * <ul>
                 * <li>x: the actual value on the x axis</li>
                 * <li>y: the actual value on the y axis</li>
                 * <li>pos_x: the position in pixels on the x axis</li>
                 * <li>pos_y: the position in pixels on the y axis</li>
                 * </ul>
                 * 
                 * @event TK.ResponseHandle#handlereleased
                 * 
                 * @param {Object} positions - An object containing all relevant positions of the pointer.
                 */
                var self = this.parent;
                var O = self.options;
                self.fire_event("handlereleased", {
                    x:     O.x,
                    y:     O.y,
                    pos_x: self.range_x.val2px(O.x),
                    pos_y: self.range_y.val2px(O.y),
                });
                enddrag.call(self);
                self.z_drag.set("node", self._zhandle);
            },
        });

        this.set("mode", O.mode);
        this.set("show_handle", O.show_handle);
        this.set("show_axis", O.show_axis);
        this.set("active", O.active);
        this.set("x", O.x);
        this.set("y", O.y);
        this.set("z", O.z);
        this.set("z_handle", O.z_handle);
        this.set("label", O.label);

    },

    redraw: function () {
        TK.Widget.prototype.redraw.call(this);
        var O = this.options;
        var I = this.invalid;

        var range_x = this.range_x;
        var range_y = this.range_y;
        var range_z = this.range_z;

        /* These are the coordinates of the corners (x1, y1, x2, y2)
         * NOTE: x,y are not necessarily in the midde. */
        var X  = this.handle;

        if (I.mode) set_main_class.call(this, O);

        if (I.hover) {
            I.hover = false;
            TK.toggle_class(this.element, "toolkit-hover", O.hover);
        }
        if (I.dragging) {
            I.dragging = false;
            TK.toggle_class(this.element, "toolkit-dragging", O.dragging);
        }

        if (I.active || I.disabled) {
            I.disabled = false;
            // TODO: this is not very nice, we should really use the options
            // for that. 1) set "active" from the mouse handlers 2) set disabled instead
            // of active
            TK.toggle_class(this.element, "toolkit-disabled", !O.active || O.disabled);
        }

        var moved = I.validate("x", "y", "z", "mode", "active", "show_handle");

        if (moved) redraw_handle.call(this, O, X);

        // Z-HANDLE

        if (I.validate("z_handle") || moved) {
            redraw_zhandle.call(this, O, X);
        }

        var delay_lines;
        
        // LABEL
        if (I.validate("label", "title", "preference") || moved) {
            delay_lines = redraw_label.call(this, O, X);
        }

        // LINES
        if (I.validate("show_axis") || moved) {
            if (!delay_lines) redraw_lines.call(this, O, X);
        }
    },
    set: function(key, value) {
        var O = this.options;

        switch (key) {
        case "z_handle":
            if (value !== false && !ZHANDLE_POSITION_circular[value]) {
                TK.warn("Unsupported z_handle option:", value);
                value = false;
            }
            if (value !== false) create_zhandle.call(this);
            break;
        case "x":
            value = this.range_x.snap(value);
            if (O.x_min !== false && value < O.x_min) value = O.x_min;
            if (O.x_max !== false && value > O.x_max) value = O.x_max;
            break;
        case "y":
            value = this.range_y.snap(value);
            if (O.y_min !== false && value < O.y_min) value = O.y_min;
            if (O.y_max !== false && value > O.y_max) value = O.y_max;
            break;
        case "z":
            value = this.range_z.snap(value);
            if (O.z_min !== false && value < O.z_min) {
                value = O.z_min;
                this.warning(this.element);
            } else if (O.z_max !== false && value > O.z_max) {
                value = O.z_max;
                this.warning(this.element);
            }
            break;
        }

        return TK.Widget.prototype.set.call(this, key, value);
    },
    destroy: function () {
        remove_zhandle.call(this);
        remove_line1.call(this);
        remove_line2.call(this);
        remove_label.call(this);
        remove_handle.call(this);
        TK.Widget.prototype.destroy.call(this);
    },
});
})(this, this.TK);
