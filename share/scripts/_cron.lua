ardour {
	["type"]    = "EditorHook",
	name        = "Timed Event Example",
	author      = "Ardour Team",
	description = "Perform actions at specific wallclock time, example record",
}

function signals ()
	return LuaSignal.Set():add ({[LuaSignal.LuaTimerDS] = true})
end

function factory ()
	local _last_time = 0
	return function (signal, ref, ...)

		-- calculate seconds since midnight
		function hhmmss (hh, mm, ss) return hh * 3600 + mm * 60 + ss end

		-- current seconds since midnight UTC
		-- (unix-time is UTC, no leap seconds, a day always has 86400 sec)
		local now = os.time () % 86400

		-- event at 09:30:00 UTC (here: rec-arm + roll)
		if (now >= hhmmss (09, 30, 00) and _last_time < hhmmss (09, 30, 00)) then
			Session:maybe_enable_record (false)
			Session:request_roll (ARDOUR.TransportRequestSource.TRS_UI)
		end

		-- event at 09:32:00 UTC (here: rec-stop)
		if (now >= hhmmss (09, 32, 00) and _last_time < hhmmss (09, 32, 00)) then
			Session:disable_record (false, false)
			Session:request_stop (false, false, ARDOUR.TransportRequestSource.TRS_UI);
		end

		_last_time = now
	end
end
