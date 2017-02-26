ardour { ["type"] = "Snippet", name = "Modulation Test" }

function factory () return function ()
	local route = Session:get_remote_nth_route(0)
	assert (route)
	local pi = route:nth_plugin(0):to_insert()
	assert (not pi:isnil())
	-- this test is for x42-eq .. port 3 is "gain"
	print (pi:load_modulation_script ([[
	function dsp_modulate(ctrl, bufs, n_samples, offset, start)
		cnt = cnt or 0
		cnt = cnt + n_samples
		local sr = Session:nominal_frame_rate ()
		if cnt > sr then cnt = cnt - sr end
		--local gain = ctrl:at (Evoral.Parameter (ARDOUR.AutomationType.PluginAutomation, 0, 3)):to_automationcontrol():to_plugincontrol()
		--assert (gain)
		local a = ARDOUR.DSP.fast_coefficient_to_dB (ARDOUR.DSP.compute_peak(bufs:get_audio(0):data(offset), n_samples, 0)) + 10
		a = math.min (20, math.max(-20, a))
		--gain:modulate_to (5 * math.sin (math.pi * cnt / sr) - a)
		ARDOUR.LuaAPI.modulate_to (ctrl, 3, 5 * math.sin (math.pi * cnt / sr) - a)
	end
	]]
	))

end end
