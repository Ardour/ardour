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

this_dir = File.dirname(__FILE__)

require 'faster_csv'
require "#{this_dir}/mackie.rb"

class Control
  attr_accessor :id, :led, :group, :name, :ordinal, :switch
  
  def initialize( obj, group )
    @id = obj.id
    @name = obj.name
    @ordinal = obj.ordinal
    @switch = obj.switch
    @group = group
  end
  
  def ordinal_name
  end
end

class Fader < Control
  def self.midi_zero_byte
    0xe0
  end
  
  def self.mask_for_id( bytes )
    bytes[0] & 0b00001111
  end
end

class Button < Control
  def self.midi_zero_byte
    0x90
  end
  
  def self.mask_for_id( bytes )
    bytes[1]
  end
end

class Led < Control
end

class LedRing < Led
end

class Pot < Control
  def self.midi_zero_byte
    0xb0
  end
  
  def self.mask_for_id( bytes )
    bytes[1] & 0b00011111
  end

  def led=( rhs )
    @led = LedRing.new( rhs, group )
  end
end

class Group < Array
  attr_accessor :name, :controls
  
  def initialize( name )
    @name = name
  end
  
  def add_control( control )
    @controls ||= Array.new
    @controls << control
  end
end

class Strip < Group
  
  attr_accessor :ordinal
  def initialize( name, ordinal )
    super( name )
    @ordinal = ordinal
  end
  
  def name
    @name == 'master' ? @name : "#{@name}_#{@ordinal}"
  end
  
  def is_master
    name == 'master'
  end
  
end

types = { 0xe0 => Fader, 0x90 => Button, 0xb0 => Pot }

# number of controls, name, switch, led, id
# anything that doesn't have the correct number
# of columns will be ignored
# actually, 'switch' means it generates data
# whereas 'led' means it receives data

class Row
  attr_accessor :count, :name, :switch, :led, :start_id, :type, :group
  attr_accessor :id, :ordinal_name, :ordinal_group, :ordinal

  def initialize( hash )
    @count = hash['count'].to_i
    @name = hash['name']
    @switch = hash['switch'].to_b
    @led = hash['led'].to_b
    @start_id = hash['id'].hex
    @type = hash['type']
    @group = hash['group']
    
    @hash = hash
  end
  
  def each_ordinal( &block )
    for i in 0...count
      @ordinal = i + 1
      @ordinal_name = count > 1 ? "#{name}_#{ordinal}" : name
      @ordinal_group = count > 1 ? "#{group}_#{ordinal}" : group
      @id = start_id + i
      
      @hash['ordinal_name'] = @ordinal_name
      @hash['ordinal_group'] = @ordinal_group
      
      yield( self )
    end
    self
  end
  
  def to_hash
    @hash
  end
end

class Surface
  attr_reader :groups, :controls_by_id, :types, :midis, :controls, :name
  
  def initialize( name = 'none' )
    @name = name
    @types = Hash.new
    @groups = Hash.new
    @controls = Array.new
    @controls_by_id = Hash.new
    @midis = Hash.new
  end
  
  def add_or_create_group( name, ordinal = nil )
    if name.nil?
      @groups['none'] = Group.new('none')
    else
      group = name =~ /strip/ || name == 'master' ? Strip.new( name, ordinal ) : Group.new( name )
      @groups[group.name] ||= group
    end
  end

  def parse( control_data )
    FasterCSV.parse( control_data, :headers => true ) do |csv_row|
      next if csv_row.entries.size < 5 || csv_row[0] =~ /^\s*#/ || csv_row['id'].nil?
      row = Row.new( csv_row )
      
      row.each_ordinal do |row|
        group = add_or_create_group( row.group, row.ordinal )
        if row.switch
          # for controls
          control = eval "#{row.type.capitalize}.new( row, group )"
          
          # for controls with leds
          control.led = Led.new( row, group ) if row.led
        else
          # for LED-only entries
          if row.led
            control = Led.new( row, group )
            control.led = control
          end
        end
        
        # add the new control to the various lookups
        # but first print a warning if the id is duplicated
        if @controls_by_id.has_key?( row.id ) && control.group.class != Strip
          duplicated = @controls_by_id[row.id]
          puts "duplicate id #{control.id}:#{control.name} of #{duplicated.id}:#{duplicated.name}"
        end
        
        @controls_by_id[row.id] = control
        @controls << control
        group << control
        
        # add incoming midi bytes
        if row.switch
          types[control.class.midi_zero_byte] = control.class
          midis[control.class.midi_zero_byte] ||= Hash.new
          midis[control.class.midi_zero_byte][row.id] = control
        end
      end
    end
    self
  end
end
