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

function remove (e, node) {
    this.fire_event("remove", node);
    if (!this.options.async)
        this.remove_node(node);
}

function colorize (e) {
    var that = this;
    var c = new TK.ColorPickerDialog({
        autoclose: true,
        hex: this.options.color,
        onapply: function (rgb, hsl, hex) { 
            if (!that.options.async)
                that.userset("color", hex);
            else
                that.fire_event("userset", "color", hex);
            
        },
        container: document.body,
    });
    c.open(e.pageX, e.pageY);
    c.show();
    this.colorpicker = c;
    w.c = c;
}

TK.Tag = TK.class({
    _class: "Tag",
    Extends: TK.Widget,
    Implements: TK.Colors,
    
    _options: Object.assign(Object.create(TK.Widget.prototype._options), {
        color: "string|null",
        tag: "string",
        async: "boolean",
        node_class: "constructor",
    }),
    options: {
        color: null,
        tag: "",
        async: false,
        node_class: TK.TagNode,
    },
    initialize: function (options) {
        TK.Widget.prototype.initialize.call(this, options);
        this.nodes = [];
    },
    destroy: function () {
        var l = this.nodes.length;
        for (var i = 0; i < l; i++)
            this.remove_node(this.nodes[i]);
        TK.Widget.prototype.destroy.call(this);
    },
    
    redraw: function () {
        var I = this.invalid;
        var O = this.options;
        if (I.color) {
            I.color = false;
            for (var i = 0; i < this.nodes.length; i++) {
                this.nodes[i].element.style.backgroundColor = O.color;
                if (O.color)
                    this.nodes[i].element.style.color = this.rgb2gray(this.hex2rgb(O.color)) > 0.5 ? "black" : "white";
                else
                    this.nodes[i].element.style.color = null;
            }
        }
        if (I.tag) {
            I.tag = false;
            for (var i = 0; i < this.nodes.length; i++)
                this.nodes[i].children[0].textContent = O.tag;
        }
        TK.Widget.prototype.redraw.call(this);
    },
    remove_node: function (node) {
        var O = this.options;
        for (var i = 0; i < this.nodes.length; i++) {
            if (this.nodes[i] == node) {
                node.set("container", false);
                this.fire_event("noderemoved", node);
                this.nodes.splice(i, 1);
                node.destroy();
                return true;
            }
        }
    },
    create_node: function (options) {
        var O = this.options;
        options = options || {};
        options.color = O.color;
        options.tag = O.tag;
        var node = new O.node_class(options, this);
        node.add_event("colorize", colorize.bind(this));
        node.add_event("remove", remove.bind(this));
        this.nodes.push(node);
        this.fire_event("nodecreated", node);
        return node;
    },
    set: function (key, value) {
        switch (key) {
            case "color":
                for (var i = 0; i < this.nodes.length; i++)
                    this.nodes[i].set("color", this.options.color);
                break;
            case "tag":
                for (var i = 0; i < this.nodes.length; i++)
                    this.nodes[i].set("tag", this.options.tag);
                break;
        }
        return TK.Widget.prototype.set.call(this, key, value);
    }
});

})(this, this.TK);
