ardour {
	["type"]    = "dsp",
	name        = "Simple Amp II",
	category    = "Example",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[
	An Example DSP Plugin for processing audio, to
	be used with Ardour's Lua scripting facility.]]
}

-- see amp1.lua
function dsp_ioconfig ()
	return { [1] = { audio_in = -1, audio_out = -1}, }
end

function dsp_configure (ins, outs)
	audio_ins = ins:n_audio();
	local audio_outs = outs:n_audio()
	assert (audio_ins == audio_outs)
end


-- this variant modifies the audio data in-place
-- in Ardour's buffer.
--
-- It relies on the fact that by default Ardour requires
-- plugins to process data in-place (zero copy).
--
-- Every assignment directly calls a c-function behind
-- the scenes to get/set the value.
-- It's a bit more efficient than "Amp I" on most systems.

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	for c = 1,audio_ins do
		-- ensure that processing does happen in-place
		local ib = in_map:get(ARDOUR.DataType("audio"), c - 1); -- get id of mapped input buffer for given cannel
		local ob = out_map:get(ARDOUR.DataType("audio"), c - 1); -- get id of mapped output buffer for given cannel
		assert (ib ~= ARDOUR.ChanMapping.Invalid);
		assert (ob ~= ARDOUR.ChanMapping.Invalid);

		local bi = bufs:get_audio(ib)
		local bo = bufs:get_audio(ob)
		assert (bi == bo)

		local a = bufs:get_audio(ib):data(offset):array() -- get a reference (pointer to array)
		for s = 1,n_samples do
			a[s] = a[s] * 2; -- modify data in-place (shared with ardour)
		end
	end
end
