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
 * Frame is a {@link TK.Container} with a {@link TK.Label} on top.
 * 
 * @extends TK.Container
 * 
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {String|Boolean} [options.label=false] - The label of the frame. Set to `false` to remove it from the DOM.
 * 
 * @class TK.Frame
 */
TK.Frame = TK.class({
    Extends: TK.Container,
    _class: "Frame",
    _options: Object.create(TK.Container.prototype._options),
    options: {
        label: false,
    },
    initialize: function (options) {
        TK.Container.prototype.initialize.call(this, options);
        /**
         * @member {HTMLDivElement} TK.Frame#element - The main DIV container.
         *   Has class <code>toolkit-frame</code>.
         */
        TK.add_class(this.element, "toolkit-frame");
    },
});
/**
 * @member {TK.Label} TK.Frame#label - The {@link TK.Label} of the frame.
 */
TK.ChildWidget(TK.Frame, "label", {
    create: TK.Label,
    option: "label",
    inherit_options: true,
    default_options: {
        class: "toolkit-frame-label"
    },
});
})(this, this.TK);
