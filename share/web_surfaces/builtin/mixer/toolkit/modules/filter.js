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
/* These formulae for 'standard' biquad filter coefficients are
 * from
 *  "Cookbook formulae for audio EQ biquad filter coefficients"
 *  by Robert Bristow-Johnson.
 *
 */

function Null(O) {
    /* this biquad does not do anything */
    return {
	b0: 1,
	b1: 1,
	b2: 1,
	a0: 1,
	a1: 1,
	a2: 1,
	sample_rate: O.sample_rate,
    };
}

function LowShelf(O) {
    var cos = Math.cos,
        sqrt = Math.sqrt,
        A = Math.pow(10, O.gain / 40),
        w0 = 2*Math.PI*O.freq/O.sample_rate,
        alpha = Math.sin(w0)/(2*O.q);
    return {
	b0:    A*( (A+1) - (A-1)*cos(w0) + 2*sqrt(A)*alpha ),
	b1:  2*A*( (A-1) - (A+1)*cos(w0)                   ),
	b2:    A*( (A+1) - (A-1)*cos(w0) - 2*sqrt(A)*alpha ),
	a0:        (A+1) + (A-1)*cos(w0) + 2*sqrt(A)*alpha,
	a1:   -2*( (A-1) + (A+1)*cos(w0)                   ),
	a2:        (A+1) + (A-1)*cos(w0) - 2*sqrt(A)*alpha,
	sample_rate: O.sample_rate,
    };
}

function HighShelf(O) {
    var cos = Math.cos;
    var sqrt = Math.sqrt;
    var A = Math.pow(10, O.gain / 40);
    var w0 = 2*Math.PI*O.freq/O.sample_rate;
    var alpha = Math.sin(w0)/(2*O.q);
    return {
	b0:    A*( (A+1) + (A-1)*cos(w0) + 2*sqrt(A)*alpha ),
	b1: -2*A*( (A-1) + (A+1)*cos(w0)                   ),
	b2:    A*( (A+1) + (A-1)*cos(w0) - 2*sqrt(A)*alpha ),
	a0:        (A+1) - (A-1)*cos(w0) + 2*sqrt(A)*alpha,
	a1:    2*( (A-1) - (A+1)*cos(w0)                   ),
	a2:        (A+1) - (A-1)*cos(w0) - 2*sqrt(A)*alpha,
	sample_rate: O.sample_rate,
    };
}

function Peaking(O) {
    var cos = Math.cos;
    var A = Math.pow(10, O.gain / 40);
    var w0 = 2*Math.PI*O.freq/O.sample_rate;
    var alpha = Math.sin(w0)/(2*O.q);
    return {
        b0:   1 + alpha*A,
        b1:  -2*cos(w0),
        b2:   1 - alpha*A,
        a0:   1 + alpha/A,
        a1:  -2*cos(w0),
        a2:   1 - alpha/A,
        sample_rate: O.sample_rate,
    };
}

function Notch(O) {
    var cos = Math.cos;
    var w0 = 2*Math.PI*O.freq/O.sample_rate;
    var alpha = Math.sin(w0)/(2*O.q);
    return {
	b0:   1,
	b1:  -2*cos(w0),
	b2:   1,
	a0:   1 + alpha,
	a1:  -2*cos(w0),
	a2:   1 - alpha,
        sample_rate: O.sample_rate,
    };
}

/* This is a standard lowpass filter with transfer function
 * H(s) = 1/(1+s)
 */
function LowPass1(O) {
    var w0 = 2*Math.PI*O.freq/O.sample_rate,
        s0 = Math.sin(w0),
        c0 = Math.cos(w0);
    return {
	b0: 1-c0,
	b1: 2*(1-c0),
        b2: 1-c0,
	a0: 1 - c0 + s0,
	a1: 2*(1-c0),
        a2: 1 - c0 - s0,
        sample_rate: O.sample_rate,
    };
}

