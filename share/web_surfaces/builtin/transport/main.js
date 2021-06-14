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

{

    const dom = {
        main       : document.getElementById('main'),
        time       : document.getElementById('time'),
        roll       : document.getElementById('roll'),
        record     : document.getElementById('record'),
        fullscreen : document.getElementById('fullscreen')
    };

    const ardour = new ArdourClient();

    let _rolling = false;
    let _record = false;

    function main () {
        addDomEventListeners();

        ardour.transport.on('time', setPosition);
        ardour.transport.on('roll', setRolling);
        ardour.transport.on('record', setRecord);

        ardour.connect();
    }

    function addDomEventListeners () {
        const roll = () => {
            setRolling(!_rolling);
            ardour.transport.roll = _rolling;
        };

        dom.roll.addEventListener('click', roll);

        const record = () => {
            setRecord(!_record);
            ardour.transport.record = _record;
        };

        dom.record.addEventListener('click', record);

        // fullscreen button
        let requestFullscreen = null, fullscreenChange = null;

        if ('requestFullscreen' in dom.main) {
            requestFullscreen = dom.main.requestFullscreen.bind(dom.main);
            fullscreenChange = 'fullscreenchange';
        } else if ('webkitRequestFullscreen' in dom.main) {
            requestFullscreen = dom.main.webkitRequestFullscreen.bind(dom.main);
            fullscreenChange = 'webkitfullscreenchange';
        }

        if (requestFullscreen && fullscreenChange) {
            dom.fullscreen.addEventListener('click', requestFullscreen);

            document.addEventListener(fullscreenChange, (e) => {
                const fullscreen = document.fullscreen || document.webkitIsFullScreen;
                dom.fullscreen.style.display = fullscreen ? 'none' : 'inline';
            });
        } else {
            dom.fullscreen.style.display = 'none';
        }

        // keyboard actions
        document.addEventListener('keydown', (e) => {
            const key = e.key.toLowerCase();

            if (key == ' ') {
                roll();
            } else if (key == 'r') {
                record();
            } else if (key == 'f') {
                requestFullscreen();
            }
        });
    }

    function setPosition (seconds) {
        const h = Math.floor(seconds / 3600),
              m = Math.floor((seconds - 3600 * h) / 60),
              s = Math.floor(seconds - 3600 * h - 60 * m),
              ms = Math.round(1000 * (seconds % 1)),
              time = (h < 10 ? '0' + h : h)
             + ':' + (m < 10 ? '0' + m : m)
             + ':' + (s < 10 ? '0' + s : s)
             + '.' + (ms < 100 ? (ms < 10 ? '00' + ms : '0' + ms) : ms);
        dom.time.innerHTML = time;
    }

    function setRolling (rolling) {
        _rolling = rolling
        const image = _rolling ? 'img/pause.svg' : 'img/play.svg';
        dom.roll.style.backgroundImage = `url(${image})`;
    }

    function setRecord (record) {
        _record = record;
        
        if (_record) {
            dom.record.style.backgroundImage = 'url(img/record-on.svg)';
            dom.record.classList.add('pulse');
        } else {
            dom.record.style.backgroundImage = 'url(img/record-off.svg)';
            dom.record.classList.remove('pulse');
        }
    }

    main();

}
