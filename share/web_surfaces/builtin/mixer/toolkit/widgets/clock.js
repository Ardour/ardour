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
var format_viewbox = TK.FORMAT("0 0 %d %d");
function draw_time(force) {
    var tmp, drawn;
    var O = this.options;
    var t = O.time;

    if ((tmp = t.getSeconds()) !== this.__sec || force) {
        this.circulars.seconds.set("value", tmp);
        this.__sec = tmp;
    }
    if ((tmp = t.getMinutes()) !== this.__min || force) {
        this.circulars.minutes.set("value", tmp);
        this.__min = tmp;
    }
    if ((tmp = t.getHours() % 12) !== this.__hour || force) {
        this.circulars.hours.set("value", tmp);
        this.__hour = tmp;
    }
    
    var args = [t,
                t.getFullYear(),
                t.getMonth(),
                t.getDate(),
                t.getDay(),
                t.getHours(),
                t.getMinutes(),
                t.getSeconds(),
                t.getMilliseconds(),
                Math.round(t.getMilliseconds() / (1000 / O.fps)),
                O.months,
                O.days];
    if ((tmp = O.label.apply(this, args)) !== this.__label || force) {
        TK.set_text(this._label, tmp);
        this.__label = tmp;
        drawn = true;
    }
    if ((tmp = O.label_upper.apply(this, args)) !== this.__upper || force) {
        TK.set_text(this._label_upper, tmp);
        this.__upper = tmp;
        drawn = true;
    }
    if ((tmp = O.label_lower.apply(this, args)) !== this.__lower || force) {
        TK.set_text(this._label_lower, tmp);
        this.__lower = tmp;
        drawn = true;
    }
    
    if (drawn)
        /**
         * Is fired when the time was drawn.
         * 
         * @param {Date} time - The time which was drawn.
         * 
         * @event TK.Clock#timedrawn
         */
        this.fire_event("timedrawn", O.time);
}
function set_labels() {
    var O = this.options;
    var E = this._label;
    var s = O.label(new Date(2000, 8, 30, 24, 59, 59, 999), 2000, 8,
                                               30, 6, 24, 59, 59, 999, 999,
                                               O.months, O.days);
    TK.set_text(E, s);
    
    E.setAttribute("transform", "");

    /* FORCE_RELAYOUT */
    
    TK.S.add(function() {
        var bb = E.getBoundingClientRect();
        if (bb.width === 0) return; // we are hidden
        var mleft   = parseInt(TK.get_style(E, "margin-left")) || 0;
        var mright  = parseInt(TK.get_style(E, "margin-right")) || 0;
        var mtop    = parseInt(TK.get_style(E, "margin-top")) || 0;
        var mbottom = parseInt(TK.get_style(E, "margin-bottom")) || 0;
        var space   = O.size - mleft - mright - this._margin * 2;
        var scale   = space / bb.width;
        var pos     = O.size / 2;
        
        TK.S.add(function() {
            E.setAttribute("transform", "translate(" + pos + "," + pos + ") "
                + "scale(" + scale + ")");

            /* FORCE_RELAYOUT */
            
            TK.S.add(function() {
                bb = E.getBoundingClientRect();
                
                TK.S.add(function() {
                    this._label_upper.setAttribute("transform", "translate(" + pos + "," + (pos - bb.height / 2 - mtop) + ") "
                        + "scale(" + (scale * O.label_scale) + ")");
                    this._label_lower.setAttribute("transform", "translate(" + pos + "," + (pos + bb.height / 2 + mtop) + ") "
                        + "scale(" + (scale * O.label_scale) + ")");
                    draw_time.call(this, true);
                }.bind(this), 1);
            }.bind(this));
        }.bind(this), 1);
    }.bind(this));
}
function timeout() {
    if (this.__to)
        window.clearTimeout(this.__to);
    var O = this.options;
    if (!O) return;
    if (O.timeout) {
        var d = O.time;
        var ts = +Date.now();

        if (O.offset) {
            ts += (O.offset|0);
        }

        d.setTime(ts);
        this.set("time", d);
            
        var targ = (O.timeout|0);
        if (O.timeadd) {
            targ += (O.timeadd|0) - ((ts % 1000)|0)
        }
        this.__to = window.setTimeout(this.__timeout, targ);
    } else this.__to = false;
}
function onhide() {
    if (this.__to) {
        window.clearTimeout(this.__to);
        this.__to = false;
    }
}