function LowPass2(O) {
    var cos = Math.cos;
    var w0 = 2*Math.PI*O.freq/O.sample_rate;
    var alpha = Math.sin(w0)/(2*O.q);
    return {
	b0:  (1 - cos(w0))/2,
	b1:   1 - cos(w0),
	b2:  (1 - cos(w0))/2,
	a0:   1 + alpha,
	a1:  -2*cos(w0),
	a2:   1 - alpha,
        sample_rate: O.sample_rate,
    };
}

function LowPass4(O) {
    O = LowPass2(O);
    O.factor = 2;
    return O;
}

/* This is a standard highpass filter with transfer function
 * H(s) = s/(1+s)
 */
function HighPass1(O) {
    var w0 = 2*Math.PI*O.freq/O.sample_rate,
        s0 = Math.sin(w0),
        c0 = Math.cos(w0);
    return {
	b0: s0,
	b1: 0,
        b2: -s0,
	a0: 1 - c0 + s0,
	a1: 2*(1-c0),
        a2: 1 - c0 - s0,
        sample_rate: O.sample_rate,
    };
}

function HighPass2(O) {
    var cos = Math.cos;
    var w0 = 2*Math.PI*O.freq/O.sample_rate;
    var alpha = Math.sin(w0)/(2*O.q);
    return {
	b0:  (1 + cos(w0))/2,
	b1: -(1 + cos(w0)),
	b2:  (1 + cos(w0))/2,
	a0:   1 + alpha,
	a1:  -2*cos(w0),
	a2:   1 - alpha,
        sample_rate: O.sample_rate,
    };
}

function HighPass4(O) {
    O = HighPass2(O);
    O.factor = 2;
    return O;
}

var Filters = {
    Null: Null,
    LowShelf:  LowShelf,
    HighShelf: HighShelf,
    Peaking:   Peaking,
    Notch:        Notch,
    LowPass1:     LowPass1,
    LowPass2:     LowPass2,
    LowPass4:     LowPass4,
    HighPass1:    HighPass1,
    HighPass2:    HighPass2,
    HighPass4:    HighPass4,
};

var standard_biquads = {
    "null":       BiquadFilter(Null),
    "low-shelf":  BiquadFilter(LowShelf),
    "high-shelf": BiquadFilter(HighShelf),
    parametric:   BiquadFilter(Peaking),
    notch:        BiquadFilter(Notch),
    lowpass1:     BiquadFilter(LowPass1),
    lowpass2:     BiquadFilter(LowPass2),
    lowpass3:     BiquadFilter(LowPass1, LowPass2),
    lowpass4:     BiquadFilter(LowPass4),
    highpass1:    BiquadFilter(HighPass1),
    highpass2:    BiquadFilter(HighPass2),
    highpass3:    BiquadFilter(HighPass1, HighPass2),
    highpass4:    BiquadFilter(HighPass4),
};

var NullModule = { freq2gain: function(f) { return 0.0; } };

function BilinearModule(w, O) {
    var log = Math.log;
    var sin = Math.sin;

    var LN10_10 = (O.factor||1.0) * 10/Math.LN10;
    var PI = +(Math.PI/O.sample_rate);
    var Ra = +((O.a0 + O.a1) * (O.a0 + O.a1) / 4);
    var Rb = +((O.b0 + O.b1) * (O.b0 + O.b1) / 4);
    var Ya = +(O.a1 * O.a0);
    var Yb = +(O.b1 * O.b0);

    if (Ra === Rb && Ya === Yb) return NullModule;

    function freq2gain(f) {
        f = +f;
        var S = +sin(PI * f)
        S *= S;
        return LN10_10 * log( (Rb - S * Yb) /
                              (Ra - S * Ya) );
    }

    return { freq2gain: freq2gain };
}

