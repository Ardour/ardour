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

			-- solo the track -- and only the track, not other tracks grouped with it.
			--
			-- Note that changing solo/mute needs to propagate implicit solo/mute.
			-- These changes have to be done atomically, so that all
			-- related solo/mute change simultaneously at the same time.
			-- This can only be done from realtime-context, so we need to queue a session-rt
			-- event using the session realtime-event dispatch mechanism:
			Session:set_control (t:solo_control(), 1, PBD.GroupControlDisposition.NoGroup)

			-- unmute the track, this also examplifies how one could use lists to modify
			-- multiple controllables at the same time (they should be of the same
			-- parameter type - e.g. mute_control() of multiple tracks, they'll all
			-- change simultaneously in rt-context)
			local ctrls = ARDOUR.ControlListPtr ()
			ctrls:push_back (t:mute_control()) -- we could add more controls to change via push_back
			Session:set_controls (ctrls, 0, PBD.GroupControlDisposition.NoGroup)

			-- add a track comment
			t:set_comment ("This is a Drum Track", nil)

			-- and set the fader to -7dB  == 10 ^ (0.05 * -7)
			t:gain_control():set_value (10 ^ (0.05 * -7), PBD.GroupControlDisposition.NoGroup)
		end
	end
end end
