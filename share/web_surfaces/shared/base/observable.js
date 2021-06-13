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

export default class Observable {

	constructor () {
		this._observers = {};
	}

	addObserver (event, observer) {
		// event=undefined means the caller is interested in observing all events
		if (!(event in this._observers)) {
			this._observers[event] = [];
		}

		this._observers[event].push(observer);
	}

	removeObserver (event, observer) {
		// event=undefined means the caller is not interested in any event anymore
		if (typeof(event) == 'undefined') {
			for (const event in this._observers) {
				this.removeObserver(event, observer);
			}
		} else {
			const index = this._observers[event].indexOf(observer);
			if (index > -1) {
				this._observers[event].splice(index, 1);
			}
		}
	}

	notifyObservers (event, ...args) {
		// always notify observers that observe all events
		if (undefined in this._observers) {
			for (const observer of this._observers[undefined]) {
				observer(event, ...args);
			}
		}

		if (event in this._observers) {
			for (const observer of this._observers[event]) {
				observer(...args);
			}
		}
	}

}
