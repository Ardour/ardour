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

function invalidate_bands() {
    this.invalid.bands = true;
    this.trigger_draw();
}

function sort_bands() {
    this.bands.sort(function (a, b) {
        return a.options.freq - b.options.freq;
    });
}

function limit_bands () {
    if (this.options.leap) return;
    sort_bands.call(this);
    for (var i = 0; i < this.bands.length; i++)
        _set_freq.call(this, i, this.bands[i]);
}

function set_freq (band) {
    if (this.options.leap) return;
    var i = this.bands.indexOf(band);
    if (i < 0) {
        TK.error("Band no member of crossover");
        return;
    }
    _set_freq.call(this, i, band);
}

function _set_freq (i, band) {
    var freq = band.options.freq;
    var dist = freq * this.get("distance")
    if (i)
        this.bands[i-1].set("x_max", freq - dist);
    if (i < this.bands.length-1)
        this.bands[i+1].set("x_min", freq + dist);
}

TK.Crossover = TK.class({
    /**
     * TK.Crossover is a {@link TK.Equalizer} displaying the response
     * of a multi-band crossover filter. TK.Crossover  uses {@link TK.CrossoverBand}
     * as response handles.
     * 
     * @class TK.Crossover
     * 
     * @extends TK.Equalizer
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Boolean} [options.leap=true] - Define if bands are allowed to leap over each other.
     * @property {Number} [options.distance=0] - Set a minimal distance between bands if leaping is not allowed.
     *   Value is a factor of x. Example: if distance=0.2 a band cannot be moved beyond 800Hz if the upper next
     *   band is at 1kHz.
     */
    _class: "Crossover",
    Extends: TK.Equalizer,
    _options: Object.assign(Object.create(TK.Equalizer.prototype._options), {
        leap: "boolean",
        distance: "number",
    }),
    options: {
        range_y: {min:-60, max: 12, scale: "linear"},
        leap: true,
        distance: 0,
    },
    initialize: function (options) {
        TK.Equalizer.prototype.initialize.call(this, options);
        /**
         * @member {HTMLDivElement} TK.Equalizer#element - The main DIV container.
         *   Has class <code>toolkit-response-handler</code>.
         */
        TK.add_class(this.element, "toolkit-crossover");
    },
    resize: function () {
        invalidate_bands.call(this);
        TK.Equalizer.prototype.resize.call(this);
    },
    redraw: function () {
        var I = this.invalid;
        var O = this.options;
        var lastb = this.bands.length - 1;
        var lastg = this.graphs.length - 1;
        if (I.validate("bands", "accuracy")) {
            I.bands = false;
            I.accuracy = false;
            sort_bands.call(this);
            for (var i = 0; i < this.bands.length; i++) {
                var f = [this.bands[i].lower.get_freq2gain()];
                if (i)
                    f.push(this.bands[i-1].upper.get_freq2gain());
                this._draw_graph(this.graphs[i], f);
            }
            this._draw_graph(this.graphs[lastg], [this.bands[lastb].upper.get_freq2gain()]);
        }
        TK.Equalizer.prototype.redraw.call(this);
    },
    add_band: function (options, type) {
        type = type || TK.CrossoverBand;
        this.add_graph();
        var r = TK.Equalizer.prototype.add_band.call(this, options, type);
        var that = this;
        r.add_event("set_freq", function (f) {
            set_freq.call(that, this);
        });
        limit_bands.call(this);
        return r;
    },
    remove_band: function (band) {
        this.remove_graph(this.graphs[this.graphs.length-1]);
        var r = TK.Equalizer.prototype.remove_band.call(this, options);
        limit_bands.call(this);
        return r;
    },
});

TK.CrossoverBand = TK.class({
    /**
     * TK.CrossoverBand is a {@link TK.EqBand} with an additional filter.
     * 
     * @param {Object} [options={ }] - An object containing additional options.
     * 
     * @property {String|Function} [lower="lowpass3"] - The type of filter for the range below cutoff frequency. See {@link TK.EqBand} for more information.
     * @property {String|Function} [upper="highpass3"] - The type of filter for the range above cutoff frequency. See {@link TK.EqBand} for more information.
     * @property {Function} [label=function (t, x, y, z) { return TK.sprintf("%.2f Hz", x); }] - The function formatting the handles label.
     * 
     * @class TK.CrossoverBand
     * 
     * @extends TK.EqBand
     */
    _class: "CrossoverBand",
    Extends: TK.EqBand,
    _options: Object.assign(Object.create(TK.EqBand.prototype._options), {
        lower: "string",
        upper: "string",
    }),
    options: {
        lower: "lowpass3",
        upper: "highpass3",
        label: function (t, x, y, z) { return TK.sprintf("%.2f Hz", x); },
        mode: "line-vertical", // undocumented, just a default differing from TK.ResponseHanlde
        preferences: [ "top-right", "right", "bottom-right", "top-left", "left", "bottom-left"], // undocumented, just a default differing from TK.ResponseHanlde
    },
    initialize: function (options) {
        /**
         * @member {TK.Filter} TK.CrossoverBand#upper - The filter providing the graphical calculations for the upper graph. 
         */
        this.upper = new TK.Filter();
        /**
         * @member {TK.Filter} TK.CrossoverBand#lower - The filter providing the graphical calculations for the lower graph. 
         */
        this.lower = new TK.Filter();
        TK.EqBand.prototype.initialize.call(this, options);
        /** 
         * @member {HTMLDivElement} TK.CrossoverBand#element - The main SVG group.
         *   Has class <code>toolkit-crossoverband</code>.
         */
        TK.add_class(this.element, "toolkit-crossoverband");
        
        this.set("lower", this.options.lower);
        this.set("upper", this.options.upper);
    },
    set: function (key, val) {
        switch (key) {
            case"lower":
                this.filter = this.lower;
                var r = TK.EqBand.prototype.set.call(this, "type", val);
                this.set("mode", "line-vertical");
                return r;
            case "upper":
                this.filter = this.upper;
                var r = TK.EqBand.prototype.set.call(this, "type", val);
                this.set("mode", "line-vertical");
                return r;
            case "freq":
            case "gain":
            case "q":
                if (this.lower)
                    this.filter = this.lower;
                val = TK.EqBand.prototype.set.call(this, key, val);
                this.filter = this.upper;
                return TK.EqBand.prototype.set.call(this, key, val);
        }
        return TK.EqBand.prototype.set.call(this, key, val);
    },
});

})(this, this.TK);
