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

require "rexml/document"
file = File.new( ARGV[0] )
doc = REXML::Document.new file

# fetch the node containing the controls
controls = XPath.first( doc, 'Session/ControlProtocols/Protocol[@name="Generic MIDI"]/controls' )

channel = 1

# A Control is a button or slider. It has an internal ID
# an incoming MIDI message, and an outgoing midi message
class Control
	
end

# Strips have solo,rec,mute,pan,fader
# Strips have midi input
# Strips have midi output
# Strips have an XML representation, or something like that
class Strip
	def initialize( node )
		@solo = node.elements['solo']
		@mute = node.elements['mute']
		@rec = node.elements['recenable']
		@fader = node.elements['IO/gaincontrol']
		@panner = node.elements['IO/Panner/StreamPanner/panner']
	end
end

# This knows how to extract a set of controls from a Route

doc.elements.each( 'Session/Routes/Route' ) do |node|
	strip = Strip.new( node )
	
   controls.add_element( 'mute',
		'id' => mute.attribute('id').value,
		'event' => "0xb0",
		'channel' => channel.to_s,
		'additional' => "0x41"
	)

end

pp controls.elements
