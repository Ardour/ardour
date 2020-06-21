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
    
TK.ResponseHandler = TK.class({
    /**
     * TK.ResponseHandler is a {@link TK.FrequencyResponse} adding some {@link TK.ResponseHandle}s. It is
     * meant as a universal user interface for equalizers and the like.
     * 
     * This class is deprecated since all relevant functionality went into
     * the base class TK.Graph. Use {@link TK.FrequencyResponse} instead.
     *
     * @class TK.ResponseHandler
     * 
     * @extends TK.FrequencyResponse
     */
    _class: "ResponseHandler",
    Extends: TK.FrequencyResponse,
    initialize: function (options) {
        TK.FrequencyResponse.prototype.initialize.call(this, options);
        /**
         * @member {HTMLDivElement} TK.ResponseHandler#element - The main DIV container.
         *   Has class <code>toolkit-response-handler</code>.
         */
        TK.add_class(this.element, "toolkit-response-handler");
        /**
         * @member {SVGImage} TK.ResponseHandler#_handles - An SVG group element containing all {@link TK.ResponseHandle} graphics.
         *   Has class <code>toolkit-response-handles</code>.
         */
        TK.add_class(this._handles, "toolkit-response-handles");
    },
});
})(this, this.TK);
