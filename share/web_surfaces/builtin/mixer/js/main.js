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
import { createRootContainer, Container, Dialog, Label, Button, DiscreteKnob,
            LinearKnob, PanKnob, StripGainFader, StripMeter, Toggle } from './tkwidget.js';

(() => {
    
    const ardour = new ArdourClient();

    async function main () {
        setupFullscreenButton();

        const root = await createRootContainer();

        ardour.mixer.on('ready', () => {
            if (root.children.length > 0) {
                root.removeChild(root.children[0]);
            }

            const mixer = new Container();
            mixer.id = 'mixer';
            mixer.appendTo(root);

            // left flexbox padding
            mixer.appendChild(new Container());

            for (const strip of ardour.mixer.strips) {
                const container = new Container();
                container.classList.add('strip');
                container.appendTo(mixer);
                createStrip(strip, container);
            }

            // right flexbox padding
            mixer.appendChild(new Container());
        });

        ardour.connect();
    }

    function createStrip (strip, container) {
        const inserts = new Button();
        inserts.text = 'ƒ';
        inserts.appendTo(container);
        inserts.classList.add('strip-inserts');
        if (strip.isVca || (strip.plugins.length == 0)) {
            inserts.classList.add('disabled');
        } else {
            inserts.callback = () => openInserts (strip.plugins);
        }

        const pan = new PanKnob();
        pan.appendTo(container);
        if (!strip.isVca) {
            pan.bindTo(strip, 'pan');
        }

        const mute = new Toggle();
        mute.text = 'Mute';
        mute.appendTo(container);
        mute.bindTo(strip, 'mute');
        mute.classList.add('strip-mute');

        const meterFader = new Container();
        meterFader.appendTo(container);
        meterFader.classList.add('strip-meter-fader');

        const gain = new StripGainFader();
        gain.appendTo(meterFader);
        gain.bindTo(strip, 'gain');

        const meter = new StripMeter();
        meter.appendTo(meterFader);
        meter.bindTo(strip, 'meter');

        const label = new Label();
        label.text = strip.name;
        label.classList.add('strip-label');
        label.appendTo(container);

        if (strip.isVca) {
            // hide inserts and pan keeping layout
            pan.element.style.visibility = 'hidden';
            inserts.element.style.visibility = 'hidden';
        }
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

    function setupFullscreenButton () {
        const doc = document.documentElement,
              button = document.getElementById('fullscreen');

        let requestFullscreen = null, fullscreenChange = null;

        if ('requestFullscreen' in doc) {
            requestFullscreen = doc.requestFullscreen.bind(doc);
            fullscreenChange = 'fullscreenchange';
        } else if ('webkitRequestFullscreen' in doc) {
            requestFullscreen = doc.webkitRequestFullscreen.bind(doc);
            fullscreenChange = 'webkitfullscreenchange';
        }

        if (requestFullscreen && fullscreenChange) {
            button.addEventListener('click', requestFullscreen);

            document.addEventListener(fullscreenChange, (e) => {
                const fullscreen = document.fullscreen || document.webkitIsFullScreen;
                button.style.display = fullscreen ? 'none' : 'inline';
            });
        } else {
            button.style.display = 'none';
        }
    }

    function openInserts (plugins) {
        const dialog = new Dialog();
        dialog.classList.add('inserts-view');
        dialog.show();

        const label = new Label();
        label.text = `WIP: This strip has ${plugins.length} plugins...`;
        label.appendTo(dialog);

        // TO DO
        /*for (const plugin of plugins) {
            createStripPlugin(plugin, container);
        }*/
    }

    main();

})();
