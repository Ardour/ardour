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

export const JSON_INF = 1.0e+128;

export const ANode = Object.freeze({
	TEMPO:                    'tempo',
	POSITION_TIME:            'position_time',
	TRANSPORT_ROLL:           'transport_roll',
	RECORD_STATE:             'record_state',
	STRIP_DESC:               'strip_desc',
	STRIP_METER:              'strip_meter',
	STRIP_GAIN:               'strip_gain',
	STRIP_PAN:                'strip_pan',
	STRIP_MUTE:               'strip_mute',
	STRIP_PLUGIN_DESC:        'strip_plugin_desc',
	STRIP_PLUGIN_ENABLE:      'strip_plugin_enable',
	STRIP_PLUGIN_PARAM_DESC:  'strip_plugin_param_desc',
	STRIP_PLUGIN_PARAM_VALUE: 'strip_plugin_param_value'
});

export class Message {

	constructor (node, addr, val) {
		this.node = node;
		this.addr = addr;
		this.val = [];

		for (const i in val) {
			if (val[i] >= JSON_INF) {
				this.val.push(Infinity);
			} else if (val[i] <= -JSON_INF) {
				this.val.push(-Infinity);
			} else {
				this.val.push(val[i]);
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

		for (const i in this.val) {
			if (this.val[i] == Infinity) {
				val.push(JSON_INF);
			} else if (this.val[i] == -Infinity) {
				val.push(-JSON_INF);
			} else {
				val.push(this.val[i]);
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
