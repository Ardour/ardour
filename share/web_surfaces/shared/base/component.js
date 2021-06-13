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

import { Message } from './protocol.js';
import Observable from './observable.js';

export class Component extends Observable {

	constructor (channel) {
		super();
		this._channel = channel;
	}

 	get channel () {
 		return this._channel;
 	}

	on (event, callback) {
		this.addObserver(event, callback);
	}
	
	notifyPropertyChanged (property) {
		this.notifyObservers(property, this['_' + property]);
	}

 	send (node, addr, val) {
		this.channel.send(new Message(node, addr, val));
 	}

 	handle (node, addr, val) {
 		return false;
 	}

 	handleMessage (msg) {
		return this.handle(msg.node, msg.addr, msg.val);
	}

	updateLocal (property, value) {
		this['_' + property] = value;
		this.notifyPropertyChanged(property);
	}

	updateRemote (property, value, node, addr) {
		this['_' + property] = value;
		this.send(node, addr || [], [value]);
	}

}

export class ChildComponent extends Component {
 	
 	constructor (parent) {
 		super(parent.channel);
 		this._parent = parent;
 	}

}

export class AddressableComponent extends ChildComponent {

	constructor (parent, addr) {
		super(parent);
		this._addr = addr;
	}
 	
 	get addr () {
 		return this._addr;
 	}

 	get addrId () {
 		return this._addr.join('-');
 	}

	updateRemote (property, value, node) {
		super.updateRemote(property, value, node, this.addr);
	}

}
