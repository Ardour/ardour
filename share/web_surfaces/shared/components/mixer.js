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

import { ChildComponent } from '../base/component.js';
import { StateNode } from '../base/protocol.js';
import Strip from './strip.js';

export default class Mixer extends ChildComponent {

	constructor (parent) {
		super(parent);
		this._strips = {};
		this._ready = false;
	}

	get ready () {
		return this._ready;
	}

	get strips () {
		return Object.values(this._strips);
	}

	getStripByName (name) {
		name = name.trim().toLowerCase();
		return this.strips.find(strip => strip.name.trim().toLowerCase() == name);
	}

 	handle (node, addr, val) {
 		if (node.startsWith('strip')) {
	 		if (node == StateNode.STRIP_DESCRIPTION) {
	 			this._strips[addr] = new Strip(this, addr, val);
	 			this.notifyPropertyChanged('strips');
	 			return true;
	 		} else {
	 			const stripAddr = [addr[0]];
	 			if (stripAddr in this._strips) {
	 				return this._strips[stripAddr].handle(node, addr, val);
	 			}
	 		}
 		} else {
 			// all initial strip description messages have been received at this point
			if (!this._ready) {
	 			this.updateLocal('ready', true);
	 			// passthrough by allowing to return false
			}
 		}

 		return false;
 	}

}
