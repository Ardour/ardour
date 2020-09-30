ardour {
	["type"]    = "EditorAction",
	name        = "Add Filters",
	license     = "MIT",
	author      = "PSmith",
	description = [[Add 'HPF/LPF' Lua Processor to all Tracks]]
}

function action_params ()
	return
	{
		["unique"]   = { title = "Only add HPF/LPF if not already present (yes/no)", default = "yes"},
		["position"] = { title = "Insert Position from top (0,..)",                   default = "0"},
	}
end


function factory (params)
	return function ()
		-- get configuration
		local p = params or {}
		local uniq = p["unique"] or "yes"
		local pos = p["position"] or 0

		-- loop over all tracks
		for t in Session:get_tracks():iter() do
			local insert = true;

			-- check if filters are present
			if uniq ~= "no" then
				local proc;
				local i = 0;
				repeat
					-- get Nth Ardour::Processor
					proc = t:nth_plugin (i)
					-- check if it's a filter
					if (not proc:isnil() and proc:display_name () == "ACE High/Low Pass Filter") then
						insert = false;
					end
					i = i + 1
				until proc:isnil() or insert == false
			end

			-- create a new processor and insert it
			if insert then
				local a = ARDOUR.LuaAPI.new_luaproc(Session, "ACE High/Low Pass Filter");
				if (not a:isnil()) then
					t:add_processor_by_index(a, pos, nil, true)
					a = nil -- explicitly drop shared-ptr reference
				end
			end
		end
	end
end
