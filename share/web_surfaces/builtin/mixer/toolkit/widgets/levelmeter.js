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
function vert(O) {
    return O.layout === "left" || O.layout === "right";
}
function clip_timeout() {
    var O = this.options;
    if (!O.auto_clip || O.auto_clip < 0) return false;
    if (this.__cto) return;
    if (O.clip)
        this.__cto = window.setTimeout(this._reset_clip, O.auto_clip);
}
function peak_timeout() {
    var O = this.options;
    if (!O.auto_peak || O.auto_peak < 0) return false;
    if (this.__pto) window.clearTimeout(this.__pto);
    var value = +this.effective_value();
    if (O.peak > O.base && value > O.base ||
        O.peak < O.base && value < O.base)
        this.__pto = window.setTimeout(this._reset_peak, O.auto_peak);
}
function label_timeout() {
    var O = this.options;
    var peak_label = (0 | O.peak_label);
    var base = +O.base;
    var label = +O.label;
    var value = +this.effective_value();

    if (peak_label <= 0) return false;

    if (this.__lto) window.clearTimeout(this.__lto);

    if (label > base && value > base ||
        label < base && value < base)

        this.__lto = window.setTimeout(this._reset_label, peak_label);
}
function top_timeout() {
    var O = this.options;
    if (!O.auto_hold || O.auto_hold < 0) return false;
    if (this.__tto) window.clearTimeout(this.__tto);
    if (O.top > O.base)
        this.__tto = window.setTimeout(
            this._reset_top,
            O.auto_hold);
    else
        this.__tto = null;
}
function bottom_timeout() {
    var O = this.options;
    if (!O.auto_hold || O.auto_hold < 0) return false;
    if (this.__bto) window.clearTimeout(this.__bto);
    if (O.bottom < O.base)
        this.__bto = window.setTimeout(this._reset_bottom, O.auto_hold);
    else
        this.__bto = null;
}
    
