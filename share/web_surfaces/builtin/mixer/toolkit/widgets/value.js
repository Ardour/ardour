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
 * @event TK.Value#useraction
 * 
 * @param {string} name - The name of the option which was changed due to the users action
 * @param {mixed} value - The new value of the option
 */
 
"use strict";
(function(w, TK){
function value_clicked(e) {
    var O = this.options;
    if (O.set === false) return;
    if (this.__editing) return false;
    TK.add_class(this.element, "toolkit-active");
    this._input.setAttribute("value", O.value);
    this.__editing = true;
    this._input.focus();
    if (O.auto_select)
        this._input.setSelectionRange(0, this._input.value.length)
    /**
     * Is fired when the value was clicked.
     * 
     * @event TK.Value#valueclicked
     * 
     * @param {number} value - The value of the widget.
     */
    this.fire_event("valueclicked", O.value);
}
function value_typing(e) {
    var O = this.options;
    if (O.set === false) return;
    if (!this.__editing) return;
    switch (e.keyCode) {
        case 27:
            // ESC
            value_done.call(this);
            /**
             * Is fired when the ESC key was pressed while editing the value.
             * 
             * @event TK.Value#valueescape
             * 
             * @param {string} value - The new value of the widget.
             */
            this.fire_event("valueescape", O.value);
            break;
        case 13:
            // ENTER
            this.userset("value", O.set ? O.set(this._input.value) : this._input.value);
            value_done.call(this);
            /**
             * Is fired after the value has been set and editing has ended.
             * 
             * @event TK.Value#valueset
             * 
             * @param {string} value - The new value of the widget.
             */
            this.fire_event("valueset", O.value);
            
            e.preventDefault();
            return false;
            break;
    }
    /**
     * Is fired when the user hits a key while editing the value.
     * 
     * @event TK.Value#valuetyping
     * 
     * @param {DOMEvent} event - The native DOM event.
     * @param {string} value - The new value of the widget.
     */
    this.fire_event("valuetyping", e, O.value);
}
function value_input() {
    var O = this.options;
    if (O.set === false) return;
    if (!this.__editing) return;
    if (O.editmode == "immediate")
        this.userset("value", O.set ? O.set(this._input.value) : this._input.value);
}
function value_done() {
    if (!this.__editing) return;
    this.__editing = false;
    TK.remove_class(this.element, "toolkit-active");
    this._input.blur();
    /**
     * Is fired when editing of the value ends.
     * 
     * @event TK.Value#valuedone
     * 
     * @param {string} value - The new value of the widget.
     */
    this.fire_event("valuedone", this.options.value);
    this.invalid.value = true;
    this.trigger_draw();
}

function submit_cb(e) {
    e.preventDefault();
    return false;
}
/**
 * TK.Value is a formatted text field displaying values and providing
 * an input field for editing the value.
 *
 * @class TK.Value
 * 
 * @extends TK.Widget
 *
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {Number} [options.value=0] - The value.
 * @property {Function} [options.format=TK.FORMAT("%.2f")] - A formatting
 *   function used to display the value.
 * @property {Integer} [options.size=5] - Size attribute of the INPUT element.
 * @property {Integer} [options.maxlength] - Maxlength attribute of the INPUT element.
 * @property {Function} [options.set=function (val) { return parseFloat(val || 0); }] -
 *   A function which is called to parse user input.
 * @property {boolean} [options.auto_select=false] - Select the entire text in the entry field if clicked (new in v1.3).
 * @property {boolean} [options.readonly=false] - Sets the readonly attribute (new in v1.3).
 * @property {string} [options.placeholder=""] - Sets the placeholder attribute (new in v1.3).
 * @property {string} [options.type="text"] - Sets the type attribute. Type can be either `text` or `password` (new in v1.3).
 * @property {string} [options.editmode="onenter"] - Sets the event to trigger the userset event. Can be one out of `onenter` or `immediate`.
 * 
 */
TK.Value = TK.class({
    _class: "Value",
    Extends: TK.Widget,
    _options: Object.assign(Object.create(TK.Widget.prototype._options), {
        value: "number|string",
        format: "function",
        size: "number",
        maxlength: "int",
        set: "object|function|boolean",
        auto_select: "boolean",
        readonly: "boolean",
        placeholder: "string",
        type: "string",
        editmode: "string",
    }),
    options: {
        value: 0,
        format: TK.FORMAT("%.2f"),
        size: 5,
        container: false,
        // set a callback function if value is editable or
        // false to disable editing. A function has to return
        // the value treated by the parent widget.
        set: function (val) { return parseFloat(val || 0); },
        auto_select: false,
        readonly: false,
        placeholder: "",
        type: "text",
        editmode: "onenter",
    },
    static_events: {
        submit: submit_cb,
        click: value_clicked,
    },
    initialize: function (options) {
        var E;
        TK.Widget.prototype.initialize.call(this, options);
        /**
         * @member {HTMLDivElement} TK.Value#element - The main DIV container.
         *   Has class <code>toolkit-value</code>.
         */
        if (!(E = this.element)) this.element = E = TK.element("div");
        TK.add_class(E, "toolkit-value");
        
        this.widgetize(E, true, true, true);
        
        /**
         * @member {HTMLInputElement} TK.Value#_input - The text input.
         *   Has class <code>toolkit-input</code>.
         */
        this._input  = TK.element("input", "toolkit-input");
        this._input.type = "text";
        E.appendChild(this._input);

        this._value_typing = value_typing.bind(this);
        this._value_done = value_done.bind(this);
        this._value_input = value_input.bind(this);
                
        this._input.addEventListener("keyup", this._value_typing);
        this._input.addEventListener("input", this._value_input);
        this._input.addEventListener("blur", this._value_done);
        this.__editing = false;
    },
    
    redraw: function () {
        var I = this.invalid;
        var O = this.options;
        var E = this._input;

        TK.Widget.prototype.redraw.call(this);

        if (I.size) {
            I.size = 0;
            E.setAttribute("size", O.size);
        }

        if (I.maxlength) {
            I.maxlength = 0;
            if (O.maxlength) E.setAttribute("maxlength", O.maxlength);
            else E.removeAttribute("maxlength");
        }
        
        if (I.placeholder) {
            I.placeholder = 0;
            if (O.placeholder) E.setAttribute("placeholder", O.placeholder);
            else E.removeAttribute("placeholder");
        }

        if ((I.value || I.format) && !this.__editing) {
            I.format = I.value = false;
            E.value = O.format(O.value);
        }
        
        if (I.readonly) {
            I.readonly = 0;
            if (O.readonly)
                E.setAttribute("readonly", "readonly");
            else
                E.removeAttribute("readonly");
        }
        
        if (I.type) {
            I.type = 0;
            E.setAttribute("type", O.type);
        }
    },
    
    destroy: function () {
        this._input.removeEventListener("keyup", this._value_typing);
        this._input.removeEventListener("blur", this._value_done);
        this._input.removeEventListener("input", this._value_input);
        this._input.remove();
        TK.Widget.prototype.destroy.call(this);
    },
});
})(this, this.TK);
