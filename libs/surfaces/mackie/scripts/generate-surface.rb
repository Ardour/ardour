#! /usr/bin/ruby

this_dir = File.dirname(__FILE__)

require 'erb'

require this_dir + '/controls.rb'

cc_template = ''
File.open( this_dir + "/surface-cc-template.erb", "r" ) { |f| cc_template = f.read }

h_template = ''
File.open( this_dir + "/surface-h-template.erb", "r" ) { |f| h_template = f.read }

sf = Surface.new( ARGV[0] )
control_data = ''
File.open( this_dir + "/#{sf.name.downcase}-controls.csv", "r") { |f| control_data = f.read }
sf.parse control_data

@result = ""
erb = ERB.new( cc_template , 0, "%<>-", "@result" )
erb.result
File.open( "#{sf.name.downcase}_surface.cc", "w" ) { |f| f.write @result }

erb = ERB.new( h_template , 0, "%<>-", "@result" )
erb.result
File.open( "#{sf.name.downcase}_surface.h", "w" ) { |f| f.write @result }

