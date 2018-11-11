ardour {
	["type"]    = "EditorHook",
	name        = "Periodically Save Snapshot",
	author      = "Ardour Lua Task Force",
	description = "Save a session-snapshot peridocally (every 15mins) named after the current date-time",
}

-- subscribe to signals
-- http://manual.ardour.org/lua-scripting/class_reference/#LuaSignal.LuaSignal
function signals ()
	return LuaSignal.Set():add ({[LuaSignal.LuaTimerDS] = true})
end

-- create callback function
function factory ()
	local _last_snapshot_time = 0 -- persistent variable

	-- callback function which invoked when signal is emitted, every 100ms
	return function (signal, ref, ...)

		local now = os.time (); -- unix-time, seconds since 1970

		-- skip initial save when script is loaded
		if (_last_snapshot_time == 0) then
			_last_snapshot_time = now;
		end

		-- every 15 mins
		if (now > _last_snapshot_time + 60 * 15) then
			_last_snapshot_time = now
			-- format date-time (avoid colon)
			local snapshot_name = os.date ("%Y-%m-%d %H.%M.%S", now)
			-- save session -- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Session
			Session:save_state ("backup " .. snapshot_name, false, false, false)
		end

	end
end
