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

// This module is currently unused as 'toolkit' provides its own transformations
// it could be still useful for developing custom widgets

export class Scale {

    constructor (min, max) {
        this.min = min;
        this.max = max;
        this.scale = this.max - this.min;
    }

    fromWidget (val) {
        return val;
    }

    toWidget (val) {
        return val;
    }

}

export class DbScale extends Scale {

    constructor () {
        super(-58.0, 6.0);
    }

    fromWidget (val) {
        return this.max + this.scale * Math.log10(val);
    }

    toWidget (val) {
        return Math.pow(10.0, (val - this.max) / this.scale);
    }

}

export class LinearScale extends Scale {

    fromWidget (val) {
        return this.min + this.scale * val;
    }

    toWidget (val) {
        return (val - this.min) / this.scale;
    }

}

export class LogScale extends Scale {

    constructor (min, max) {
        super(Math.log(min), Math.log(max));
    }

    fromWidget (val) {
        return Math.exp(this.min + this.scale * val);
    }

    toWidget (val) {
        return (Math.log(val) - this.min) / this.scale;
    }

}
