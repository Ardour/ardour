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

 // This example does not call the API methods in control.js,
 // instead it couples the widgets directly to the message stream

import { ANode, Message } from '/shared/message.js';
import { ArdourClient } from '/shared/ardour.js';

import { Switch, DiscreteSlider, ContinuousSlider, LogarithmicSlider,
        StripPanSlider, StripGainSlider, StripMeter } from './widget.js';

(() => {

    const MAX_LOG_LINES = 1000;
    
    const ardour = new ArdourClient(location.host);
    const widgets = {};

    main();

    function main () {
        ardour.getSurfaceManifest().then((manifest) => {
            const div = document.getElementById('manifest');
            div.innerHTML = `${manifest.name.toUpperCase()} v${manifest.version} — ${manifest.description}`;
        });

        ardour.addCallbacks({
            onConnected: (error) => { log('Client connected', 'info'); },
            onDisconnected: (error) => { log('Client disconnected', 'error'); },
            onMessage: processMessage,
            onStripDescription: createStrip,
            onStripPluginDescription: createStripPlugin,
            onStripPluginParamDescription: createStripPluginParam
        });
        
        ardour.connect();
    }

    function createStrip (stripId, name, isVca) {
        const domId = `strip-${stripId}`;
        if (document.getElementById(domId) != null) {
            return;
        }

        const strips = document.getElementById('strips');
        const div = createElem(`<div class="strip" id="${domId}"></div>`, strips);
        createElem(`<label class="comp-name" for="${domId}">∿&emsp;&emsp;${name}</label>`, div);
        
        // meter
        const meter = new StripMeter();
        meter.el.classList.add('slider-meter');
        meter.appendTo(div);
        connectWidget(meter, ANode.STRIP_METER, stripId);

        // gain
        let holder = createElem(`<div class="strip-slider"></div>`, div); 
        createElem(`<label>Gain</label>`, holder);
        const gain = new StripGainSlider();
        gain.appendTo(holder);
        connectWidget(gain, ANode.STRIP_GAIN, stripId);

        if (!isVca) {
            // pan
            holder = createElem(`<div class="strip-slider"></div>`, div); 
            createElem(`<label>Pan</label>`, holder);
            const pan = new StripPanSlider();
            pan.appendTo(holder);
            connectWidget(pan, ANode.STRIP_PAN, stripId);
        }
    }

    function createStripPlugin (stripId, pluginId, name) {
        const domId = `plugin-${stripId}-${pluginId}`;
        if (document.getElementById(domId) != null) {
            return;
        }

        const strip = document.getElementById(`strip-${stripId}`);
        const div = createElem(`<div class="plugin" id="${domId}"></div>`, strip);
        createElem(`<label class="comp-name">⨍&emsp;&emsp;${name}</label>`, div);
        
        const enable = new Switch();
        enable.el.classList.add('plugin-enable');
        enable.appendTo(div);
        connectWidget(enable, ANode.STRIP_PLUGIN_ENABLE, stripId, pluginId);
    }

    function createStripPluginParam (stripId, pluginId, paramId, name, valueType, min, max, isLog) {
        const domId = `param-${stripId}-${pluginId}-${paramId}`;
        if (document.getElementById(domId) != null) {
            return;
        }

        let param, cssClass;

        if (valueType == 'b') {
            cssClass = 'boolean';
            param = new Switch();
        } else if (valueType == 'i') {
            cssClass = 'discrete';
            param = new DiscreteSlider(min, max);
        } else if (valueType == 'd') {
            cssClass = 'continuous';
            if (isLog) {
                param = new LogarithmicSlider(min, max);
            } else {
                param = new ContinuousSlider(min, max);
            }
        }

        const plugin = document.getElementById(`plugin-${stripId}-${pluginId}`);
        const div = createElem(`<div class="plugin-param ${cssClass}" id="${domId}"></div>`, plugin);
        createElem(`<label for="${domId}">${name}</label>`, div);

        param.el.name = domId;
        param.appendTo(div);
        connectWidget(param, ANode.STRIP_PLUGIN_PARAM_VALUE, stripId, pluginId, paramId);
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

    function connectWidget (widget, node, ...addr) {
        const nodeAddrId = Message.nodeAddrId(node, addr);

        widgets[nodeAddrId] = widget;

        widget.callback = (val) => {
            const msg = new Message(node, addr, [val]);
            log(`↗ ${msg}`, 'message-out');
            ardour.send(msg);
        };
    }

    function processMessage (msg) {
        log(`↙ ${msg}`, 'message-in');

        if (widgets[msg.nodeAddrId]) {
            widgets[msg.nodeAddrId].value = msg.val[0];
        }
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

})();
