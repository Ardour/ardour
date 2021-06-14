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

import loadToolkit from './tkloader.js';
import { BaseWidget, BaseContainer, BaseDialog, BaseControl } from './widget.js';

export async function createRootContainer () {
    await loadToolkit();
    const root = new Container();
    root.tk = new TK.Root({id: 'root'});
    document.body.appendChild(root.element);
    return root;
}

class TkWidget extends BaseWidget {

    constructor (tk) {
        super();
        this.tk = tk;
    }

    get element () {
        return this.tk.element;
    }

}

class TkControl extends BaseControl {

    constructor (tk) {
        super();
        this.tk = tk;
    }

    get element () {
        return this.tk.element;
    }

}

class TkRangeControl extends TkControl {

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

class TkKnob extends TkRangeControl {

    constructor (options) {
        super(new TK.Knob(options));
    }

}

class TkFader extends TkRangeControl {

    constructor (options) {
        super(new TK.Fader(options));
    }

}

class TkMeter extends TkRangeControl {

    constructor (options) {
        super(new TK.LevelMeter(options));
    }

    set value (val) {
        this.tk.set('value', val);
    }

}

export class Label extends TkWidget {

    constructor () {
        super(new TK.Label());
    }

    set text (text) {
        this.tk.set('label', text);
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
        // fails unless ev.stopPropagation() is called in the button event
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

export class Button extends TkControl {

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

export class Toggle extends TkControl {

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

export class AudioStripGainFader extends TkFader {

    constructor () {
        super({
            scale: 'decibel',
            labels: TK.FORMAT("%d"),
            min: -58.0,
            max: 6.0
        });
    }

}

export class MidiStripGainFader extends TkFader {

    constructor () {
        super({
            scale: 'linear',
            labels: TK.FORMAT("%d"),
            min: 0,
            max: 127
        });
    }

}

export class AudioStripMeter extends TkMeter {

    constructor () {
        super({
            show_scale: false,
            scale: 'decibel',
            min: -58.0,
            max: 6.0
        });
    }

}

export class MidiStripMeter extends TkMeter {

    constructor () {
        super({
            show_scale: false,
            scale: 'linear',
            min: 0,
            max: 127
        });
    }

}

export class LinearKnob extends TkKnob {

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

export class LogKnob extends TkKnob {

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

export class DiscreteKnob extends TkKnob {

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

export class PanKnob extends TkKnob {

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
