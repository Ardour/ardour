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

# TODO : type hints for values returned by all async methods

from typing import Coroutine
from websockets import connect

from message import Message


class ArdourWebsocket:

    def __init__(self, host: str, port: int) -> None:
        self._host = host
        self._port = port
        self._conn = None
        self._socket = None

    @property
    def url(self):
        return f'ws://{self._host}:{self._port}'

    async def connect(self):
        self._socket = await connect(self.url)

    async def close(self):
        await self._socket.close()

    async def stream(self):
        while True:
            yield await self.receive()

    async def receive(self):
        return Message.from_json(await self._socket.recv())

    async def send(self, msg: Message):
        await self._socket.send(msg.to_json())

    # allow using instances as async generators
    
    async def __aenter__(self):
        self._conn = connect(self.url)
        self._socket = await self._conn.__aenter__()        
        return self

    async def __aexit__(self, *args, **kwargs):
        await self._conn.__aexit__(*args, **kwargs)
