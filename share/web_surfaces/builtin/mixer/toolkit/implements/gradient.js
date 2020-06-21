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
TK.Gradient = TK.class({
    /**
     * TK.Gradient provides a function to set the background of a
     * DOM element to a CSS gradient according on the users browser and version.
     * TK.Gradient needs a {@link TK.Range} to be implemented on.
     *
     * @mixin TK.Gradient
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Object|Boolean} options.gradient - Gradient definition for the background.
     *   Keys are ints or floats as string corresponding to the widgets scale.
     *   Values are valid css color strings like <code>#ff8000</code> or <code>rgb(0,56,103)</code>.
     *   If set to false the css style color is used.
     * @property {String|Boolean} [options.background="#000000"] - Background color if no gradient is used.
     *   Values are valid css color strings like <code>#ff8000</code> or <code>rgb(0,56,103)</code>.
     *   If set to false the css style color is used.
     */
    _class: "Gradient",
    Implements: TK.Ranged,
    _options: Object.assign(TK.Ranged.prototype._options, {
        gradient: "mixed",
        background: "mixed",
    }),
    options: {
        gradient:        false,
        background:      false
    },
    draw_gradient: function (element, gradient, fallback, range) {
        /**
         * This function generates a string from a given
         * gradient object to set as a CSS background for a DOM element.
         * If element is given, the function automatically sets the
         * background. If gradient is omitted, the gradient is taken from
         * options. Fallback is used if no gradient can be created.
         * If fallback is omitted, <code>options.background</code> is used. if no range
         * is set Gradient assumes that the implementing instance has
         * {@link TK.Range} functionality.
         *
         * @method TK.Gradient#draw_gradient
         * 
         * @param {DOMNode} element - The DOM node to apply the gradient to.
         * @param {Object} [gradient=this.options.gradient] - Gradient definition for the background, e.g. <code>{"-96": "rgb(30,87,153)", "-0.001": "rgb(41,137,216)", "0": "rgb(32,124,202)", "24": "rgb(125,185,232)"}</code>.
         * @param {string} [fallback=this.options.background] - If no gradient can be applied, use this as background color string.
         * @param {TK.Range} [range=this] - If a specific range is set, it is used for the calculations. If not, we expect the widget itself provides {@link TK.Ranged} functionality.
         * 
         * @returns {string} A string to be used as background CSS.
         *
         * @mixes TK.Ranged
         * 
         * @emits TK.Gradient#backgroundchanged
         */
        
        //  {"-96": "rgb(30,87,153)", "-0.001": "rgb(41,137,216)", "0": "rgb(32,124,202)", "24": "rgb(125,185,232)"}
        // becomes:
//         background: url("data:image/svg+xml;utf8,<svg xmlns='http://www.w3.org/2000/svg' width='10' height='10'><linearGradient id='gradient'><stop offset='10%' stop-color='#F00'/><stop offset='90%' stop-color='#fcc'/> </linearGradient><rect fill='url(#gradient)' x='0' y='0' width='100%' height='100%'/></svg>");
//         background: -moz-linear-gradient(top, rgb(30,87,153) 0%, rgb(41,137,216) 50%, rgb(32,124,202) 51%, rgb(125,185,232) 100%);
//         background: -webkit-gradient(linear, left top, left bottom, color-stop(0%,rgb(30,87,153)), color-stop(50%,rgb(41,137,216)), color-stop(51%,rgb(32,124,202)), color-stop(100%,rgb(125,185,232)));
//         background: -webkit-linear-gradient(top, rgb(30,87,153) 0%,rgb(41,137,216) 50%,rgb(32,124,202) 51%,rgb(125,185,232) 100%);
//         background: -o-linear-gradient(top, rgb(30,87,153) 0%,rgb(41,137,216) 50%,rgb(32,124,202) 51%,rgb(125,185,232) 100%);
//         background: -ms-linear-gradient(top, rgb(30,87,153) 0%,rgb(41,137,216) 50%,rgb(32,124,202) 51%,rgb(125,185,232) 100%);
//         background: linear-gradient(to bottom, rgb(30,87,153) 0%,rgb(41,137,216) 50%,rgb(32,124,202) 51%,rgb(125,185,232) 100%);
//         filter: progid:DXImageTransform.Microsoft.gradient( startColorstr='#1e5799', endColorstr='#7db9e8',GradientType=0 );
        
        var bg = "";
        range = range || this;
        if (!gradient && !this.options.gradient)
            bg = fallback || this.options.background;
        else {
            gradient = gradient || this.options.gradient;
            
            var ms_first   = "";
            var ms_last    = "";
            var m_svg      = "";
            var m_regular  = "";
            var m_webkit   = "";
            var s_ms       = "background filter: progid:DXImageTransform."
                           + "Microsoft.gradient( startColorstr='%s', "
                           + "endColorstr='%s',GradientType='%d' )";
            var s_svg      = "url(\"data:image/svg+xml;utf8,<svg "
                           + "xmlns='http://www.w3.org/2000/svg' width='100%%' "
                           + "height='100%%'>"
                           + "<linearGradient id='%s_gradient' %s>%s"
                           + "</linearGradient><rect fill='url(#%s_gradient)' "
                           + "x='0' y='0' width='100%%' "
                           + "height='100%%'/></svg>\")";
            var s_regular  = "%slinear-gradient(%s, %s)";
            var s_webkit   = "-webkit-gradient(linear, %s, %s)";
            var c_svg      = "<stop offset='%s%%' stop-color='%s'/>";
            var c_regular  = "%s %s%%, ";
            var c_webkit   = "color-stop(%s%%, %s), ";
            
            var d_w3c = {};
            d_w3c["s"+"left"]       = "to top";
            d_w3c["s"+"right"]      = "to top";
            d_w3c["s"+"top"]        = "to right";
            d_w3c["s"+"bottom"]     = "to right";
            
            var d_regular = {};
            d_regular["s"+"left"]   = "bottom";
            d_regular["s"+"right"]  = "bottom";
            d_regular["s"+"top"]    = "left";
            d_regular["s"+"bottom"] = "left";
            
            var d_webkit = {};
            d_webkit["s"+"left"]    = "left bottom, left top";
            d_webkit["s"+"right"]   = "left bottom, left top";
            d_webkit["s"+"top"]     = "left top, right top";
            d_webkit["s"+"bottom"]  = "left top, right top";
            
            var d_ms = {};
            d_ms["s"+"left"]     = 'x1="0%" y1="0%" x2="0%" y2="100%"';
            d_ms["s"+"right"]    = 'x1="0%" y1="0%" x2="0%" y2="100%"';
            d_ms["s"+"top"]      = 'x1="100%" y1="0%" x2="0%" y2="0%"';
            d_ms["s"+"bottom"]   = 'x1="100%" y1="0%" x2="0%" y2="0%"';
            
            var keys = Object.keys(gradient);
            for (var i = 0; i < keys.length; i++) {
                keys[i] = parseFloat(keys[i]);
            }
            keys = keys.sort(this.options.reverse ?
                function (a,b) { return b-a } : function (a,b) { return a-b });
            
            for (var i = 0; i < keys.length; i++) {
                var ps = (100*range.val2coef(range.snap(keys[i]))).toFixed(2);
                if (!ms_first) ms_first = gradient[i];
                ms_last = gradient[keys[i] + ""];
                
                m_svg     += TK.sprintf(c_svg, ps, gradient[keys[i] + ""]);
                m_regular += TK.sprintf(c_regular, gradient[keys[i] + ""], ps);
                m_webkit  += TK.sprintf(c_webkit, ps, gradient[keys[i] + ""]);
            }
            m_regular = m_regular.substr(0, m_regular.length -2);
            m_webkit  = m_regular.substr(0, m_webkit.length -2);
            
            if (TK.browser.name === "IE" && TK.browser.version <= 8)
                    bg = (TK.sprintf(s_ms, ms_last, ms_first, this._vert() ? 0:1));
                
            else if (TK.browser.name === "IE" && TK.browser.version === 9)
                bg = (TK.sprintf(s_svg, this.options.id,
                      d_ms["s"+this.options.layout],
                      m_svg, this.options.id));
            
            else if (TK.browser.name === "IE" && TK.browser.version >= 10)
                bg = (TK.sprintf(s_regular, "-ms-",
                      d_regular["s" + this.options.layout],
                      m_regular));
            
            else if (TK.browser.name=="Firefox")
                bg = (TK.sprintf(s_regular, "-moz-",
                      d_regular["s"+this.options.layout],
                      m_regular));
            
            else if (TK.browser.name === "Opera" && TK.browser.version >= 11)
                bg = (TK.sprintf(s_regular, "-o-",
                      d_regular["s"+this.options.layout],
                      m_regular));
            
            else if (TK.browser.name === "Chrome" && TK.browser.version < 10
                  || TK.browser.name === "Safari" && TK.browser.version < 5.1)
                bg = (TK.sprintf(s_webkit,
                      d_webkit["s"+this.options.layout],
                      m_regular));
            
            else if (TK.browser.name === "Chrome" || TK.browser.name === "Safari")
                bg = (TK.sprintf(s_regular, "-webkit-",
                      d_regular["s"+this.options.layout],
                      m_regular));
            
            else
                bg = (TK.sprintf(s_regular, "",
                      d_w3c["s"+this.options.layout],
                      m_regular));
        }
        
        if (element) {
            element.style.background = bg ? bg : void 0;
            /**
             * Is fired when the gradient was created.
             *
             * @event TK.Gradient#backgroundchanged
             * 
             * @param {HTMLElement} element - The element which background has changed.
             * @param {string} background - The background of the element as CSS color string.
             */
            this.fire_event("backgroundchanged", element, bg);
        }
        return bg;
    }
});
})(this, this.TK);
