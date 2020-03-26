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

class Widget {

    constructor (node, addr, html) {
        this.node = node;
        this.addr = addr;
        const template = document.createElement('template');
        template.innerHTML = html;
        this.el = template.content.firstChild;
    }

    attach (parent, callback) {
        parent.appendChild(this.el);

        if (callback) {
            this.callback = callback;
        }
    }

    callback (value) {
        // do nothing by default
    }

    get hash () {
        return [this.node, this.addr];
    }

}

class Switch extends Widget {

    constructor (node, addr) {
        super (node, addr, `<input type="checkbox" class="widget-switch">`);
        this.el.addEventListener('input', (ev) => this.callback(this.value));
    }

    get value () {
        return this.el.checked;
    }

    set value (val) {
        this.el.checked = val;
    }
     
}

class Slider extends Widget {

    constructor (node, addr, min, max, step) {
        const html = `<input type="range" class="widget-slider"
            min="${min}" max="${max}" step="${step}">`;
        super(node, addr, html);
        this.min = min;
        this.max = max;
        this.el.addEventListener('input', (ev) => this.callback(this.value));
    }

    get value () {
        return parseFloat(this.el.value)
    }

    set value (val) {
        this.el.value = val;
    }

}

class DiscreteSlider extends Slider {

    constructor (node, addr, min, max) {
        super(node, addr, min, max, 1);
    }

}

class ContinuousSlider extends Slider {

    constructor (node, addr, min, max) {
        super(node, addr, min, max, 0.001);
    }

}

class LogarithmicSlider extends ContinuousSlider {

    constructor (node, addr, min, max) {
        super(node, addr, 0, 1.0);
        this.minVal = Math.log(min);
        this.maxVal = Math.log(max);
        this.scale = this.maxVal - this.minVal;
    }

    get value () {
        return Math.exp(this.minVal + this.scale * super.value);
    }

    set value (val) {
        this.el.value = (Math.log(val) - this.minVal) / this.scale;
    }

}

class StripPanSlider extends ContinuousSlider {

    constructor (node, addr) {
        super(node, addr, -1.0, 1.0);
    }

}

class StripGainSlider extends ContinuousSlider {

    constructor (node, addr) {
        super(node, addr, 0, 1.0)
        this.minVal = -58.0;
        this.maxVal = 6.0;
        this.scale = (this.maxVal - this.minVal);
    }

    get value () {
        return this.maxVal + Math.log10(super.value) * this.scale;
    }

    set value (val) {
        this.el.value = Math.pow(10.0, (val - this.maxVal) / this.scale);
    }

}

class StripMeter extends Widget {

    constructor (node, addr) {
        super(node, addr, `<label></label>`);
    }

    set value (val) {
        this.el.innerHTML = val == -Infinity ? '-∞' : `${Math.round(val)} dB`;
    }

}
