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
function LinearSnapModule(stdlib, foreign) {
    var min = +foreign.min;
    var max = +foreign.max;
    var step = +foreign.step;
    var base = +foreign.base;

    var floor = stdlib.Math.floor;
    var ceil  = stdlib.Math.ceil;

    function low_snap(v, direction) {
        v = +v;
        direction = +direction;
        var n = 0.0;
        var t = 0.0;

        if (!(v > min)) {
            v = min;
            direction = 1.0;
        } else if (!(v < max)) {
            v = max;
            direction = +1.0;
        }

        t = (v - base)/step;

        if (direction > 0.0) n = ceil(t);
        else if (direction < 0.0) n = floor(t);
        else {
            if (t - floor(t) < 0.5) {
                n = floor(t);
            } else {
                n = ceil(t);
            }
        }

        return base + step * n;
    }

    /**
     * Returns the nearest value on the grid which is bigger than <code>value</code>.
     *
     * @method TK.Ranged#snap_up
     *
     * @param {number} value - The value to snap.
     *
     * @returns {number} The snapped value.
     */
    function snap_up(v) {
        v = +v;
        return +low_snap(v, 1.0);
    }

    /**
     * Returns the nearest value on the grid which is smaller than <code>value</code>.
     *
     * @method TK.Ranged#snap_down
     *
     * @param {number} value - The value to snap.
     *
     * @returns {number} The snapped value.
     */
    function snap_down(v) {
        v = +v;
        return +low_snap(v, -1.0);
    }

    /**
     * Returns the nearest value on the grid. Its rounding behavior is similar to that
     * of <code>Math.round</code>.
     *
     * @method TK.Ranged#snap
     *
     * @param {number} value - The value to snap.
     *
     * @returns {number} The snapped value.
     */
    function snap(v) {
        v = +v;
        return +low_snap(v, 0.0);
    }

    return {
        snap_up : snap_up,
        snap_down : snap_down,
        snap : snap
    };
}

