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

/**
 * TK.Notifications is a {@link TK.Container} displaying {@link TK.Notification}
 *   popups.
 * 
 * @class TK.Notifications
 * 
 * @extends TK.Container
 * 
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {String} [options.stack="end"] - Define the position a new {@link TK.Notification}
 *   is appended to the container, either `end` or `start`.
 */

TK.Notifications = TK.class({
    
    _class: "Notifications",
    Extends: TK.Container,
    
    _options: Object.assign(TK.Container.prototype._options, {
      stack: "string",
    }),
    options: {
      stack: "start",
    },
    
    initialize: function (options) {
        TK.Container.prototype.initialize.call(this, options);
        TK.add_class(this.element, "toolkit-notifications");
    },
    
    notify: function (options) {
    /**
     * Create and show a new notification.
     * 
     * @method TK.Notifications#notify
     * 
     * @param {Object} [options={ }] - An object containing initial options. - Options for the {@link TK.Notification} to add
     * 
     */
      var n = new TK.Notification(options);
      this.add_child(n);
      if (this.options.stack == "start")
        this.element.insertBefore(n.element, this.element.firstChild);
      else
        this.element.appendChild(n.element);
      return n;
    }
});
    
})(this, this.TK);
