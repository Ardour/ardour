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

/* TODO */

var build_dragcapture = function () {
    this.dragcapture = new TK.DragCapture(this, {
        state: true,
        onstartcapture: function (state) {
            console.log(state, "start");
            return true;
        },
        onmovecapture: function (state) {
            console.log(state, "move");
        },
        onstopcapture: function () {
            console.log("stop");
        }
    });
}

TK.SortableList = TK.class({
    
    _class: "SortableList",
    Extends: TK.List,
    
    _options: Object.assign(Object.create(TK.Container.prototype._options), {
        sortable: "boolean",
    }),
    options: {
        sortable: false,
        item_class: TK.SortableListItem,
    },
    initialize: function (options) {
        TK.List.prototype.initialize.call(this, options);
        this.element.add_class("toolkit-sortable-list");
    },
    add_item: function (item, pos) {
        var O = this.options;
        var item = TK.List.prototype.add_item.call(this, item, pos);
        item.set("sortable", O.sortable);
    },
    set: function (key, value) {
        switch (key) {
            case "sortable":
                var I = this.options.items;
                for (var i = 0; i < I.length; i++)
                    I[i].set("sortable", value);
                if (value && !this.dragcapture)
                    build_dragcapture.call(this);
                break;
        }
        return TK.List.prototype.set.call(this, key, value);
    }
});
    
    
})(this, this.TK);
