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

class ValueType {

	constructor (rawType) {
		this._rawType = rawType;
	}

	get isBoolean () {
		return this._rawType == 'b';
	}

	get isInteger () {
		return this._rawType == 'i';
	}

	get isDouble () {
		return this._rawType == 'd';
	}

}

export default class Parameter extends AddressableComponent {

	constructor (parent, addr, desc) {
		super(parent, addr);
		this._name = desc[0];
		this._valueType = new ValueType(desc[1]);
		this._min = desc[2];
		this._max = desc[3];
		this._isLog = desc[4];
		this._value = 0;
	}

	get plugin () {
		return this._parent;
	}

	get name () {
		return this._name;
	}

	get valueType () {
		return this._valueType;
	}

	get min () {
		return this._min;
	}

	get max () {
		return this._max;
	}

	get isLog () {
		return this._isLog;
	}

	get value () {
		return this._value;
	}

	set value (value) {
		this.updateRemote('value', value, StateNode.STRIP_PLUGIN_PARAM_VALUE);
	}

 	handle (node, addr, val) {
		if (node == StateNode.STRIP_PLUGIN_PARAM_VALUE) {
			this.updateLocal('value', val[0]);
			return true;
 		}

 		return false;
 	}

}
