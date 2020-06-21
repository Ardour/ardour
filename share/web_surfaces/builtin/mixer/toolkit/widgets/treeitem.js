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

var toggle_collapsed = function () {
    set_collapsed.call(this, !TK.has_class(this.element, "toolkit-collapsed"));
}
var set_collapsed = function (c) {
    this.set("collapsed", c);
}

var reset_size = function (state) {
    if (state !== "show") return;
    //this.element.style.height = null;
}

TK.TreeItem = TK.class({
    
    _class: "TreeItem",
    Extends: TK.ListItem,
    
    _options: Object.assign(Object.create(TK.Widget.prototype._options), {
        collapsed: "boolean",
        collapsable: "boolean",
        force_collapsable: "boolean",
    }),
    options: {
        collapsed: false,
        collapsable: true,
        force_collapsable: false,
    },
    initialize: function (options) {
        this.list = new TK.List({
            "onset_display_state": reset_size
        });
        this.flex = new TK.Container({"class":"toolkit-flex"});
        
        TK.ListItem.prototype.initialize.call(this, options);
        TK.add_class(this.element, "toolkit-tree-item");
        
        TK.ListItem.prototype.append_child.call(this, this.flex);
        TK.ListItem.prototype.add_child.call(this, this.list);
        this.flex.show();
        
        this.collapse = new TK.Button({"class":"toolkit-collapse"});
        this.append_child(this.collapse);
        this.collapse.add_event("click", toggle_collapsed.bind(this));
        
        if (this.options.collapsable && this.options.collapsed) {
          this.list.element.style.height = "0px";
          this.list.hide();
        }
    },
    append_child: function (child) {
        this.invalid._list = true;
        this.trigger_resize();
        if (TK.ListItem.prototype.isPrototypeOf(child)) {
            return this.list.append_child(child);
        } else {
            return this.flex.append_child(child);
        }
    },
    add_child : function(child) {
        this.trigger_resize();
        if (TK.ListItem.prototype.isPrototypeOf(child)) {
            return this.list.add_child(child);
        } else {
            return this.flex.add_child(child);
        }
    },
    remove_child : function(child) {
        if (TK.ListItem.prototype.isPrototypeOf(child)) {
            var r = this.list.remove_child(child);
            return r;
        } else {
          this.flex.remove_child(child);
        }
        this.invalid._list = true;
        this.trigger_resize();
    },
    redraw: function () {
        TK.ListItem.prototype.redraw.call(this);
        var I = this.invalid;
        var O = this.options;
        var E = this.element;
        var F = this.flex.element;
        if (I._list) {
            I.collapsed = true;
            if (this.list.children && this.list.children.length) {
                if (this.list.element.parentElement != E)
                    E.appendChild(this.list.element);
                this.add_class("toolkit-has-tree");
            } else {
                if (this.list.element.parentElement == E)
                    E.removeChild(this.list.element);
                this.remove_class("toolkit-has-tree");
            }
        }
        if (I._list || I.collapsable || I.force_collapsable) {
            if ((this.list.children && this.list.children.length && O.collapsable) || O.force_collapsable)
                F.appendChild(this.collapse.element);
            else if (this.collapse.element.parentElement == E)
                F.removeChild(this.collapse.element);
            TK.toggle_class(E, "toolkit-force-collapsable", O.force_collapsable);
        }
        if (I.collapsed) {
            I.collapsed = false;
            var s = this.list.element.style;
            if (O.collapsed) {
                var h = this.list.element.offsetHeight;
                s["height"] = h + "px";
                window.requestAnimationFrame(function () {s["height"] = "0px";});
            } else {
                var list = this.list.element;
                /* This is a train */
                TK.S.add(function() {
                  var h0 = list.offsetHeight;
                  var duration = TK.get_duration(list);
                  TK.S.add(function() {
                    s["height"] = "auto";
                    TK.S.add(function() {
                      var h = list.offsetHeight;
                      TK.S.add(function() {
                        s["height"] = h0 + "px";
                        TK.S.add_next(function () {
                          s["height"] = h + "px";

                          setTimeout(function () {
                            s.height = null;
                          }, duration);
                        });
                      }, 1)
                    });
                  }, 1);
                });
            }
            TK.toggle_class(E, "toolkit-collapsed", O.collapsed);
        }
        I._list = I.collapsable = I.force_collapsable = false;
    },
    set: function (key, value) {
        var O = this.options;
        switch (key) {
            case "collapsed":
                if (!this.list) break;
                if (!value)
                    this.list.show();
                else
                    this.list.hide();
                break;
        }
        return TK.ListItem.prototype.set.call(this, key, value);
    }
});

})(this, this.TK);
