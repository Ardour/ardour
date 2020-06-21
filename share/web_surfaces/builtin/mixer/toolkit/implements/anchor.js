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
 * TK.Anchor provides a single function translate_anchor
 * which returns real x and y values from a relative positioning.
 * For example positioning a {@link TK.Window} with anchor <code>center</code>
 * needs to subtract half of its width from y and half of its height
 * from x to appear at the correct position.
 *
 * @mixin TK.Anchor
 */
TK.Anchor = TK.class({
    /**
     * Returns real x and y values from a relative positioning.
     * 
     * @method TK.Anchor#translate_anchor
     * 
     * @param {string} [anchor="top-left"] - Position of the anchor.
     * @param {number} x - X position to translate.
     * @param {number} y - Y position to translate.
     * @param {number} width - Width of the element.
     * @param {number} height - Height of the element.
     * 
     * @returns {object} Object with members x and y as numbers
     */
    translate_anchor: function (anchor, x, y, width, height) {
        switch (anchor) {
            case "top-left":
                break;
            case "top":
                x += width / 2;
                break;
            case "top-right":
                x += width;
                break;
            case "left":
                y += height / 2;
                break;
            case "center":
                x += width / 2;
                y += height / 2;
                break;
            case "right":
                x += width;
                y += height / 2;
                break;
            case "bottom-left":
                y += height;
                break;
            case "bottom":
                x += width / 2;
                y += height;
                break;
            case "bottom-right":
                x += width;
                y += height;
                break;
            default:
                throw new Error("Unsupported anchor position");
        }
        return {x: Math.round(x), y: Math.round(y)};
    }
});
})(this, this.TK);
