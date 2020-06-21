/*
 * Copyright © 2020 Luciano Iam <lucianito@gmail.com>
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

import loadToolkit from './tkloader.js';
import { BaseContainer, BaseControl } from './widget.js';

class Control extends BaseControl {

    get element () {
        return this.tk.element;
    }

}

class RangeControl extends Control {

    constructor (tk) {
        super();

        this.tk = tk;
        this.lastValue = NaN;

        this.tk.add_event('useraction', (name, value) => {
            if ((name == 'value') && (this.lastValue != value)) {
                this.lastValue = value;
                this.callback(value);
            }
        });
    }

    get value () {
        return this.tk.get('value');
    }

    set value (val) {
        this.tk.set('value', val);
    }

}

class Knob extends RangeControl {

    constructor (options) {
        super(new TK.Knob(options));
    }

}

export async function createRootContainer () {
    await loadToolkit();
    const root = new Container();
    root.tk = new TK.Root({id: 'root'});
    document.body.appendChild(root.element);
    return root;
}

export class Container extends BaseContainer {

    constructor () {
        super();
        this.tk = new TK.Container();
    }

    get element () {
        return this.tk.element;
    }

    appendChild (child) {
        super.appendChild(child);
        this.tk.append_child(child.tk);
    }

}

export class Toggle extends Control {

    constructor () {
        super(new TK.Toggle());
        this.tk.add_event('toggled', (state) => this.callback(state));
    }

    get value () {
        return this.tk.get('state');
    }

    set value (val) {
        this.tk.set('state', val);
    }
     
}

export class StripGainFader extends RangeControl {

    constructor () {
        super(new TK.Fader({
            scale: 'decibel',
            min: -58.0,
            max: 6.0
        }));
    }

}

export class StripMeter extends RangeControl {

    constructor () {
        super(new TK.LevelMeter({
            show_scale: false,
            scale: 'decibel',
            min: -58.0,
            max: 6.0
        }));
    }

    set value (val) {
        this.tk.set('value', val);
    }

}

export class LinearKnob extends Knob {

    constructor (min, max) {
        super({
            scale: 'linear',
            min: min,
            max: max
        });
    }

}

export class LogKnob extends Knob {

    constructor (min, max) {
        super({
            scale: 'log2',
            min: min,
            max: max
        });
    }

}

export class DiscreteKnob extends Knob {

    constructor (min, max, step) {
        super({
            scale: 'linear',
            min: min,
            max: max,
            snap: step || 1
        });
    }

}

// TODO: consider switching to [0-1.0] pan scale in Ardour surface code

export class PanKnob extends Knob {

    constructor () {
        super({
            scale: 'linear',
            min: -1.0,
            max: 1.0
        });
    }

}