function BiquadModule(w, O) {
    var log = Math.log;
    var sin = Math.sin;

    var LN10_10 = (O.factor||1.0) * 10/Math.LN10;
    var PI = +(Math.PI/O.sample_rate);
    var Ra = +((O.a0 + O.a1 + O.a2) * (O.a0 + O.a1 + O.a2) / 4);
    var Rb = +((O.b0 + O.b1 + O.b2) * (O.b0 + O.b1 + O.b2) / 4);
    var Xa = +(4 * O.a0 * O.a2);
    var Ya = +(O.a1 * (O.a0 + O.a2));
    var Xb = +(4 * O.b0 * O.b2);
    var Yb = +(O.b1 * (O.b0 + O.b2));

    if (Ra === Rb && Ya === Yb && Xa === Xb) return NullModule;

    function freq2gain(f) {
        f = +f;
        var S = +sin(PI * f)
        S *= S;
        return LN10_10 * log( (Rb - S * (Xb * (1 - S) + Yb)) /
                              (Ra - S * (Xa * (1 - S) + Ya)) );
    }

    return { freq2gain: freq2gain };
}

function BiquadFilter1(trafo) {
    function factory(stdlib, O) {
        return BiquadModule(stdlib, trafo(O));
    }

    return factory;
}

function BiquadFilterN(trafos) {
    function factory(stdlib, O) {
        var A = new Array(trafos.length);
        var i;

        for (i = 0; i < trafos.length; i ++) {
            A[i] = BiquadModule(stdlib, trafos[i](O)).freq2gain;
        }

        return {
            freq2gain: function(f) {
                var ret = 0.0;
                var i;

                for (i = 0; i < A.length; i++) {
                    ret += A[i](f);
                }

                return ret;
            }
        };
    }

    return factory;
}

function BiquadFilter() {
    if (arguments.length === 1) return BiquadFilter1(arguments[0]);

    return BiquadFilterN.call(this, Array.prototype.slice.call(arguments));
}

TK.BiquadFilter = BiquadFilter;

function reset() {
    this.freq2gain = null;
    /**
     * Is fired when a filters drawing function is reset.
     * 
     * @event TK.Filter#reset
     */
    this.fire_event("reset");
}

TK.Filter = TK.class({
    /**
     * TK.Filter provides the math for calculating a gain from
     * a given frequency for different types of biquad filters.
     *
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Stgring|Function} [options.type="parametric"] - The type of the filter. Possible values are
     *   <code>parametric</code>, <code>notch</code>, <code>low-shelf</code>,
     *   <code>high-shelf</code>, <code>lowpass[n]</code> or <code>highpass[n]</code>.
     * @property {Number} [options.freq=1000] - The initial frequency.
     * @property {Number} [options.gain=0] - The initial gain.
     * @property {Number} [options.q=1] - The initial Q of the filter.
     * @property {Number} [options.sample_rate=44100] - The sample rate.
     *
     * @mixin TK.Filter
     * 
     * @extends TK.Base
     * 
     * @mixes TK.AudioMath
     * @mixes TK.Notes
     */
     
     /**
      * Returns the gain for a given frequency
      * 
      * @method TK.Filter#freq2gain
      * 
      * @param {number} frequency - The frequency to calculate the gain for.
      * 
      * @returns {number} gain - The gain at the given frequency.
      */ 
    _class: "Filter",
    Extends: TK.Base,
    _options: {
        type: "string|function",
        freq: "number",
        gain: "number",
        q: "number",
        sample_rate: "number",
    },
    options: {
        type: "parametric",
        freq: 1000,
        gain: 0,
        q:    1,
        sample_rate: 44100,
    },
    static_events: {
        set_freq: reset,
        set_type: reset,
        set_q: reset,
        set_gain: reset,
        initialized: reset,
    },
    create_freq2gain: function() {
        var O = this.options;
        var m;

        if (typeof(O.type) === "string") {
            m = standard_biquads[O.type];

            if (!m) {
                TK.error("Unknown standard filter: "+O.type);
                return;
            }
        } else if (typeof(O.type) === "function") {
            m = O.type;
        } else {
            TK.error("Unsupported option 'type'.");
            return;
        }
        this.freq2gain = m(window, O).freq2gain;
    },
    get_freq2gain: function() {
        if (this.freq2gain === null) this.create_freq2gain();
        return this.freq2gain;
    },
    reset: reset,
});

TK.Filters = Filters;

})(this, this.TK);
