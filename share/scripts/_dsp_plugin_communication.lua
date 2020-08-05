ardour { ["type"] = "dsp", name = "DSP Plugin Communication" }
function dsp_ioconfig () return { { audio_in = -1, audio_out = -1} } end

function dsp_init (rate)
	self:shmem ():allocate (1)
end

function dsp_configure (ins, outs)
end

function dsp_params ()
	return
	{
		{ ["type"] = "output", name = "self", min = 0, max = 8},
		{ ["type"] = "output", name = "gain", min = 0, max = 2},
	}
end

function dsp_run (ins, outs, n_samples)
	local ctrl = CtrlPorts:array ()
	local route = self:route ()
	local shmem = self:shmem ()

	-- count plugins
	local i = 0;
	local l = 0;
	local s = -1; -- 'self' this plugin instance

	-- iterate overall plugins on this track,
	-- find all LuaProc instances of this plugin (unique_id),
	repeat
		local proc = route:nth_plugin (i)
		if not proc:isnil ()
			and not proc:to_insert():plugin (0):to_luaproc():isnil ()
			and proc:to_insert():plugin (0):unique_id () == self:unique_id () then
			if (self:id ():to_s() == proc:to_insert():plugin (0):id ():to_s()) then
				s = l; -- *this* plugin instance
			end
			if l == 0 then
				-- use shared-memory are of the first plugin instance for all.
				--
				-- (the first plugin writes there, all later plugins only read,
				-- plugins on a track are executed in order, in the same thread)
				shmem = proc:to_insert():plugin (0):to_luaproc():shmem ()
			end
			l = l + 1 -- count total instances of this plugin-type
		end
		i = i + 1
	until proc:isnil ()

	assert (s >= 0)
	ctrl[1] = s;

	-- calculate digital peak of all channels
	local peak = 0
	for c = 1,#ins do
		if not ins[c]:sameinstance (outs[c]) then
			ARDOUR.DSP.copy_vector (outs[c], ins[c], n_samples)
		end
		peak = ARDOUR.DSP.compute_peak(outs[c], n_samples, peak)
	end


	-- actual inter-plugin communication
	local a = shmem:to_float (0):array ()
	if s == 0 then
		-- the first plugin saves the peak
		a[1] = peak
		ctrl[2] = -1
	else
		-- all later plugins display the difference to the first.
		if (a[1] == 0) then
			ctrl[2] = 1
		else
			ctrl[2] = peak / a[1]
		end
	end
end
