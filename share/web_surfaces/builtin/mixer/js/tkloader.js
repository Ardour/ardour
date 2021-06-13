/*
 * Copyright © 2020 Luciano Iam <oss@lucianoiam.com>
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

// This is a convenience module for loading the 'toolkit' audio widget library
// https://github.com/DeutscheSoft/toolkit
// GPLv3 license, docs at http://docs.deuso.de/Toolkit/

const STYLES = [];

const SCRIPTS = [
    'G',
    'toolkit',
    'implements/audiomath',
    'implements/base',
    'implements/anchor',
    'implements/globalcursor',
    'implements/ranged',
    'implements/gradient',
    'implements/warning',
    'widgets/widget',
    'modules/circular',
    'modules/dragcapture',
    'modules/dragvalue',
    'modules/range',
    'modules/scale',
    'modules/scrollvalue',
    'widgets/icon',
    'widgets/label',
    'widgets/button',
    'widgets/container',
    'widgets/dialog',
    'widgets/meterbase',
    'widgets/state',
    'widgets/toggle',
    'widgets/levelmeter',
    'widgets/value',
    'widgets/fader',
    'widgets/knob',
    'widgets/root'
];

export default async function loadToolkit() {
    const tkPath = '../toolkit';
    // stylesheets can be loaded in parallel
    const p = STYLES.map((style) => loadStylesheet(import.meta, `${tkPath}/styles/${style}.css`));
    await Promise.all(p);
    // scripts need to be loaded sequentially
    for (const script of SCRIPTS) {
        await loadScript(import.meta, `${tkPath}/${script}.js`);
    }
}

async function loadStylesheet(importMeta, path) {
    return new Promise((resolve, reject) => {
        const link = document.createElement('link');
        link.rel = 'stylesheet';
        link.type = 'text/css';
        link.href = `${getModuleBasename(importMeta)}/${path}`;
        link.addEventListener('error', (ev) => reject(Error('Stylesheet failed to load')));
        link.addEventListener('load', resolve);
        document.head.appendChild(link);
    });
}

async function loadScript(importMeta, path) {
    return new Promise((resolve, reject) => {
        const script = document.createElement('script'); 
        script.src = `${getModuleBasename(importMeta)}/${path}`;
        script.addEventListener('error', (ev) => reject(Error('Script failed to load')));
        script.addEventListener('load', resolve);
        document.body.appendChild(script);
    });
}

function getModuleBasename(importMeta) {
    return importMeta.url.substring(0, importMeta.url.lastIndexOf('/'));
}
