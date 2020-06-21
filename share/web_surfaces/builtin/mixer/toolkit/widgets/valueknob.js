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
 * @event TK.ValueKnob#useraction
 * 
 * @param {string} name - The name of the option which was changed due to the users action
 * @param {mixed} value - The new value of the option
 */
 
"use strict";
(function(w, TK){
function value_clicked() {
    var self = this.parent;
    var knob = self.knob;
    knob.scroll.set("active", false);
    knob.drag.set("active", false);
    /**
     * Is fired when the user starts editing the value manually.
     * 
     * @event TK.ValueButton#valueedit
     * 
     * @param {number} value - The value of the widget.
     */
    self.fire_event("valueedit", this.options.value);
}
function value_done() {
    var self = this.parent;
    var knob = self.knob;
    knob.scroll.set("active", true);
    knob.drag.set("active", true);
    /**
     * Is fired when the user finished editing the value manually.
     * 
     * @event TK.ValueButton#valueset
     * 
     * @param {number} value - The value of the widget.
     */
    self.fire_event("valueset", this.options.value);
}
TK.ValueKnob = TK.class({
    /**
     * This widget combines a {@link TK.Knob}, a {@link TK.Label}  and a {@link TK.Value} whose
     * value is synchronized. It inherits all options from {@link TK.Knob} and {@link TK.Value}.
     *
     * @class TK.ValueKnob
     * 
     * @extends TK.Widget
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {String} [options.title=false] - Title of the knob. Set to `false` to hide the element from the DOM.
     * @property {Function} [options.value_format=TK.FORMAT("%.2f")] - Callback to format the value.
     * @property {Number} [options.value_set=function (val) { return parseFloat(val || 0); }] - A function which is called to parse user input in the {@link TK.Value}
     * @property {Number} [options.value_size=5] - Amount of digits for the value input.
     * @property {Number} [options.show_value=true] - Set to `false` to hide the {@link TK.Value}.
     * @property {Number} [options.show_knob=true] - Set to `false` to hide the {@link TK.Knob}.
     */
    _class: "ValueKnob",
    Extends: TK.Widget,
    _options: Object.create(TK.Widget.prototype._options),
    options: { },
    initialize: function (options) {
        TK.Widget.prototype.initialize.call(this, options);
        var E;
        /**
         * @member {HTMLDivElement} TK.ValueKnob#element - The main DIV container.
         *   Has class <code>toolkit-valueknob</code>.
         */
        if (!(E = this.element)) this.element = E = TK.element("div");
        TK.add_class(E, "toolkit-valueknob");

        this.widgetize(E, true, true, true);
    },
    get_range: function() {
        return this.knob.get_range();
    },
    set: function (key, value) {
        /* this gets triggered twice, but we need it in order to make the snapping work */
        if (key === "value" && this.knob)
            value = this.knob.set("value", value);

        return TK.Widget.prototype.set.call(this, key, value);
    },
});
/**
 * @member {TK.Label} TK.ValueKnob#label - The {@link TK.Label} widget.
 */
TK.ChildWidget(TK.ValueKnob, "label", {
    create: TK.Label,
    option: "title",
    toggle_class: true,
    map_options: {
        title: "label",
    },
});
/**
 * @member {TK.Knob} TK.ValueKnob#knob - The {@link TK.Knob} widget.
 */
TK.ChildWidget(TK.ValueKnob, "knob", {
    create: TK.Knob,
    show: true,
    inherit_options: true,
    toggle_class: true,
    userset_delegate: true,
});
/**
 * @member {TK.Value} TK.ValueKnob#value - The {@link TK.Value} widget.
 */
TK.ChildWidget(TK.ValueKnob, "value", {
    create: TK.Value,
    show: true,
    inherit_options: true,
    map_options: {
        value_format: "format",
        value_set: "set",
        value_size: "size",
    },
    static_events: {
        valueclicked: value_clicked,
        valuedone: value_done,
    },
    toggle_class: true,
    userset_delegate: true,
});
})(this, this.TK);
