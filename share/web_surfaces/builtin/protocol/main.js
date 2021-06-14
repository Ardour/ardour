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

        await ardour.connect();

        const manifest = await ardour.getSurfaceManifest();
        document.getElementById('manifest').innerHTML = manifest.name.toUpperCase()
                        + ' v' + manifest.version + ' — ' + manifest.description;
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

}
