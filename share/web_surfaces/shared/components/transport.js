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

const NodeToProperty = Object.freeze({
	[StateNode.TRANSPORT_TEMPO]  : 'tempo',
	[StateNode.TRANSPORT_TIME]   : 'time',
	[StateNode.TRANSPORT_ROLL]   : 'roll',
	[StateNode.TRANSPORT_RECORD] : 'record'
});

export default class Transport extends ChildComponent {

	constructor (parent) {
		super(parent);
		this._time = 0;
		this._tempo = 0;
		this._roll = false;
		this._record = false;
	}
	
	get time () {
		return this._time;
	}

	get tempo () {
		return this._tempo;
	}

	set tempo (bpm) {
		this.updateRemote('tempo', bpm, StateNode.TRANSPORT_TEMPO);
	}

	get roll () {
		return this._roll;
	}

	set roll (value) {
		this.updateRemote('roll', value, StateNode.TRANSPORT_ROLL);
	}

	get record () {
		return this._record;
	}

	set record (value) {
		this.updateRemote('record', value, StateNode.TRANSPORT_RECORD);
	}

 	handle (node, addr, val) {
 		if (node in NodeToProperty) {
 			this.updateLocal(NodeToProperty[node], val[0]);
 			return true;
 		}

 		return false;
 	}

}
