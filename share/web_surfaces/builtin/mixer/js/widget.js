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

export class BaseWidget {

    constructor (element) {
        if (element) {
            this.element = element;
        }
    }

    appendTo (container) {
        container.appendChild(this);
    }

    get id () {
        return this.element.id;
    }

    set id (id) {
        this.element.id = id;
    }

    get classList () {
        return this.element.classList;
    }

}

export class BaseContainer extends BaseWidget {

    constructor () {
        super();
        this.children = [];
    }

    appendChild (child) {
        this.children.push(child);
    }

}

export class BaseDialog extends BaseContainer {

    constructor () {
        super();
        this.children = [];
    }

    set closeButton (button) {
        // empty
    }

    show () {
        // empty
    }

    close () {
        // empty
    }

    onClose () {
        // empty
    }

}

export class BaseControl extends BaseWidget {

    get value () {
        // empty
    }

    set value (val) {
        // empty
    }

    callback (val) {
        // empty
    }

    bindTo (component, property) {
        // ardour → ui
        this.value = component[property];
        component.on(property, (value) => this.value = value);
        // ui → ardour
        this.callback = (value) => component[property] = value;
    }

}
