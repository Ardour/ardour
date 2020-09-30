ardour {
	["type"]    = "dsp",
	name        = "ACE Cross Fade",
	category    = "Amplifier",
	license     = "MIT",
	author      = "Ardour Community",
	description = [[Auotomatable Crossfade. Channels are grouped:
Mono out:  In 1/2 -> Out 1
Stereo out: In 1/3 -> Out 1, In 2/4 -> Out 2
Quad out: In 1/5 -> Out 1, In 2/6 -> Out 2, In 3/7 -> Out 3, In 4/8 -> Out 4
]]
}

function dsp_ioconfig ()
	return {
		connect_all_audio_outputs = true, -- override strict-i/o
		-- in theory any combination with N_in = 2 * N_out is possible
		{ audio_in = 2, audio_out = 1},
		{ audio_in = 4, audio_out = 2},
		{ audio_in = 8, audio_out = 4},
	}
end

function dsp_params ()
	return { { ["type"] = "input", name = "A/B", min = 0, max = 1, default = 0} }
end

local sr = 48000
local cur_a = 0.0
local cur_b = 0.0

local n_aout = 0

function dsp_init (rate)
	sr = rate
end

function dsp_configure (ins, outs)
	n_ainp = ins:n_audio ()
	n_aout = outs:n_audio ()
	assert (n_aout * 2 == n_ainp)
end

-- the DSP callback function
function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local ctrl = CtrlPorts:array() -- get control port array
	local target_B = ctrl[1]
	local target_A = 1 - target_B

	local gA = cur_a
	local gB = cur_b

	for c = 1, n_aout do
		local o = out_map:get (ARDOUR.DataType ("audio"), c - 1)
		if o == ARDOUR.ChanMapping.Invalid then
			goto next
		end

		local in_a = c
		local in_b = c + n_aout
		local ia = in_map:get (ARDOUR.DataType ("audio"), in_a - 1)
		local ib = in_map:get (ARDOUR.DataType ("audio"), in_b - 1)

		local buf_aout = bufs:get_audio(o)

		-- optimize hard A/B fixed gain case (copy buffers)
		if cur_a == target_A and cur_b == target_B then
			if target_A == 1.0 then
				if ia == ARDOUR.ChanMapping.Invalid then
					buf_aout:silence (n_samples, offset)
				elseif buf_aout ~= bufs:get_audio(ia) then
					buf_aout:read_from (bufs:get_audio(ia):data (0), n_samples, offset, offset)
				end
				goto next
			elseif target_B == 1.0 then
				if ib == ARDOUR.ChanMapping.Invalid then
					buf_aout:silence (n_samples, offset)
				else
					assert (buf_aout ~= bufs:get_audio(ib))
					buf_aout:read_from (bufs:get_audio(ib):data (0), n_samples, offset, offset)
				end
				goto next
			end
		end

		-- apply gain to each input channel in-place
		if ia ~= ARDOUR.ChanMapping.Invalid and ia ~= ib then
			cur_a = ARDOUR.Amp.apply_gain (bufs:get_audio(ia), sr, n_samples, gA, target_A, offset)
		end
		if ib ~= ARDOUR.ChanMapping.Invalid and ia ~= ib then
			cur_b = ARDOUR.Amp.apply_gain (bufs:get_audio(ib), sr, n_samples, gB, target_B, offset)
		end

		-- copy input to output if needed (first set of channels may be in-place)
		if ia == ARDOUR.ChanMapping.Invalid then
			buf_aout:silence (n_samples, offset)
		elseif buf_aout ~= bufs:get_audio(ia) then
			buf_aout:read_from (bufs:get_audio(ia):data (0), n_samples, offset, offset)
		end

		-- add the second buffer
		if ib ~= ARDOUR.ChanMapping.Invalid then
			ARDOUR.DSP.mix_buffers_no_gain (buf_aout:data (offset), bufs:get_audio(ib):data (offset), n_samples)
		end

		::next::
	end
end
