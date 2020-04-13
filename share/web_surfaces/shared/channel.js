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

export class MessageChannel {

	constructor (host) {
		// https://developer.mozilla.org/en-US/docs/Web/API/URL/host
		this._host = host;
	}

	async open () {
		return new Promise((resolve, reject) => {
			this._socket = new WebSocket(`ws://${this._host}`);

			this._socket.onclose = () => this.onClose();

			this._socket.onerror = (error) => this.onError(error);

			this._socket.onmessage = (event) => {
				this.onMessage (Message.fromJsonText(event.data));
			};

			this._socket.onopen = resolve;
		});
	}

	close () {
		this._socket.close();
	}

	send (msg) {
		if (this._socket) {
			this._socket.send(msg.toJsonText());
		} else {
			throw Error('MessageChannel: cannot call send() before open()');
		}
	}

	onClose () {}
	onError (error) {}
	onMessage (msg) {}

}
