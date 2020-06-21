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
function toggle(e) {
    var self = this.parent;
    e.preventDefault();
    e.stopPropagation();
    return collapse.call(self, !self.options.expanded);
}
function collapse(state) {
    this.userset("expanded", state);
    return false;
}
function visible_when_expanded(widget) {
    var v = widget.options._expanded;
    return v !== false;
}
function visible_when_collapsed(widget) {
    var v = widget.options._collapsed;
    return v === true;
}
function is_visible(widget) {
    var value = this.options.always_expanded || this.options.expanded;

    if (value) {
        return visible_when_expanded(widget);
    } else {
        return visible_when_collapsed(widget);
    }
}
function changed_expanded(value) {
    var group = this.options.group;
    var other_expander;
    var grp;

    if (group) {
        grp = expander_groups[group];
        if (value) {
            other_expander = grp.active;
            grp.active = this;
            if (other_expander && other_expander !== this)
                other_expander.set("expanded", false); 
        } else if (grp.active === this) {
            grp.active = false;
            if (grp.default) grp.default.set("expanded", true);
        }
    }
    update_visibility.call(this);
}
function add_to_group(group) {
    var grp;
    var O = this.options;
    if (!(grp = expander_groups[group]))
        expander_groups[group] = grp = { active: false, default: false };

    if (O.group_default) {
        grp.default = this;
        if (!grp.active) {
            this.set("expanded", true);
            return;
        }
    }

    if (O.expanded)
        changed_expanded.call(this, O.expanded);
}

function remove_from_group(group) {
    var grp = expander_groups[group];

    if (grp.default === this) grp.default = false;
    if (grp.active === this) {
        grp.active = false;
        if (grp.default) grp.default.set("expanded", true);
    }
}
function remove_group_default(group) {
    if (!group) return;
    var grp = expander_groups[group];
    grp.default = false;
}
function update_visibility() {
    var C = this.children;
    var value = this.options.always_expanded || this.options.expanded;

    if (C) {
        var test = value ? visible_when_expanded : visible_when_collapsed;
        for (var i = 0; i < C.length; i++) {
            if (test(C[i]))
                this.show_child(i);
            else
                this.hide_child(i);
        }
    }

    if (value) {
        this.fire_event("expand");
        /**
         * Is fired when the expander expands.
         * 
         * @event TK.Expander#expand
         */
    } else {
        /**
         * Is fired when the expander collapses.
         * 
         * @event TK.Expander#collapse
         */
        this.fire_event("collapse");
    }
}
var expander_groups = { };
w.eg = expander_groups;
TK.Expander = TK.class({
    /**
     * TK.Expander is a container which can be toggled between two different states,
     * expanded and collapsed. It can be used to implement overlay popups, but it is
     * not limited to that application.
     * In expanded mode the container has the class <code>toolkit-expanded</code>.
     * Child widgets are shown or hidden depending on the state of the two pseudo
     * options <code>_expanded</code> and <code>_collapsed</code>. If a child widget
     * of the expander has <code>_expanded</code> set to true it will be shown in
     * expanded state. If a child widget has <code>_collapsed</code> set to false, it
     * will be shown in collapsed state. This feature can be used to make interfaces
     * more reactive.
     * 
     * @class TK.Expander
     * 
     * @extends TK.Container
     *
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Boolean} [options.expanded=false] - The state of the widget.
     * @property {Boolean} [options.always_expanded=false] - This essentially overwrites
     *   the <code>expanded</code> option. This can be used to switch this widget to be
     *   always expanded, e.g. when the screen size is big enough.
     * @property {String} [options.group=""] - If set, this expander is grouped together with
     *   all other expanders of the same group name. At most one expander of the same group
     *   can be open at one time.
     * @property {Boolean} [options.group_default=false] - If set, this expander is expanded
     *   if all other group members are collapsed.
     * @property {String} [options.icon=""] - Icon of the {@link TK.Button} which toggles expanded state.
     * @property {String} [options.label=""] - Label of the {@link TK.Button} which toggles expanded state.
     * @property {Boolean} [options.show_button=true] - Set to `false` to hide the {@link TK.Button} toggling expanded state.
     */
    _class: "Expander",
    _options: Object.assign(Object.create(TK.Container.prototype._options), {
        expanded: "boolean",
        always_expanded: "boolean",
        group: "string",
        group_default: "boolean",
        label: "string",
        icon: "string",
    }),
    options: {
        expanded: false,
        always_expanded: false,
        group_default: false,
        label: "",
        icon: "",
    },
    static_events: {
        set_expanded: changed_expanded,
        set_always_expanded: update_visibility,
        set_group: function(value) {
            if (value) add_to_group.call(this, value);
        }
    },
    Extends: TK.Container,
    /**
     * Toggles the collapsed state of the widget.
     * 
     * @method TK.Expander#toggle
     */
    toggle: function() {
        toggle.call(this);
    },
    redraw: function() {
        var I = this.invalid;
        var O = this.options;

        TK.Container.prototype.redraw.call(this);

        if (I.always_expanded) {
            this[O.always_expanded ? "add_class" : "remove_class"]("toolkit-always-expanded");
        }

        if (I.expanded || I.always_expanded) {
            I.always_expanded = I.expanded = false; 
            var v = O.always_expanded || O.expanded;
            this[v ? "add_class" : "remove_class"]("toolkit-expanded");
            this.trigger_resize();
        }
    },
    initialize: function (options) {
        TK.Container.prototype.initialize.call(this, options);
        /**
         * @member {HTMLDivElement} TK.Expander#element - The main DIV container.
         *   Has class <code>toolkit-expander</code>.
         */
        TK.add_class(this.element, "toolkit-expander");

        this._update_visibility = update_visibility.bind(this);

        if (this.options.group) add_to_group.call(this, this.options.group);

        this.set("expanded", this.options.expanded);
        this.set("always_expanded", this.options.always_expanded);
        
    },
    add_child: function(child) {
        TK.Container.prototype.add_child.call(this, child);
        if (!is_visible.call(this, child)) this.hide_child(child);
        child.add_event("set__expanded", this._update_visibility);
        child.add_event("set__collapsed", this._update_visibility);
    },
    remove_child: function(child) {
        TK.Container.prototype.remove_child.call(this, child);
        child.remove_event("set__expanded", this._update_visibility);
        child.remove_event("set__collapsed", this._update_visibility);
    },
    set: function(key, value) {
        var group;
        if (key === "group") {
            group = this.options.group;
            // this is reached from init, where this element was never added
            // to the group.
            if (group && value !== group) remove_from_group.call(this, group);
        } else if (key === "group_default") {
            if (!value && this.options.group_default)
                remove_group_default.call(this, this.options.group);
        }
        return TK.Container.prototype.set.call(this, key, value);
    },
});
/**
 * @member {TK.Button} TK.Expander#button - The button for toggling the state of the expander.
 */
TK.ChildWidget(TK.Expander, "button", {
    create: TK.Button,
    show: true,
    map_options: {
        label: "label",
        icon: "icon",
    },
    default_options: {
        _expanded: true,
        _collapsed: true,
        class: "toolkit-toggle-expand"
    },
    static_events: {
        click: toggle,
    },
});
})(this, this.TK);
