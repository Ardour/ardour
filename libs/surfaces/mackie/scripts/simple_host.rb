#! /usr/bin/ruby
# Copyright (C) 2006,2007 John Anderson

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

require 'mackie'

buttons = {}
pots = {}

while !File.exist? ARGV[0]
  sleep 0.010
end

file = File.open( ARGV[0], 'r+' )
mck = Mackie.new( file )

# faders to minimum. bcf2000 doesn't respond
mck.write_sysex "\x61"

# all leds off. bcf2000 doesn't respond
mck.write_sysex "\x62"

# get version. comes back as ASCII bytes
version = mck.sysex "\x13\x00"
puts "version: #{version.map{|x| x.chr}}"

# respond to control movements
while bytes = file.read( 3 )
  puts "received: %02.x %02.x %02.x" % [ bytes[0], bytes[1], bytes[2] ]
  output = nil
  case bytes[0] & 0b11110000
  when 0xe0
    # fader moved, so respond if move is OK
    output = bytes
  when 0x90
    # button pressed
    case bytes[1]
    when 0x68..0x6f
      # do nothing - touch detection
      puts "touch detect: %02.x" % bytes[2]
    else
      # treat all buttons as toggles
      button_id = bytes[1]
      
      # only toggle on release. Not working. All buttons send press
      # and then release signals
      if bytes[2] == 0
        if buttons.include?( button_id )
          # toggle button state
          puts "button id #{buttons[button_id]} to #{!buttons[button_id]}"
          buttons[button_id] = !buttons[button_id]
        else
          # create a new button as on
          puts "adding button id #{button_id}"
          buttons[button_id] = true
        end
        bytes[2] = buttons[button_id] ? 0x7f : 0
        output = bytes
      end
    end
  when 0xb0
    # pots, jog wheel, external
    case bytes[1]
    when 0x10..0x17
      #pot turned
      pot_id = bytes[1] & 0b00000111
      direction = bytes[2] & 0b01000000
      delta = bytes[2] & 0b00111111
      sign = direction == 0 ? 1 : -1
      
      if pots.include? pot_id
        current_led_pos = pots[pot_id]
      else
        current_led_pos = pots[pot_id] = 6
      end
      new_led_pos = current_led_pos + sign
      new_led_pos = case
        when new_led_pos <= 0
          0
        when new_led_pos >= 11
          11
        else
          new_led_pos
      end
        
      pots[pot_id] = new_led_pos
      
      puts "pot #{pot_id} turned #{sign} #{direction == 0 ? 'clockwise' : 'widdershins'}: %02.x to #{new_led_pos}" % delta
      
      output = bytes
      output[1] += 0x20
      output[2] = 0b01000000
      #~ modes:
      #~ 0 - single dot
      #~ 1 - boost/cut
      #~ 2 - wrap
      #~ 3 - spread
      mode = pot_id < 4 ? pot_id : 0
      output[2] |= ( mode << 4 )
      output[2] += ( new_led_pos ) & 0b00001111
    when 0x2e
      # external controller
    when 0x3c
      # jog wheel
    end
  else
    puts "don't know what this means"
  end
  
  # output bytes
  if output
    #sleep 0.1
    puts "sending: %02.x %02.x %02.x" % [ output[0], output[1], output[2] ]
    begin
      res = file.write output
      puts "res: #{res}"
      file.flush
    rescue => e
      puts "oops #{e}"
      file.close
      file = File.open ARGV[0], 'r+'
    end
  end
end
