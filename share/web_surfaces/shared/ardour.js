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

import { Component } from './base/component.js';
import { StateNode } from './base/protocol.js';
import MessageChannel from './base/channel.js';
import Mixer from './components/mixer.js';
import Transport from './components/transport.js';

function getOption (options, key, defaultValue) {
	return options ? (key in options ? options[key] : defaultValue) : defaultValue;
}

export default class ArdourClient extends Component {

	constructor (options) {
		super(new MessageChannel(getOption(options, 'host', location.host)));

		if (getOption(options, 'components', true)) {
			this._mixer = new Mixer(this);
			this._transport = new Transport(this);
			this._components = [this._mixer, this._transport];
		} else {
			this._components = [];
		}

		this._autoReconnect = getOption(options, 'autoReconnect', true);
		this._connected = false;

		this.channel.onMessage = (msg, inbound) => this._handleMessage(msg, inbound);
		this.channel.onError = (err) => this.notifyObservers('error', err);
	}

	// Access to the object-oriented API (enabled by default)

	get mixer () {
		return this._mixer;
	}

	get transport () {
		return this._transport;
	}

	// Low level control messages flow through a WebSocket

	async connect () {
		this.channel.onClose = async () => {
			if (this._connected) {
				this._setConnected(false);
			}

			if (this._autoReconnect) {
				await this._sleep(1000);
				await this._connect();
			}
		};

		await this._connect();
	}

	disconnect () {
		this.channel.onClose = () => {};
		this.channel.close();
		this._connected = false;
	}

	send (msg) {
		this.channel.send(msg);
	}

	async sendAndReceive (msg) {
		return await this.channel.sendAndReceive(msg);
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
		await this.channel.open();
		this._setConnected(true);
	}

	_setConnected (connected) {
		this._connected = connected;
		this.notifyPropertyChanged('connected');
	}

	_handleMessage (msg, inbound) {
		this.notifyObservers('message', msg, inbound);

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
