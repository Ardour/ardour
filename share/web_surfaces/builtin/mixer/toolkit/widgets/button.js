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

TK.Button = TK.class({
    /**
     * TK.Button is a simple, clickable widget containing an
     * {@link TK.Icon} and a {@link TK.Label} to trigger functions.
     * Button serves as base for other widgets, too, e.g.
     * {@link TK.Toggle}, {@link TK.ConfirmButton} and {@link TK.Select}.
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {String|Boolean} [options.label=false] - Text for the
     *   button label. Set to <code>false</code> to remove the label
     *   from DOM.
     * @property {String|Boolean} [options.icon=false] - URL to an image
     *   file or an icon class (see styles/fonts/Toolkit.html). If set
     *   to <code>false</code>, the icon is removed from DOM.
     * @property {Boolean} [options.state=false] - State of the button,
     *   reflected as class <code>toolkit-active</code>.
     * @property {String} [options.layout="horizontal"] - Define the
     *   arrangement of label and icon. <code>vertical</code> means icon
     *   above the label, <code>horizontal</code> places the icon left
     *   to the label.
     * 
     * @extends TK.Widget
     * 
     * @class TK.Button
     */
    _class: "Button",
    Extends: TK.Widget,
    _options: Object.assign(Object.create(TK.Widget.prototype._options), {
        label: "string|boolean",
        icon: "string|boolean",
        state: "boolean",
        layout: "string",
    }),
    options: {
        label:            false,
        icon:            false,
        state:            false,
        layout:           "horizontal"
    },
    initialize: function (options) {
        var E;
        TK.Widget.prototype.initialize.call(this, options);
        /**
         * @member {HTMLDivElement} TK.Button#element - The main DIV element.
         *   Has class <code>toolkit-button</code>.
         */
        if (!(E = this.element)) this.element = E = TK.element("div");
        TK.add_class(E, "toolkit-button");
        this.widgetize(E, true, true, true);
    },
    destroy: function () {
        TK.Widget.prototype.destroy.call(this);
    },

    redraw: function() {
        TK.Widget.prototype.redraw.call(this);
        var I = this.invalid;
        var O = this.options;
        var E = this.element;
        
        if (I.layout) {
            I.layout = false;
            TK.toggle_class(E, "toolkit-vertical", O.layout === "vertical");
            TK.toggle_class(E, "toolkit-horizontal", O.layout !== "vertical");
        }

        if (I.state) {
            I.state = false;
            TK.toggle_class(E, "toolkit-active", O.state);
        }
    },
});
/**
 * @member {TK.Icon} TK.Button#icon - The {@link TK.Icon} widget.
 */
TK.ChildWidget(TK.Button, "icon", {
    create: TK.Icon,
    option: "icon",
    inherit_options: true,
    toggle_class: true,
});
/**
 * @member {TK.Label} TK.Button#label - The {@link TK.Label} of the button.
 */
TK.ChildWidget(TK.Button, "label", {
    create: TK.Label,
    option: "label",
    inherit_options: true,
    toggle_class: true,
});
})(this, this.TK);
