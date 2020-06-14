/*
 * Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

import ArdourClient from '/shared/ardour.js';

import { Switch, DiscreteSlider, ContinuousSlider, LogarithmicSlider,
        StripPanSlider, StripGainSlider, StripMeter } from './widget.js';

(() => {

    const MAX_LOG_LINES = 1000;
    
    const ardour = new ArdourClient();

    async function main () {
        ardour.on('connected', (connected) => {
            if (connected) {
                log('Client connected', 'info');
            } else {
                log('Client disconnected', 'error');
            }
        });

        ardour.on('message', (msg, inbound) => {
            if (inbound) {
                log(`↙ ${msg}`, 'message-in');
            } else {
                log(`↗ ${msg}`, 'message-out');
            }
        });

        ardour.mixer.on('ready', () => {
            const div = document.getElementById('strips');
            for (const strip of ardour.mixer.strips) {
                createStrip(strip, div);
            }
        });

        await ardour.connect();

        const manifest = await ardour.getSurfaceManifest();
        document.getElementById('manifest').innerHTML = manifest.name.toUpperCase()
                        + ' v' + manifest.version + ' — ' + manifest.description;
    }

    function createStrip (strip, parentDiv) {
        const domId = `strip-${strip.addrId}`;
        if (document.getElementById(domId) != null) {
            return;
        }

        const div = createElem(`<div class="strip" id="${domId}"></div>`, parentDiv);
        createElem(`<label class="comp-name" for="${domId}">∿&emsp;&emsp;${strip.name}</label>`, div);
        
        // meter
        const meter = new StripMeter();
        meter.el.classList.add('slider-meter');
        meter.appendTo(div);
        bind(strip, 'meter', meter);

        // gain
        let holder = createElem(`<div class="strip-slider"></div>`, div); 
        createElem(`<label>Gain</label>`, holder);
        const gain = new StripGainSlider();
        gain.appendTo(holder);
        bind(strip, 'gain', gain);

        if (!strip.isVca) {
            // pan
            holder = createElem(`<div class="strip-slider"></div>`, div); 
            createElem(`<label>Pan</label>`, holder);
            const pan = new StripPanSlider();
            pan.appendTo(holder);
            bind(strip, 'pan', pan);
        }

        for (const plugin of strip.plugins) {
            createStripPlugin(plugin, div);
        }
    }

    function createStripPlugin (plugin, parentDiv) {
        const domId = `plugin-${plugin.addrId}`;
        if (document.getElementById(domId) != null) {
            return;
        }

        const div = createElem(`<div class="plugin" id="${domId}"></div>`, parentDiv);
        createElem(`<label class="comp-name">⨍&emsp;&emsp;${name}</label>`, div);
        
        const enable = new Switch();
        enable.el.classList.add('plugin-enable');
        enable.appendTo(div);
        bind(plugin, 'enable', enable);

        for (const param of plugin.parameters) {
            createStripPluginParam(param, div);
        }
    }

    function createStripPluginParam (param, parentDiv) {
        const domId = `param-${param.addrId}`;
        if (document.getElementById(domId) != null) {
            return;
        }

        let widget, cssClass;

        if (param.valueType.isBoolean) {
            cssClass = 'boolean';
            widget = new Switch();
        } else if (param.valueType.isInteger) {
            cssClass = 'discrete';
            widget = new DiscreteSlider(param.min, param.max);
        } else if (param.valueType.isDouble) {
            cssClass = 'continuous';
            if (param.isLog) {
                widget = new LogarithmicSlider(param.min, param.max);
            } else {
                widget = new ContinuousSlider(param.min, param.max);
            }
        }

        const div = createElem(`<div class="plugin-param ${cssClass}" id="${domId}"></div>`, parentDiv);
        createElem(`<label for="${domId}">${param.name}</label>`, div);

        widget.el.name = domId;
        widget.appendTo(div);
        bind(param, 'value', widget);
    }

    function createElem (html, parent) {
        const t = document.createElement('template');
        t.innerHTML = html;
        
        const elem = t.content.firstChild;
        
        if (parent) {
            parent.appendChild(elem);
        }

        return elem;
    }

    function bind (component, property, widget) {
        // ardour → ui
        widget.value = component[property];
        component.on(property, (value) => widget.value = value);
        // ui → ardour
        widget.callback = (value) => component[property] = value;
    }

    function log (message, className) {
        const output = document.getElementById('log');

        if (output.childElementCount > MAX_LOG_LINES) {
            output.removeChild(output.childNodes[0]);
        }

        const pre = document.createElement('pre');
        pre.innerHTML = message;
        pre.className = className;

        output.appendChild(pre);
        output.scrollTop = output.scrollHeight;
    }

    main();

})();
