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

/* helpers */

var pad = function (n, width, z) {
    z = z || '0';
    n = n + '';
    return n.length >= width ? n : new Array(width - n.length + 1).join(z) + n;
}

var decode_args = function () {
    /* decode random arguments. Expects an arguments array
     * from another function call as first argument and a
     * series of member names the arguments should be decoded
     * to. E.g. decode_args(arguments, "r", "g", "b")
     * Arguments array can consist of:
         * A single array: member names are mapped to the array
         * ([[50,100,150]],"r","g","b") => {"r":50,"g":100,"b":150}
         * 
         * An object already containing the members: object is returned
         * ([{"r":50,"g":100,"b":150}, ...) => {"r":50,"g":100,"b":150}
         * 
         * Multiple values: values are mapped to member names
         * ([50,100,150],"r","g","b") => {"r":50,"g":100,"b":150}
     */
    var out = {};
    if (arguments[0][0] instanceof Array) {
        for (var i = 0; i < arguments.length - 1; i++)
            out[arguments[i+1]] = arguments[0][0][i];
    } else if (typeof(arguments[0][0]) === "object") {
        out = arguments[0][0];
    } else {
        for (var i = 0; i < arguments.length - 1; i++)
            out[arguments[i+1]] = arguments[0][i];
    }
    return out;
}

var decode_color = function (args) {
    /* detects type of input and disassembles it to a useful object.
     * Only argument is an arguments array from another function.
         * (["lightcoral"]) => {"type":"string","hex":"#F08080","string":"lightcoral","r":240,"g":128,"b":128}
         * (["#F08080"]) => {"type":"hex","hex":"#F08080","r":240,"g":128,"b":128}
         * ([[0,0.31,0.28]] => {"type":"hsl","h":0,"s":0.31,"l":0.28}
         * ([240,128,128] => {"type":"rgb","r":240,"g":128,"b":128}
         * ([{"r":240,"g":128,"b":128}] => {"type":"rgb","r":240,"g":128,"b":128}
     */
    if (typeof args[0] === "string" && args[0][0] === "#") {
        // HEX string
        var res = hex2rgb(args[0]);
        res["type"] = "hex";
        res["hex"] = args[0];
        return res;
    }
    if (typeof args[0] === "string" && color_names[args[0]]) {
        // color string
        var res = hex2rgb("#" + color_names[args[0]]);
        res["type"] = "string";
        res["string"] = args[0];
        res["hex"] = color_names[args[0]];
        return res;
    }
    var S = decode_args(arguments, "a", "b", "c");
    if (S.a > 0 && S.a < 1 || S.b > 0 && S.b < 1 || S.c > 0 && S.c < 1) {
        // HSL
        return { "h": S.a, "s": S.b, "l": S.c, "type": "hsl" };
    }
    // RGB
    return { "r": S.a, "g": S.b, "b": S.c, "type": "rgb" };
}


/* RGB */

var rgb2hex = function () {
    var col = decode_args(arguments, "r", "g", "b");
    return pad(parseInt(col.r).toString(16),2) +
           pad(parseInt(col.g).toString(16),2) +
           pad(parseInt(col.b).toString(16),2);
}
var rgb2hsl = function () {
    var col = decode_args(arguments, "r", "g", "b");
    var r = col.r, g = col.g, b = col.b;
    
    r /= 255, g /= 255, b /= 255;

    var max = Math.max(r, g, b), min = Math.min(r, g, b);
    var h, s, l = (max + min) / 2;

    if (max == min) {
        h = s = 0; // achromatic
    } else {
        var d = max - min;
        s = l > 0.5 ? d / (2 - max - min) : d / (max + min);

        switch (max) {
            case r: h = (g - b) / d + (g < b ? 6 : 0); break;
            case g: h = (b - r) / d + 2; break;
            case b: h = (r - g) / d + 4; break;
        }
    
        h /= 6;
    }
    return { h:Math.min(1, Math.max(0, h)),
             s:Math.min(1, Math.max(0, s)),
             l:Math.min(1, Math.max(0, l)) };
}
var rgb2bw = function () {
    return rgb2gray.apply(null, arguments) >= 0.5 ? "#000000" : "#ffffff";
}
var rgb2wb = function () {
    return rgb2gray.apply(null, arguments) < 0.5 ? "#000000" : "#ffffff";
}
var rgb2gray = function () {
    var col = decode_args(arguments, "r", "g", "b");
    return (col.r * 0.2126 + col.g * 0.7152 + col.b * 0.0722) / 255;
}


