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

import json

from enum import Enum
from typing import List, Union


class Node(Enum):

    TEMPO                    = "tempo"
    STRIP_DESC               = "strip_desc"
    STRIP_METER              = "strip_meter"
    STRIP_GAIN               = "strip_gain"
    STRIP_PAN                = "strip_pan"
    STRIP_MUTE               = "strip_mute"
    STRIP_PLUGIN_DESC        = "strip_plugin_desc"
    STRIP_PLUGIN_ENABLE      = "strip_plugin_enable"
    STRUP_PLUGIN_PARAM_DESC  = "strip_plugin_param_desc"
    STRIP_PLUGIN_PARAM_VALUE = "strip_plugin_param_value"


TypedValue = Union[bool, int, float, str]
ValueList = List[TypedValue]
AddressList = [int]

JSON_INFINITY = 1.0e+128


class Message:

    def __init__(self, node: Node, addr: AddressList = [], val: ValueList = []) -> None:
        self.node = node
        self.addr = addr
        self.val = val

    def node_addr_hash(self):
        return self.node.value + '_' + '_'.join([str(a) for a in self.addr])

    @classmethod
    def from_json(cls, data: str) -> 'Message':
        msg = json.loads(data)
        val = []
        for v in msg.get('val', []):
            if isinstance(v, float):
                if v >= JSON_INFINITY:
                    val.append(float('inf'))
                elif v <= -JSON_INFINITY:
                    val.append(float('-inf'))
                else:
                    val.append(v)
            else:
                val.append(v)
        return Message(Node(msg['node']), msg.get('addr', []), val)

    def to_json(self) -> str:
        val = []
        for v in self.val:
            if isinstance(v, float):
                if v == float('inf'):
                    val.append(JSON_INFINITY)
                elif v == float('-inf'):
                    val.append(-JSON_INFINITY)
                else:
                    val.append(v)
            else:
                val.append(v)
        return json.dumps({'node': self.node.value, 'addr': self.addr, 'val': val})

    def __str__(self):
        return f'node = {self.node.value}, addr = {self.addr}, val = {self.val}'
