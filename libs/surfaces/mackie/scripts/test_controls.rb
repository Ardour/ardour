#! /usr/bin/ruby

require 'controls.rb'
require 'pp'

sf = Surface.new
sf.parse( ARGV[0] )
sf.types.each{|k,v| puts "%02.x #{v}" % k}

