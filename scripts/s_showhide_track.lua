ardour { ["type"] = "Snippet", name = "Show/Hide TimeAxisView" }

function factory () return function ()
	-- get a route from the session by Presentation-Order
	-- http://ardourman/lua-scripting/class_reference/#ARDOUR:Session
	local route = Session:get_remote_nth_route(2)
	assert (route) -- abort if it does not exist
	print (route:name())

	-- the GUI timeline representation of a Track/Bus is a "Route Time Axis View" Object
	local rtav = Editor:rtav_from_route (route) -- lookup RTAV

	-- the show/hide state applies to any "Time Axis View", cast RTAV to TAV.
	Editor:hide_track_in_display (rtav:to_timeaxisview(), false --[[true: only if selected; false: any]])


	-- look up the route named "Audio"
	route = Session:route_by_name("Audio")
	assert (route) -- abort if it does not exist

	Editor:show_track_in_display (Editor:rtav_from_route (route):to_timeaxisview(), false --[[move into view]])

end end
