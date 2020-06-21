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
 
/**
 * The <code>useraction</code> event is emitted when a widget gets modified by user interaction.
 * The event is emitted for the option <code>value</code>.
 *
 * @event TK.Knob#useraction
 * 
 * @param {string} name - The name of the option which was changed due to the users action
 * @param {mixed} value - The new value of the option
 */
     
"use strict";
(function(w, TK){
var format_viewbox = TK.FORMAT("0 0 %d %d");
function dblclick() {
    this.userset("value", this.options.reset);
    /**
     * Is fired when the knob receives a double click in order to reset to initial value.
     * 
     * @event TK.Knob#doubleclick
     * 
     * @param {number} value - The value of the widget.
     */
    this.fire_event("doubleclick", this.options.value);
}
function module_range() {
    return this.parent.circular;
}
/**
 * TK.Knob is a {@link TK.Circular} inside of an SVG which can be
 * modified both by dragging and scrolling utilizing {@link TK.DragValue}
 * and {@link TK.ScrollValue}.
 * It inherits all options of {@link TK.Circular} and {@link TK.DragValue}.
 * The options listed below consist of options from the contained widgets,
 * only showing the default values.
 *
 * @class TK.Knob
 * 
 * @extends TK.Widget
 *
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {Object} [options.hand={width: 1, length: 12, margin: 24}]
 * @property {Number} [options.margin=13]
 * @property {Number} [options.thickness=6]
 * @property {Number} [options.step=1] 
 * @property {Number} [options.shift_up=4]
 * @property {Number} [options.shift_down=0.25]
 * @property {Object} [options.dot={length: 6, margin: 13, width: 2}]
 * @property {Object} [options.marker={thickness: 6, margin: 13}]
 * @property {Object} [options.label={margin: 10, align: "outer", format: function(val){return val;}}]
 * @property {Number} [options.basis=300] - Distance to drag between <code>min</code> and <code>max</code>.

 */
TK.Knob = TK.class({
    _class: "Knob",
    Extends: TK.Widget,
    _options: Object.assign(Object.create(TK.Widget.prototype._options), TK.Circular.prototype._options,
                            TK.DragValue.prototype._options, {
        size: "number",
        reset: "number",
    }),
    options: Object.assign({}, TK.Circular.prototype.options, {
        size: 100,
        hand: {width: 1, length: 12, margin: 24},
        margin: 13,
        thickness: 6,
        step: 1,
        shift_up: 4,
        shift_down: 0.25,
        dot: {length: 6, margin: 13, width: 2},
        marker: {thickness: 6, margin: 13},
        label: {margin: 12, align: "outer", format: function(val){return val;}},
        direction: "polar",
        rotation:       45,
        blind_angle:    20,
        basis: 300,
    }),
    static_events: {
        dblclick: dblclick,
    },
    initialize: function (options) {
        TK.Widget.prototype.initialize.call(this, options);
        options = this.options;
        var E, S;
        /**
         * @member {HTMLDivElement} TK.Knob#element - The main DIV container.
         *   Has class <code>toolkit-knob</code>.
         */
        if (!(E = this.element)) this.element = E = TK.element("div")
        TK.add_class(E, "toolkit-knob");

        /**
         * @member {SVGImage} TK.Knob#svg - The main SVG image.
         */
        this.svg = S = TK.make_svg("svg");
        
        var co = TK.object_and(options, TK.Circular.prototype._options);
        co = TK.object_sub(co, TK.Widget.prototype._options);
        co.container = S;

        /**
         * @member {TK.Circular} TK.Knob#circular - The {@link TK.Circular} module.
         */
        this.circular = new TK.Circular(co);

        this.widgetize(E, true, true, true);
        
        /**
         * @member {TK.DragValue} TK.Knob#drag - Instance of {@link TK.DragValue} used for the
         *   interaction.
         */
        this.drag = new TK.DragValue(this, {
            node:    S,
            range:   module_range,
            direction: options.direction,
            rotation: options.rotation,
            blind_angle: options.blind_angle,
            limit: true,
        });
        /**
         * @member {TK.ScrollValue} TK.Knob#scroll - Instance of {@link TK.ScrollValue} used for the
         *   interaction.
         */
        this.scroll = new TK.ScrollValue(this, {
            node:    S,
            range:   module_range,
            limit: true,
        });

        E.appendChild(S);
        this.set("base", options.base);
        if (options.reset === void(0))
            options.reset = options.value;
        this.add_child(this.circular);
    },

    get_range: function() {
        return this.circular;
    },
    
    destroy: function () {
        this.drag.destroy();
        this.scroll.destroy();
        this.circular.destroy();
        TK.Widget.prototype.destroy.call(this);
    },

    redraw: function() {
        var I = this.invalid;
        var O = this.options;

        if (I.size) {
            I.size = false;
            this.svg.setAttribute("viewBox", format_viewbox(O.size, O.size));
        }

        TK.Widget.prototype.redraw.call(this);
    },
    /**
     * This is an alias for {@link TK.Circular#add_label} of the internal
     * circular instance.
     *
     * @method TK.Knob#add_label
     */
    add_label: function(x) {
        return this.circular.add_label(x);
    },

    /**
     * This is an alias for {@link TK.Circular#remove_label} of the internal
     * circular instance.
     *
     * @method TK.Knob#remove_label
     */
    remove_label: function(x) {
        this.circular.remove_label(x);
    },

    set: function(key, value) {
        if (key === "base") {
            if (value === false) value = this.options.min;
        }
        // TK.Circular does the snapping
        if (!TK.Widget.prototype._options[key]) {
            if (TK.Circular.prototype._options[key])
                value = this.circular.set(key, value);
            if (TK.DragValue.prototype._options[key])
                this.drag.set(key, value);
        }
        return TK.Widget.prototype.set.call(this, key, value);
    },
});
})(this, this.TK);
