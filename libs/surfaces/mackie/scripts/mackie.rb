class String
  def to_bytes
    arr = []
    each_byte{|x| arr << x}
    arr
  end
end

class Array
  def to_hex
    map{|x| "%2.0x" % x}
  end

  alias as_hex to_hex
end

class String
  def to_b
    to_i != 0 || %w{true t yes y}.include?( self.downcase )
  end
end

class Fixnum
  def to_hex
    "%02x" % self
  end
end

class Mackie
  attr_accessor :file
  
	def initialize( file )
		@file = file
	end
	
	# send and receive a sysex message
  # after wrapping in the header and the eox byte
	def sysex( msg )
		puts "Mackie write: #{msg.unpack('C*').to_hex.inspect}"
		write_sysex( msg )
		response = read_sysex
		puts "Mackie response: #{response.to_hex.inspect}"
		response[5..-1]
	end
	
	# returns an array of bytes
	def read_sysex
	  buf = []
	  while ( nc = @file.read( 1 ) )[0] != 0xf7
      buf << nc[0]
	  end
	  buf
  end
  
	# send and flush a sysex message
  # after wrapping in the header and the eox byte
  def write_sysex( msg )
    @file.write( hdrlc + msg + "\xf7" )
    @file.flush
  end
  
  def write( msg )
    @file.write msg
    @file.flush
  end
  
  def translate_seven_segment( char )
    case char
      when 0x40..0x60
        char - 0x40
      when 0x21..0x3f
        char
      else
        0x00
    end
  end
  
  # display the msg (which can be only 2 characters)
  # append the number of stops. Options are '..', '. ', '. ', '  '
  def two_char( msg, stops = '  ' )
    two = Array.new
    two << translate_seven_segment( msg.upcase[0] )
    two << translate_seven_segment( msg.upcase[1] )
    
    two[0] += 0x40 if stops[0] == '.'[0]
    two[1] += 0x40 if stops[1] == '.'[0]
    
    midi_msg = [0xb0, 0x4a, two[1], 0x4b, two[0] ]
    write midi_msg.pack( 'C*' )
  end
  
  # send and receive the device initialisation
  def init
    response = sysex( "\x00" )

    # decode host connection query
    status = response[0]
    raise( "expected 01, got " + response.inspect ) if status != 1
    
    serial = response[1..7]
    challenge = response[8..11]

    # send host connection reply
    reply = "\x02" + serial.pack('C*') + challenge.pack('C*')
    response = sysex reply

    # decode host connection confirmation
    status = response[0]
    raise ( "expected 03, got " + response.inspect ) if status != 3
  end

	def hdrlc
		"\xf0\x00\x00\x66\x10"
	end
	
	def hdrlcxt
		"\xf0\x00\x00\x66\x11"
	end
end
