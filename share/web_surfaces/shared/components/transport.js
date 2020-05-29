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

import { RootComponent } from '../base/component.js';
import { StateNode } from '../base/protocol.js';

export class Transport extends RootComponent {

	constructor (channel) {
		super(channel);
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
		this.updateRemote('tempo', bpm, StateNode.TEMPO);
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
		this.updateRemote('record', value, StateNode.RECORD_STATE);
	}

 	handle (node, addr, val) {
 		switch (node) {
 			case StateNode.TEMPO:
 				this.updateLocal('tempo', val[0]);
 				break;
 			case StateNode.POSITION_TIME:
 				this.updateLocal('time', val[0]);
 				break;
 			case StateNode.TRANSPORT_ROLL:
 				this.updateLocal('roll', val[0]);
 				break;
 			case StateNode.RECORD_STATE:
 				this.updateLocal('record', val[0]);
 				break;
 			default:
 				return false;
 		}

 		return true;
 	}

}
