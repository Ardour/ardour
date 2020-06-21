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

TK.List = TK.class({
    /**
     * TK.List is a sortable {@link TK.Container} for {@TK.ListItems}s.
     *   the element is a UL instead of a DIV.
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Function|Boolean} [options.sort=false] - A function
     *   expecting arguments A and B, returning a number <0, if A comes first and >0,
     *   if B comes first. 0 keeps both elements in place. Please refer to the
     *   compareFunction at <a href="https://www.w3schools.com/jsref/jsref_sort.asp">W3Schools</a>
     *   for any further information.
     * 
     * @class TK.List
     * 
     * @extends TK.Container
     */
    _options: Object.assign(Object.create(TK.Container.prototype._options), {
      sort: "function",
    }),
    _class: "List",
    Extends: TK.Container,
    
    initialize: function (options) {
        this.element = TK.element("ul", "toolkit-list");
        TK.Container.prototype.initialize.call(this, options);
    },
    static_events: {
      set_sort: function(f) {
        if (typeof(f) === "function") {
          var C = this.children.slice(0);
          C.sort(f);
          for (var i = 0; i < C.length; i++) {
            this.element.appendChild(C[i].element);
          }
        }
      },
    },
    append_child: function(w) {
      TK.Container.prototype.append_child.call(this, w);
      var O = this.options;
      var C = this.children;
      if (O.sort) {
        C.sort(O.sort);
        var pos = C.indexOf(w);
        if (pos !== C.length - 1)
          this.element.insertBefore(w.element, C[pos+1].element);
      }
    },
});
    
})(this, this.TK);