TK.LevelMeter = TK.class({
    /**
     * TK.LevelMeter is a fully functional meter bar displaying numerical values.
     * TK.LevelMeter is an enhanced {@link TK.MeterBase}'s containing a clip LED,
     * a peak pin with value label and hold markers.
     * In addition, LevelMeter has an optional falling animation, top and bottom peak
     * values and more.
     *
     * @class TK.LevelMeter
     * 
     * @extends TK.MeterBase
     *
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Boolean} [options.show_clip=false] - If set to <code>true</code>, show the clipping LED.
     * @property {Number} [options.clipping=0] - If clipping is enabled, this is the threshold for the
     *   clipping effect.
     * @property {Integer|Boolean} [options.auto_clip=false] - This is the clipping timeout. If set to
     *   <code>false</code> automatic clipping is disabled. If set to <code>n</code> the clipping effect
     *   times out after <code>n</code> ms, if set to <code>-1</code> it remains forever.
     * @property {Boolean} [options.clip=false] - If clipping is enabled, this option is set to
     *   <code>true</code> when clipping happens. When automatic clipping is disabled, it can be set to
     *   <code>true</code> to set the clipping state.
     * @property {Object} [options.clip_options={}] - Additional options for the {@link TK.State} clip LED.
     * @property {Boolean} [options.show_hold=false] - If set to <code>true</code>, show the hold value LED.
     * @property {Integer} [options.hold_size=1] - Size of the hold value LED in the number of segments.
     * @property {Number|boolean} [options.auto_hold=false] - If set to <code>false</code> the automatic
     *   hold LED is disabled, if set to <code>n</code> the hold value is reset after <code>n</code> ms and
     *   if set to <code>-1</code> the hold value is not reset automatically.
     * @property {Number} [options.top=false] - The top hold value. If set to <code>false</code> it will
     *   equal the meter level.
     * @property {Number} [options.bottom=false] - The bottom hold value. This only exists if a
     *   <code>base</code> value is set and the value falls below the base.
     * @property {Boolean} [options.show_peak=false] - If set to <code>true</code>, show the peak label.
     * @property {Integer|Boolean} [options.peak_label=false] - If set to <code>false</code> the automatic peak
     *   label is disabled, if set to <code>n</code> the peak label is reset after <code>n</code> ms and
     *   if set to <code>-1</code> it remains forever.
     * @property {Function} [options.format_peak=TK.FORMAT("%.2f")] - Formatting function for the peak label.
     * @property {Number} [options.falling=0] - If set to a positive number, activates the automatic falling
     *   animation. The meter level will fall by this amount per frame.
     * @property {Number} [options.falling_fps=24] - This is the number of frames of the falling animation.
     *   It is not an actual frame rate, but instead is used to determine the actual speed of the falling
     *   animation together with the option <code>falling</code>.
     * @property {Number} [options.falling_init=2] - Initial falling delay in number of frames. This option
     *   can be used to delay the start of the falling animation in order to avoid flickering if internal
     *   and external falling are combined.
     */
    _class: "LevelMeter",
    Extends: TK.MeterBase,
    _options: Object.assign(Object.create(TK.MeterBase.prototype._options), {
        falling: "number",
        falling_fps: "number",
        falling_init: "number",
        peak: "number",
        top: "number",
        bottom: "number",
        hold_size: "int",
        show_hold: "boolean",
        clipping: "number",
        auto_clip: "int|boolean",
        auto_peak: "int|boolean",
        peak_label: "int",
        auto_hold: "int|boolean",
        format_peak: "function",
        clip_options: "object",
    }),
    options: {
        clip:         false,
        falling:      0,
        falling_fps:  24,
        falling_init: 2,
        peak:         false,
        top:          false,
        bottom:       false,
        hold_size:    1,
        show_hold:    false,
        clipping:     0,
        auto_clip:    false,
        auto_peak:    false,
        peak_label:   false,
        auto_hold:    false,
        format_peak: TK.FORMAT("%.2f"),
        clip_options: {}
    },
    static_events: {
        set_label: label_timeout,
        set_bottom: bottom_timeout,
        set_top: top_timeout,
        set_peak: peak_timeout,
        set_clip: function(value) {
            if (value) {
                clip_timeout.call(this);
            }
        },
        set_show_peak: peak_timeout,
        set_auto_clip: function(value) {
            if (this.__cto && 0|value <=0)
                window.clearTimeout(this.__cto);
        },
        set_auto_peak: function(value) {
            if (this.__pto && 0|value <=0)
                window.clearTimeout(this.__pto);
        },
        set_peak_label: function(value) {
            if (this.__lto && 0|value <=0)
                window.clearTimeout(this.__lto);
        },
        set_auto_hold: function(value) {
            if (this.__tto && 0|value <=0)
                window.clearTimeout(this.__tto);
            if (this.__bto && 0|value <=0)
                window.clearTimeout(this.__bto);
        },
    },
    
    initialize: function (options) {
        /* track the age of the value option */
        this.track_option("value");
        TK.MeterBase.prototype.initialize.call(this, options);
        this._reset_label = this.reset_label.bind(this);
        this._reset_clip = this.reset_clip.bind(this);
        this._reset_peak = this.reset_peak.bind(this);
        this._reset_top = this.reset_top.bind(this);
        this._reset_bottom = this.reset_bottom.bind(this);
        
        /**
         * @member {HTMLDivElement} TK.LevelMeter#element - The main DIV container.
         *   Has class <code>toolkit-level-meter</code>.
         */
        TK.add_class(this.element, "toolkit-level-meter");

        var O = this.options;
        
        if (O.peak === false)
            O.peak = O.value;
        if (O.top === false)
            O.top = O.value;
        if (O.bottom === false)
            O.bottom = O.value;
        if (O.falling < 0)
            O.falling = -O.falling;
    },
    
    redraw: function () {
        var O = this.options;
        var I = this.invalid;
        var E = this.element;

        if (I.show_hold) {
            I.show_hold = false;
            TK.toggle_class(E, "toolkit-has-hold", O.show_hold);
        }

        if (I.top || I.bottom) {
            /* top and bottom require a meter redraw, so lets invalidate
             * value */
            I.top = I.bottom = false;
            I.value = true;
        }

        if (I.base)
          I.value = true;

        TK.MeterBase.prototype.redraw.call(this);

        if (I.clip) {
            I.clip = false;
            TK.toggle_class(E, "toolkit-clipping", O.clip);
        }
    },
    destroy: function () {
        TK.MeterBase.prototype.destroy.call(this);
    },
    /**
     * Resets the peak label.
     * 
     * @method TK.LevelMeter#reset_peak
     * 
     * @emits TK.LevelMeter#resetpeak
     */
    reset_peak: function () {
        if (this.__pto) clearTimeout(this.__pto);
        this.__pto = false;
        this.set("peak", this.effective_value());
        /**
         * Is fired when the peak was reset.
         * 
         * @event TK.LevelMeter#resetpeak
         */
        this.fire_event("resetpeak");
    },
    /**
     * Resets the label.
     * 
     * @method TK.LevelMeter#reset_label
     * 
     * @emits TK.LevelMeter#resetlabel
     */
    reset_label: function () {
        if (this.__lto) clearTimeout(this.__lto);
        this.__lto = false;
        this.set("label", this.effective_value());
        /**
         * Is fired when the label was reset.
         * 
         * @event TK.LevelMeter#resetlabel
         */
        this.fire_event("resetlabel");
    },
    /**
     * Resets the clipping LED.
     * 
     * @method TK.LevelMeter#reset_clip
     * 
     * @emits TK.LevelMeter#resetclip
     */
    reset_clip: function () {
        if (this.__cto) clearTimeout(this.__cto);
        this.__cto = false;
        this.set("clip", false);
        /**
         * Is fired when the clipping LED was reset.
         * 
         * @event TK.LevelMeter#resetclip
         */
        this.fire_event("resetclip");
    },
    /**
     * Resets the top hold.
     * 
     * @method TK.LevelMeter#reset_top
     * 
     * @emits TK.LevelMeter#resettop
     */
    reset_top: function () {
        this.set("top", this.effective_value());
        /**
         * Is fired when the top hold was reset.
         * 
         * @event TK.LevelMeter#resettop
         */
        this.fire_event("resettop");
    },
    /**
     * Resets the bottom hold.
     * 
     * @method TK.LevelMeter#reset_bottom
     * 
     * @emits TK.LevelMeter#resetbottom
     */
    reset_bottom: function () {
        this.set("bottom", this.effective_value());
        /**
         * Is fired when the bottom hold was reset.
         * 
         * @event TK.LevelMeter#resetbottom
         */
        this.fire_event("resetbottom");
    },
    /**
     * Resets all hold features.
     * 
     * @method TK.LevelMeter#reset_all
     * 
     * @emits TK.LevelMeter#resetpeak
     * @emits TK.LevelMeter#resetlabel
     * @emits TK.LevelMeter#resetclip
     * @emits TK.LevelMeter#resettop
     * @emits TK.LevelMeter#resetbottom
     */
    reset_all: function () {
        this.reset_label();
        this.reset_peak();
        this.reset_clip();
        this.reset_top();
        this.reset_bottom();
    },

    effective_value: function() {
        var O = this.options;
        var falling = +O.falling;
        if (O.falling <= 0) return O.value;
        var value = +O.value, base = +O.base;

        var age = +this.value_time.value;

        if (!(age > 0)) age = Date.now();
        else age = +(Date.now() - age);

        var frame_length = 1000.0 / +O.falling_fps;

        if (age > O.falling_init * frame_length) {
            if (value > base) {
                value -= falling * (age / frame_length);
                if (value < base) value = base;
            } else {
                value += falling * (age / frame_length);
                if (value > base) value = base;
            }
        }

        return value;
    },

    /*
     * This is an _internal_ method, which calculates the non-filled regions
     * in the overlaying canvas as pixel positions. The canvas is only modified
     * using this information when it has _actually_ changed. This can save a lot
     * of performance in cases where the segment size is > 1 or on small devices where
     * the meter has a relatively small pixel size.
     */
    calculate_meter: function(to, value, i) {
        var O = this.options;
        var falling = +O.falling;
        var base    = +O.base;
        value = +value;

        // this is a bit unelegant...
        if (falling) {
            value = this.effective_value();
            // continue animation
            if (value !== base) {
                this.invalid.value = true;
                // request another frame
                this.trigger_draw_next();
            }
        }

        i = TK.MeterBase.prototype.calculate_meter.call(this, to, value, i);

        if (!O.show_hold) return i;

        // shorten things
        var hold       = +O.top;
        var segment   = O.segment|0;
        var hold_size = (O.hold_size|0) * segment;
        var base      = +O.base;
        var pos;

        if (hold > base) {
            /* TODO: lets snap in set() */
            pos = this.val2px(hold)|0;
            if (segment !== 1) pos = segment*(Math.round(pos/segment)|0);

            to[i++] = pos;
            to[i++] = pos+hold_size;
        }

        hold = +O.bottom;

        if (hold < base) {
            pos = this.val2px(hold)|0;
            if (segment !== 1) pos = segment*(Math.round(pos/segment)|0);

            to[i++] = pos;
            to[i++] = pos+hold_size;
        }

        return i;
    },
    
    // GETTER & SETTER
    set: function (key, value) {
        if (key === "value") {
            var O = this.options;
            var base = O.base;

            // snap will enforce clipping
            value = this.snap(value);

            if (O.falling) {
                var v = this.effective_value();
                if (v >= base && value >= base && value < v ||
                    v <= base && value <= base && value > v) {
                    /* NOTE: we are doing a falling animation, but maybe its not running */
                    if (!this.invalid.value) {
                        this.invalid.value = true;
                        this.trigger_draw();
                    }
                    return;
                }
            }
            if (O.auto_clip !== false && value > O.clipping && !this.has_base()) {
                this.set("clip", true);
            }
            if (O.show_label && O.peak_label !== false &&
                (value > O.label && value > base || value < O.label && value < base)) {
                this.set("label", value);
            }
            if (O.auto_peak !== false &&
                (value > O.peak && value > base || value < O.peak && value < base)) {
                this.set("peak", value);
            }
            if (O.auto_hold !== false && O.show_hold && value > O.top) {
                this.set("top", value);
            }
            if (O.auto_hold !== false && O.show_hold && value < O.bottom && this.has_base()) {
                this.set("bottom", value);
            }
        } else if (key === "top" || key === "bottom") {
            value = this.snap(value);
        }
        return TK.MeterBase.prototype.set.call(this, key, value);
    }
});

