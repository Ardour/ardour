ardour {
	["type"]    = "session",
	name        = "Mixer Scene Sequencer",
	license     = "MIT",
	author      = "John Devlin, Robin Gareus",
	description = [[Recall a Mixer Scene when the playhead passes over a Marker named 'MS <nuber>' where <number> indicates the scene to recall.]]
}

function factory ()

	local MIXER_SCENE_MARKER_PREFIX = "MS "

	return function (n_samples)
		if (not Session:transport_rolling()) then
			-- not rolling, nothing to do.
			return
		end

		local pos = Session:transport_sample() -- current playhead position
		local loc = Session:locations() -- all marker locations

		local tpos = Temporal.timepos_t(pos)
		local mpos = loc:first_mark_after(tpos, false)

		if (mpos == Temporal.timepos_t.max(tpos:time_domain())) then
			-- no marker was found
			return
		end

		local mloc = loc:first_mark_at(mpos, Temporal.timecnt_t(0))
		if not mloc then
			-- no marker found at that location
			return
		end

		if (pos + n_samples > mpos:samples()) then
			-- the marker is within this sample block
			if mloc:name():sub(1, MIXER_SCENE_MARKER_PREFIX:len()) == MIXER_SCENE_MARKER_PREFIX then
				-- the marker name begins with MIXER_SCENE_MARKER_PREFIX: get the remainder of the name (should be a number)
				local msNum = tonumber(mloc:name():sub(MIXER_SCENE_MARKER_PREFIX:len() + 1))
				if msNum and msNum >= 1 and msNum <= 8 then
					Session:apply_nth_mixer_scene(msNum - 1)
				end
			end
		end

	end
end
