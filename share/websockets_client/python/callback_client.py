"""
  Copyright (C) 2020 Luciano Iam <lucianito@gmail.com>

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
"""

import asyncio
import threading

from typing import Any

from message import Node, Message, TypedValue, ValueList, AddressList
from websocket import ArdourWebsocket


"""
Creates a background thread to run the asyncio based client
"""

class ArdourClient:

    def __init__(self, host: str = '127.0.0.1', port: int = 9000) -> None:
        self._host = host
        self._port = port
        self._callbacks = []
        self._loop = None
        self._socket = None
        self._recv_task = None

    def add_callback(self, callback: Any) -> None:
        self._callbacks.append(callback)

    def remove_callback(self, callback: Any) -> None:
        self._callbacks.remove(callback)

    def connect(self) -> None:
        threading.Thread(target=lambda: asyncio.run(self._async_main())).start()

    def close(self) -> None:
        self._loop.call_soon_threadsafe(self._recv_task.cancel)
        self._loop = None
        self._socket = None
        self._recv_task = None

    def get_tempo(self) -> None:
        self._send(Node.TEMPO)

    def get_strip_gain(self, strip_id: int) -> None:
        self._send(Node.STRIP_GAIN, (strip_id,))

    def get_strip_pan(self, strip_id: int) -> None:
        self._send(Node.STRIP_PAN, (strip_id,))

    def get_strip_mute(self, strip_id: int) -> None:
        self._send(Node.STRIP_MUTE, (strip_id,))

    def get_strip_plugin_enable(self, strip_id: int, plugin_id: int) -> None:
        self._send(Node.STRIP_PLUGIN_ENABLE, (strip_id, plugin_id,))

    def get_strip_plugin_param_value(self, strip_id: int, plugin_id: int, param_id: int) -> None:
        self._send(Node.STRIP_PLUGIN_PARAM_VALUE, (strip_id, plugin_id, param_id,))

    def set_tempo(self, bpm: float) -> None:
        self._send(Node.TEMPO, (), (bpm,))

    def set_strip_gain(self, strip_id: int, db: float) -> None:
        self._send(Node.STRIP_GAIN, (strip_id,), (db,))

    def set_strip_pan(self, strip_id: int, value: float) -> None:
        self._send(Node.STRIP_PAN, (strip_id,), (value,))

    def set_strip_mute(self, strip_id: int, value: bool) -> None:
        self._send(Node.STRIP_MUTE, (strip_id,), (value,))

    def set_strip_plugin_enable(self, strip_id: int, plugin_id: int, value: bool) -> None:
        self._send(Node.STRIP_PLUGIN_ENABLE, (strip_id, plugin_id,), (value,))

    def set_strip_plugin_param_value(self, strip_id: int, plugin_id: int, param_id: int, value: TypedValue) -> None:
        self._send(Node.STRIP_PLUGIN_PARAM_VALUE, (strip_id, plugin_id, param_id,), (value,))

    def _send(self, node: Node, addr: AddressList = [], val: ValueList = []) -> None:
        if self._loop and self._socket:
            msg = Message(node, addr, val)
            asyncio.run_coroutine_threadsafe(self._socket.send(msg), self._loop)

    async def _async_main(self):
        async with ArdourWebsocket(self._host, self._port) as socket:
            self._loop = asyncio.get_event_loop()
            self._socket = socket
            self._recv_task = asyncio.create_task(self._recv_coro())
            await self._recv_task

    async def _recv_coro(self):
        try:
            async for msg in self._socket.stream():
                name = f'on_{msg.node.value}'
                args = msg.addr + msg.val

                for callback in self._callbacks:
                    if callable(callback):
                        callback(msg)

                    elif hasattr(callback, name):
                        getattr(callback, name)(*args)

        except asyncio.CancelledError:
            pass
