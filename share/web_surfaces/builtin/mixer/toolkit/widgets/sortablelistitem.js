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

var build_sorter = function () {
    this.sorter = new TK.Button({"class":"toolkit-sorter",container:this.element});
    this.add_child(this.sorter);
}

TK.SortableListItem = TK.class({
    
    _class: "SortableListItem",
    Extends: TK.ListItem,
    
    _options: Object.assign(Object.create(TK.ListItem.prototype._options), {
        sortable: "boolean",
    }),
    options: {
        sortable: false,
    },
    initialize: function (options) {
        TK.ListItem.prototype.initialize.call(this, options);
        this.element.add_class("toolkit-sortable-list-item");
    },
    redraw: function () {
        TK.ListItem.prototype.redraw.call(this);
        var I = this.invalid;
        var O = this.options;
        if (I.sortable) {
            if (O.sortable) {
                if (!this.sorter) {
                    build_sorter.call(this);
                } else {
                    this.element.appendChild(this.sorter.element);
                }
            } else {
                if (this.sorter)
                    this.element.removeChild(this.sorter.element);
            }
        }
    },
    set: function (key, value) {
        return TK.ListItem.prototype.set.call(this, key, value);
    }
});
    
    
})(this, this.TK);
