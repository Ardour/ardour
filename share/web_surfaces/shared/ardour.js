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

import { ControlMixin } from './control.js';
import { MetadataMixin } from './metadata.js';
import { Message } from './message.js';
import { MessageChannel } from './channel.js';

// See ControlMixin and MetadataMixin for available APIs
// See ArdourCallback for an example callback implementation

class BaseArdourClient {

	constructor (host) {
		this._callbacks = [];
		this._connected = false;
		this._pendingRequest = null;
		this._channel = new MessageChannel(host || location.host);

		this._channel.onError = (error) => {
			this._fireCallbacks('error', error);
		};

		this._channel.onMessage = (msg) => {
			this._onChannelMessage(msg);
		};
	}

	addCallbacks (callbacks) {
		this._callbacks.push(callbacks);
	}

	async connect (autoReconnect) {
		this._channel.onClose = async () => {
			if (this._connected) {
				this._fireCallbacks('disconnected');
				this._connected = false;
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

	// Private methods
	
	async _connect () {
		await this._channel.open();
		this._connected = true;
		this._fireCallbacks('connected');
	}

	_send (node, addr, val) {
		const msg = new Message(node, addr, val);
		this.send(msg);
		return msg;
	}

	async _sendAndReceive (node, addr, val) {
		return new Promise((resolve, reject) => {
			const nodeAddrId = this._send(node, addr, val).nodeAddrId;
			this._pendingRequest = {resolve: resolve, nodeAddrId: nodeAddrId};
		});
	}

	async _sendRecvSingle (node, addr, val) {
		return (await this._sendAndReceive (node, addr, val))[0];
	}

	_onChannelMessage (msg) {
		if (this._pendingRequest && (this._pendingRequest.nodeAddrId == msg.nodeAddrId)) {
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

		for (const callbacks of this._callbacks) {
			if (method in callbacks) {
				callbacks[method](...args)
			}
		}
	}

	_fetchResponseStatusError (status) {
		return new Error(`HTTP response status ${status}`);
	}

	async _sleep (t) {
		return new Promise(resolve => setTimeout(resolve, 1000));
	}

}

export class ArdourClient extends mixin(BaseArdourClient, ControlMixin, MetadataMixin) {}

function mixin (dstClass, ...classes) {
	for (const srcClass of classes) {
		for (const propName of Object.getOwnPropertyNames(srcClass.prototype)) {
			if (propName != 'constructor') {
				dstClass.prototype[propName] = srcClass.prototype[propName];
			}
		}
	}
	return dstClass;
}