/* HSL */

function hue2rgb(p, q, t) {
    if (t < 0) t += 1;
    if (t > 1) t -= 1;
    if (t < 1/6) return p + (q - p) * 6 * t;
    if (t < 1/2) return q;
    if (t < 2/3) return p + (q - p) * (2/3 - t) * 6;
    return Math.min(1, Math.max(0, p));
}
var hsl2rgb = function () {
    var col = decode_args(arguments, "h", "s", "l");
    var h = col.h, s = col.s, l = col.l;
    
    var r, g, b;

    if (s == 0) {
        r = g = b = l; // achromatic
    } else {

        var q = l < 0.5 ? l * (1 + s) : l + s - l * s;
        var p = 2 * l - q;
    
        r = hue2rgb(p, q, h + 1/3);
        g = hue2rgb(p, q, h);
        b = hue2rgb(p, q, h - 1/3);
    }
    return { r:r*255, g:g*255, b:b*255 };
}
var hsl2hex = function () {
    return rgb2hex(hsl2rgb.apply(null, arguments));
}
var hsl2bw = function () {
    return rgb2bw(hsl2rgb.apply(null, arguments));
}
var hsl2wb = function () {
    return rgb2wb(hsl2rgb.apply(null, arguments));
}
var hsl2gray = function () {
    return rgb2gray(hsl2rgb.apply(null, arguments));
}


/* HEX */

var hex2rgb = function (hex) {
    hex = hex || "000000";
    if (hex[0] == "#")
        hex = hex.substr(1);
    if (hex.length == 3)
        return { r: parseInt("0x"+hex[0] + hex[0]),
                 g: parseInt("0x"+hex[1] + hex[1]),
                 b: parseInt("0x"+hex[2] + hex[2]) };
    return { r: parseInt("0x"+hex.substr(0,2)),
             g: parseInt("0x"+hex.substr(2,2)),
             b: parseInt("0x"+hex.substr(4,2)) };
}
var hex2hsl = function (hex) {
    return rgb2hsl(hex2rgb(hex))
}
var hex2bw = function (hex) {
    return rgb2bw(hex2rgb(hex))
}
var hex2wb = function (hex) {
    return rgb2wb(hex2rgb(hex))
}
var hex2gray = function (hex) {
    return rgb2gray(hex2rgb(hex))
}


/* STRING */

var name2hex = function (name) {
    return color_names[name.toLowerCase];
}
var name2rgb = function (name) {
    return hex2rgb(color_names[name.toLowerCase]);
}
var name2hsl = function (name) {
    return hex2hsl(color_names[name.toLowerCase]);
}
var name2bw = function (name) {
    return hex2bw(color_names[name.toLowerCase]);
}
var name2wb = function (name) {
    return hex2wb(color_names[name.toLowerCase]);
}
var name2gray = function (name) {
    return hex2gray(color_names[name.toLowerCase]);
}


/* UNIVERSAL */

var color2rgb = function () {
    var C = decode_color(arguments);
    switch (C.type) {
        case "rgb": return C;
        case "hex": return C;
        case "hsl": return rgb2hsl(C);
        case "string": return C;
    }
}
var color2hsl = function () {
    var C = decode_color(arguments);
    switch (C.type) {
        case "rgb": return rgb2hsl(C);
        case "hex": return rgb2hsl(C);
        case "hsl": return C;
        case "string": return rgb2hsl(C);
    }
}
var color2hex = function () {
    var C = decode_color(arguments);
    switch (C.type) {
        case "rgb": return rgb2hex(C);
        case "hex": return C.hex;
        case "hsl": return hsl2hex(C);
        case "string": return rgb2hex(C);
    }
}
var color2bw = function () {
    var C = decode_color(arguments);
    switch (C.type) {
        case "rgb": return rgb2bw(C);
        case "hex": return rgb2bw(C);
        case "hsl": return hsl2bw(C);
        case "string": return rgb2bw(C);
    }
}
var color2wb = function () {
    var C = decode_color(arguments);
    switch (C.type) {
        case "rgb": return rgb2wb(C);
        case "hex": return rgb2wb(C);
        case "hsl": return hsl2wb(C);
        case "string": return rgb2wb(C);
    }
}
var color2gray = function () {
    var C = decode_color(arguments);
    switch (C.type) {
        case "rgb": return rgb2bw(C);
        case "hex": return rgb2bw(C);
        case "hsl": return hsl2bw(C);
        case "string": return rgb2bw(C);
    }
}


