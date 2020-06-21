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

function add_meters (cnt, options) {
    for (var i = 0; i < cnt; i++)
        this.add_meter(options);
}
function add_meter (options) {
    var l = this.meters.length;
    var O = options;
    var opt = extract_child_options(O, l);
    var m = new TK.LevelMeter(opt);

    this.meters.push(m);
    this.append_child(m);
}
function remove_meter (meter) {
    /* meter can be int or meter instance */
    var I = this.invalid;
    var M = this.meters;
    
    var m = -1;
    if (typeof meter == "number") {
        m = meter;
    } else  {
        for (var i = 0; i < M.length; i++) {
            if (M[i] == meter) {
                m = i;
                break;
            }
        }
    }
    if (m < 0 || m > M.length - 1) return;
    this.remove_child(M[m]);
    M[m].set("container", null);
    // TODO: no destroy function in levelmeter at this point?
    //this.meters[m].destroy();
    M = M.splice(m, 1);
}
    
    
TK.MultiMeter = TK.class({
    /**
     * TK.MultiMeter is a collection of {@link TK.LevelMeter}s to show levels of channels
     * containing multiple audio streams. It offers all options of {@link TK.LevelMeter} and
     * {@link TK.MeterBase} which are passed to all instantiated level meters.
     *
     * @class TK.MultiMeter
     * 
     * @extends TK.Container
     * 
     * @param {Object} [options={ }] - An object containing initial options.
     * 
     * @property {Number} [options.count=2] - The amount of level meters.
     * @property {String} [options.title=""] - The title of the multi meter. Set to `false` to hide the title from the DOM.
     * @property {Array<String>} [options.titles=["L", "R"]] - An Array containing titles for the level meters. Their order is the same as the meters.
     * @property {Array<Number>} [options.values=[]] - An Array containing values for the level meters. Their order is the same as the meters.
     * @property {Array<Number>} [options.labels=[]] - An Array containing label values for the level meters. Their order is the same as the meters.
     * @property {Array<Boolean>} [options.clips=[]] - An Array containing clippings for the level meters. Their order is the same as the meters.
     * @property {Array<Number>} [options.peaks=[]] - An Array containing peak values for the level meters. Their order is the same as the meters.
     * @property {Array<Number>} [options.tops=[]] - An Array containing values for top for the level meters. Their order is the same as the meters.
     * @property {Array<Number>} [options.bottoms=[]] - An Array containing values for bottom for the level meters. Their order is the same as the meters.
     */
    _class: "MultiMeter",
    Extends: TK.Container,
    
    /* TODO: The following sucks cause we need to maintain it according to
    LevelMeters and MeterBases options. */
    _options: Object.assign(Object.create(TK.Container.prototype._options), {
        count: "int",
        title: "boolean|string",
        titles: "array",
        layout: "string",
        show_scale: "boolean",
    }),
    options: {
        count: 2,
        title: false,
        titles: ["L", "R"],
        layout: "left",
        show_scale: true,
    },
    initialize: function (options) {
        TK.Container.prototype.initialize.call(this, options, true);
        /**
         * @member {HTMLDivElement} TK.MultiMeter#element - The main DIV container.
         *   Has class <code>toolkit-multi-meter</code>.
         */
        TK.add_class(this.element, "toolkit-multi-meter");
        this.meters = [];
        var O = this.options;
    },
    
    redraw: function () {
        var O = this.options;
        var I = this.invalid;
        var E = this.element;
        var M = this.meters;
        
        if (I.count) {
            while (M.length > O.count)
                remove_meter.call(this, M[M.length-1]);
            while (M.length < O.count)
                add_meter.call(this, O);
            E.setAttribute("class", E.getAttribute("class").replace(/toolkit-count-[0-9]*/g, ""));
            E.setAttribute("class", E.getAttribute("class").replace(/ +/g, " "));
            TK.add_class(E, "toolkit-count-" + O.count);
        }
        
        if (I.layout || I.count) {
            I.count = I.layout = false;
            TK.remove_class(E, "toolkit-vertical", "toolkit-horizontal", "toolkit-left",
                            "toolkit-right", "toolkit-top", "toolkit-bottom");
            switch (O.layout) {
                case "left":
                    TK.add_class(E, "toolkit-vertical", "toolkit-left");
                    break;
                case "right":
                    TK.add_class(E, "toolkit-vertical", "toolkit-right");
                    break;
                case "top":
                    TK.add_class(E, "toolkit-horizontal", "toolkit-top");
                    break;
                case "bottom":
                    TK.add_class(E, "toolkit-horizontal", "toolkit-bottom");
                    break;
                default:
                    throw("unsupported layout");
            }
            switch (O.layout) {
                case "top":
                case "left":
                    for (var i = 0; i < M.length - 1; i++)
                        M[i].set("show_scale", false);
                    if (M.length)
                        M[this.meters.length - 1].set("show_scale", O.show_scale);
                    break;
                case "bottom":
                case "right":
                    for (var i = 0; i < M.length; i++)
                        M[i].set("show_scale", false);
                    if (M.length)
                        M[0].set("show_scale", O.show_scale);
                    break;
            }
        }
        
        TK.Container.prototype.redraw.call(this);
    },
});

/**
 * @member {HTMLDivElement} TK.MultiMeter#title - The {@link TK.Label} widget displaying the meters title.
 */
TK.ChildWidget(TK.MultiMeter, "title", {
    create: TK.Label,
    show: false,
    option: "title",
    default_options: { "class" : "toolkit-title" },
    map_options: { "title" : "label" },
    toggle_class: true,
});



/*
 * This could be moved into TK.ChildWidgets(),
 * which could in similar ways be used in the buttonarray,
 * pager, etc.
 *
 */
 
var mapped_options = {
    titles: "title",
    layout: "layout",
};

function map_child_option_simple(value, key) {
    var M = this.meters, i;
    for (i = 0; i < M.length; i++) M[i].set(key, value);
}

function map_child_option(value, key) {
    var M = this.meters, i;
    if (Array.isArray(value)) {
        for (i = 0; i < M.length && i < value.length; i++) M[i].set(key, value[i]);
    } else {
        for (i = 0; i < M.length; i++) M[i].set(key, value);
    }
}

TK.add_static_event(TK.MultiMeter, "set_titles", function(value, key) {
    map_child_option.call(this, value, "title");
});

for (var key in TK.object_sub(TK.LevelMeter.prototype._options, TK.Container.prototype._options)) {
    if (TK.MultiMeter.prototype._options[key]) continue;
    var type = TK.LevelMeter.prototype._options[key];
    if (type.search("array") !== -1) {
        TK.MultiMeter.prototype._options[key] = type;
        mapped_options[key] = key;
        TK.add_static_event(TK.MultiMeter, "set_"+key, map_child_option_simple);
    } else {
        TK.MultiMeter.prototype._options[key] = "array|"+type;
        mapped_options[key] = key;
        TK.add_static_event(TK.MultiMeter, "set_"+key, map_child_option);
    }
    if (key in TK.LevelMeter.prototype.options)
        TK.MultiMeter.prototype.options[key] = TK.LevelMeter.prototype.options[key];
}

function extract_child_options(O, i) {
    var o = {}, value, type;

    for (var key in mapped_options) {
        var ckey = mapped_options[key];
        if (!O.hasOwnProperty(key)) continue;
        value = O[key];
        type = TK.LevelMeter.prototype._options[key] || "";
        if (Array.isArray(value) && type.search("array") === -1) {
            if (i < value.length) o[ckey] = value[i];
        } else {
            o[ckey] = value;
        }
    }

    return o;
}
})(this, this.TK);
