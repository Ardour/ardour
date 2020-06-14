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

export default class Observable {

	constructor () {
		this._observers = {};
	}

	addObserver (property, observer) {
		// property=undefined means the caller is interested in observing all properties
		if (!(property in this._observers)) {
			this._observers[property] = [];
		}

		this._observers[property].push(observer);
	}

	removeObserver (property, observer) {
		// property=undefined means the caller is not interested in any property anymore
		if (typeof(property) == 'undefined') {
			for (const property in this._observers) {
				this.removeObserver(property, observer);
			}
		} else {
			const index = this._observers[property].indexOf(observer);

			if (index > -1) {
				this._observers[property].splice(index, 1);
			}
		}
	}

	notifyObservers (property) {
		// always notify observers that observe all properties
		if (undefined in this._observers) {
			for (const observer of this._observers[undefined]) {
				observer(this, property);
			}
		}

		if (property in this._observers) {
			for (const observer of this._observers[property]) {
				observer(this);
			}
		}
	}

}
