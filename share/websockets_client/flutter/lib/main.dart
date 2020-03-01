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

import 'dart:math';

import 'package:flutter/material.dart';
import 'client.dart';
import 'message.dart';

void main() => runApp(DemoApp());

class DemoApp extends StatelessWidget {
  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Ardour Demo',
      theme: ThemeData(
        primarySwatch: Colors.blueGrey,
      ),
      home: DemoHomePage(title: 'Ardour Client Demo Home Page'),
    );
  }
}

class DemoHomePage extends StatefulWidget {
  DemoHomePage({Key key, this.title}) : super(key: key);

  final String title;

  @override
  _DemoHomePageState createState() => _DemoHomePageState();
}

class _DemoHomePageState extends State<DemoHomePage> {
  final _client = ArdourClient().connect();
  double _strip0SliderValue = 0;

  _DemoHomePageState() {
    _client.stream.listen((msg) {
      if ((msg.node == Node.STRIP_GAIN) &&
          msg.addr.isNotEmpty &&
          (msg.addr[0] == 0) &&
          msg.val.isNotEmpty) {
        final sliderValue = min(pow(10.0, (msg.val[0] - 6.0) / 64.0), 1.0);
        setState(() => this._strip0SliderValue = sliderValue);
      }
    });
  }

  void _sendStrip0Gain(double sliderValue) {
    final db = 6.0 + 64.0 * log(sliderValue) / ln10;
    _client.setStripGain(0, db);
    setState(() => this._strip0SliderValue = sliderValue);
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      appBar: AppBar(
        title: Text(widget.title),
      ),
      body: Center(
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          children: <Widget>[
            Text('Gain for strip id = 0'),
            Slider(value: _strip0SliderValue, onChanged: _sendStrip0Gain),
            StreamBuilder<Message>(
              stream: _client.stream,
              builder: (BuildContext context, AsyncSnapshot<Message> snapshot) {
                if (snapshot.hasError) {
                  return Text('Error: ${snapshot.error}');
                } else {
                  return Text(snapshot.data.toString());
                }
                return null; // unreachable
              },
            )
          ],
        ),
      ),
    );
  }
}