function ArraySnapModule(stdlib, foreign, heap) {
    var values = new stdlib.Float64Array(heap);
    var len = (heap.byteLength>>3)|0;
    var min = +(foreign.min !== void 0 ? foreign.min : values[0]);
    var max = +(foreign.max !== void 0 ? foreign.max : values[len-1]);

    function low_snap(v, direction) {
        v = +v;
        direction = +direction;
        var a = 0;
        var mid = 0;
        var b = 0;
        var t = 0.0;

        b = len-1;

        if (!(v > min)) v = min;
        if (!(v < max)) v = max;

        if (!(v < +values[b << 3 >> 3])) return +values[b << 3 >> 3];
        if (!(v > +values[0])) return +values[0];

        do {
            mid = (a + b) >>> 1;
            t = +values[mid << 3 >> 3];
            if (v > t) a = mid;
            else if (v < t) b = mid;
            else return t;
        } while (((b - a)|0) > 1);

        if (direction > 0.0) return +values[b << 3 >> 3];
        else if (direction < 0.0) return +values[a << 3 >> 3];

        if (values[b << 3 >> 3] - v <= v - values[a << 3 >> 3]) return +values[b << 3 >> 3];
        return +values[a << 3 >> 3];
    }

    function snap_up(v) {
        v = +v;
        return +low_snap(v, 1.0);
    }

    function snap_down(v) {
        v = +v;
        return +low_snap(v, -1.0);
    }

    function snap(v) {
        v = +v;
        return +low_snap(v, 0.0);
    }

    return {
        snap_up : snap_up,
        snap_down : snap_down,
        snap : snap
    };
}
function NullSnapModule(stdlib, foreign, heap) {
    var min = +foreign.min;
    var max = +foreign.max;

    function snap(v) {
        v = +v;
        if (!(v < max)) v = max;
        if (!(v > min)) v = min;
        return v;
    }

    return {
        snap: snap,
        snap_up: snap,
        snap_down: snap,
    };
}
function num_sort(a) {
    a = a.slice(0);
    a.sort(function(a, b) { return a - b; });
    return a;
}
function update_snap() {
    var O = this.options;
    // Notify that the ranged options have been modified
    if (Array.isArray(O.snap)) {
        Object.assign(this, ArraySnapModule(window, O, new Float64Array(num_sort(O.snap)).buffer));
    } else if (typeof O.snap === "number" && O.snap > 0.0) {
        Object.assign(this, LinearSnapModule(window, { min : Math.min(O.min, O.max), max : Math.max(O.min, O.max), step : O.snap, base: O.base||0 }));
    } else if (O.min < Infinity && O.max > -Infinity) {
        Object.assign(this, NullSnapModule(window, { min : Math.min(O.min, O.max), max : Math.max(O.min, O.max) }));
    } else {
        Object.assign(this, {
            snap: function(v) { return +v; },
            snap_up: function(v) { return +v; },
            snap_down: function(v) { return +v; },
        });
    }
}
function TRAFO_PIECEWISE(stdlib, foreign, heap) {
    var reverse = foreign.reverse|0;
    var l = heap.byteLength >> 4;
    var X = new Float64Array(heap, 0, l);
    var Y = new Float64Array(heap, l*8, l);
    var basis = +foreign.basis;

    function val2based(coef, size) {
        var a = 0,
            b = (l-1)|0,
            mid = 0,
            t = 0.0;

        coef = +coef;
        size = +size;

        if (!(coef > +Y[0])) return +X[0] * size;
        if (!(coef < +Y[b << 3 >> 3])) return +X[b << 3 >> 3] * size;

        do {
            mid = (a + b) >>> 1;
            t = +Y[mid << 3 >> 3];
            if (coef > t) a = mid;
            else if (coef < t) b = mid;
            else return +X[mid << 3 >> 3] * size;
        } while (((b - a)|0) > 1);

        /* value lies between a and b */

        t = (+X[b << 3 >> 3] - +X[a << 3 >> 3]) / (+Y[b << 3 >> 3] - +Y[a << 3 >> 3]);

        t = +X[a << 3 >> 3] + (coef - +Y[a << 3 >> 3]) * t;

        t *= size;

        if (reverse) t = size - t;

        return t;
    }
    function based2val(coef, size) {
        var a = 0,
            b = (l-1)|0,
            mid = 0,
            t = 0.0;

        coef = +coef;
        size = +size;
        if (reverse) coef = size - coef;
        coef /= size;

        if (!(coef > 0)) return Y[0];
        if (!(coef < 1)) return Y[b << 3 >> 3];

        do {
            mid = (a + b) >>> 1;
            t = +X[mid << 3 >> 3];
            if (coef > t) a = mid;
            else if (coef < t) b = mid;
            else return +Y[mid << 3 >> 3];
        } while (((b - a)|0) > 1);

        /* value lies between a and b */

        t = (+Y[b << 3 >> 3] - +Y[a << 3 >> 3]) / (+X[b << 3 >> 3] - +X[a << 3 >> 3]);

        return +Y[a << 3 >> 3] + (coef - +X[a << 3 >> 3]) * t;
    }
    function val2px(n) { return val2based(n, basis || 1); }
    function px2val(n) { return based2val(n, basis || 1); }
    function val2coef(n) { return val2based(n, 1); }
    function coef2val(n) { return based2val(n, 1); }
    return {
        val2based:val2based,
        based2val:based2val,
        val2px:val2px,
        px2val:px2val,
        val2coef:val2coef,
        coef2val:coef2val,
    };
};
function TRAFO_FUNCTION(stdlib, foreign) {
    var reverse = foreign.reverse|0;
    var min = +foreign.min;
    var max = +foreign.max;
    var scale = foreign.scale;
    var basis = +foreign.basis;
    function val2based(value, size) {
        value = +value;
        size = +size;
        value = scale(value, foreign, false) * size;
        if (reverse) value = size - value;
        return value;
    }
    function based2val(coef, size) {
        coef = +coef;
        size = +size;
        if (reverse) coef = size - coef;
        coef = scale(coef/size, foreign, true);
        return coef;
    }
    function val2px(n) { return val2based(n, basis || 1); }
    function px2val(n) { return based2val(n, basis || 1); }
    function val2coef(n) { return val2based(n, 1); }
    function coef2val(n) { return based2val(n, 1); }
    return {
        val2based:val2based,
        based2val:based2val,
        val2px:val2px,
        px2val:px2val,
        val2coef:val2coef,
        coef2val:coef2val,
    };
}
function TRAFO_LINEAR(stdlib, foreign) {
    var reverse = foreign.reverse|0;
    var min = +foreign.min;
    var max = +foreign.max;
    var basis = +foreign.basis;
    function val2based(value, size) {
        value = +value;
        size = +size;
        value = ((value - min) / (max - min)) * size;
        if (reverse) value = size - value;
        return value;
    }
    function based2val(coef, size) {
        coef = +coef;
        size = +size;
        if (reverse) coef = size - coef;
        coef = (coef / size) * (max - min) + min;
        return coef;
    }
    // just a wrapper for having understandable code and backward
    // compatibility
    function val2px(n) { n = +n; if (basis == 0.0) basis = 1.0; return +val2based(n, basis); }
    // just a wrapper for having understandable code and backward
    // compatibility
    function px2val(n) { n = +n; if (basis == 0.0) basis = 1.0; return +based2val(n, basis); }
    // calculates a coefficient for the value
    function val2coef(n) { n = +n; return +val2based(n, 1.0); }
    // calculates a value from a coefficient
    function coef2val(n) { n = +n; return +based2val(n, 1.0); }
    return {
        /**
         * Transforms a value from the coordinate system to the interval <code>0</code>...<code>basis</code>.
         *
         * @method TK.Ranged#val2based
         *
         * @param {number} value
         * @param {number} [basis=1]
         *
         * @returns {number}
         */
        val2based:val2based,
        /**
         * Transforms a value from the interval <code>0</code>...<code>basis</code> to the coordinate system.
         *
         * @method TK.Ranged#based2val
         *
         * @param {number} value
         * @param {number} [basis=1]
         *
         * @returns {number}
         */
        based2val:based2val,
        /**
         * This is an alias for {@link TK.Ranged#val2px}.
         *
         * @method TK.Ranged#val2px
         *
         * @param {number} value
         *
         * @returns {number}
         */
        val2px:val2px,
        /**
         * This is an alias for {@link TK.Ranged#px2val}.
         *
         * @method TK.Ranged#px2val
         *
         * @param {number} value
         *
         * @returns {number}
         */
        px2val:px2val,
        /**
         * Calls {@link based2val} with <code>basis = 1</code>.
         *
         * @method TK.Ranged#val2coef
         *
         * @param {number} value
         *
         * @returns {number}
         */
        val2coef:val2coef,
        /**
         * Calls {@link based2val} with <code>basis = 1</code>.
         *
         * @method TK.Ranged#coef2val
         *
         * @param {number} value
         *
         * @returns {number}
         */
        coef2val:coef2val,
    };
}
function TRAFO_LOG(stdlib, foreign) {
    var db2scale = stdlib.TK.AudioMath.db2scale;
    var scale2db = stdlib.TK.AudioMath.scale2db;
    var reverse = foreign.reverse|0;
    var min = +foreign.min;
    var max = +foreign.max;
    var log_factor = +foreign.log_factor;
    var trafo_reverse = foreign.trafo_reverse|0;
    var basis = +foreign.basis;
    function val2based(value, size) {
        value = +value;
        size = +size;
        value = +db2scale(value, min, max, size, trafo_reverse, log_factor);
        if (reverse) value = size - value;
        return value;
    }
    function based2val(coef, size) {
        coef = +coef;
        size = +size;
        if (reverse) coef = size - coef;
        coef = +scale2db(coef, min, max, size, trafo_reverse, log_factor);
        return coef;
    }
    function val2px(n) { return val2based(n, basis || 1); }
    function px2val(n) { return based2val(n, basis || 1); }
    function val2coef(n) { return val2based(n, 1); }
    function coef2val(n) { return based2val(n, 1); }
    return {
        val2based:val2based,
        based2val:based2val,
        val2px:val2px,
        px2val:px2val,
        val2coef:val2coef,
        coef2val:coef2val,
    };
}
function TRAFO_FREQ(stdlib, foreign) {
    var freq2scale = stdlib.TK.AudioMath.freq2scale;
    var scale2freq = stdlib.TK.AudioMath.scale2freq;
    var reverse = foreign.reverse|0;
    var min = +foreign.min;
    var max = +foreign.max;
    var trafo_reverse = foreign.trafo_reverse|0;
    var basis = +foreign.basis;
    function val2based(value, size) {
        value = +value;
        size = +size;
        value = +freq2scale(value, min, max, size, trafo_reverse);
        if (reverse) value = size - value;
        return value;
    }
    function based2val(coef, size) {
        coef = +coef;
        size = +size;
        if (reverse) coef = size - coef;
        coef = +scale2freq(coef, min, max, size, trafo_reverse);
        return coef;
    }
    function val2px(n) { return val2based(n, basis || 1); }
    function px2val(n) { return based2val(n, basis || 1); }
    function val2coef(n) { return val2based(n, 1); }
    function coef2val(n) { return based2val(n, 1); }
    return {
        val2based:val2based,
        based2val:based2val,
        val2px:val2px,
        px2val:px2val,
        val2coef:val2coef,
        coef2val:coef2val,
    };
}
function update_transformation() {
    var O = this.options;
    var scale = O.scale;

    var module;

    if (typeof scale === "function") {
        module = TRAFO_FUNCTION(w, O);
    } else if (Array.isArray(scale)) {
        var i = 0;
        if (scale.length % 2) {
            TK.error("Malformed piecewise-linear scale.");
        }

        for (i = 0; i < scale.length/2 - 1; i++) {
            if (!(scale[i] >= 0 && scale[i] <= 1))
                TK.error("piecewise-linear x value not in [0,1].");
            if (!(scale[i] < scale[i+1]))
                TK.error("piecewise-linear array not sorted.");
        }
        for (i = scale.length/2; i < scale.length - 1; i++) {
            if (!(scale[i] < scale[i+1]))
                TK.error("piecewise-linear array not sorted.");
        }

        module = TRAFO_PIECEWISE(w, O, new Float64Array(scale).buffer);
    } else switch (scale) {
        case "linear":
            module = TRAFO_LINEAR(w, O);
            break;
        case "decibel":
            O.trafo_reverse = 1;
            module = TRAFO_LOG(w, O);
            break;
        case "log2":
            O.trafo_reverse = 0;
            module = TRAFO_LOG(w, O);
            break;
        case "frequency":
            O.trafo_reverse = 0;
            module = TRAFO_FREQ(w, O);
            break;
        case "frequency-reverse":
            O.trafo_reverse = 1;
            module = TRAFO_FREQ(w, O);
            break;
        default:
            TK.warn("Unsupported scale", scale);
    }

    Object.assign(this, module);
}
function set_cb(key, value) {
    switch (key) {
    case "min":
    case "max":
    case "snap":
        update_snap.call(this);
        /* fall through */
    case "log_factor":
    case "scale":
    case "reverse":
    case "basis":
        update_transformation.call(this);
        this.fire_event("rangedchanged");
        break;
    }
}
/**
 * @callback TK.Ranged~scale_cb
 *
 * @param {number} value - The value to be transformed.
 * @param {Object} [options={ }] - An object containing initial options. - The options of the corresponding {@link TK.Ranged} object.
 * @param {boolean} [inverse=false] - Determines if the value is to be transformed from or
 *   to the coordinate system.
 *
 * @returns {number} The transformed value.
 */
