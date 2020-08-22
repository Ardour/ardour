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
import { BaseWidget, BaseContainer, BaseDialog, BaseControl } from './widget.js';

export async function createRootContainer () {
    await loadToolkit();
    const root = new Container();
    root.tk = new TK.Root({id: 'root'});
    document.body.appendChild(root.element);
    return root;
}

class Widget extends BaseWidget {

    constructor (tk) {
        super();
        this.tk = tk;
    }

    get element () {
        return this.tk.element;
    }

}

export class Label extends Widget {

    constructor () {
        super(new TK.Label());
    }

    set text (text) {
        this.tk.set('label', text);
    }
    
}

class Control extends BaseControl {

    constructor (tk) {
        super();
        this.tk = tk;
    }

    get element () {
        return this.tk.element;
    }

}

class RangeControl extends Control {

    constructor (tk) {
        super(tk);

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

export class Dialog extends BaseDialog {

    constructor () {
        super();
        this.tk = new TK.Dialog({
            anchor: 'center',   // center v&h
            auto_close: true,
            auto_remove: true,
            showing_duration: 10,
            hiding_duration: 10,
            container: document.getElementById('root')
        });
    }

    get element () {
        return this.tk.element;
    }

    set closeButton (button) {
        button.callback = () => this.close();
        this.appendChild(button);
    }

    appendChild (child) {
        super.appendChild(child);
        this.tk.append_child(child.tk);
    }

    show () {
        // opening a TK.Dialog with auto_close=true from a TK.Button callback 
        // fails otherwise ev.stopPropagation() is called in the button event
        // handler or setTimeout() is used here
        setTimeout(() => {
            this.tk.set('display_state', 'show');
            this.tk.add_event('close', (ev) => this.onClose(ev));
        }, 0);
    }

    close () {
        this.tk.close();
    }

}

export class Button extends Control {

    constructor () {
        super(new TK.Button());
        this.tk.add_event('click', (ev) => this.callback(ev));
    }
    
    set text (text) {
        this.tk.set('label', text);
    }

    set icon (icon) {
        // see toolkit/styles/Toolkit.html
        this.tk.set('icon', icon);
        this.element.style.border = 'none';
    }

}

export class Toggle extends Control {

    constructor () {
        super(new TK.Toggle());
        this.tk.add_event('toggled', (state) => this.callback(state));
    }
    
    set text (text) {
        this.tk.set('label', text);
    }

    get value () {
        return this.tk.get('state');
    }

    set value (val) {
        this.tk.set('state', val);
    }
     
    setIcons (inactive, active) {
        this.tk.set('icon', inactive);
        this.tk.set('icon_active', active);
        this.element.style.border = 'none';
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
            max: max,
            hand: {
                width: 5,
                length: 15
            }
        });
    }

}

export class LogKnob extends Knob {

    constructor (min, max) {
        super({
            scale: 'frequency',
            min: min,
            max: max,
            hand: {
                width: 5,
                length: 15
            }
        });
    }

}

export class DiscreteKnob extends Knob {

    constructor (min, max, step) {
        super({
            scale: 'linear',
            min: min,
            max: max,
            snap: step || 1,
            hand: {
                width: 5,
                length: 15
            }
        });
    }

}

export class PanKnob extends Knob {

    constructor () {
        super({
            //scale: 'linear',
            scale: (k) => 1.0 - k,
            min: 0,
            max: 1.0,
            hand: {
                width: 5,
                length: 15
            }
        });
    }

}
