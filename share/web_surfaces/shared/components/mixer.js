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

import { RootComponent } from '../base/component.js';
import { StateNode } from '../base/protocol.js';
import { Strip } from './strip.js';

export class Mixer extends RootComponent {

	constructor (channel) {
		super(channel);
		this._strips = {};
		this._ready = false;
	}

	get ready () {
		return this._ready;
	}

	get strips () {
		return Object.values(this._strips);
	}

	getStripByName (name) {
		name = name.trim().toLowerCase();
		return this.strips.find(strip => strip.name.trim().toLowerCase() == name);
	}

 	handle (node, addr, val) {
 		if (node.startsWith('strip')) {
	 		if (node == StateNode.STRIP_DESCRIPTION) {
	 			this._strips[addr] = new Strip(this, addr, val);
	 			this.notifyObservers('strips');
	 		} else {
	 			const stripAddr = [addr[0]];
	 			if (stripAddr in this._strips) {
	 				this._strips[stripAddr].handle(node, addr, val);
	 			} else {
	 				return false;
	 			}
	 		}

	 		return true;
 		}

 		/*
		   RECORD_STATE signals all mixer initial state has been sent because
		   it is the last message to arrive immediately after client connection,
		   see WebsocketsDispatcher::update_all_nodes() in dispatcher.cc
		  
		   For this to work the mixer component needs to receive incoming
		   messages before the transport component, otherwise the latter would
		   consume RECORD_STATE.
		  
		   Some ideas for a better implementation of mixer readiness detection:
		
 		   - Implement message bundles like OSC to pack all initial state
 		     updates into a single unit
 		   - Move *_DESCRIPTION messages to single message with val={JSON data},
 		     currently val only supports primitive data types
 		   - Append a termination or mixer ready message in update_all_nodes(),
 		     easiest but the least elegant
		*/
		
		if (!this._ready && (node == StateNode.RECORD_STATE)) {
 			this.updateLocal('ready', true);
		}

 		return false;
 	}

}
