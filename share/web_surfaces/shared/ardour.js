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

import { MessageChannel, Node } from './channel.js';

export class Ardour {

	constructor () {
		this.channel = new MessageChannel(location.host);
	}

	async open () {
		await this.channel.open();
	}

	close () {
		this.channel.close();
	}

	async getAvailableSurfaces () {
		const response = await fetch('/surfaces.json');
		
		if (response.status == 200) {
			return await response.json();
		} else {
			throw this._fetchResponseStatusError(response.status);
		}
	}

	async getSurfaceManifest () {
		const response = await fetch('manifest.xml');

		if (response.status == 200) {
			const xmlText = await response.text();
			const xmlDoc = new DOMParser().parseFromString(xmlText, 'text/xml');
			return {
				name: xmlDoc.getElementsByTagName('Name')[0].getAttribute('value'),
				description: xmlDoc.getElementsByTagName('Description')[0].getAttribute('value')
			}
		} else {
			throw this._fetchResponseStatusError(response.status);
		}
	}

	async getTempo () {
		return await this._sendAndReceive(Node.TEMPO);
	}
	
	async getStripGain (stripId) {
		return await this._sendAndReceive(Node.STRIP_GAIN, [stripId]);
	}

	async getStripPan (stripId) {
		return await this._sendAndReceive(Node.STRIP_PAN, [stripId]);
	}

	async getStripMute (stripId) {
		return await this._sendAndReceive(Node.STRIP_MUTE, [stripId]);
	}

	async getStripPluginEnable (stripId, pluginId) {
		return await this._sendAndReceive(Node.STRIP_PLUGIN_ENABLE, [stripId, pluginId]);
	}

	async getStripPluginParamValue (stripId, pluginId, paramId) {
		return await this._sendAndReceive(Node.STRIP_PLUGIN_PARAM_VALUE, [stripId, pluginId, paramId]);
	}

	async setTempo (bpm) {
		this._send(Node.TEMPO, [], [bpm]);
	}

	async setStripGain (stripId, db) {
		this._send(Node.STRIP_GAIN, [stripId], [db]);
	}

	async setStripPan (stripId, value) {
		this._send(Node.STRIP_PAN, [stripId], [value]);
	}

	async setStripMute (stripId, value) {
		this._send(Node.STRIP_MUTE, [stripId], [value]);
	}

	async setStripPluginEnable (stripId, pluginId, value) {
		this._send(Node.STRIP_PLUGIN_ENABLE, [stripId, pluginId], [value]);
	}

	async setStripPluginParamValue (stripId, pluginId, paramId, value) {
		this._send(Node.STRIP_PLUGIN_PARAM_VALUE, [stripId, pluginId, paramId], [value]);
	}

	async _send (addr, node, val) {
		this.channel.send(new Message(addr, node, val));
	}

	async _sendAndReceive (addr, node, val) {
		this._send(addr, node, val);

		// TO DO - wait for response
	}

	_fetchResponseStatusError (status) {
		return new Error(`HTTP response status ${status}`);
	}

}
