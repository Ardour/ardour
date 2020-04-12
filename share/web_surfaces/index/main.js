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

import { Ardour } from '/shared/ardour.js';

(() => {

    async function main () {
        try {
            const surfaces = await new Ardour().getAvailableSurfaces();
            printSurfaces(surfaces);
        } catch (err) {
            printError(`Error loading surfaces list: ${err.message}`);
        }

        document.getElementById('loading').style.display = 'none';
    }

    function printSurfaces (surfaces) {
        for (const group of surfaces) {
            const ul = document.querySelector(`#${group.path} > ul`);
            
            const li = document.createElement('li');
            li.innerHTML = `<li>
                <span>Filesystem location:</span>
                &thinsp;
                <span class="filesystem-path">${group.filesystemPath}</span>
            </li>`;
            ul.appendChild(li);

            if (group.surfaces.length > 0) {
                group.surfaces.sort((a, b) => a.name.localeCompare(b.name));

                for (const surface of group.surfaces) {
                    const li = document.createElement('li');
                    li.innerHTML = `<li>
                        <a href="${group.path}/${surface.path}/">${surface.name}</a>
                        &thinsp;
                        <span class="surface-version">(${surface.version})</span>
                        <p>${surface.description}</p>
                    </li>`;
                    ul.appendChild(li);
                }
            } else {
                const li = document.createElement('li');
                li.innerHTML = 'No surfaces found';
                ul.appendChild(li);
            }
        }

        document.getElementById('index').style.display = 'inline';
    }

    function printError (message) {
        const error = document.getElementById('error');
        error.innerHTML = message;
        error.style.display = 'inline';
    }

    main();

})();
