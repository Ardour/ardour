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
            const index = await new Ardour().getAvailableSurfaces();
            printIndex(index);
        } catch (err) {
            printError(`Error loading index: ${err.message}`);
        }

        document.getElementById('loading').style.display = 'none';
    }

    function printIndex (index) {
        for (const group of index) {
            const surfaces = group.surfaces;
            const groupPath = group['path'];
            const groupPathSpan = document.querySelector(`#${groupPath} span`);
            
            groupPathSpan.innerHTML = group['diskPath'];

            if (surfaces.length > 0) {
                const ul = document.querySelector(`#${groupPath} > ul`);
                surfaces.sort((a, b) => a.name.localeCompare(b.name));
                
                for (const surface of surfaces) {
                    const li = document.createElement('li');
                    li.innerHTML = `<li>
                        <a href="${groupPath}/${surface.path}/">${surface.name}</a>
                        <p>${surface.description}</p>
                    </li>`;
                    ul.appendChild(li);
                }
            } else {
                const p = document.createElement('p');
                p.innerHTML = '<p>No surfaces found</p>';
                document.getElementById(groupPath).appendChild(p);
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