TK.Ranged = TK.class({
    /**
     * TK.Ranged combines functionality for two distinct purposes.
     * Firstly, TK.Ranged can be used to snap values to a virtual grid.
     * This grid is defined by the options <code>snap</code>,
     * <code>step</code>, <code>min</code>, <code>max</code> and <code>base</code>.
     * The second feature of TK.anged is that it allows transforming values between coordinate systems.
     * This can be used to transform values from and to linear scales in which they are displayed on the
     * screen. It is used inside of Toolkit to translate values (e.g. in Hz or dB) to pixel positions or
     * percentages, for instance in widgets such as {@link TK.Scale}, {@link TK.MeterBase} or
     * {@link TK.Graph}.
     *
     * TK.Ranged features several types of coordinate systems which are often used in audio applications.
     * They can be configured using the <code>options.scale</code> option, possible values are:
     * <ul>
     *  <li><code>linear</code> for linear coordinates,
     *  <li><code>decibel</code> for linear coordinates,
     *  <li><code>log2</code> for linear coordinates,
     *  <li><code>frequency</code> for linear coordinates or
     *  <li><code>frequency-reverse"</code> for linear coordinates.
     * </ul>
     * If <code>options.scale</code> is a function, it is used as the coordinate transformation.
     * Its signature is {@link TK.Ranged~scale_cb}. This allows the definition of custom
     * coordinate transformations, which go beyond the standard types.
     *
     * @param {Object} [options={ }] - An object containing initial options.
     *
     * @property {String|Array<Number>|Function} [options.scale="linear"] -
     *  The type of the scale. Either one of <code>linear</code>, <code>decibel</code>, <code>log2</code>,
     *  <code>frequency</code> or <code>frequency-reverse</code>; or an array containing a
     *  piece-wise linear scale;
     *  or a callback function of type {@link TK.Ranged~scale_cb}.
     * @property {Boolean} [options.reverse=false] - Reverse the scale of the range.
     * @property {Number} [options.basis=1] - The size of the linear scale. Set to pixel width or height
     * if used for drawing purposes or to 100 for percentages.
     * @property {Number} [options.min=0] - Minimum value of the range.
     * @property {Number} [options.max=1] - Maximum value of the range.
     * @property {Number} [options.log_factor=1] - Used to overexpand logarithmic curves. 1 keeps the
     *  natural curve while values above 1 will overbend.
     * @property {Number|Array.<number>} [options.snap=0] -
     *  Defines a virtual grid.
     *  If <code>options.snap</code> is a positive number, it is interpreted as the distance of
     *  grid points.
     *  Then, inside of the interval <code>options.min</code> ... <code>options.max</code> the grid
     *  points are <code> options.base + n * options.snap </code> where <code>n</code> is any
     *  integer. Any values outside of that interval are snapped to the biggest or smallest grid
     *  point, respectively.
     *  In order to define grids with non-uniform spacing, set <code>options.snap</code> to an Array
     *  of grid points.
     * @property {Number} [options.base=0] - Base point. Used e.g. to mark 0dB on a fader from -96dB to 12dB.
     * @property {Number} [options.step=0] - Step size. Used for instance by {@link TK.ScrollValue}
     *  as the step size.
     * @property {Number} [options.shift_up=4] - Multiplier for increased stepping speed, e.g. used by
     *  {@link TK.ScrollValue} when simultaneously pressing 'shift'.
     * @property {Number} [options.shift_down=0.25] - Multiplier for descresed stepping speed, e.g. used by
     *  {@link TK.ScrollValue} when simultaneously pressing 'shift' and 'ctrl'.
     *
     * @mixin TK.Ranged
     */

    _class: "Ranged",
    options: {
        scale:          "linear",
        reverse:        false,
        basis:          1,
        min:            0,
        max:            1,
        base:           0,
        step:           0,
        shift_up:       4,
        shift_down:     0.25,
        snap:           0,
        round:          true, /* default for TK.Range, no dedicated option */
        log_factor:     1,
        trafo_reverse:  false, /* used internally, no documentation */
    },
    _options: {
        scale: "string|array|function",
        reverse: "boolean",
        basis: "number",
        min: "number",
        max: "number",
        base: "number",
        step: "number",
        shift_up: "number",
        shift_down: "number",
        snap: "mixed",
        round: "boolean",
        log_factor: "number",
        trafo_reverse: "boolean",
    },
    static_events: {
        set: set_cb,
        initialized: function() {
            var O = this.options;
            if (!(O.min <= O.max))
                TK.warn("Ranged needs min <= max. min: ", O.min, ", max:", O.max, ", options:", O);
            update_snap.call(this);
            update_transformation.call(this);
        },
    },
});
})(this, this.TK);
