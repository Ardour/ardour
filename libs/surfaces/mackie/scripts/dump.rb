#! /usr/bin/ruby

while !File.exist? ARGV[0]
  sleep 0.010
end

file = File.open ARGV[0], 'r'

while bytes = file.sysread( 3 )
  puts "%02x %02x %02x" % [ bytes[0], bytes[1], bytes[2] ]
end
