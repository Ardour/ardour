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

import { MetadataMixin } from './metadata.js';
import { ControlMixin } from './control.js';
import { Message } from './message.js';
import { MessageChannel } from './channel.js';

// See *Mixin for the available APIs

class BaseArdourClient {

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

	// Private methods

	_send (node, addr, val) {
		const msg = new Message(node, addr, val);
		this.send(msg);
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

export class ArdourClient extends mixin(BaseArdourClient, ControlMixin, MetadataMixin) {}

function mixin (dstClass, ...classes) {
	for (const srcClass of classes) {
		for (const methName of Object.getOwnPropertyNames(srcClass.prototype)) {
			if (methName != 'constructor') {
				dstClass.prototype[methName] = srcClass.prototype[methName];
			}
		}
	}
	return dstClass;
}
