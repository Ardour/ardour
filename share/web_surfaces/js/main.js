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

    function buildItem (group, model) {
        const li = document.createElement('li');
        li.innerHTML = `<li>
                        <a href="${group}/${model.id}/">${model.name}</a>
                        <p>${model.description}</p>
                    </li>`;
        return li;
    }

    function printIndex (index) {
        ['builtin', 'user'].forEach((group) => {
            const ul = document.getElementById(group);
            let models = index[group];
            if (models.length > 0) {
                models.sort((a, b) => a.name.localeCompare(b.name));
                for (model of models) {
                    ul.appendChild(buildItem(group, model));
                }
            } else {
                ul.parentNode.style.display = 'none';
            }
        });
    }

    async function main () {
        try {
            const indexUrl = `${location.protocol}//${location.host}/${INDEX_RESOURCE}`;
            const index = await fetchIndex (indexUrl);
            printIndex (index);
        } catch (err) {
            const content = document.getElementById('content');
            content.innerHTML = `<pre>${INDEX_RESOURCE}: ${err.message}</pre>`;
        }
    }

    main();

})();
