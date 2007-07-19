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

require 'controls.rb'
require 'mackie.rb'

if ARGV.size != 2
  puts "#$0 /dev/snd/midiXXXX control-file.csv"
  exit 1
end

while !File.exist? ARGV[0]
  sleep 0.010
end

#mapping_csv = ARGV[1] || "mackie-controls.csv"
mapping_csv = ARGV[1]
puts "mapping_csv: #{mapping_csv}"
puts ""

file = File.open ARGV[0], 'r+'
mck = Mackie.new( file )

# faders to minimum. bcf2000 doesn't respond
mck.write_sysex "\x61"

# all leds off. bcf2000 doesn't respond
mck.write_sysex "\x62"

# get version. comes back as ASCII bytes
version = mck.sysex "\x13\x00"
puts "version: #{version.map{|x| x.chr}}"

# write a welcome message. bcf2000 responds with exact
# string but doesn't display anything
# 0 offset,
#~ file.write hdr + "\x12\x3fLCDE\xf7"
#~ file.flush
#~ answer = read_sysex file
#~ puts "answer: #{answer[hdr.length..-1].map{|x| x.chr}}"

# write to BBT display
#~ file.write hdr + "\x10LCDE\xf7"
#~ file.flush
#~ bbt = []
#~ while ( nc = file.read( 1 ) )[0] != 0xf7
  #~ bbt << nc[0]
#~ end
#~ puts "bbt: #{bbt[hdr.length..-1].map{|x| x.chr}}"

# write 7-segment display
#~ file.write hdr + "\x11LCDE\xf7"
#~ file.flush

# go offline. bcf2000 doesn't respond
#~ file.write( hdr + "\x0f\x7f\xf7" )
#~ file.flush

sf = Surface.new
control_data = ""
File.open( mapping_csv ) { |f| control_data = f.read }
sf.parse( control_data )

# send all faders to 0, but bounce them first
# otherwise the bcf gets confused
sf.midis[0xe0].values.find_all{|x| x.class == Fader}.each do |x|
  bytes = Array.new
  bytes[0] = 0xe0 + x.ordinal - 1
  bytes[1] = 0x1
  bytes[2] = 0x1
  file.write bytes.pack( 'C*' )
  bytes[0] = 0xe0 + x.ordinal - 1
  bytes[1] = 0x0
  bytes[2] = 0x0
  file.write bytes.pack( 'C*' )
end
file.flush

# respond to control movements
while bytes = mck.file.read( 3 )
  print "received: %02.x %02.x %02.x" % [ bytes[0], bytes[1], bytes[2] ]
  midi_type = bytes[0] & 0b11110000

  control_id = sf.types[midi_type].mask_for_id( bytes )
  control = sf.midis[midi_type][control_id]
  
  print " Control Type: %-7s, " % sf.types[midi_type]
  print "id: %4x" % control_id
  print ", control: %15s" % ( control ? control.name : "nil control" )
  print ", %15s" % ( control ? control.group.name : "nil group" )
  print "\n"
end
