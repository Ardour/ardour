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

 // This example does not call the API methods in ardour.js,
 // instead it interacts at a lower level by coupling the widgets
 // tightly to the message stream

import { ANode, Message } from '/shared/message.js';
import { Ardour } from '/shared/ardour.js';

import { Switch, DiscreteSlider, ContinuousSlider, LogarithmicSlider,
        StripPanSlider, StripGainSlider, StripMeter } from './widget.js';

(() => {

    const MAX_LOG_LINES = 1000;
    const FEEDBACK_NODES = [ANode.STRIP_GAIN, ANode.STRIP_PAN, ANode.STRIP_METER,
                            ANode.STRIP_PLUGIN_ENABLE, ANode.STRIP_PLUGIN_PARAM_VALUE];
    
    const ardour = new Ardour(location.host);
    const widgets = {};

    main();

    function main () {
        ardour.getSurfaceManifest().then((manifest) => {
            const div = document.getElementById('manifest');
            div.innerHTML = `${manifest.name} v${manifest.version}`;
        });

        ardour.addCallback({
            onMessage: (msg) => {
                log(`↙ ${msg}`, 'message-in');

                if (msg.node == ANode.STRIP_DESC) {
                    createStrip (msg.addr, ...msg.val);
                } else if (msg.node == ANode.STRIP_PLUGIN_DESC) {
                    createStripPlugin (msg.addr, ...msg.val);
                } else if (msg.node == ANode.STRIP_PLUGIN_PARAM_DESC) {
                    createStripPluginParam (msg.addr, ...msg.val);
                } else if (FEEDBACK_NODES.includes(msg.node)) {
                    if (widgets[msg.hash]) {
                        widgets[msg.hash].value = msg.val[0];
                    }
                }
            },

            onError: () => {
                log('Client error', 'error');
            }
        });
        
        ardour.open();
    }

    function createStrip (addr, name) {
        const id = `strip-${addr[0]}`;
        const strips = document.getElementById('strips');
        const div = createElem(`<div class="strip" id="${id}"></div>`, strips);
        createElem(`<label class="comp-name" for="${id}">∿&emsp;&emsp;${name}</label>`, div);
        
        // meter
        const meter = new StripMeter('strip_meter', addr);
        meter.el.classList.add('slider-meter');
        meter.attach(div);
        register(meter);

        // gain
        let holder = createElem(`<div class="strip-slider"></div>`, div); 
        createElem(`<label>Gain</label>`, holder);
        const gain = new StripGainSlider('strip_gain', addr);
        gain.attach(holder, (val) => send(gain));
        register(gain);

        // pan
        holder = createElem(`<div class="strip-slider"></div>`, div); 
        createElem(`<label>Pan</label>`, holder);
        const pan = new StripPanSlider('strip_pan', addr);
        pan.attach(holder, (val) => send(pan));
        register(pan);
    }

    function createStripPlugin (addr, name) {
        const strip = document.getElementById(`strip-${addr[0]}`);
        const id = `plugin-${addr[0]}-${addr[1]}`;
        const div = createElem(`<div class="plugin" id="${id}"></div>`, strip);
        createElem(`<label class="comp-name">⨍&emsp;&emsp;${name}</label>`, div);
        const enable = new Switch('strip_plugin_enable', addr);
        enable.el.classList.add('plugin-enable');
        enable.attach(div, (val) => send(enable));
        register(enable);
    }

    function createStripPluginParam (addr, name, data_type, min, max, is_log) {
        let param, clazz;

        if (data_type == 'b') {
            clazz = 'boolean';
            param = new Switch('strip_plugin_param_value', addr);
        } else if (data_type == 'i') {
            clazz = 'discrete';
            param = new DiscreteSlider('strip_plugin_param_value', addr, min, max);
        } else if (data_type == 'd') {
            clazz = 'continuous';
            if (is_log) {
                param = new LogarithmicSlider('strip_plugin_param_value', addr, min, max);
            } else {
                param = new ContinuousSlider('strip_plugin_param_value', addr, min, max);
            }
        }

        const plugin = document.getElementById(`plugin-${addr[0]}-${addr[1]}`);
        const id = `param-${addr[0]}-${addr[1]}-${addr[2]}`;
        const div = createElem(`<div class="plugin-param ${clazz}" id="${id}"></div>`, plugin);
        createElem(`<label for="${id}">${name}</label>`, div);

        param.attach(div, (val) => send(param));
        param.el.name = id;
        register(param);
    }

    function send (widget) {
        const msg = new Message(widget.node, widget.addr, [widget.value]);
        log(`↗ ${msg}`, 'message-out');
        ardour.send(msg);
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

    function register (widget) {
        widgets[widget.hash] = widget;
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
