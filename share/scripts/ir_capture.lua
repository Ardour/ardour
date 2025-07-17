ardour {
	["type"] = "dsp",
	name = "Lua IR Capture",
	license = "MIT",
	author = "Ardour Team",
	description = [[
	1. Calibrate I/O Latency (Menu > Window > Audio/MIDI Setup)
	2. Create a mono track - this will be used to record the IR.
	3. Disconnect the input and output of that track.
	4. Add a mono bus and load this plugin on the bus.
	5. Connect the left output of the bus to the mono track's input.
	   The deconvolved Impulse Response will be output on that channel.
	6. Connect the right output of the bus to a hardware playback port, matching the system you want to capture.
	   A sine-sweep will be played on that channel.
	7. Connect the bus' input to the return signal (hardware capture) of the system under test.
	8. Record Arm the track (from 2)
	9. Record Arm the session
	10. Open the GUI of this plugin, enable "Capture"
	11. Roll the transport.
	12. Wait 10 sec (recording stops automatically).
	13. Select the region on the mono track: Region > Edit > Strip Silence and/or manually tweak the region start:
	    move it close to the initial impulse.
	14. Region > Gain > Normalize, or manually adjust gain.
	15. Disable region-fades of the region: Region > Fades
	16. Export the region to save the IR file (Region > Export)
	]]
}

function dsp_ioconfig () return
	{
		connect_all_audio_outputs = true, -- override strict-i/o
		{ audio_in = 1, audio_out = 2},
	}
end

function dsp_params ()
	return {
		{ ["type"] = "input", name = "Capture", min = 0, max = 1, default = 0, toggled = true },
		{ ["type"] = "output", name = "State", min = 0, max = 2, enum = true, scalepoints =
			{
				["Idle"] = 0,
				["Capturing"] = 1,
				["Finalize"] = 2,
			}
		},
		{ ["type"] = "output", name = "IR Signal Level", min = -120, max = 0 },
	}
end

local conv
local state
local sweep_sin
local peak
local rec_len
local n_captured
local n_playback

function gen_sweep (fmin, fmax, t_sec, rate)
	local n_samples_pre = rate * 0.1
	local n_samples_sin = rate * t_sec
	local n_samples_end = rate * 0.03
	local n_samples = n_samples_pre + n_samples_sin + n_samples_end

	local amp = 0.5

	local a = math.log (fmax / fmin) / n_samples_sin
	local b = fmin / (a * rate)
	local r = 4.0 * a * a / amp

	local cmem = ARDOUR.DSP.DspShm (n_samples * 2)
	local ss = cmem:to_float (0):array()
	local si = cmem:to_float (n_samples):array()

	for i = 0, n_samples - 1 do
		local j = n_samples - i - 1

		local gain = 1.0

		if i < n_samples_pre then
			gain = math.sin (0.5 * math.pi * i / n_samples_pre)
		elseif j < n_samples_end then
			gain = math.sin (0.5 * math.pi * j / n_samples_end)
		end

		local d = b * math.exp (a * (i - n_samples_pre))
		local p = d - b
		local x = gain * math.sin (2 * math.pi * (p - math.floor (p)))

		ss[i + 1] = x * amp
		si[j + 1] = x * d * r
	end

	sweep_sin = ARDOUR.AudioRom.new_rom (cmem:to_float (0), n_samples)

	-- de-convolver
	local sweep_inv = ARDOUR.AudioRom.new_rom (cmem:to_float (n_samples), n_samples)
	conv = ARDOUR.DSP.Convolution (Session, 1, 1)
	conv:add_impdata (0, 0, sweep_inv, 1.0, 0, 0, 0, 0)
	conv:restart ()

	sweep_inv = nil
	cmem = nil
	collectgarbage ()
end

function dsp_init (rate)
	local fmax = 20000
	local slen = 5 -- sec
	local rlen = slen + 5 -- sec

	if fmax > rate * .47 then
		fmax = rate * .47
	end
	gen_sweep (20, fmax, slen, rate)

	rec_len = rlen * rate

	n_captured = 0
	n_playback = 0
	peak = 0
	state = 0
end

function dsp_latency ()
	return conv:latency()
end

function reset ()
	state = 0
	if n_captured > 0 then
		conv:restart ()
	end
	n_captured = 0
	n_playback = 0
end

function dsp_runmap (bufs, in_map, out_map, n_samples, offset)
	local ctrl = CtrlPorts:array() -- get control port array
	local capture = ctrl[1] > 0

	if state == 0 and capture and Session:actively_recording () then
		state = 1
		peak = 0
		print ("START CAPTURE")
	end
	if state == 2 then
		Session:request_stop (false, false, ARDOUR.TransportRequestSource.TRS_UI)
		reset ()
	end

	-- check I/O mapping
	local input = in_map:get (ARDOUR.DataType ("audio"), 0)
	local outir = out_map:get (ARDOUR.DataType ("audio"), 0)
	local outsw = out_map:get (ARDOUR.DataType ("audio"), 1)

	if input == ARDOUR.ChanMapping.Invalid or outir == ARDOUR.ChanMapping.Invalid or outsw == ARDOUR.ChanMapping.Invalid or input ~= outir then
		reset ();
	end

	if state ~= 1 then
		bufs:get_audio (outir):silence (n_samples, offset)
	end

	if state == 1 then -- capture
		conv:run_mono_buffered (bufs:get_audio(input):data (offset), n_samples)

		if n_samples + n_captured > rec_len then
			state = 2
		end

		peak = ARDOUR.DSP.compute_peak (bufs:get_audio(input):data (offset), n_samples, peak) -- compute digital peak
		n_captured = n_captured + n_samples
	end

	if state == 1 then
		-- play back sine-sweep
		local copied = sweep_sin:read (bufs:get_audio (outsw):data (offset), n_playback, n_samples, 0)
		n_playback = n_playback + copied
		if copied < n_samples then
			bufs:get_audio (outsw):silence (n_samples - copied, offset + copied)
		end
	else
		bufs:get_audio (outsw):silence (n_samples, offset)
	end

	ctrl[2] = state
	ctrl[3] = ARDOUR.DSP.accurate_coefficient_to_dB (peak)
end
