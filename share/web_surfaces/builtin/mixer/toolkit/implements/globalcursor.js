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
/**
 * TK.GlobalCursor adds global cursor classes to ensure
 * one of the <a href="https://developer.mozilla.org/de/docs/Web/CSS/cursor">standard cursors</a>
 * is shown in the overall application.
 *
 * @mixin TK.GlobalCursor
 */
TK.GlobalCursor = TK.class({
    _class: "GlobalCursor",
    /**
     * Adds a class <code>"toolkit-cursor-" + cursor</code> to the <code>document.body</code> to show a specific cursor.
     * 
     * @method TK.GlobalCursor#global_cursor
     * 
     * @param {string} cursor - The name of the <a href="https://developer.mozilla.org/de/docs/Web/CSS/cursor">cursor</a> to show.
     * 
     * @emits TK.GlobalCursor#globalcursor
     */
    global_cursor: function (cursor) {
        TK.add_class(document.body, "toolkit-cursor-" + cursor);
        /**
         * Is fired when a cursor gets set.
         * 
         * @event TK.GlobalCursor#globalcursor
         * 
         * @param {string} cursor - The name of the <a href="https://developer.mozilla.org/de/docs/Web/CSS/cursor">cursor</a> to show. 
         */
        this.fire_event("globalcursor", cursor);
    },
    /**
     * Removes the class from <code>document.body</code> node.
     *
     * @method TK.GlobalCursor#remove_cursor
     * 
     * @param {string} cursor - The name of the <a href="https://developer.mozilla.org/de/docs/Web/CSS/cursor">cursor</a> to remome.
     * 
     * @emits TK.GlobalCursor#cursorremoved
     */
    remove_cursor: function (cursor) {
        TK.remove_class(document.body, "toolkit-cursor-" + cursor);
        /**
         * Is fired when a cursor is removed.
         * 
         * @event TK.GlobalCursor#cursorremoved
         * 
         * @param {string} cursor - The name of the <a href="https://developer.mozilla.org/de/docs/Web/CSS/cursor">cursor</a> to remove.
         */
        this.fire_event("cursorremoved", cursor);
    }
});
})(this, this.TK);
