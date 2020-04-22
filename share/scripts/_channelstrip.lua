ardour {
	["type"]    = "EditorAction",
	name        = "Channel Strip Setup",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Add Compressor and EQ plugin to every selected track and if a bus name 'Reverb' exists post-fader reverb send]]
}

function factory (params)
	return function ()
		-- helper functions
		function plugin_uri (proc)
			return proc:to_plugininsert():plugin(0):get_info().unique_id
		end
		function add_lv2_plugin (route, pluginname, position)
			local p = ARDOUR.LuaAPI.new_plugin (Session, pluginname, ARDOUR.PluginType.LV2, "")
			if not p:isnil () then
				route:add_processor_by_index (p, position, nil, true)
			end
		end
		function test_send_dest (send, route)
			local intsend = send:to_internalsend()
			if intsend:isnil () then return false end
			return intsend:target_route () == route
		end

		-- Test if there is a Bus named "Reverb"
		local reverb_bus = Session:route_by_name ("Reverb")
		if reverb_bus:isnil () or not reverb_bus:to_track ():isnil () then
			reverb_bus = nil;
		end

		-- Iterate over all selected Track/Bus
		-- http://manual.ardour.org/lua-scripting/class_reference/#ArdourUI:TrackSelection
		local sel = Editor:get_selection ()
		for route in sel.tracks:routelist ():iter () do
			local have_eq   = false
			local comp_pos  = 0
			local add_send  = false

			-- skip master-bus
			if route:is_master() then goto next end

			-- iterate over all plugins on the route,
			-- check if a a-eq or a-comp is already present
			local i = 0;
			repeat
				local proc = route:nth_plugin (i) -- get Nth Ardour::Processor
				if (not proc:isnil()) and plugin_uri (proc) == "urn:ardour:a-eq" then
					have_eq = true;
				end
				if (not proc:isnil()) and (plugin_uri (proc) == "urn:ardour:a-comp" or plugin_uri (proc) == "urn:ardour:a-comp#stereo") then
					comp_pos = i + 1; -- remember position of compressor
				end
				i = i + 1
			until proc:isnil()

			-- check if there is a send to the reverb bus 
			if reverb_bus then
				add_send = true;
				i = 0;
				repeat
					local send = route:nth_send (i) -- get Nth Ardour::Send
					if not send:isnil() and test_send_dest (send, reverb_bus) then
						add_send = false
					end
					i = i + 1
				until send:isnil()
			end

			-- plugins are inserted at the top (pos = 0).
			-- So they have to be added in reverse order:
			-- If the compressor is not yet present (comp_pos == 0),
			-- first add the EQ, and then the compressor before the EQ.
			-- Otherwise add the EQ after the compressor.
			if not have_eq then
				add_lv2_plugin (route, "urn:ardour:a-eq", comp_pos)
			end

			if comp_pos == 0 then
				if route:n_inputs ():n_audio () == 2 then
					add_lv2_plugin (route, "urn:ardour:a-comp#stereo", 0)
				else
					add_lv2_plugin (route, "urn:ardour:a-comp", 0)
				end
			end
	
			--add send to reverb bus, post-fader, just before the track's mains output.
			if add_send then
				Session:add_internal_send (reverb_bus, route:main_outs (), route)
			end

			::next::

		end
	end
end

function icon (params) return function (ctx, width, height, fg)
	function rnd (x) return math.floor (x + 0.5) end

	local wc = rnd (width * 0.5) - 0.5
	local wl = wc - math.floor (width * .25)
	local wr = wc + math.floor (width * .25)

	local h0 = height * 0.2
	local h1 = height * 0.8

	local fw = math.floor (width * .1)
	local fl = rnd (height * .35) - 1
	local fc = rnd (height * .65) - 1
	local fr = rnd (height * .45) - 1

	ctx:set_source_rgba (ARDOUR.LuaAPI.color_to_rgba (fg))
	ctx:set_line_width (1)

	ctx:move_to (wl, h0)
	ctx:line_to (wl, h1)
	ctx:move_to (wc, h0)
	ctx:line_to (wc, h1)
	ctx:move_to (wr, h0)
	ctx:line_to (wr, h1)
	ctx:stroke ()

	ctx:set_line_width (2)

	ctx:move_to (wl - fw, fl)
	ctx:line_to (wl + fw, fl)
	ctx:move_to (wc - fw, fc)
	ctx:line_to (wc + fw, fc)
	ctx:move_to (wr - fw, fr)
	ctx:line_to (wr + fw, fr)
	ctx:stroke ()
end end
