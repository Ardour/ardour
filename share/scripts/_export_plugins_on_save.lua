ardour {
	["type"]    = "EditorHook",
	name        = "Save Extra Data (instruments)",
	author      = "Ardour Team",
	description = "Export custom data when the session is saved",
}

-- subscribe to signals
-- http://manual.ardour.org/lua-scripting/class_reference/#LuaSignal.LuaSignal
function signals ()
	s = LuaSignal.Set()
	s:add ({[LuaSignal.StateSaved] = true})
	return s
end

-- create callback functions
function factory () return function (signal, ...)
	assert (signal == LuaSignal.StateSaved)

	local all_instruments = {}

	-- iterate over all routes
	for r in Session:get_routes():iter() do
		local proc = r:the_instrument() -- get instrument processor (if any)
		if proc:isnil() then goto nextroute end -- skip tracks/busses without instrument
		local pi = proc:to_insert() -- check if it's a plugin-insert
		if pi:isnil() then goto nextroute end

		local pp = pi:plugin (0) -- get first instance
		all_instruments[r:name()] = string.format ("%s (%s)", proc:name(), pp:unique_id())

		::nextroute::
	end

	if next (all_instruments) ~= nil then -- check if table is not empty
		-- write to a file in the session-folder
		file = io.open (ARDOUR.LuaAPI.build_filename (Session:path(), Session:name () .. ".instruments.txt"), "w")
		for nme, nfo in pairs (all_instruments) do
			file:write (nme .. ": " .. nfo)
		end
		file:close()
	end

end end
