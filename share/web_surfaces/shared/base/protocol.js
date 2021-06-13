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

export const JSON_INF = 1.0e+128;

export const StateNode = Object.freeze({
	STRIP_DESCRIPTION              : 'strip_description',
	STRIP_METER                    : 'strip_meter',
	STRIP_GAIN                     : 'strip_gain',
	STRIP_PAN                      : 'strip_pan',
	STRIP_MUTE                     : 'strip_mute',
	STRIP_PLUGIN_DESCRIPTION       : 'strip_plugin_description',
	STRIP_PLUGIN_ENABLE            : 'strip_plugin_enable',
	STRIP_PLUGIN_PARAM_DESCRIPTION : 'strip_plugin_param_description',
	STRIP_PLUGIN_PARAM_VALUE       : 'strip_plugin_param_value',
	TRANSPORT_TEMPO                : 'transport_tempo',
	TRANSPORT_TIME                 : 'transport_time',
	TRANSPORT_ROLL                 : 'transport_roll',
	TRANSPORT_RECORD               : 'transport_record'
});

export class Message {

	constructor (node, addr, val) {
		this.node = node;
		this.addr = addr;
		this.val = [];

		for (const v of val) {
			if (v >= JSON_INF) {
				this.val.push(Infinity);
			} else if (v <= -JSON_INF) {
				this.val.push(-Infinity);
			} else {
				this.val.push(v);
			}
		}
	}

	static nodeAddrId (node, addr) {
		return [node].concat(addr || []).join('_');
	}

	static fromJsonText (jsonText) {
		let rawMsg = JSON.parse(jsonText);
		return new Message(rawMsg.node, rawMsg.addr || [], rawMsg.val);
	}

	toJsonText () {
		let val = [];

		for (const v of this.val) {
			if (v == Infinity) {
				val.push(JSON_INF);
			} else if (v == -Infinity) {
				val.push(-JSON_INF);
			} else {
				val.push(v);
			}
		}
		
		return JSON.stringify({node: this.node, addr: this.addr, val: val});
	}

	get nodeAddrId () {
		return Message.nodeAddrId(this.node, this.addr);
	}

	toString () {
		return `${this.node} (${this.addr}) = ${this.val}`;
	}

}
