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
 * @event TK.Slider#useraction
 * 
 * @param {string} name - The name of the option which was changed due to the users action
 * @param {mixed} value - The new value of the option
 */
     
"use strict";
(function(w, TK){
function dblclick() {
    this.userset("value", this.options.reset);
    /**
     * Is fired when the slider receives a double click in order to reset to initial value.
     * 
     * @event TK.Slider#doubleclick
     * 
     * @param {number} value - The value of the widget.
     */
    this.fire_event("doubleclick", this.options.value);
}

function set_background(horiz, vert, size) {
    var E = this.element;
    E.style["background-position"] = "-"+horiz+"px -"+vert+"px";
    
    E.style["-webkit-background-size"] = size;
    E.style["-moz-background-size"] = size;
    E.style["-ms-background-size"] = size;
    E.style["-o-background-size"] = size;
    E.style["background-size"] = size;
}
/**
 * TK.Slider is a {@link TK.Widget} moving its background image
 * according to its value. It can be used to show strips of
 * e.g. 3D-rendered faders or knobs. It's important to set the
 * width and height of the widget in CSS according to the frames in
 * the background file. If alignment is `horizontal` the background image
 * is as height as the widget, the width keeps the ratio intact. Overall
 * width of the image should be frames * width. If alignment is `vertical`
 * the background image is as wide as the widget and the height of the
 * image keeps the ratio intact. The height should be height of widget
 * times the amount of frames.
 * TK.Slider uses {@link TK.DragValue} and {@link TK.ScrollValue}
 * for setting its value.
 * It inherits all options of {@link TK.DragValue}, {@link TK.Ranged} and {@link TK.Warning}.
 *
 * @class TK.Slider
 * 
 * @extends TK.Widget
 *
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {Number} [options.value=0] - The current value.
 * @property {Integer} [options.frames=1] - The amount of frames contained
 *     in the background image.
 * @property {String} [options.alignment="horizontal"] - The direction
 *     of the frames in the image, next to (`horizontal`) or among each other (`vertical`).
 * @property {String|Booelan} [options.image=false] - The image containing all frames for the slider.
 *     Set to `false` to set the background image via external CSS.
 * 
 */
TK.Slider = TK.class({
    _class: "Slider",
    Extends: TK.Widget,
    Implements: [TK.Ranged, TK.Warning],
    _options: Object.assign(Object.create(TK.Widget.prototype._options),
                            TK.Ranged.prototype._options,
                            TK.DragValue.prototype._options, {
        value: "number",
        frames: "int",
        alignment: "string",
        image: "string|boolean",
        _width: "number",
        _height: "number",
        
    }),
    options: {
        value: 0,
        frames: 1,
        alignment: "horizontal",
        image: false,
        
        direction: "polar",
        rotation:       45,
        blind_angle:    20,
        basis: 300,
    },
    static_events: {
        dblclick: dblclick,
    },
    initialize: function (options) {
        TK.Widget.prototype.initialize.call(this, options);
        var E;
        /**
         * @member {HTMLDivElement} TK.Slider#element - The main DIV container.
         *   Has class <code>toolkit-slider</code>.
         */
        if (!(E = this.element)) this.element = E = TK.element("div");
        TK.add_class(E, "toolkit-slider");
        this.widgetize(E, true, true, true);
        /**
         * @member {TK.DragValue} TK.Knob#drag - Instance of {@link TK.DragValue} used for
         *   interaction.
         */
        this.drag = new TK.DragValue(this, {
            node:    E,
            classes: E,
            direction: this.options.direction,
            rotation: this.options.rotation,
            blind_angle: this.options.blind_angle,
        });
        /**
         * @member {TK.ScrollValue} TK.Knob#scroll - Instance of {@link TK.ScrollValue} used for
         *   interaction.
         */
        this.scroll = new TK.ScrollValue(this, {
            node:    E,
            classes: E,
        });

        if (options.reset === void(0))
            options.reset = options.value;
    },

    destroy: function () {
        this.drag.destroy();
        this.scroll.destroy();
        TK.Widget.prototype.destroy.call(this);
    },

    redraw: function() {
        var I = this.invalid;
        var O = this.options;
        var E = this.element;
        
        if (I.image) {
            I.image = false;
            if (O.image)
                this.element.style["background-image"] = "url('" + O.image + "')";
            else
                this.element.style["background-image"] = void 0;
            I.value = true;
        }
        
        if (I.value || I.alignment || O.frames) {
            I.value = false;
            I.alignment = false;
            I.frames = false;
            var coef = this.val2coef(O.value);
            var frame = Math.round(Math.max(0, O.frames - 1) * coef);
            switch (O.alignment) {
                default:
                    TK.warn("Unknown alignment, only 'vertical' and 'horizontal' are allowed");
                    break;
                case "vertical":
                    set_background.call(this, 0, frame * O._width, "100% auto");
                    break;
                case "horizontal":
                    set_background.call(this, frame * O._height, 0, "auto 100%");
                    break;
            }
        }
        
        TK.Widget.prototype.redraw.call(this);
    },
    
    resize: function () {
        this.set("_width", TK.outer_width(this.element));
        this.set("_height", TK.outer_height(this.element));
    },

    set: function(key, value) {
        switch (key) {
            case "value":
                if (value > this.options.max || value < this.options.min)
                    this.warning(this.element);
                value = this.snap(value);
                break;
        }
        if (TK.DragValue.prototype._options[key])
            this.drag.set(key, value);
        return TK.Widget.prototype.set.call(this, key, value);
    },
});
})(this, this.TK);
