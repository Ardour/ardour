ardour {
	["type"]    = "EditorHook",
	name        = "OSC Callback Example",
	author      = "Ardour Lua Task Force",
	description = "Send OSC messages",
}

function action_params ()
	return
	{
		["uri"] = { title = "OSC URI ", default = "osc.udp://localhost:7890"},
	}
end


function signals ()
	s = LuaSignal.Set()
	s:add (
		{
			[LuaSignal.SoloActive] = true,
			[LuaSignal.RegionPropertyChanged] = true,
			[LuaSignal.Exported] = true,
			[LuaSignal.TransportStateChange] = true
		}
	)
	return s
end

function factory (params)
	return function (signal, ref, ...)
		local uri = params["unique"] or "osc.udp://localhost:7890"
		local tx = ARDOUR.LuaOSC.Address (uri)
		-- debug print (stdout)
		-- print (signal, ref, ...)

		if (signal == LuaSignal.Exported) then
			tx:send ("/session/exported", "ss", ...)
		elseif (signal == LuaSignal.SoloActive) then
			tx:send ("/session/solo_changed", "")
		elseif (signal == LuaSignal.TransportStateChange) then
			tx:send ("/session/transport", "if",
				Session:transport_frame(), Session:transport_speed())
		elseif (signal == LuaSignal.RegionPropertyChanged) then
			obj,pch = ...
			tx:send ("/region_property_changed", "sTTiii",
				obj:name (),
				(pch:containsFramePos (ARDOUR.Properties.Start)),
				(pch:containsFramePos (ARDOUR.Properties.Length)),
				obj:position (), obj:start (), obj:length ())
		end
	end
end
