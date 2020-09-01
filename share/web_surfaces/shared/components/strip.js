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

import { AddressableComponent } from '../base/component.js';
import { StateNode } from '../base/protocol.js';
import Plugin from './plugin.js';

const NodeToProperty = Object.freeze({
	[StateNode.STRIP_METER] : 'meter',
	[StateNode.STRIP_GAIN]  : 'gain',
	[StateNode.STRIP_PAN]   : 'pan',
	[StateNode.STRIP_MUTE]  : 'mute'
});

export default class Strip extends AddressableComponent {

	constructor (parent, addr, desc) {
		super(parent, addr);
		this._plugins = {};
		this._name = desc[0];
		this._hasPan = desc[1];
		this._meter = 0;
		this._gain = 0;
		this._pan = 0;
		this._mute = false;
	}

	get plugins () {
		return Object.values(this._plugins);
	}

	get name () {
		return this._name;
	}

	get hasPan () {
		return this._hasPan;
	}

	get meter () {
		return this._meter;
	}

	get gain () {
		return this._gain;
	}

	set gain (db) {
		this.updateRemote('gain', db, StateNode.STRIP_GAIN);
	}

	get pan () {
		return this._pan;
	}

	set pan (value) {
		this.updateRemote('pan', value, StateNode.STRIP_PAN);
	}

	get mute () {
		return this._mute;
	}

	set mute (value) {
		this.updateRemote('mute', value, StateNode.STRIP_MUTE);
	}

 	handle (node, addr, val) {
 		if (node.startsWith('strip_plugin')) {
	 		if (node == StateNode.STRIP_PLUGIN_DESCRIPTION) {
	 			this._plugins[addr] = new Plugin(this, addr, val);
	 			this.notifyPropertyChanged('plugins');
	 			return true;
	 		} else {
	 			const pluginAddr = [addr[0], addr[1]];
	 			if (pluginAddr in this._plugins) {
	 				return this._plugins[pluginAddr].handle(node, addr, val);
	 			}
	 		}
 		} else if (node in NodeToProperty) {
 			this.updateLocal(NodeToProperty[node], val[0]);
 			return true;
 		}

 		return false;
 	}

}
