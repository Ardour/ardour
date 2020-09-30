ardour {
	["type"]    = "EditorAction",
	name        = "Add Scopes",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Add 'Inline Scope' Lua Processor to all Tracks]]
}

function action_params ()
	return
	{
		["unique"]   = { title = "Only add Scope if non is present already (yes/no)", default = "yes"},
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

			-- check if a scope is already present
			if uniq ~= "no" then
				local proc;
				local i = 0;
				repeat
					-- get Nth Ardour::Processor
					proc = t:nth_plugin (i)
					-- check if it's a scope
					if (not proc:isnil() and proc:display_name () == "ACE Inline Scope") then
						insert = false;
					end
					i = i + 1
				until proc:isnil() or insert == false
			end

			-- create a new processor and insert it
			if insert then
				local a = ARDOUR.LuaAPI.new_luaproc(Session, "ACE Inline Scope");
				if (not a:isnil()) then
					t:add_processor_by_index(a, pos, nil, true)
					ARDOUR.LuaAPI.set_processor_param (a, 0, 5) -- timescale 5sec
					-- ARDOUR.LuaAPI.set_processor_param (a, 1, 1) -- logscale on
					-- ARDOUR.LuaAPI.set_processor_param (a, 2, 3) -- "Max" height
					a = nil -- explicitly drop shared-ptr reference
				end
			end
		end
	end
end


function icon (params) return function (ctx, width, height)
	local wh = math.min (width, height) * .5
	local x0 = math.ceil (wh * .4)
	local x1 = math.floor (wh * 1.6)
	ctx:translate (math.floor (width * .5 - wh), math.floor (height * .5 - wh))
	ctx:rectangle (wh * .4, wh * .4, wh * 1.2, wh * 1.2)
	ctx:set_source_rgba (.7, .7, .7, 1)
	ctx:fill ()
	ctx:set_line_width (1)
	ctx:set_source_rgba (.0, .0, .0, 1)
	ctx:move_to (x0, wh)
	for x = x0, x1 do
		ctx:line_to (x, wh - math.sin (2 * math.pi * (x-x0) / (x1-x0)) * wh * .5)
	end
	ctx:stroke ()
end end
