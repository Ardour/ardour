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

import { Channel } from './channel.js';

export class Ardour {

	constructor () {
		this.channel = new Channel(location.host);
	}

	async open () {
		await this.channel.open();
	}

	close () {
		this.channel.close();
	}

	async getAvailableSurfaces () {
		const response = await fetch('/index.json');
		
		if (response.status == 200) {
			return await response.json();
		} else {
			throw new Error(`HTTP response status ${response.status}`);
		}
	}

	// TO DO - add methods for dealing with messages flowing from/to the WebSockets channel

}
