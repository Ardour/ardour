/*
 * Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>
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

const JSON_INF = 1.0e+128;

class Connection {

    constructor (host, port) {
        this.socket = new WebSocket(`ws://${host}:${port}`);
        this.socket.onopen = () => this.openCallback();
        this.socket.onclose = () => this.closeCallback();
        this.socket.onerror = (error) => this.errorCallback(error);
        this.socket.onmessage = (event) => this._onMessage(event);
    }
    
    openCallback () {
        // empty
    }

    closeCallback () {
        // empty
    }
    
    errorCallback (error) {
        // empty
    }

    messageCallback (node, addr, val) {
        // empty
    }

    send (node, addr, val) {
        for (const i in val) {
            if (val[i] == Infinity) {
                val[i] = JSON_INF;
            } else if (val[i] == -Infinity) {
                val[i] = -JSON_INF;
            }
        }
        
        const json = JSON.stringify({node: node, addr: addr, val: val});

        this.socket.send(json);
    }

    _onMessage (event) {
        const msg = JSON.parse(event.data);

        for (const i in msg.val) {
            if (msg.val[i] >= JSON_INF) {
                msg.val[i] = Infinity;
            } else if (msg.val[i] <= -JSON_INF) {
                msg.val[i] = -Infinity;
            }
        }

        this.messageCallback(msg.node, msg.addr || [], msg.val);
    }

}
