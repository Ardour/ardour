ardour { ["type"] = "Snippet", name = "Plugin Order Reverse" }

function factory () return function ()
	local sel = Editor:get_selection ()
	-- for each selected track/bus
	for r in sel.tracks:routelist ():iter () do
		print ("Route:", r:name ())
		local neworder = ARDOUR.ProcessorList(); -- create a PluginList
		local i = 0;
		repeat -- iterate over all plugins/processors
			local proc = r:nth_processor (i)
			if not proc:isnil () then
				-- append plugin to list
				neworder:push_back(proc)
			end
			i = i + 1
		until proc:isnil ()
		-- reverse list
		neworder:reverse()
		-- and set new order
		r:reorder_processors (neworder, nil)
	end
end end
