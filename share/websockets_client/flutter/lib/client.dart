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

import 'dart:async';
import 'package:stream_channel/stream_channel.dart';

import 'channel.dart';
import 'message.dart';

class ArdourClient {
  StreamChannel<Message> _channel;
  Uri _uri;
  Stream<Message> _stream;

  ArdourClient({host = '127.0.0.1', port = 9000}) {
    this._uri = Uri.parse('ws://$host:$port');
  }

  Stream<Message> get stream {
    return _stream;
  }

  ArdourClient connect() {
    this._channel = ArdourMessageChannel.connect(this._uri);
    this._stream = _channel.stream.asBroadcastStream();
    return this;
  }

  Future<double> getTempo() async {
    return (await _sendAndReceive(Node.TEMPO, [], []))[0];
  }

  Future<double> getStripGain(int stripId) async {
    return (await _sendAndReceive(Node.STRIP_GAIN, [stripId], []))[0];
  }

  Future<double> getStripPan(int stripId) async {
    return (await _sendAndReceive(Node.STRIP_PAN, [stripId], []))[0];
  }

  Future<bool> getStripMute(int stripId) async {
    return (await _sendAndReceive(Node.STRIP_MUTE, [stripId], []))[0];
  }

  Future<bool> getStripPluginEnable(int stripId, int pluginId) async {
    return (await _sendAndReceive(
        Node.STRIP_PLUGIN_ENABLE, [stripId, pluginId], []))[0];
  }

  Future<dynamic> getStripPluginParamValue(
      int stripId, int pluginId, int paramId) async {
    return (await _sendAndReceive(
        Node.STRIP_PLUGIN_PARAM_VALUE, [stripId, pluginId, paramId], []))[0];
  }

  void setTempo(double bpm) {
    this._send(Node.TEMPO, [], [bpm]);
  }

  void setStripGain(int stripId, double db) {
    this._send(Node.STRIP_GAIN, [stripId], [db]);
  }

  void setStripPan(int stripId, double value) {
    this._send(Node.STRIP_PAN, [stripId], [value]);
  }

  void setStripMute(int stripId, bool value) {
    this._send(Node.STRIP_MUTE, [stripId], [value]);
  }

  void setStripPluginEnable(int stripId, int pluginId, bool value) {
    this._send(Node.STRIP_PLUGIN_ENABLE, [stripId, pluginId], [value]);
  }

  void setStripPluginParamValue(
      int stripId, int pluginId, int paramId, dynamic value) {
    this._send(
        Node.STRIP_PLUGIN_PARAM_VALUE, [stripId, pluginId, paramId], [value]);
  }

  Message _send(Node node, List<int> addr, List<dynamic> val) {
    final msg = Message(node, addr, val);
    this._channel.sink.add(msg);
    return msg;
  }

  Future<List<dynamic>> _sendAndReceive(
      Node node, List<int> addr, List<dynamic> val) async {
    final hash = this._send(node, addr, val).nodeAddrHash();
    final respMsg =
        await this.stream.firstWhere((msg) => msg.nodeAddrHash() == hash);
    return respMsg.val;
  }
}
