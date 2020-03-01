#!/usr/bin/env python3
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
import random
import sys

from asyncio_client import ArdourClient


async def demo():
    queue = asyncio.Queue()
    loop = asyncio.get_event_loop()
    loop.add_reader(sys.stdin.fileno(), lambda q: q.put_nowait(sys.stdin.readline()), queue)
    async with ArdourClient() as client:
        recv_task = asyncio.create_task(recv_coro(client))
        send_task = asyncio.create_task(ui_coro(client, queue))
        await asyncio.wait((recv_task, send_task))

async def recv_coro(client):
    async for msg in client.stream():   
        print(f'↙ {msg}')

async def ui_coro(client, queue):
    await asyncio.sleep(1)
    while True:
        print('\nPress Enter to print mute status for strip id = 0')
        await queue.get()
        is_mute = await client.get_strip_mute(0)
        print(f'Mute status for strip id = 0 is: {is_mute}')
        await asyncio.sleep(1)
        print('\nType a new gain value in dB for strip id = 0 and hit ⮐')
        try:
            val = float(await queue.get())
            await client.set_strip_gain(0, val)
            print('OK')
        except ValueError:
            print('Invalid value')
        await asyncio.sleep(1)


if __name__ == '__main__':
    try:
        asyncio.run(demo())
    except KeyboardInterrupt:
        print('Bye!')
