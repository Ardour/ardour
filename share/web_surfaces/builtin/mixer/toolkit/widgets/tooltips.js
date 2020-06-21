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
function get_event(event) {
    // return the right event if touch surface is used
    // with multiple fingers
    return (event.touches && event.touches.length)
          ? event.touches[0] : event;
}
/**
 * TK.GlobalTooltip is a small text field following the mouse.
 *   Due to optimization it is implemented as a table with a single cell
 *   containing the content.
 * 
 * @class TK.Tooltip
 */
TK.GlobalTooltip = function() {
    var overlay = TK.element("div", "toolkit-tooltip");
    var table   = TK.element("div", "toolkit-table");
    var row     = TK.element("div", "toolkit-row");
    var cell    = TK.element("div", "toolkit-cell");

    var tmp = row.cloneNode();

    var spacer_tl = cell.cloneNode();

    tmp.appendChild(spacer_tl);
    tmp.appendChild(cell.cloneNode());
    tmp.appendChild(cell.cloneNode());

    table.appendChild(tmp);

    tmp = row.cloneNode();

    var entry = cell.cloneNode();
    entry.className += " toolkit-entry";

    var entry_tl = cell.cloneNode();
    entry_tl.className += " toolkit-tt-container";
    entry_tl.appendChild(entry);

    tmp.appendChild(cell.cloneNode());
    tmp.appendChild(entry_tl);
    tmp.appendChild(cell.cloneNode());

    table.appendChild(tmp);

    tmp = row.cloneNode();

    tmp.appendChild(cell.cloneNode());
    tmp.appendChild(cell.cloneNode());
    tmp.appendChild(cell.cloneNode());

    table.appendChild(tmp);
    overlay.appendChild(table);

    var tooltips = [];

    function current_callback() {
        for (var i = 0; i < tooltips.length; i++) {
            if (tooltips[i] && tooltips[i].length) {
                return tooltips[i][0];
            }
        }
    }

    var ev = null;

    function redraw() {
        var e = ev;
        ev = null;
        var current = current_callback();

        if (!current) {
            hide();
            return;
        }

        var w = e.clientX;
        var h = e.clientY / window.innerHeight;

        spacer_tl.style.width = w > 0 ? w + "px" : "0.01%";
        spacer_tl.style.height = h > 0 ? (h * 100).toFixed(2) + "%" : "0.01%";

        current(e, entry);
    }

    function onmove_mouse(e) {
        if (!ev) {
            TK.S.add(redraw, 1);
        }
        ev = e;
    }

    function onmove_touch(e) {
        onmove_mouse(get_event(e));
    }

    var hidden = false;
    var num_tooltips = 0;
    
    /**
     * Hide the tooltips
     * 
     * @method TK.Tooltip#hide
     */
    function hide() {
        document.removeEventListener("mousemove", onmove_mouse);
        document.removeEventListener("touchmove", onmove_touch);
        overlay.style.display = "none";
        hidden = true;
    }
    /**
     * Show the tooltips
     * 
     * @method TK.Tooltip#show
     */
    function show() {
        if (!overlay.parentNode)
            document.body.appendChild(overlay);
        document.addEventListener("mousemove", onmove_mouse);
        document.addEventListener("touchmove", onmove_touch);
        overlay.style.removeProperty("display");
        hidden = false;
    }
    /**
     * Add a new tooltip.
     * 
     * @method TK.Tooltip#add
     * 
     * @param {int} priority - The priority of the tooltip. States its position in the list.
     * @param {Function} onmove - The function which sets the text of the tooltip while moving the mouse.
     */
    function add(priority, onmove) {
        if (!tooltips[priority]) tooltips[priority] = [];
        tooltips[priority].push(onmove);
        if (hidden) show();
        num_tooltips++;
    }
    /**
     * Remove a tooltip.
     * 
     * @method TK.Tooltip#remove
     * 
     * @param {int} priority - The priority of the tooltip.
     * @param {Function} onmove - The function which sets the text of the tooltip while moving the mouse.
     */
    function remove(priority, onmove) {
        if (!tooltips[priority]) return;
        var i = tooltips[priority].indexOf(onmove);

        if (i === -1) return;
        if (tooltips[priority].length === 1) tooltips[priority] = null;
        else tooltips[priority].splice(i, 1);
        if (--num_tooltips === 0) hide();
    }

    hide();

    this.show = show;
    this.hide = hide;
    this.add = add;
    this.remove = remove;
    this.trigger = onmove_touch;
    
    /**
     * @member {HTMLDivElement} TK.Tooltip#element - The overlay containing the tooltip table.
     *   Has class <code>toolkit-tooltip</code>.
     */
    this.element = overlay;
    /** 
     * @member {HTMLDivElement} TK.Tooltip#_table - The table containing the tooltips.
     *   Has class <code>toolkit-table</code>.
     */
    this._table = table;
    /**
     * @member {HTMLDivElement} TK.Tooltip#_entry - The element containing the tooltip text.
     *   Has class <code>toolkit-entry</code>.
     */
    this._entry = entry;
};
TK.GlobalTooltip.prototype = {
    destroy: function() {
        this.hide();
    },
};
TK.tooltip = new TK.GlobalTooltip();
})(this, TK);
