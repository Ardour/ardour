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
 * The event is emitted for the options <code>selected</code> and <code>value</code>.
 *
 * @event TK.Select#useraction
 * 
 * @param {string} name - The name of the option which was changed due to the users action
 * @param {mixed} value - The new value of the option
 */
 
"use strict";
(function(w, TK){

function hide_list() {
    this.__transition = false;
    this.__timeout = false;
    if (!this.__open) {
        this._list.remove();
    } else {
        document.addEventListener("touchstart", this._global_touch_start);
        document.addEventListener("mousedown", this._global_touch_start);
    }
}
function show_list(show) {
    if (show) {
        var ew = TK.outer_width(this.element, true);
        document.body.appendChild(this._list);
        var cw = TK.width();
        var ch = TK.height();
        var sx = TK.scroll_left();
        var sy = TK.scroll_top();
        TK.set_styles(this._list, {
            "opacity": "0",
            "maxHeight": ch+"px",
            "maxWidth": cw+"px",
            "minWidth": ew+"px"
        });
        var lw = TK.outer_width(this._list, true);
        var lh = TK.outer_height(this._list, true);
        TK.set_styles(this._list, {
            "top": Math.min(TK.position_top(this.element) + TK.outer_height(this.element, true), ch + sy - lh) + "px",
            "left": Math.min(TK.position_left(this.element), cw + sx - lw) + "px",
        });
    } else {
        document.removeEventListener("touchstart", this._global_touch_start);
        document.removeEventListener("mousedown", this._global_touch_start);
    }
    TK.set_style(this._list, "opacity", show ? "1" : "0");
    this.__transition = true;
    this.__open = show;
    if (this.__timeout !== false) window.clearTimeout(this.__timeout);
    var dur = TK.get_duration(this._list);
    this.__timeout = window.setTimeout(hide_list.bind(this), dur);
}

function low_remove_entry(entry) {
  var li = entry.element;
  var entries = this.entries;
  var id = entries.indexOf(entry);

  if (id === -1) throw new Error("Entry removed twice.");

  // remove from DOM
  if (li.parentElement == this._list)
      this._list.removeChild(li);
  // remove from list
  entries.splice(id, 1);
  // selection
  var sel = this.options.selected;
  if (sel !== false) {
      if (sel > id) {
          this.options.selected --;
      } else if (sel === id) {
          this.options.selected = false;
          this.set("label", "");
      }
  }
  this.invalid.entries = true;
  this.select(this.options.selected);
  /**
   * Is fired when a new entry is added to the list.
   * 
   * @event TK.Select.entryremoved
   * 
   * @param {Object} entry - An object containing the members <code>title</code> and <code>value</code>.
   */
  this.fire_event("entryremoved", entry);
}

TK.Select = TK.class({
    /**
     * TK.Select provides a {@link TK.Button} with a select list to choose from
     * a list of {@TK.SelectEntry}.
     *
     * @class TK.Select
     * 
     * @extends TK.Button
     *
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Integer|Boolean} [options.selected=false] - The index of the selected {@TK.SelectEntry}.
     *   Set to `false` to unselect any already selected entries.
     * @property {mixed} [options.value] - The value of the selected entry.
     * @property {Boolean} [options.auto_size=true] - If `true`, the TK.Select is
     *   auto-sized to be as wide as the widest {@TK.SelectEntry}.
     * @property {Array<Object>} [options.entries=[]] - The list of {@TK.SelectEntry}. Each member is an
     *   object with the two properties <code>title</code> and <code>value</code>, a string used
     *   as label for constructing a {@TK.SelectEntry} or an instance of {@TK.SelectEntry}.
     *
     */
    _class: "Select",
    Extends: TK.Button,
    _options: Object.assign(Object.create(TK.Button.prototype._options), {
        entries: "array",
        selected: "int",
        value: "mixed",
        auto_size: "boolean",
        show_list: "boolean",
        sort: "function",
        resized: "boolean",
    }),
    options: {
        entries: [], // A list of strings or objects {title: "Title", value: 1} or SelectEntry instance
        selected: false,
        value: false,
        auto_size: true,
        show_list: false,
        icon: "arrowdown",
    },
    static_events: {
        click: function(e) { this.set("show_list", !this.options.show_list); },
        "set_show_list": function (v) {this.set("icon", (v ? "arrowup" : "arrowdown"));},
    },
    initialize: function (options)  {
        this.__open = false;

        this.__timeout = -1;
        
        /**
         * @member {Array} TK.Select#entries - An array containing all entry objects with members <code>title</code> and <code>value</code>.
         */
        this.entries = [];
        this._active = null;
        TK.Button.prototype.initialize.call(this, options);
        /**
         * @member {HTMLDivElement} TK.Select#element - The main DIV container.
         *   Has class <code>toolkit-select</code>.
         */
        TK.add_class(this.element, "toolkit-select");
        
        /**
         * @member {HTMLListElement} TK.Select#_list - A HTML list for displaying the entry titles.
         *   Has class <code>toolkit-select-list</code>.
         */
        this._list = TK.element("ul", "toolkit-select-list");
        this._global_touch_start = function (e) {
            if (this.__open && !this.__transition &&
                !this._list.contains(e.target) &&
                !this.element.contains(e.target)) {

                this.show_list(false);
            }
        }.bind(this);
        var sel = this.options.selected;
        var val = this.options.value; 
        this.set("entries",  this.options.entries);
        if (sel === false && val !== false) {
            this.set("value", val);
        } else {
            this.set("selected", sel);
        }
    },
    destroy: function () {
        this.clear();
        this._list.remove();
        TK.Button.prototype.destroy.call(this);
    },
    
    /**
     * Show or hide the select list
     * 
     * @method TK.Select#show_list
     * 
     * @param {boolean} show - `true` to show and `false` to hide the list
     *   of {@link TK.SelectEntry}.
     */
    show_list: function (s) {
        this.set("show_list", !!s);
    },
    
    /**
     * Select a {@link TK.SelectEntry} by its index.
     * 
     * @method TK.Select#select
     * 
     * @param {Integer} index - The index of the {@link TK.SelectEntry} to select.
     */
    select: function (id) {
        this.set("selected", id);
    },
    /**
     * Select a {@link TK.SelectEntry} by its value.
     * 
     * @method TK.Select#select_value
     * 
     * @param {mixed} value - The value of the {@link TK.SelectEntry} to select.
     */
    select_value: function (value) {
        var id = this.index_by_value.call(this, value);
        this.set("selected", id);
    },
    /**
     * Select a {@link TK.SelectEntry} by its title.
     * 
     * @method TK.Select#select_title
     * 
     * @param {mixed} title - The title of the {@link TK.SelectEntry} to select.
     */
    select_title: function (title) {
        var id = this.index_by_title.call(this, title);
        this.set("selected", id);
    },
    /**
     * Replaces the list of {@link TK.SelectEntry} to select from with an entirely new one.
     * 
     * @method TK.Select#set_entries
     * 
     * @param {Array} entries - An array of {@link TK.SelectEntry} to set as the new list to select from.
     *   Please refer to {@link TK.Select#add_entry} for more details.
     */
    set_entries: function (entries) {
        // Replace all entries with a new options list
        this.clear();
        this.add_entries(entries);
        this.select(this.index_by_value.call(this, this.options.value));
    },
    /**
     * Adds new {@link TK.SelectEntry} to the end of the list to select from.
     * 
     * @method TK.Select#add_entries
     * 
     * @param {Array} entries - An array of {@link TK.SelectEntry} to add to the end of the list
     *   of {@link TK.SelectEntry} to select from. Please refer to {@link TK.Select#add_entry}
     *   for more details.
     */
    add_entries: function (entries) {
        for (var i = 0; i < entries.length; i++)
            this.add_entry(entries[i]);
    },
    /**
     * Adds a single {@link TK.SelectEntry} to the end of the list.
     * 
     * @method TK.Select#add_entry
     * 
     * @param {mixed} entry - A string to be displayed and used as the value,
     *   an object with members <code>title</code> and <code>value</code>
     *   or an instance of {@link TK.SelectEntry}.
     * 
     * @emits TK.Select.entryadded
     */
    add_entry: function (ent) {
        var O = this.options;
        var entry;
        var entries = this.entries;

        if (TK.SelectEntry.prototype.isPrototypeOf(ent)) {
            entry = ent;
        } else {
            entry = new TK.SelectEntry({
                value: (typeof ent === "string") ? ent : ent.value,
                title: (typeof ent === "string")
                       ? ent : (ent.title !== void(0))
                       ? ent.title : ent.value.toString()
            });
        }
        this.add_child(entry);
        entries.push(entry);
        entry.set("container", this._list)

        var id;

        if (O.sort) {
          entries.sort(O.sort);
          id = entries.indexOf(entry);
          if (id !== entries.length - 1)
            this._list.insertBefore(entry.element, entries[id+1].element);
        } else {
          id = entries.length - 1;
        }
        
        this.invalid.entries = true;

        if (this.options.selected === id) {
            this.invalid.selected = true;
            this.trigger_draw();
        } else if (this.options.selected > id) {
            this.set("selected", this.options.selected+1);
        } else {
            this.trigger_draw();
        }
        /**
         * Is fired when a new {@link TK.SelectEntry} is added to the list.
         * 
         * @event TK.Select#entryadded
         * 
         * @param {TK.SelectEntry} entry - A new {@link TK.SelectEntry}.
         */
        this.fire_event("entryadded", entry);
    },
    /**
     * Remove a {@link TK.SelectEntry} from the list by its index.
     * 
     * @method TK.Select#remove_id
     * 
     * @param {Integer} index - The index of the {@link TK.SelectEntry} to be removed from the list.
     * 
     * @emits TK.Select#entryremoved
     */
    remove_index: function (index) {
        var entry = this.entries[index];
        this.remove_child(entry);
    },
    /**
     * Remove a {@link TK.SelectEntry} from the list by its value.
     * 
     * @method TK.Select#remove_value
     * 
     * @param {mixed} value - The value of the {@link TK.SelectEntry} to be removed from the list.
     * 
     * @emits TK.Select#entryremoved
     */
    remove_value: function (val) {
        this.remove_id(this.index_by_value.call(this, val));
    },
    /**
     * Remove an entry from the list by its title.
     * 
     * @method TK.Select#remove_title
     * 
     * @param {string} title - The title of the entry to be removed from the list.
     * 
     * @emits TK.Select#entryremoved
     */
    remove_title: function (title) {
        this.remove_id(this.index_by_title.call(this, title));
    },
    /**
     * Remove an entry from the list.
     * 
     * @method TK.Select#remove_entry
     * 
     * @param {TK.SelectEntry} entry - The {@link TK.SelectEntry} to be removed from the list.
     * 
     * @emits TK.Select#entryremoved
     */
    remove_entry: function (entry) {
        this.remove_child(entry);
    },
    remove_entries: function (a) {
        for (var i = 0; i < a.length; i++)
            this.remove_entry(a[i]);
    },
    remove_child: function(child) {
      TK.Button.prototype.remove_child.call(this, child);
      if (TK.SelectEntry.prototype.isPrototypeOf(child)) {
        low_remove_entry.call(this, child);
      }
    },
    /**
     * Get the index of a {@link TK.SelectEntry} by its value.
     * 
     * @method TK.Select#index_by_value
     * 
     * @param {Mixed} value - The value of the {@link TK.SelectEntry}.
     * 
     * @returns {Integer|Boolean} The index of the entry or `false`.
     */
    index_by_value: function (val) {
        var entries = this.entries;
        for (var i = 0; i < entries.length; i++) {
            if (entries[i].options.value === val)
                return i;
        }
        return false;
    },
    /**
     * Get the index of a {@link TK.SelectEntry} by its title/label.
     * 
     * @method TK.Select#index_by_title
     * 
     * @param {String} title - The title/label of the {@link TK.SelectEntry}.
     * 
     * @returns {Integer|Boolean} The index of the entry or `false`.
     */
    index_by_title: function (title) {
        var entries = this.entries;
        for (var i = 0; i < entries.length; i++) {
            if (entries[i].options.title === title)
                return i;
        }
        return false;
    },
    /**
     * Get the index of a {@link TK.SelectEntry} by the {@link TK.SelectEntry} itself.
     * 
     * @method TK.Select#index_by_entry
     * 
     * @param {TK.SelectEntry} entry - The {@link TK.SelectEntry}.
     * 
     * @returns {Integer|Boolean} The index of the entry or `false`.
     */
    index_by_entry: function (entry) {
        var pos = this.entries.indexOf(entry);
        return pos === -1 ? false : pos;
    },
    /**
     * Get a {@link TK.SelectEntry} by its value.
     * 
     * @method TK.Select#entry_by_value
     * 
     * @param {Mixed} value - The value of the {@link TK.SelectEntry}.
     * 
     * @returns {TK.SelectEntry|False} The {@link TK.SelectEntry} or `false`.
     */
    entry_by_value: function (val) {
        var entries = this.entries;
        for (var i = 0; i < entries.length; i++) {
            if (entries[i].options.value === val)
                return entries[i];
        }
        return false;
    },
    /**
     * Get a {@link TK.SelectEntry} by its title/label.
     * 
     * @method TK.Select#entry_by_title
     * 
     * @param {String} title - The title of the {@link TK.SelectEntry}.
     * 
     * @returns {TK.SelectEntry|Boolean} The {@link TK.SelectEntry} or `false`.
     */
    entry_by_title: function (title) {
        var entries = this.entries;
        for (var i = 0; i < entries.length; i++) {
            if (entries[i].options.title === title)
                return entries[i];
        }
        return false;
    },
    /**
     * Get a {@link TK.SelectEntry} by its index.
     * 
     * @method TK.Select#entry_by_index
     * 
     * @param {Integer} index - The index of the {@link TK.SelectEntry}.
     * 
     * @returns {TK.SelectEntry|Boolean} The {@link TK.SelectEntry} or `false`.
     */
    entry_by_index: function (index) {
        if (index >= 0 && index < entries.length && entries[index])
            return entries[i];
        return false;
    },
    /**
     * Get a value by its {@link TK.SelectEntry} index.
     * 
     * @method TK.Select#value_by_index
     * 
     * @param {Integer} index - The index of the {@link TK.SelectEntry}.
     * 
     * @returns {Mixed|Boolean} The value of the {@link TK.SelectEntry} or `false`.
     */
    value_by_index: function(index) {
        var entries = this.entries;
        if (index >= 0 && index < entries.length && entries[index]) {
          return entries[index].options.value;
        }
        return false;
    },
    /**
     * Get the value of a {@link TK.SelectEntry}.
     * 
     * @method TK.Select#value_by_entry
     * 
     * @param {TK.SelectEntry} entry - The {@link TK.SelectEntry}.
     * 
     * @returns {mixed} The value of the {@link TK.SelectEntry}.
     */
    value_by_entry: function(entry) {
        return entry.options.value;
    },
    /**
     * Get the value of a {@link TK.SelectEntry} by its title/label.
     * 
     * @method TK.Select#value_by_title
     * 
     * @param {String} title - The title of the {@link TK.SelectEntry}.
     * 
     * @returns {Mixed|Boolean} The value of the {@link TK.SelectEntry} or `false`.
     */
    value_by_title: function (title) {
        var entries = this.entries;
        for (var i = 0; i < entries.length; i++) {
            if (entries[i].options.title === title)
                return entries[i].options.value;
        }
        return false;
    },
    /**
     * Remove all {@link TK.SelectEntry} from the list.
     * 
     * @method TK.Select#clear
     * 
     * @emits TK.Select#cleared
     */
    clear: function () {
        TK.empty(this._list);
        this.select(false);
        var entries = this.entries.slice(0);
        for (var i = 0; i < entries.length; i++) {
          this.remove_child(entries[i]);
        }
        /**
         * Is fired when the list is cleared.
         * 
         * @event TK.Select.cleared
         */
        this.fire_event("cleared");
    },

    redraw: function() {
        TK.Button.prototype.redraw.call(this);

        var I = this.invalid;
        var O = this.options;
        var E = this.element;

        if (I.selected || I.value) {
            I.selected = I.value = false;
            if (this._active) {
                TK.remove_class(this._active, "toolkit-active");
            }
            var entry = this.entries[O.selected];

            if (entry) {
                this._active = entry.element;
                TK.add_class(entry.element, "toolkit-active");
            } else {
                this._active = null;
            }
        }

        if (I.validate("entries", "auto_size")) {

            I.show_list = true;

            var L;

            if (O.auto_size && (L = this._label)) {
                var width = 0;
                E.style.width = "auto";
                var orig_content = document.createDocumentFragment();
                while (L.firstChild) orig_content.appendChild(L.firstChild);
                var entries = this.entries;
                for (var i = 0; i < entries.length; i++) {
                    L.appendChild(document.createTextNode(entries[i].options.title));
                    L.appendChild(document.createElement("BR"));
                }
                TK.S.add(function() {
                    width = TK.outer_width(E, true);
                    TK.S.add(function() {
                        while (L.firstChild) L.removeChild(L.firstChild);
                        L.appendChild(orig_content);
                        TK.outer_width(E, true, width);
                    }, 1);
                });
            }
        }

        if (I.validate("show_list", "resized")) {
            show_list.call(this, O.show_list);
        }
    },
    /**
     * Get the currently selected {@link TK.SelectEntry}.
     * 
     * @method TK.Select#current
     * 
     * @returns {TK.SelectEntry|Boolean} The currently selected {@link TK.SelectEntry} or `false`.
     */
    current: function() {
        if (this.options.selected !== false)
            return this.entries[this.options.selected];
        return false;
    },
    /**
     * Get the currently selected {@link TK.SelectEntry}'s index. Just for the sake of completeness, this
     *   function abstracts `options.selected`.
     * 
     * @method TK.Select#current_index
     * 
     * @returns {Integer|Boolean} The index of the currently selected {@link TK.SelectEntry} or `false`.
     */
    current_index: function() {
        return this.options.selected;
    },
    /**
     * Get the currently selected {@link TK.SelectEntry}'s value.
     * 
     * @method TK.Select#current_value
     * 
     * @returns {Mixed|Boolean} The value of the currently selected {@link TK.SelectEntry} or `false`.
     */
    current_value: function() {
        var w = this.current();
        if (w) return w.get("value");
        return false;
    },
    set: function (key, value) {
        if (key === "value") {
            this.set("selected", this.index_by_value.call(this, value));
            return this.current_value();
        }
        
        value = TK.Button.prototype.set.call(this, key, value);

        switch (key) {
            case "selected":
                var entry = this.current();
                if (entry !== false) {
                    TK.Button.prototype.set.call(this, "value", entry.options.value); 
                    this.set("label", entry.options.label);
                } else {
                    TK.Button.prototype.set.call(this, "value", void 0); 
                    this.set("label", false);
                }
                break;
            case "entries":
                this.set_entries(value);
                break;
        }
        return value;
    }
});


function on_select(e) {
    var w = this.parent;
    var id = w.index_by_entry(this);
    var entry = this;
    e.stopPropagation();
    e.preventDefault();

    if (w.userset("selected", id) === false) return false;
    w.userset("value", this.options.value);
    /**
     * Is fired when a selection was made by the user. The arguments
     * are the value of the currently selected {@link TK.SelectEntry}, its index, its title and the {@link TK.SelectEntry} instance.
     * 
     * @event TK.Select#select
     * 
     * @param {mixed} value - The value of the selected entry.
     * @param {number} value - The ID of the selected entry.
     * @param {string} value - The title of the selected entry.
     * @param {string} value - The title of the selected entry.
     */
    w.fire_event("select", entry.options.value, id, entry.options.title);
    w.show_list(false);

    return false;
}

TK.SelectEntry = TK.class({
    /**
     * TK.SelectEntry provides a {@link TK.Label} as an entry for {@link TK.Select}.
     *
     * @class TK.SelectEntry
     * 
     * @extends TK.Label
     *
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {String} [options.title=""] - The title of the entry. Kept for backward compatibility, deprecated, use label instead.
     * @property {mixed} [options.value] - The value of the selected entry.
     *
     */
    _class: "SelectEntry",
    Extends: TK.Label,
    
    _options: Object.assign(Object.create(TK.Label.prototype._options), {
        value: "Mixed",
        title: "String",
    }),
    options: {
        title: "",
        value: null
    },
    initialize: function (options) {
        var E = this.element = TK.element("li", "toolkit-option");
        TK.Label.prototype.initialize.call(this, options);
        this.set("title", this.options.title);
    },
    static_events: {
      touchstart: on_select,
      mousedown: on_select,
    },
    set: function (key, value) {
        switch (key) {
            case "title":
                this.set("label", value);
                break;
            case "label":
                this.options.title = value;
                break;
        }
        return TK.Label.prototype.set.call(this, key, value);
    }
});

})(this, this.TK);
