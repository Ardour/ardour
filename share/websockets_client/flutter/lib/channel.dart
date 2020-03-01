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

import 'package:async/async.dart';
import 'package:stream_channel/stream_channel.dart';
import 'package:web_socket_channel/web_socket_channel.dart';

import 'message.dart';

class ArdourMessageChannel {
  static StreamChannel<Message> connect(Uri uri) {
    final streamTransformer = StreamTransformer<dynamic, Message>.fromHandlers(
        handleData: (dynamic event, EventSink output) {
      try {
        output.add(Message.fromJson(event));
      } catch (e) {
        output.addError(e);
      }
    });

    final streamSinkTransformer =
        StreamSinkTransformer<Message, dynamic>.fromHandlers(
            handleData: (Message event, EventSink output) {
      output.add(event.toJson());
    });

    final transformer = StreamChannelTransformer<Message, dynamic>(
        streamTransformer, streamSinkTransformer);

    final channel = WebSocketChannel.connect(uri);

    return channel.transform<Message>(transformer);
  }
}
