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

import { MessageChannel } from './base/channel.js';
import { StateNode } from './base/protocol.js';
import { Mixer } from './components/mixer.js';
import { Transport } from './components/transport.js';

export class ArdourClient {

	constructor (handlers, options) {
		this._options = options || {};
		this._components = [];
		this._connected = false;

		this._channel = new MessageChannel(this._options['host'] || location.host);

		this._channel.onMessage = (msg, inbound) => {
			this._handleMessage(msg, inbound);
		};

		if (!('components' in this._options) || this._options['components']) {
			this._mixer = new Mixer(this._channel);
			this._transport = new Transport(this._channel);
			this._components.push(this._mixer, this._transport);
		}

		this.handlers = handlers;
	}

	set handlers (handlers) {
		this._handlers = handlers || {};
		this._channel.onError = this._handlers['onError'] || console.log;
	}

	// Access to the object-oriented API (enabled by default)

	get mixer () {
		return this._mixer;
	}

	get transport () {
		return this._transport;
	}

	// Low level control messages flow through a WebSocket

	async connect (autoReconnect) {
		this._channel.onClose = async () => {
			if (this._connected) {
				this._setConnected(false);
			}

			if ((autoReconnect == null) || autoReconnect) {
				await this._sleep(1000);
				await this._connect();
			}
		};

		await this._connect();
	}

	disconnect () {
		this._channel.onClose = () => {};
		this._channel.close();
		this._connected = false;
	}

	send (msg) {
		this._channel.send(msg);
	}

	async sendAndReceive (msg) {
		return await this._channel.sendAndReceive(msg);
	}

	// Surface metadata API goes over HTTP

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
			const manifest = {};
			const xmlText = await response.text();
			const xmlDoc = new DOMParser().parseFromString(xmlText, 'text/xml');
			
			for (const child of xmlDoc.children[0].children) {
				manifest[child.tagName.toLowerCase()] = child.getAttribute('value');
			}

			return manifest;
		} else {
			throw this._fetchResponseStatusError(response.status);
		}
	}

	// Private methods
	
	async _sleep (t) {
		return new Promise(resolve => setTimeout(resolve, t));
	}

	async _connect () {
		await this._channel.open();
		this._setConnected(true);
	}

	_setConnected (connected) {
		this._connected = connected;
		
		if (this._handlers['onConnected']) {
			this._handlers['onConnected'](this._connected);
		}
	}

	_handleMessage (msg, inbound) {
		if (this._handlers['onMessage']) {
			this._handlers['onMessage'](msg, inbound);
		}

		if (inbound) {
			for (const component of this._components) {
				if (component.handleMessage(msg)) {
					break;
				}
			}
		}
	}

	_fetchResponseStatusError (status) {
		return new Error(`HTTP response status ${status}`);
	}

}
