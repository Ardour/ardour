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

import { AddressableComponent } from '../base/component.js';
import { StateNode } from '../base/protocol.js';
import Parameter from './parameter.js';

export default class Plugin extends AddressableComponent {

	constructor (parent, addr, desc) {
		super(parent, addr);
		this._parameters = {};
		this._name = desc[0];
		this._enable = false;
	}

	get strip () {
		return this._parent;
	}

	get parameters () {
		return Object.values(this._parameters);
	}

	get name () {
		return this._name;
	}

	get enable () {
		return this._enable;
	}

	set enable (value) {
		this.updateRemote('enable', value, StateNode.STRIP_PLUGIN_ENABLE);
	}

 	handle (node, addr, val) {
		if (node.startsWith('strip_plugin_param')) {
	 		if (node == StateNode.STRIP_PLUGIN_PARAM_DESCRIPTION) {
	 			this._parameters[addr] = new Parameter(this, addr, val);
	 			this.notifyPropertyChanged('parameters');
	 			return true;
	 		} else {
	 			if (addr in this._parameters) {
	 				return this._parameters[addr].handle(node, addr, val);
	 			}
	 		}
 		} else if (node == StateNode.STRIP_PLUGIN_ENABLE) {
 			this.updateLocal('enable', val[0]);
			return true;
 		}

 		return false;
 	}

}
