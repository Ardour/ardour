#! /usr/bin/ruby

require 'mackie.rb'

@file = File.open '/dev/snd/midiC2D0', 'r+'

@led_8_on = [ 0x90, 0x18, 0x7f ]
@hci = [ 0, 0xf7 ]
@version_req = [ 0x13, 0, 0xf7 ]

