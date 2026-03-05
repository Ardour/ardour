ardour {
	["type"]    = "EditorAction",
	name        = "Set I/O config",
	license     = "MIT",
	author      = "Robin Gareus, Wargreen",
	description = [[Set a custom track I/O configuration for all selected tracks]]
}

function factory(unused_params)
return function()

	local dialog_options = {
		{ type = "number", key = "inputs", title = "Number of inputs", min = 1, max = 64, step = 1, digits = 0, default = 2 },
		{ type = "number", key = "outputs", title = "Number of outputs", min = 1, max = 64, step = 1, digits = 0, default = 2 },
	}

	local rv = LuaDialog.Dialog("Select number of ports", dialog_options):run()

	-- Return if the user cancelled
	if not rv then
		return
	end

	local target_in = rv["inputs"]
	local target_out = rv["outputs"]

	print ("Reconfigure to", target_in, target_out);

	-- get Editor selection:
	sel = Editor:get_selection ()
	-- for each selected track/bus
	for route in sel.tracks:routelist ():iter () do
		print (route:name())

		-- unset strict io if needeed
		if target_in ~= target_out then
			route:set_strict_io (false)
		end

		-- process input
		local i = route:input ()
		local nbr_iports = i:n_ports ():n_audio ()

		if target_in > nbr_iports then
			local to_add = target_in - nbr_iports
			print (" - Add", to_add,  "inputs")
			for n = 1, to_add do
				i:add_port ("", ARDOUR.DataType ("Audio"))
			end
		end

		if target_in < nbr_iports then
			for n = nbr_iports, target_in, -1 do
				print (" - Remove Input", n)
				if not i:nth(n):isnil() then
					i:remove_port (i:nth(n))
				end
			end
		end

		-- process output
		local o = route:output ()
		local nbr_oports = o:n_ports ():n_audio ()

		if target_out > nbr_oports then
			local to_add = target_out - nbr_oports
			print (" - Add", to_add,  "outputs")
			for n = 1, to_add do
				o:add_port ("", ARDOUR.DataType ("Audio"))
			end
		end

		if target_out < nbr_oports then
			for n = nbr_oports - 1, target_out, -1 do
				if not o:nth(n):isnil() then
					print (" - Remove Output", n)
					o:remove_port (o:nth(n))
				end
			end
		end

	end -- for each track
end end
