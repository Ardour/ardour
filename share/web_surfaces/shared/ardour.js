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

import { ANode, Message } from './message.js';
import { MessageChannel } from './channel.js';

export class Ardour {

	constructor () {
		this._callbacks = [];
		this._pendingRequest = null;
		this._channel = new MessageChannel(location.host);

		this._channel.onError = (error) => {
			this._fireCallbacks('error', error);
		};

		this._channel.onMessage = (msg) => {
			this._onChannelMessage(msg);
		};
	}

	addCallback (callback) {
		this._callbacks.push(callback);
	}

	async open () {
		this._channel.onClose = () => {
			this._fireCallbacks('error', new Error('Message channel unexpectedly closed'));
		};

		await this._channel.open();
	}

	close () {
		this._channel.onClose = () => {};
		this._channel.close();
	}

	send (msg) {
		this._channel.send(msg);
	}

	// Surface metadata API over HTTP

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
				description: xmlDoc.getElementsByTagName('Description')[0].getAttribute('value'),
				version: xmlDoc.getElementsByTagName('Version')[0].getAttribute('value')
			}
		} else {
			throw this._fetchResponseStatusError(response.status);
		}
	}

	// Surface control API over WebSockets
	// clients need to call open() before calling these methods

	async getTempo () {
		return (await this._sendAndReceive(ANode.TEMPO))[0];
	}
	
	async getStripGain (stripId) {
		return (await this._sendAndReceive(ANode.STRIP_GAIN, [stripId]))[0];
	}

	async getStripPan (stripId) {
		return (await this._sendAndReceive(ANode.STRIP_PAN, [stripId]))[0];
	}

	async getStripMute (stripId) {
		return (await this._sendAndReceive(ANode.STRIP_MUTE, [stripId]))[0];
	}

	async getStripPluginEnable (stripId, pluginId) {
		return (await this._sendAndReceive(ANode.STRIP_PLUGIN_ENABLE, [stripId, pluginId]))[0];
	}

	async getStripPluginParamValue (stripId, pluginId, paramId) {
		return (await this._sendAndReceive(ANode.STRIP_PLUGIN_PARAM_VALUE, [stripId, pluginId, paramId]))[0];
	}

	setTempo (bpm) {
		this._send(ANode.TEMPO, [], [bpm]);
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

	// Private methods

	_send (node, addr, val) {
		const msg = new Message(node, addr, val);
		this._channel.send(msg);
		return msg;
	}

	async _sendAndReceive (node, addr, val) {
		return new Promise((resolve, reject) => {
			const hash = this._send(node, addr, val).hash;
			this._pendingRequest = {resolve: resolve, hash: hash};
		});
	}

	_onChannelMessage (msg) {
		if (this._pendingRequest && (this._pendingRequest.hash == msg.hash)) {
			this._pendingRequest.resolve(msg.val);
			this._pendingRequest = null;
		} else {
			this._fireCallbacks('message', msg);
			this._fireCallbacks(msg.node, ...msg.addr, ...msg.val);
		}
	}

	_fireCallbacks (name, ...args) {
		// name_with_underscores -> onNameWithUnderscores
		const method = 'on' + name.split('_').map((s) => {
			return s[0].toUpperCase() + s.slice(1).toLowerCase();
		}).join('');

		for (const callback of this._callbacks) {
			if (method in callback) {
				callback[method](...args)
			}
		}
	}

	_fetchResponseStatusError (status) {
		return new Error(`HTTP response status ${status}`);
	}

}
