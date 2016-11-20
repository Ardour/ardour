ardour { ["type"] = "Snippet", name = "Track Properties" }

function factory () return function ()
	--- iterate over all tracks
	for t in Session:get_tracks():iter() do
		-- t is-a http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Track

		-- operate one those with "Drum" in the name
		if  (t:name ():find ("Drum")) then

			-- print the name, and number of audio in/out
			-- see also http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:ChanCount
			print (t:name (), "| Audio In:", t:n_inputs ():n_audio (), "Audio Out:", t:n_outputs ():n_audio ())

			-- get the track's http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:PresentationInfo
			pi = t:presentation_info_ptr ()

			-- set the track's color to orange - hex RGBA
			pi:set_color (0xff8800ff)

			-- phase invert the 1st channel
			t:phase_control():set_phase_invert (1, true)

			-- solo the track  -- and only the track, 
			-- not other tracks grouped with it.
			t:solo_control():set_value (1, PBD.GroupControlDisposition.NoGroup)

			-- unmute the track
			t:mute_control():set_value (0, PBD.GroupControlDisposition.NoGroup)

			-- add a track comment
			t:set_comment ("This is a Drum Track", nil)

			-- and set the fader to -7dB  == 10 ^ (0.05 * -7)
			t:gain_control():set_value (10 ^ (0.05 * -7), PBD.GroupControlDisposition.NoGroup)
		end
	end
end end
