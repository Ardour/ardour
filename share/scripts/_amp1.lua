ardour {
	["type"]    = "dsp",
	name        = "Simple Amp",
	category    = "Example",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[
	An Example DSP Plugin for processing audio, to
	be used with Ardour's Lua scripting facility.]]
}


-- return possible i/o configurations
function dsp_ioconfig ()
	-- -1, -1 = any number of channels as long as input and output count matches
	return { [1] = { audio_in = -1, audio_out = -1}, }
end

-- optional function, called when configuring the plugin
function dsp_configure (ins, outs)
	-- store configuration in global variable
	audio_ins = ins:n_audio();
	local audio_outs = outs:n_audio()
	assert (audio_ins == audio_outs)
end

-- this variant asks for a complete *copy* of the
-- audio data in a lua-table.
-- after processing the data is copied back.
--
-- this also exemplifies the direct "connect and run" process function,
-- where the channel-mapping needs to be done in lua.

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	for c = 1,audio_ins do
		-- Note: lua starts counting at 1, ardour's ChanMapping::get() at 0
		local ib = in_map:get(ARDOUR.DataType("audio"), c - 1); -- get id of mapped input buffer for given channel
		local ob = out_map:get(ARDOUR.DataType("audio"), c - 1); -- get id of mapped output buffer for given channel
		assert (ib ~= ARDOUR.ChanMapping.Invalid);
		assert (ob ~= ARDOUR.ChanMapping.Invalid);
		local a = bufs:get_audio (ib):data (offset):get_table(n_samples) -- copy audio-data from input buffer
		for s = 1,n_samples do
			a[s] = a[s] * 2; -- amplify data in lua table
		end
		bufs:get_audio(ob):data(offset):set_table(a, n_samples) -- copy back
	end
end
