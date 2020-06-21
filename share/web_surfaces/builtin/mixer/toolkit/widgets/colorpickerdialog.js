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
(function (w, TK) {

function cancel () {
    var self = this.parent;
    self.fire_event.call(self, "cancel");
    self.close();
}

function apply (color) {
    var self = this.parent;
    self.fire_event.call(self, "apply", color);
    self.close();
}

/**
 * A {@link TK.Dialog} window containing a {@link TK.ColorPicker}. It can be opened
 * programatically and closes automatically on the appropriate user
 * interactions like hitting ESC or clicking `apply`. TK.ColorPickerDialog
 * inherits all options of TK.ColorPicker.
 * 
 * @class TK.ColorPickerDialog
 * 
 * @extends TK.Dialog
 * 
 * @param {Object} [options={ }] options - An object containing initial options.
 * 
 */
TK.ColorPickerDialog = TK.class({
    
    _class: "ColorPickerDialog",
    Extends: TK.Dialog,
    
    initialize: function (options) {
        TK.Dialog.prototype.initialize.call(this, options);
        /** @member {HTMLDivElement} TK.ColorPickerDialog#element - The main DIV container.
         * Has class <code>toolkit-color-picker-dialog</code>.
         */
        TK.add_class(this.element, "toolkit-color-picker-dialog");
    },
});
    
/**
 * @member {TK.ColorPicker} TK.ColorPickerDialog#colorpicker - The {@ink TK.ColorPicker} widget.
 */
TK.ChildWidget(TK.ColorPickerDialog, "colorpicker", {
    create: TK.ColorPicker,
    show: true,
    inherit_options: true,
    userset_delegate: true,
    static_events: {
        cancel: cancel,
        apply: apply,
    },
});
    
})(this, this.TK);
