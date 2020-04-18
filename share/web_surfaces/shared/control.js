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

import { ANode } from './message.js';

// Surface control API over WebSockets

export class ControlMixin {

	async getTempo () {
		return await this._sendRecvSingle(ANode.TEMPO);
	}

	async getTransportRoll () {
		return await this._sendRecvSingle(ANode.TRANSPORT_ROLL);
	}

	async getRecordState () {
		return await this._sendRecvSingle(ANode.RECORD_STATE);
	}
	
	async getStripGain (stripId) {
		return await this._sendRecvSingle(ANode.STRIP_GAIN, [stripId]);
	}

	async getStripPan (stripId) {
		return await this._sendRecvSingle(ANode.STRIP_PAN, [stripId]);
	}

	async getStripMute (stripId) {
		return await this._sendRecvSingle(ANode.STRIP_MUTE, [stripId]);
	}

	async getStripPluginEnable (stripId, pluginId) {
		return await this._sendRecvSingle(ANode.STRIP_PLUGIN_ENABLE, [stripId, pluginId]);
	}

	async getStripPluginParamValue (stripId, pluginId, paramId) {
		return await this._sendRecvSingle(ANode.STRIP_PLUGIN_PARAM_VALUE, [stripId, pluginId, paramId]);
	}

	setTempo (bpm) {
		this._send(ANode.TEMPO, [], [bpm]);
	}

	setTransportRoll (value) {
		this._send(ANode.TRANSPORT_ROLL, [], [value]);
	}

	setRecordState (value) {
		this._send(ANode.RECORD_STATE, [], [value]);
	}

	setStripGain (stripId, db) {
		this._send(ANode.STRIP_GAIN, [stripId], [db]);
	}

	setStripPan (stripId, value) {
		this._send(ANode.STRIP_PAN, [stripId], [value]);
	}

	setStripMute (stripId, value) {
		this._send(ANode.STRIP_MUTE, [stripId], [value]);
	}

	setStripPluginEnable (stripId, pluginId, value) {
		this._send(ANode.STRIP_PLUGIN_ENABLE, [stripId, pluginId], [value]);
	}

	setStripPluginParamValue (stripId, pluginId, paramId, value) {
		this._send(ANode.STRIP_PLUGIN_PARAM_VALUE, [stripId, pluginId, paramId], [value]);
	}

}
