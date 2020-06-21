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
 * Adds the class "toolkit-warn" on <code>this.element</code> for a certain
 * period of time. It is used e.g. in {@link TK.ResponseHandle} or {@link TK.Knob} when the value
 * exceeds the range.
 *
 * @mixin TK.Warning
 */
TK.Warning = TK.class({
    _class: "Warning",
    /** 
     * Adds the class <code>toolkit-warn</code> to the given element and
     * sets a timeout after which the class is removed again. If there
     * already is a timeout waiting it gets updated.
     *
     * @method TK.Warning#warning
     * 
     * @emits TK.Warning#warning
     * 
     * @param {HTMLElement|SVGElement} element - The DOM node the class should be added to.
     * @param {Number} [timeout=250] - The timeout in ms until the class should be removed again.
     */
    warning: function (element, timeout) {
        if (!timeout) timeout = 250;
        if (this.__wto) window.clearTimeout(this.__wto);
        this.__wto = null;
        TK.add_class(element, "toolkit-warn");
        this.__wto = window.setTimeout(function () {
            TK.remove_class(element, "toolkit-warn");
        }.bind(this), timeout);
        /**
         * Gets fired when {@link TK.Warning#warning} was called.
         * 
         * @event TK.Warning#warning 
         */
        this.fire_event("warning");
    }
});
})(this, this.TK);