/**
 * TK.Colors provides a couple of functions for easy-to-use color calculations
 * and conversions. Functions requiring RGB or HSL color definitions as
 * argument (all `rgb2x` and `hsl2x`) can be called with different types of arguments
 * to make using them more convenient. Examples:
 * <ul><li>`rgb2hsl(240, 128, 128)`</li>
 * <li>rgb2hsl({'r':240,'g':128,'b':128})</li>
 * <li>rgb2hsl([240, 128, 128])</li></ul>
 * The universal functions `color2x` take even more diverse arguments.
 * The following examples all define the same color:
 * <ul><li>("lightcoral")</li>
 * <li>("#F08080")</li>
 * <li>([0,0.31,0.28])</li>
 * <li>(240,128,128)</li>
 * <li>({"r":240,"g":128,"b":128})</li></ul>
 * 
 * @mixin TK.Colors
 */

TK.Colors = TK.class({
    /**
     * Returns an object containing red ('r'), green ('g') and blue ('b')
     * from any type of valid color.
     * 
     * @method TK.Colors#color2rgb
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or hue (0..1) or object with members `r, g, b` or `h, s, l` or array of RGB or HSL or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {object} Object with members r, g and b as numbers (0..255)
     */
    color2rgb: color2rgb,
    
    /**
     * Returns an object containing hue ('h'), saturation ('s') and lightness ('l')
     * from any type of valid color.
     * 
     * @method TK.Colors#color2hsl
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or hue (0..1) or object with members `r, g, b` or `h, s, l` or array of RGB or HSL or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {object} Object with members h, s and l as numbers (0..1)
     */
    color2hsl: color2hsl,
    
    /**
     * Returns a hex color string
     * from any type of valid color.
     * 
     * @method TK.Colors#color2hex
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or hue (0..1) or object with members `r, g, b` or `h, s, l` or array of RGB or HSL or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {string} Hex color string.
     */
    color2hex: color2hex,
    
    /**
     * Returns a hex color string either black or white at highest contrast compared to the argument
     * from any type of valid color.
     * 
     * @method TK.Colors#color2bw
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or hue (0..1) or object with members `r, g, b` or `h, s, l` or array of RGB or HSL or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {string} Hex color string.
     */
    color2bw: color2bw,
    
    /**
     * Returns a hex color string either black or white at lowest contrast compared to the argument
     * from any type of valid color.
     * 
     * @method TK.Colors#color2wb
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or hue (0..1) or object with members `r, g, b` or `h, s, l` or array of RGB or HSL or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {string} Hex color string.
     */
    color2wb: color2wb,
    
    /**
     * Returns a hex color string of the grayscaled argument
     * from any type of valid color.
     * 
     * @method TK.Colors#color2gray
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or hue (0..1) or object with members `r, g, b` or `h, s, l` or array of RGB or HSL or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {string} Hex color string.
     */
    color2gray: color2gray,
    
    
    
    /**
     * Returns an object containing hue ('h'), saturation ('s') and lightness ('l')
     * from a RGB color.
     * 
     * @method TK.Colors#rgb2hsl
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or object with members `r, g, b` or array of RGB or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {object} Object with members h, s and l as numbers (0..1)
     */
    rgb2hsl: rgb2hsl,
    
    /**
     * Returns a hex color string
     * from a RGB color.
     * 
     * @method TK.Colors#rgb2hex
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or object with members `r, g, b` or array of RGB or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {string} Hex color string.
     */
    rgb2hex: rgb2hex,
    
    /**
     * Returns a hex color string either black or white at highest contrast compared to the argument
     * from a RGB color.
     * 
     * @method TK.Colors#rgb2bw
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or object with members `r, g, b` or array of RGB or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {string} Hex color string.
     */
    rgb2bw: rgb2bw,
    
    /**
     * Returns a hex color string either black or white at lowest contrast compared to the argument
     * from a RGB color.
     * 
     * @method TK.Colors#rgb2wb
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or object with members `r, g, b` or array of RGB or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {string} Hex color string.
     */
    rgb2wb: rgb2wb,
    
    /**
     * Returns a hex color string of the grayscaled argument
     * from a RGB color.
     * 
     * @method TK.Colors#rgb2gray
     * 
     * @param {number|array|object|string} 1st_value - red (0..255) or object with members `r, g, b` or array of RGB or color name or hex string.
     * @param {number} [2nd_value] - green (0..255) or saturation (0..1)
     * @param {number} [3rd_value] - blue (0..255) or lightnes (0..1)
     * 
     * @returns {string} Hex color string.
     */
    rgb2gray: rgb2gray,
    
    
    
    /**
     * Returns an object containing red ('r'), green ('g') and blue ('b')
     * from a HSL color.
     * 
     * @method TK.Colors#hsl2rgb
     * 
     * @param {number|array|object} 1st_value - hue (0..1) or object with members `h, s, l` or array of HSL.
     * @param {number} [2nd_value] - saturation (0..1)
     * @param {number} [3rd_value] - lightness (0..1)
     * 
     * @returns {object} Object with members r, g and b as numbers (0..255)
     */
    hsl2rgb: hsl2rgb,
    
    /**
     * Returns a hex color string
     * from a HSL color.
     * 
     * @method TK.Colors#hsl2hex
     * 
     * @param {number|array|object} 1st_value - hue (0..1) or object with members `h, s, l` or array of HSL.
     * @param {number} [2nd_value] - saturation (0..1)
     * @param {number} [3rd_value] - lightness (0..1)
     * 
     * @returns {string} Hex color string.
     */
    hsl2hex: hsl2hex,
    
    /**
     * Returns a hex color string either black or white at highest contrast compared to the argument
     * from a HSL color.
     * 
     * @method TK.Colors#hsl2bw
     * 
     * @param {number|array|object} 1st_value - hue (0..1) or object with members `h, s, l` or array of HSL.
     * @param {number} [2nd_value] - saturation (0..1)
     * @param {number} [3rd_value] - lightness (0..1)
     * 
     * @returns {string} Hex color string.
     */
    hsl2bw: hsl2bw,
    
    /**
     * Returns a hex color string either black or white at lowest contrast compared to the argument
     * from a HSL color.
     * 
     * @method TK.Colors#hsl2wb
     * 
     * @param {number|array|object} 1st_value - hue (0..1) or object with members `h, s, l` or array of HSL.
     * @param {number} [2nd_value] - saturation (0..1)
     * @param {number} [3rd_value] - lightness (0..1)
     * 
     * @returns {string} Hex color string.
     */
    hsl2wb: hsl2wb,
    
    /**
     * Returns a hex color string of the grayscaled argument
     * from a HSL color.
     * 
     * @method TK.Colors#hsl2gray
     * 
     * @param {number|array|object} 1st_value - hue (0..1) or object with members `h, s, l` or array of HSL.
     * @param {number} [2nd_value] - saturation (0..1)
     * @param {number} [3rd_value] - lightness (0..1)
     * 
     * @returns {string} Hex color string.
     */
    hsl2gray: hsl2gray,
    
    
    
    /**
     * Returns an object containing red ('r'), green ('g') and blue ('b')
     * from a hex color string.
     * 
     * @method TK.Colors#hex2rgb
     * 
     * @param {string} hex - Hex color string.
     * 
     * @returns {object} Object with members r, g and b as numbers (0..255)
     */
    hex2rgb: hex2rgb,
    
    /**
     * Returns an object containing hue ('h'), saturation ('s') and lightness ('l')
     * from a hex color string.
     * 
     * @method TK.Colors#hex2hsl
     * 
     * @param {string} hex - Hex color string.
     * 
     * @returns {object} Object with members h, s and l as numbers (0..1)
     */
    hex2hsl: hex2hsl,
    
    /**
     * Returns a hex color string either black or white at highest contrast compared to the argument
     * from a hex color string.
     * 
     * @method TK.Colors#hex2bw
     * 
     * @param {string} hex - Hex color string.
     * 
     * @returns {string} Hex color string.
     */
    hex2bw: hex2bw,
    
    /**
     * Returns a hex color string either black or white at lowest contrast compared to the argument
     * from a hex color string.
     * 
     * @method TK.Colors#hex2wb
     * 
     * @param {string} hex - Hex color string.
     * 
     * @returns {string} Hex color string.
     */
    hex2wb: hex2wb,
    
    /**
     * Returns a hex color string of the grayscaled argument
     * from a hex color string.
     * 
     * @method TK.Colors#hex2gray
     * 
     * @param {string} hex - Hex color string.
     * 
     * @returns {string} Hex color string.
     */
    hex2gray: hex2gray,
    
    
    
    /**
     * Returns an object containing red ('r'), green ('g') and blue ('b')
     * from a color name.
     * 
     * @method TK.Colors#name2rgb
     * 
     * @param {string} color - Color name.
     * 
     * @returns {object} Object with members r, g and b as numbers (0..255)
     */
    name2rgb: name2rgb,
    
    /**
     * Returns an object containing hue ('h'), saturation ('s') and lightness ('l')
     * from a color name.
     * 
     * @method TK.Colors#name2hsl
     * 
     * @param {string} color - Color name.
     * 
     * @returns {object} Object with members h, s and l as numbers (0..1)
     */
    name2hsl: name2hsl,
    
    /**
     * Returns a hex color string
     * from a color name.
     * 
     * @method TK.Colors#name2hex
     * 
     * @param {string} color - Color name.
     * 
     * @returns {string} Hex color string.
     */
    name2hex: name2hex,
    
    /**
     * Returns a hex color string either black or white at highest contrast compared to the argument
     * from a color name.
     * 
     * @method TK.Colors#name2bw
     * 
     * @param {string} color - Color name.
     * 
     * @returns {string} Hex color string.
     */
    name2bw: name2bw,
    
    /**
     * Returns a hex color string either black or white at lowest contrast compared to the argument
     * from a color name.
     * 
     * @method TK.Colors#name2wb
     * 
     * @param {string} color - Color name.
     * 
     * @returns {string} Hex color string.
     */
    name2wb: name2wb,
    
    /**
     * Returns a hex color string of the grayscaled argument
     * from a color name.
     * 
     * @method TK.Colors#name2gray
     * 
     * @param {string} color - Color name.
     * 
     * @returns {string} Hex color string.
     */
    name2gray: name2gray,
    
});

