/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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
import { createRootContainer, Container, Dialog, Label, Button, Toggle,
            DiscreteKnob, LinearKnob, LogKnob, PanKnob,
            AudioStripGainFader, MidiStripGainFader,
            AudioStripMeter, MidiStripMeter  } from './tkwidget.js';

{

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

    function createStrip (strip, container) {
        const plugins = new Button();
        plugins.text = 'ƒ';
        plugins.classList.add('strip-plugins');
        plugins.appendTo(container);

        if (strip.plugins.length == 0) {
            plugins.classList.add('disabled');
            plugins.element.style.visibility = 'hidden';
        } else {
            plugins.callback = () => openPlugins (strip);
        }

        if (strip.isMidi || strip.isVca) {
            plugins.element.style.visibility = 'hidden';
        }

        const pan = new PanKnob();
        pan.appendTo(container);

        if (strip.hasPan) {
            pan.bindTo(strip, 'pan');
        } else {
            pan.element.style.visibility = 'hidden';
        }

        const mute = new Toggle();
        mute.text = 'Mute';
        mute.classList.add('strip-mute');
        mute.appendTo(container);
        mute.bindTo(strip, 'mute');

        const meterFader = new Container();
        meterFader.classList.add('strip-meter-fader');
        meterFader.appendTo(container);

        const gain = strip.isMidi ? new MidiStripGainFader : new AudioStripGainFader();
        gain.appendTo(meterFader);
        gain.bindTo(strip, 'gain');

        const meter = strip.isMidi ? new MidiStripMeter() : new AudioStripMeter();
        meter.appendTo(meterFader);
        meter.bindTo(strip, 'meter');

        const label = new Label();
        label.text = strip.name;
        label.classList.add('strip-label');
        label.appendTo(container);
    }

    function openPlugins (strip) {
        const dialog = new Dialog();
        dialog.id = 'plugins-dialog';

        const close = new Button();
        close.id = 'plugins-close';
        close.icon = 'close';
        dialog.closeButton = close;

        const plugins = new Container();
        plugins.id = 'plugins';
        plugins.appendTo(dialog);

        const label = new Label();
        label.id = 'plugins-title';
        label.text = strip.name;
        label.appendTo(plugins);

        for (const plugin of strip.plugins) {
            createStripPlugin(plugin, plugins);
        }

        dialog.onClose = () => {
            // disconnect all parameters
            for (const plugin of strip.plugins) {
                for (const param of plugin.parameters) {
                    param.removeObserver();
                }
            }
        };

        dialog.show();
    }

    function createStripPlugin (plugin, dialog) {
        const enableAndName = new Container();
        enableAndName.appendTo(dialog);

        const enable = new Toggle();
        enable.setIcons('unchecked', 'checked');
        enable.appendTo(enableAndName);
        enable.bindTo(plugin, 'enable');

        const label = new Label();
        label.text = plugin.name;
        label.appendTo(enableAndName);

        const container = new Container();
        container.classList.add('plugin-parameters');
        container.appendTo(dialog);

        for (const param of plugin.parameters) {
            createStripPluginParam(param, container);
        }
    }

    function createStripPluginParam (param, container) {
        let widget;

        if (param.valueType.isBoolean) {
            widget = new Toggle();
            widget.setIcons('unchecked', 'checked');
        } else if (param.valueType.isInteger) {
            widget = new DiscreteKnob(param.min, param.max);
        } else if (param.valueType.isDouble) {
            if (param.isLog) {
                widget = new LogKnob(param.min, param.max);
            } else {
                widget = new LinearKnob(param.min, param.max);
            }
        }

        const labeledWidget = new Container();
        labeledWidget.classList.add('plugin-parameter');
        labeledWidget.appendTo(container);

        const widgetContainer = new Container();
        widgetContainer.classList.add('plugin-parameter-widget');
        widgetContainer.appendTo(labeledWidget);

        widget.appendTo(widgetContainer);
        widget.bindTo(param, 'value');

        const label = new Label();
        label.text = param.name;
        label.appendTo(labeledWidget);
    }

    main();

}
