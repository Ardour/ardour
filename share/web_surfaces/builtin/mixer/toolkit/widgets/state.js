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
/**
 * The TK.State widget is a multi-functional adaption of a traditional LED. It
 * is able to show different colors as well as on/off states. The
 * "brightness" can be set seamlessly. Classes can be used to display
 * different styles. TK.State extends {@link TK.Widget}.
 *
 * The LED effect is implemented as a DIV element, which is overlayed by
 * a DIV element with class <code>toolkit-mask</code>. `options.state`
 * changes the opacity of the mask element.
 *
 * @class TK.State
 * 
 * @extends TK.Widget
 *
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {Number} [options.state=0] - The state. To toggle between `on|off` set to `1|0`.
 *   Set to fractions of `1` to change "brightness", e.g. `0.5`. Values > 0 trigger setting
 *   the class `toolkit-state-on`, while a state of `0` results in class `toolkit-state-off`.
 * @property {String|Boolean} [options.color=false] - A CSS color string for the state LED or
 *   `false` to set the background via external CSS.
 */
TK.State = TK.class({
    _class: "State",
    Extends: TK.Widget,
    _options: Object.assign(Object.create(TK.Widget.prototype._options), {
        state: "number|boolean",
        color: "string|boolean",
    }),
    options: {
        state:           0,     // the initial state (0 ... 1)
        color:           false, // the base color
    },
    initialize: function (options) {
        TK.Widget.prototype.initialize.call(this, options);

        var E;
        /**
         * @member {HTMLDivElement} TK.State#element - The main DIV container.
         *   Has class <code>toolkit-state</code>.
         */
        if (!(E = this.element)) this.element = E = toolkit.element("div");
        TK.add_class(E, "toolkit-state");
        this.widgetize(E, true, true, true);
        
        /**
         * @member {HTMLDivElement} TK.State#_mask - A DIV for masking the background.
         *   Has class <code>toolkit-mask</code>.
         */
        this._mask   = TK.element("div","toolkit-mask");

        E.appendChild(this._mask);
    },
    destroy: function () {
        this._mask.remove();
        TK.Widget.prototype.destroy.call(this);
    },

    redraw: function() {
        TK.Widget.prototype.redraw.call(this);
        var I = this.invalid;
        var O = this.options;

        if (I.color) {
            I.color = false;
            if (O.color)
                this.element.style["background-color"] = O.color;
            else
                this.element.style["background-color"] = void 0;
        }

        if (I.state) {
            I.state = false;
            var v = +O.state;
            if (!(v >= 0)) v = 0;
            if (!(v <= 1)) v = 1;

            if (!O.state) {
                this.remove_class("toolkit-state-on");
                this.add_class("toolkit-state-off");
            } else {
                this.remove_class("toolkit-state-off");
                this.add_class("toolkit-state-on");
            }
            this._mask.style["opacity"] = "" + (1 - v);
        }
    }
});
})(this, this.TK);
