ardour {
	["type"]    = "EditorAction",
	name        = "Faders to Trims",
	license     = "MIT",
	author      = "PSmith",
	description = [[Add 'Trim' plugins to all tracks. Move the fader value into the trim.]]
}

function action_params ()
	return
	{
	}
end


function factory (params)
	return function ()
		-- loop over all tracks
		for t in Session:get_tracks():iter() do
			
			fader_value = t:gain_control():get_value()
			if fader_value == 1 then
				goto skip
			end
			if t:gain_control():automation_state() ~= ARDOUR.AutoState.Off then
				goto skip
			end

			-- TODO: skip MIDI tracks without or with a post-fader synth
			-- (fader is MIDI-velocity)

			v = math.log(fader_value, 10)
			trim_gain = 20*v
			fader_pos = 0
			local proc;
			local i = 0;
			
			repeat
				-- find the fader proc
				proc = t:nth_processor (i)
				if (not proc:isnil() and proc:display_name () == "Fader") then
					fader_pos = i
				end
				i = i + 1
			until proc:isnil()
			
			-- apply the trim
			trim = t:nth_processor (fader_pos+1)
			if (not trim:isnil() and trim:display_name () == "ACE Amplifier") then
				--existing trim found; sum the trim and the fader gain, and set the trim to that value
				cur_gain = ARDOUR.LuaAPI.get_processor_param (trim, 0)
				ARDOUR.LuaAPI.set_processor_param (trim, 0, trim_gain+cur_gain)
			else
				--create a new Trim processor, and set its value to match the fader
				local a = ARDOUR.LuaAPI.new_luaproc(Session, "ACE Amplifier");
				if (not a:isnil()) then
					t:add_processor_by_index(a, fader_pos-1, nil, true)
					ARDOUR.LuaAPI.set_processor_param (a, 0, trim_gain)
					a = nil -- explicitly drop shared-ptr reference
				end
			end

			--zero the fader gain
			t:gain_control():set_value(1, PBD.GroupControlDisposition.NoGroup)

			::skip::

		end --foreach track

	end  --function

end --factory
