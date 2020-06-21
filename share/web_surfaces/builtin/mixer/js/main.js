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
import { createRootContainer, Container, DiscreteKnob, LinearKnob, PanKnob,
            StripGainFader, StripMeter, Toggle } from './tkwidget.js';

(() => {
    
    const ardour = new ArdourClient();

    async function main () {
        const root = await createRootContainer();

        ardour.mixer.on('ready', () => {
            if (root.children.length > 0) {
                root.removeChild(root.children[0]);
            }

            const mixer = new Container();
            mixer.id = 'mixer';
            mixer.appendTo(root);

            mixer.appendChild(new Container());

            for (const strip of ardour.mixer.strips) {
                const container = new Container();
                container.classList = 'strip';
                container.appendTo(mixer);
                createStrip(strip, container);
            }

            mixer.appendChild(new Container());
        });

        ardour.connect();
    }

    function createStrip (strip, container) {
        const pan = new PanKnob();
        pan.classList += 'pan';
        pan.appendTo(container);
        if (strip.isVca) {
            // hide pan, keeping layout
            pan.element.style.visibility = 'hidden';
        } else {
            pan.bindTo(strip, 'pan');
        }

        const meterFader = new Container();
        meterFader.appendTo(container);
        meterFader.classList = 'strip-meter-fader';

        const gain = new StripGainFader();
        gain.appendTo(meterFader);
        gain.bindTo(strip, 'gain');

        const meter = new StripMeter();
        meter.appendTo(meterFader);
        meter.bindTo(strip, 'meter');

        // TO DO
        /*for (const plugin of strip.plugins) {
            createStripPlugin(plugin, container);
        }*/
    }

    function createStripPlugin (plugin, container) {
        const enable = new Toggle();
        enable.appendTo(container);
        enable.bindTo(plugin, 'enable');

        for (const param of plugin.parameters) {
            createStripPluginParam(param, container);
        }
    }

    function createStripPluginParam (param, container) {
        let widget;

        if (param.valueType.isBoolean) {
            widget = new Toggle();
        } else if (param.valueType.isInteger) {
            widget = new DiscreteKnob(param.min, param.max);
        } else if (param.valueType.isDouble) {
            if (param.isLog) {
                widget = new LogKnob(param.min, param.max);
            } else {
                widget = new LinearKnob(param.min, param.max);
            }
        }

        widget.appendTo(container);
        widget.bindTo(param, 'value');
    }

    main();

})();
