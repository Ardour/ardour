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

import time

from callback_client import ArdourClient


class DemoListener:

    """
    See Node class in message.py to know what other callbacks are available
    """

    def on_strip_gain(self, strip_id, db):
        print(f'Strip id = {strip_id} gain was set to {db} dB')

    def on_strip_mute(self, strip_id, value):
        print(f'Strip id = {strip_id} mute was set to {value}')


def catch_all(msg):
     print(f'↙ {msg}')


if __name__ == '__main__':
    # create the client, no action performed other than object construction
    client = ArdourClient()

    # callbacks can be functions that accept an incoming Message as the argument
    client.add_callback(catch_all)

    # can also pass an object with multiple callbacks
    client.add_callback(DemoListener())

    # connect and start listening to the websocket from a background thread
    client.connect()

    try:
        while True:
            time.sleep(1)
            val = input('\nType a new gain value in dB for strip id = 0 and hit ⮐\n')
            try:
                client.set_strip_gain(0, float(val))
                print('OK')
            except ValueError:
                print('Invalid value')
    except KeyboardInterrupt:
        # client background thread is not daemonic, closing connection will end it
        client.close()
        print('Bye!')
