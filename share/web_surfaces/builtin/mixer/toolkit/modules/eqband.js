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

var type_to_mode = {
    parametric: "circular",
    notch: "line-vertical",
    lowpass1: "block-right",
    lowpass2: "block-right",
    lowpass3: "block-right",
    lowpass4: "block-right",
    highpass1: "block-left",
    highpass2: "block-left",
    highpass3: "block-left",
    highpass4: "block-left",
    "low-shelf": "line-vertical",
    "high-shelf": "line-vertical",
};

var type_to_pref = {
    parametric: ["left", "top", "right", "bottom"],
    notch: ["left", "right", "top", "bottom"],
    lowpass1: ["left", "top-left", "bottom-left", "right", "top-right", "bottom-right", "top", "bottom"],
    lowpass2: ["left", "top-left", "bottom-left", "right", "top-right", "bottom-right", "top", "bottom"],
    lowpass3: ["left", "top-left", "bottom-left", "right", "top-right", "bottom-right", "top", "bottom"],
    lowpass4: ["left", "top-left", "bottom-left", "right", "top-right", "bottom-right", "top", "bottom"],
    highpass1: ["right", "top-right", "bottom-right", "left", "top-left", "bottom-left", "top", "bottom"],
    highpass2: ["right", "top-right", "bottom-right", "left", "top-left", "bottom-left", "top", "bottom"],
    highpass3: ["right", "top-right", "bottom-right", "left", "top-left", "bottom-left", "top", "bottom"],
    highpass4: ["right", "top-right", "bottom-right", "left", "top-left", "bottom-left", "top", "bottom"],
    "low-shelf": ["left", "right", "top", "bottom"],
    "high-shelf": ["left", "right", "top", "bottom"],
};

TK.EqBand = TK.class({
    /**
     * An TK.EqBand extends a {@link TK.ResponseHandle} and holds a
     * dependent {@link TK.Filter}. It is used as a fully functional representation
     * of a single equalizer band in {@link TK.Equalizer} TK.EqBand needs a {@link TK.Chart} 
     * or any other derivate to be drawn in.
     *
     * @class TK.EqBand
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {String|Function} [options.type="parametric"] - The type of the filter.
     *   Possible values are <code>parametric</code>, <code>notch</code>,
     *   <code>low-shelf</code>, <code>high-shelf</code>, <code>lowpass[n]</code> or
     *   <code>highpass[n]</code>.
     * @property {Number} options.freq - Frequency setting. This is an alias for the option <code>x</code>
     *   defined by {@link TK.ResponseHandle}.
     * @property {Number} options.gain - Gain setting. This is an alias for the option <code>y</code>
     *   defined by {@link TK.ResponseHandle}.
     * @property {Number} options.q - Quality setting. This is an alias for the option <code>z</code>
     *   defined by {@link TK.ResponseHandle}.
     *
     * @extends TK.ResponseHandle
     */
    _class: "EqBand",
    Extends: TK.ResponseHandle,
    _options: Object.assign(Object.create(TK.ResponseHandle.prototype._options), {
        type: "string|function",
        gain: "number",
        freq: "number",
        x: "number",
        y: "number",
        z: "number",
        q: "number",
    }),
    options: {
        type:    "parametric"
    },
    static_events: {
        set_freq: function(v) { this.set("x", v); },
        set_gain: function(v) { this.set("y", v); },
        set_q: function(v) { this.set("z", v); },
        useraction: function(k, v) {
            if (k === 'x') this.set("freq", v);
            if (k === 'y') this.set("gain", v);
            if (k === 'z') this.set("q", v);
        },
    },
    
    initialize: function (options) {
        /**
         * @member {TK.Filter} TK.EqBand#filter - The filter providing the graphical calculations. 
         */
        this.filter = new TK.Filter({
            type: options.type,
        });
        
        TK.ResponseHandle.prototype.initialize.call(this, options);
        
        if (options.mode) var _m = options.mode;
        this.set("type", this.options.type);
        if (_m) this.set("mode", options.mode);

        if (options.x !== void(0))
            this.set("x", options.x);
        else if (options.freq !== void(0))
            this.set("freq", options.freq);
        if (options.y !== void(0))
            this.set("y", options.y);
        else if (options.gain !== void(0))
            this.set("gain", options.gain);
        if (options.z !== void(0))
            this.set("z", options.z);
        else if (options.q !== void(0))
            this.set("q", options.q);
        
        /** 
         * @member {HTMLDivElement} TK.EqBand#element - The main SVG group.
         *   Has class <code>toolkit-eqband</code>.
         */
        TK.add_class(this.element, "toolkit-eqband");
        
        this.filter.reset();
    },

    /**
     * Calculate the gain for a given frequency in Hz.
     *
     * @method TK.EqBand#freq2gain
     * 
     * @param {number} freq - The frequency.
     * 
     * @returns {number} The gain at the given frequency.
     */
    freq2gain: function (freq) {
        return this.filter.get_freq2gain()(freq);
    },
    
    // GETTER & SETTER
    set: function (key, value) {
        switch (key) {
            case "type":
                if (typeof value === "string") {
                    var mode = type_to_mode[value];
                    var pref = type_to_pref[value];
                    if (!mode) {
                        TK.warn("Unsupported type:", value);
                        return;
                    }
                    this.set("mode", mode);
                    this.set("preferences", pref);
                    this.set("show_axis", mode === "line-vertical");
                }
                this.filter.set("type", value);
                break;
            case "freq":
            case "gain":
            case "q":
                value = this.filter.set(key, value);
                break;
            case "x":
                value = this.range_x.snap(value);
                break;
            case "y":
                value = this.range_y.snap(value);
                break;
            case "z":
                value = this.range_z.snap(value);
                break;
        }
        return TK.ResponseHandle.prototype.set.call(this, key, value);
    }
});
})(this, this.TK);
