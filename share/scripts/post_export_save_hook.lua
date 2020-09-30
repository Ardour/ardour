ardour {
	["type"]    = "EditorHook",
	name        = "Save Snapshot after Export",
	author      = "Ardour Team",
	description = "Save a session-snapshot on export, named after the export-timespan",
}

-- subscribe to signals
-- http://manual.ardour.org/lua-scripting/class_reference/#LuaSignal.LuaSignal
function signals ()
	s = LuaSignal.Set()
	s:add ({[LuaSignal.Exported] = true})
	return s
end

-- create callback functions
function factory ()
	-- callback function which invoked when signal is emitted
	return function (signal, ref, ...)
		-- 'Exported' passes 2 strings: current time-span name, path to exported file
		-- (see C++ libs/ardour/export_handler.cc  Session::Exported )
		local timespan_name, file_path = ...
		-- save session -- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Session
		Session:save_state ("export-" .. timespan_name, false, false, false)
	end
end
