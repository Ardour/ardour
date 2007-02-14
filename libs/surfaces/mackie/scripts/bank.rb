#! /usr/bin/ruby

class Bank
	attr_accessor :routes, :strips, :current
	
	def initialize( routes = 17, strips = 8, current = 0 )
		@routes = routes
		@strips = strips
		@current = current
	end
	
	def left
    new_initial = current - routes
    if new_initial < 0
      new_initial = 0
    end
    current = new_initial
    self
	end
	
	def right
    delta = routes - ( strips + current ) - 1
    puts "delta: #{delta}"
    if delta > strips
      delta = strips
    end
    @current += delta
    self
	end
end

b=Bank.new