TK.Clock = TK.class({
    /**
     * TK.Clock shows a customized clock with circulars displaying hours, minutes
     * and seconds. It additionally offers three freely formatable labels.
     *
     * @class TK.Clock
     * 
     * @extends TK.Widget
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Integer} [options.thickness=10] - Thickness of the rings in percent of the maximum dimension.
     * @property {Integer} [options.margin=0] - Margin between the {@link TK.Circular} in percent of the maximum dimension.
     * @property {Integer} [options.size=200] - Width and height of the widget.
     * @property {Boolean} [options.show_seconds=true] - Show seconds ring.
     * @property {Boolean} [options.show_minutes=true] - Show minutes ring.
     * @property {Boolean} [options.show_hours=true] - Show hours ring.
     * @property {Integer} [options.timeout=1000] - The timeout of the redraw trigger.
     * @property {Integer} [options.timeadd=10] - Set additional milliseconds to add to the timeout target system clock regulary.
     * @property {Integer} [options.offset=0] - If a timeout is set offset the system time in milliseconds.
     * @property {Integer} [options.fps=25] - Framerate for calculating SMTP frames
     * @property {Array<String>} [options.months=["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]] - Array containing all months names.
     * @property {Array<String>} [options.days=["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"]] - Array containing all days names.
     * @property {Function} [options.label=function (_date, year, month, date, day, hour, minute, second, millisecond, frame, months, days) { return ((hour < 10) ? ("0" + hour) : hour) + ":" + ((minute < 10) ? ("0" + minute) : minute) + ":" + ((second < 10) ? ("0" + second) : second);] - Callback to format the main label.
     * @property {Function} [options.label_upper=function (_date, year, month, date, day, hour, minute, second, millisecond, frame, months, days) { return days[day]; }] - Callback to format the upper label.
     * @property {Function} [options.label_lower=function (_date, year, month, date, day, hour, minute, second, millisecond, frame, months, days) { return ((date < 10) ? ("0" + date) : date) + ". " + months[month] + " " + year; }] - Callback to format the lower label.
     * @property {Number} [options.label_scale=0.33] - The scale of `label_upper` and `label_lower` compared to the main label.
     * @property {Number|String|Date} [options.time] - Set a specific time and date. To avoid auto-udates, set `timeout` to 0.
     *   For more information about the value, please refer to <a href="https://www.w3schools.com/jsref/jsref_obj_date.asp">W3Schools</a>.
     */
    _class: "Clock",
    Extends: TK.Widget,
    _options: Object.assign(Object.create(TK.Widget.prototype._options), {
        thickness:    "number",
        margin:       "number",
        size:         "number",
        show_seconds: "boolean",
        show_minutes: "boolean",
        show_hours:   "boolean",
        timeout:      "int",
        timeadd:      "int",
        offset:       "int",
        fps:          "number",
        months:       "array",
        days:         "array",
        label:        "function",
        label_upper:  "function",
        label_lower:  "function",
        label_scale:  "number",
        time: "object|string|number",
    }),
    options: {
        thickness:    10,         // thickness of the rings
        margin:       0,          // margin between the circulars
        size:         200,        // diameter of the whole clock
        show_seconds: true,       // show the seconds ring
        show_minutes: true,       // show the minutes ring
        show_hours:   true,       // show the hours ring
        timeout:      1000,          // set a timeout to update the clock with the
                                  // system clock regulary
        timeadd:      10,          // set additional milliseconds for the
                                  // timeout target
                                  // system clock regulary
        offset:       0,          // if a timeout is set offset the system time
                                  // in milliseconds
        fps:          25,         // framerate for calculatind SMTP frames
        months:       ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"],
        days:         ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"],
        label: function (_date, year, month, date, day, hour, minute, second, millisecond, frame, months, days) {
            return ((hour < 10) ? ("0" + hour) : hour) + ":" +
                   ((minute < 10) ? ("0" + minute) : minute) + ":" +
                   ((second < 10) ? ("0" + second) : second);
        },
        label_upper: function (_date, year, month, date, day, hour, minute, second, millisecond, frame, months, days) {
            return days[day];
        },
        label_lower: function (_date, year, month, date, day, hour, minute, second, millisecond, frame, months, days) {
            return ((date < 10) ? ("0" + date) : date) + ". " + months[month] + " " + year;
        },
        label_scale: 0.33           // the scale of the upper and lower labels
                                   // compared to the main label
    },
    static_events: {
        hide: onhide,
        show: timeout,
        timeout: timeout,
    },
    initialize: function (options) {
        var E, S;
        /**
         * @member {Object} TK.Clock#circulars - An object holding all three TK.Circular as members <code>seconds</code>, <code>minutes</code> and <code>hours</code>.
         */
        this.circulars = {};
        this._margin = -1;
        TK.Widget.prototype.initialize.call(this, options);
        this.set("time", this.options.time);
        
        /**
         * @member {HTMLDivElement} TK.Clock#element - The main DIV element. Has class <code>toolkit-clock</code> 
         */
        if (!(E = this.element)) this.element = E = TK.element("div");
        /**
         * @member {SVGImage} TK.Clock#svg - The main SVG image.
         */
        this.svg = S = TK.make_svg("svg");
        this.widgetize(E, true, true, true);
        TK.add_class(E, "toolkit-clock");
        
        /**
         * @member {SVGText} TK.Clock#_label - The center label showing the time. Has class<code>toolkit-label</code>
         */
        this._label       = TK.make_svg("text", {
            "class":       "toolkit-label",
            "text-anchor": "middle",
            "style":       "dominant-baseline: central;"
        });
        /**
         * @member {SVGText} TK.Clock#_label_upper - The upper label showing the day. Has class<code>toolkit-label-upper</code>
         */
        this._label_upper = TK.make_svg("text", {
            "class": "toolkit-label-upper",
            "text-anchor": "middle",
            "style":       "dominant-baseline: central;"
        });
        /** @member {SVGText} TK.Clock#_label_lower - The lower label showing the date. Has class<code>toolkit-label-lower</code>
         */
        this._label_lower = TK.make_svg("text", {
            "class": "toolkit-label-lower",
            "text-anchor": "middle",
            "style":       "dominant-baseline: central;"
        });
        S.appendChild(this._label);
        S.appendChild(this._label_upper);
        S.appendChild(this._label_lower);
        E.appendChild(S);

        var circ_options = {
            container: S,
            show_hand: false,
            start: 270,
            angle: 360,
            min: 0
        };
        
        /**
         * @member {Object} TK.Clock#circulars - An object containing the {@link TK.Circular}
         * widgets. Members are `seconds`, `minutes` and `hours`.
         */
        this.circulars.seconds = new TK.Circular(Object.assign({}, circ_options,
            {max: 60, "class": "toolkit-seconds"}));
        this.circulars.minutes = new TK.Circular(Object.assign({}, circ_options,
            {max: 60, "class": "toolkit-minutes"}));
        this.circulars.hours   = new TK.Circular(Object.assign({}, circ_options,
            {max: 12, "class": "toolkit-hours"}));

        this.add_child(this.circulars.seconds);
        this.add_child(this.circulars.minutes);
        this.add_child(this.circulars.hours);
        
        // start the clock
        this.__timeout = timeout.bind(this);
    },

    redraw: function () {
        var I = this.invalid, O = this.options;

        TK.Widget.prototype.redraw.call(this);

        if (I.size) {
            this.svg.setAttribute("viewBox", format_viewbox(O.size, O.size));
        }

        if (I.validate("show_hours", "show_minutes", "show_seconds", "thickness", "margin") || I.size) {
            var margin = 0;
            for (var i in this.circulars) {
                var circ = this.circulars[i];
                if (O["show_" + i]) {
                    circ.set("thickness", O.thickness);
                    circ.set("show_base", true);
                    circ.set("show_value", true);
                    circ.set("size", O.size);
                    circ.set("margin", margin);
                    margin += O.thickness;
                    margin += circ._get_stroke();
                    margin += O.margin;
                } else {
                    circ.set("show_base", false);
                    circ.set("show_value", false);
                }
            }
            if(this._margin < 0) {
                this._margin = margin;
            } else {
                this._margin = margin;
            }
            // force set_labels
            I.label = true;
        }

        if (I.validate("label", "months", "days", "size", "label_scale")) {
            set_labels.call(this);
        }

        if (I.validate("time", "label", "label_upper", "label_lower", "label_scale")) {
            draw_time.call(this, false);
        }
    },
    
    destroy: function () {
        this._label.remove();
        this._label_upper.remove();
        this._label_lower.remove();
        this.circulars.seconds.destroy();
        this.circulars.minutes.destroy();
        this.circulars.hours.destroy();
        if (this.__to)
            window.clearTimeout(this.__to);
        TK.Widget.prototype.destroy.call(this);
    },
    
    set: function (key, value) {
        switch (key) {
            case "time":
                if (Object.prototype.toString.call(value) === '[object Date]')
                    break;
                value = new Date(value);
                break;
        }
        return TK.Widget.prototype.set.call(this, key, value);
    },
});
})(this, this.TK);