/**
 * @member {TK.State} TK.LevelMeter#clip - The {@link TK.State} instance for the clipping LED.
 * @member {HTMLDivElement} TK.LevelMeter#clip.element - The DIV element of the clipping LED.
 *   Has class <code>toolkit-clip</code>.
 */
TK.ChildWidget(TK.LevelMeter, "clip", {
    create: TK.State,
    show: false,
    map_options: {
        clip: "state",
    },
    default_options: {
        "class": "toolkit-clip"
    },
    toggle_class: true,
});
/**
 * @member {HTMLDivElement} TK.LevelMeter#_peak - The DIV element for the peak marker.
 *   Has class <code>toolkit-peak</code>.
 */
TK.ChildElement(TK.LevelMeter, "peak", {
    show: false,
    create: function() {
        var peak = TK.element("div","toolkit-peak");
        peak.appendChild(TK.element("div","toolkit-peak-label"));
        return peak;
    },
    append: function() {
        this._bar.appendChild(this._peak);
    },
    toggle_class: true,
    draw_options: [ "peak" ],
    draw: function (O) {
        if (!this._peak) return;
        var n = this._peak.firstChild;
        TK.set_text(n, O.format_peak(O.peak));
        if (O.peak > O.min && O.peak < O.max && O.show_peak) {
            this._peak.style.display = "block";
            var pos = 0;
            if (vert(O)) {
                pos = O.basis - this.val2px(this.snap(O.peak));
                pos = Math.min(O.basis, pos);
                this._peak.style.top = pos + "px";
            } else {
                pos = this.val2px(this.snap(O.peak));
                pos = Math.min(O.basis, pos)
                this._peak.style.left = pos + "px";
            }
        } else {
            this._peak.style.display = "none";
        }
        /**
         * Is fired when the peak was drawn.
         * 
         * @event TK.LevelMeter#drawpeak
         */
        this.fire_event("drawpeak");
    },
});
})(this, this.TK);
