ardour {
	["type"]    = "EditorHook",
	name        = "Varispeed Test - 100ms Callback",
	author      = "Ardour Team",
	description = "An example script that invokes a callback a every 0.1sec and modifies the transport speed",
}

function signals ()
	s = LuaSignal.Set()
	s:add (
		{
			[LuaSignal.LuaTimerDS] = true,
		}
	)
	return s
end

function factory (params)
	-- upindex variables
	local cnt = 0
	local speed = 0
	local delta = 0.01
	return function (signal, ref, ...)
		cnt = (cnt + 1) % 5 -- divide clock: every half a second
		if cnt == 0 then
			if speed < -0.25 then delta = delta * -1 end
			if speed >  0.25 then delta = delta * -1 end
			speed = speed + delta
			Session:request_transport_speed (speed, ARDOUR.TransportRequestSource.TRS_UI)
		end
	end
end