var color_names = {
    "lightcoral" : "f08080",
    "salmon" : "fa8072",
    "darksalmon" : "e9967a",
    "lightsalmon" : "ffa07a",
    "crimson" : "dc143c",
    "red" : "ff0000",
    "firebrick" : "b22222",
    "darkred" : "8b0000",
    "pink" : "ffc0cb",
    "lightpink" : "ffb6c1",
    "hotpink" : "ff69b4",
    "deeppink" : "ff1493",
    "mediumvioletred" : "c71585",
    "palevioletred" : "db7093",
    "coral" : "ff7f50",
    "tomato" : "ff6347",
    "orangered" : "ff4500",
    "darkorange" : "ff8c00",
    "orange" : "ffa500",
    "gold" : "ffd700",
    "yellow" : "ffff00",
    "lightyellow" : "ffffe0",
    "lemonchiffon" : "fffacd",
    "lightgoldenrodyellow" : "fafad2",
    "papayawhip" : "ffefd5",
    "moccasin" : "ffe4b5",
    "peachpuff" : "ffdab9",
    "palegoldenrod" : "eee8aa",
    "khaki" : "f0e68c",
    "darkkhaki" : "bdb76b",
    "lavender" : "e6e6fa",
    "thistle" : "d8bfd8",
    "plum" : "dda0dd",
    "violet" : "ee82ee",
    "orchid" : "da70d6",
    "fuchsia" : "ff00ff",
    "magenta" : "ff00ff",
    "mediumorchid" : "ba55d3",
    "mediumpurple" : "9370db",
    "amethyst" : "9966cc",
    "blueviolet" : "8a2be2",
    "darkviolet" : "9400d3",
    "darkorchid" : "9932cc",
    "darkmagenta" : "8b008b",
    "purple" : "800080",
    "indigo" : "4b0082",
    "slateblue" : "6a5acd",
    "darkslateblue" : "483d8b",
    "mediumslateblue" : "7b68ee",
    "greenyellow" : "adff2f",
    "chartreuse" : "7fff00",
    "lawngreen" : "7cfc00",
    "lime" : "00ff00",
    "limegreen" : "32cd32",
    "palegreen" : "98fb98",
    "lightgreen" : "90ee90",
    "mediumspringgreen" : "00fa9a",
    "springgreen" : "00ff7f",
    "mediumseagreen" : "3cb371",
    "seagreen" : "2e8b57",
    "forestgreen" : "228b22",
    "green" : "008000",
    "darkgreen" : "006400",
    "yellowgreen" : "9acd32",
    "olivedrab" : "6b8e23",
    "olive" : "808000",
    "darkolivegreen" : "556b2f",
    "mediumaquamarine" : "66cdaa",
    "darkseagreen" : "8fbc8f",
    "lightseagreen" : "20b2aa",
    "darkcyan" : "008b8b",
    "teal" : "008080",
    "aqua" : "00ffff",
    "cyan" : "00ffff",
    "lightcyan" : "e0ffff",
    "paleturquoise" : "afeeee",
    "aquamarine" : "7fffd4",
    "turquoise" : "40e0d0",
    "mediumturquoise" : "48d1cc",
    "darkturquoise" : "00ced1",
    "cadetblue" : "5f9ea0",
    "steelblue" : "4682b4",
    "lightsteelblue" : "b0c4de",
    "powderblue" : "b0e0e6",
    "lightblue" : "add8e6",
    "skyblue" : "87ceeb",
    "lightskyblue" : "87cefa",
    "deepskyblue" : "00bfff",
    "dodgerblue" : "1e90ff",
    "cornflowerblue" : "6495ed",
    "royalblue" : "4169e1",
    "blue" : "0000ff",
    "mediumblue" : "0000cd",
    "darkblue" : "00008b",
    "navy" : "000080",
    "midnightblue" : "191970",
    "cornsilk" : "fff8dc",
    "blanchedalmond" : "ffebcd",
    "bisque" : "ffe4c4",
    "navajowhite" : "ffdead",
    "wheat" : "f5deb3",
    "burlywood" : "deb887",
    "tan" : "d2b48c",
    "rosybrown" : "bc8f8f",
    "sandybrown" : "f4a460",
    "goldenrod" : "daa520",
    "darkgoldenrod" : "b8860b",
    "peru" : "cd853f",
    "chocolate" : "d2691e",
    "saddlebrown" : "8b4513",
    "sienna" : "a0522d",
    "brown" : "a52a2a",
    "maroon" : "800000",
    "white" : "ffffff",
    "snow" : "fffafa",
    "honeydew" : "f0fff0",
    "mintcream" : "f5fffa",
    "azure" : "f0ffff",
    "aliceblue" : "f0f8ff",
    "ghostwhite" : "f8f8ff",
    "whitesmoke" : "f5f5f5",
    "seashell" : "fff5ee",
    "beige" : "f5f5dc",
    "oldlace" : "fdf5e6",
    "floralwhite" : "fffaf0",
    "ivory" : "fffff0",
    "antiquewhite" : "faebd7",
    "linen" : "faf0e6",
    "lavenderblush" : "fff0f5",
    "mistyrose" : "ffe4e1",
    "gainsboro" : "dcdcdc",
    "lightgrey" : "d3d3d3",
    "silver" : "c0c0c0",
    "darkgray" : "a9a9a9",
    "gray" : "808080",
    "dimgray" : "696969",
    "lightslategray" : "778899",
    "slategray" : "708090",
    "darkslategray" : "2f4f4f",
    "black" : "000000"
}

})(this, this.TK);
