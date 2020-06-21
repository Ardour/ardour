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

function autoclose_cb(e) {
  var curr = e.target;
  while (curr) {
    // TODO: if a dialog is opened out of a dialog both should avoid
    // closing any of those on click. former version:
    //if (curr === this.element) return;
    // this closes tagger in Cabasa Dante Tagger when interacting
    // with the colorpicker.
    // workaround for the moment:
    // don't close on click on any dialog
    if (curr.classList.contains("toolkit-dialog")) return;
    curr = curr.parentElement;
  }
  this.close();
}

function activate_autoclose() {
  if (this._autoclose_active) return;
  document.body.addEventListener("click", this._autoclose_cb);
  this._autoclose_active = true;
}

function deactivate_autoclose() {
  if (!this._autoclose_active) return;
  document.body.removeEventListener("click", this._autoclose_cb);
  this._autoclose_active = false;
}

TK.Dialog = TK.class({
/**
 * TK.Dialog provides a hovering area which can be closed by clicking/tapping
 * anywhere on the screen. It can be automatically pushed to the topmost
 * DOM position as a child of an AWML-ROOT or the BODY element. On close
 * it can be removed from the DOM. The {@link TK.Anchor}-functionality
 * makes positioning the dialog window straight forward.
 *
 * @class TK.Dialog
 * 
 * @extends TK.Container
 * @implments TK.Anchor
 *
 * @param {Object} [options={ }] - An object containing initial options.
 * 
 * @property {Boolean} [options.visible=true] - Hide or show the dialog.
 * @property {String} [options.anchor="top-left"] - Origin of `x` and `y` coordinates.
 * @property {Number} [options.x=0] - X-position of the dialog.
 * @property {Number} [options.y=0] - Y-position of the dialog.
 * @property {boolean} [options.auto_close=false] - Set dialog to `visible=false` if clicked outside in the document.
 * @property {boolean} [options.auto_remove=false] - Remove the dialogs DOM node after setting `visible=false`.
 * @property {boolean} [options.toplevel=false] - Add the dialog DOM node to the topmost position in DOM on `visible=true`. Topmost means either a parenting `AWML-ROOT` or the `BODY` node.
 * 
 */
    _class: "Dialog",
    Extends: TK.Container,
    Implements: TK.Anchor,
    _options: Object.assign(Object.create(TK.Container.prototype._options), {
        visible: "boolean",
        anchor: "string",
        x: "number",
        y: "number",
        auto_close: "boolean",
        auto_remove: "boolean",
        toplevel: "boolean",
    }),
    options: {
        visible: true,
        anchor: "top-left",
        x: 0,
        y: 0,
        auto_close: false,
        auto_remove: false,
        toplevel: false,
    },
    static_events: {
      hide: function() {
        deactivate_autoclose.call(this);
        if (this.options.auto_remove)
            this.element.remove();
        this.fire_event("close");
      },
      set_display_state: function(val) {
        var O = this.options;

        if (val === "show") {
          if (O.auto_close)
            activate_autoclose.call(this);
          this.trigger_resize();
        } else {
          deactivate_autoclose.call(this);
        }

        if (val === "showing") {
          var C = O.container;
          if (C) C.appendChild(this.element);
          this.reposition();
        }

      },
      set_auto_close: function(val) {
        if (val) { 
          if (!this.hidden()) activate_autoclose.call(this);
        } else {
          deactivate_autoclose.call(this);
        }
      },
      set_visible: function (val) {
        var O = this.options;
        if (val) {
          deactivate_autoclose.call(this);
          if (O.toplevel && O.container.tagName !== "AWML-ROOT" && O.container.tagName !== "BODY") {
            var p = this.element;
            while ((p = p.parentElement) && p.tagName !== "AWML-ROOT" && p.tagName !== "BODY") {};
            this.set("container", p);
          }
          this.show();
        } else {
          O.container = this.element.parentElement;
          this.hide();
        }
      },
    },
    initialize: function (options) {
        TK.Container.prototype.initialize.call(this, options);
        TK.add_class(this.element, "toolkit-dialog");
        var O = this.options;
        /* This cannot be a default option because document.body
         * is not defined there */
        if (!O.container) O.container = window.document.body;
        this._autoclose_active = false;
        this._autoclose_cb = autoclose_cb.bind(this);
        this.set('visible', O.visible);
        if (O.visible)
          this.force_show()
        else
          this.force_hide()
    },
    resize: function() {
        if (this.options.visible)
          this.reposition();
    },
    redraw: function () {
        TK.Container.prototype.redraw.call(this);
        var I = this.invalid;
        var O = this.options;
        var E = this.element;
        if (I.x || I.y || I.anchor) {
            var bodybox = document.body.getBoundingClientRect();
            var sw = bodybox.width;
            var sh = bodybox.height;
            var box = this.element.getBoundingClientRect();
            I.x = I.y = I.anchor = false;
            var box = E.getBoundingClientRect();
            var pos = this.translate_anchor(O.anchor, O.x, O.y, -box.width, -box.height);
            pos.x = Math.min(sw - box.width, Math.max(0, pos.x));
            pos.y = Math.min(sh - box.height, Math.max(0, pos.y));
            E.style.left = pos.x + "px"
            E.style.top  = pos.y + "px"
        }
    },
    /**
     * Open the dialog. Optionally set x and y position regarding `anchor`.
     *
     * @method TK.Dialog#open
     * 
     * @param {Number} [x] - New X-position of the dialog.
     * @param {Number} [y] - New Y-position of the dialog.
     */
    open: function (x, y) {
        
        this.fire_event("open");
        this.userset("visible", true);
        if (typeof x !== "undefined")
            this.set("x", x);
        if (typeof y !== "undefined")
            this.set("y", y);
    },
    /**
     * Close the dialog. The node is removed from DOM if `auto_remove` is set to `true`.
     *
     * @method TK.Dialog#close
     */
    close: function () {
        this.userset("visible", false);
    },
    /**
     * Reposition the dialog to the current `x` and `y` position.
     *
     * @method TK.Dialog#reposition
     */
    reposition: function () {
        var O = this.options;
        this.set("x", O.x);
        this.set("y", O.y);
    }
});
    
    
})(this, this.TK);
