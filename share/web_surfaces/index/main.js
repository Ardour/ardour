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

(() => {

    const INDEX_RESOURCE = 'index.json';

    async function fetchIndex (url) {
        const response = await fetch(url);
        
        if (response.status == 200) {
            return await response.json();
        } else {
            throw new Error(`HTTP response status ${response.status}`);
        }
    }

    function buildItem (groupPath, surface) {
        const li = document.createElement('li');
        li.innerHTML = `<li>
                        <a href="${groupPath}/${surface.path}/">${surface.name}</a>
                        <p>${surface.description}</p>
                    </li>`;
        return li;
    }

    function printIndex (index) {
        for (let group of index) {
            const path = group['path'];
            const span = document.querySelector(`#${path} span`);
            span.innerHTML = group['diskPath'];

            let surfaces = group.surfaces;

            if (surfaces.length > 0) {
                const ul = document.querySelector(`#${path} > ul`);
                surfaces.sort((a, b) => a.name.localeCompare(b.name));
                
                for (surface of surfaces) {
                    ul.appendChild(buildItem(path, surface));
                }
            } else {
                const p = document.createElement('p');
                p.innerHTML = '<p>No surfaces found</p>';
                document.getElementById(path).appendChild(p);
            }
        }

        document.getElementById('index').style.display = 'inline';
    }

    function printError (message) {
        const error = document.getElementById('error');
        error.innerHTML = message;
        error.style.display = 'inline';
    }

    async function main () {
        try {
            const indexUrl = `${location.protocol}//${location.host}/${INDEX_RESOURCE}`;
            const index = await fetchIndex (indexUrl);
            printIndex(index);
        } catch (err) {
            printError(`Error loading ${INDEX_RESOURCE}: ${err.message}`);
        }

        document.getElementById('loading').style.display = 'none';
    }

    main();

})();
