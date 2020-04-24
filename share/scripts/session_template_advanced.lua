ardour {
	["type"]    = "SessionInit",
	name        = "Advanced Session",
	description = [[Allow to configure master-bus and autoconnect settings]],
	master_bus  = 0
}

function factory () return function ()

	local auto_connect_in = {
		[0] = "Manually",
		[1] = "automatically to physical inputs",
	}

	local auto_connect_out = {
		[0] = "Manually",
		[1] = "automatically to physical outputs",
		[2] = "automatically to master bus",
	}

	local dialog_options = {
		{ type = "heading", title = "Customize Session: " .. Session:name () },
		{ type = "number",   key = "master",    title = "Master bus channels",  min = 0, max = 24, step = 1, digits = 0, default = 2 },
		{ type = "checkbox", key = "monitor",   title = "Add monitor section", default = ARDOUR.config():get_use_monitor_bus () },
		{ type = "dropdown", key = "ac_input",  title = "Autoconnect Inputs",
			values = {
				[auto_connect_in[0]] = 0,
				[auto_connect_in[1]] = 1,
			},
			default = auto_connect_in[ARDOUR.config():get_input_auto_connect ()]
		},
		{ type = "dropdown", key = "ac_output", title = "Autoconnect Outputs",
			values = {
				[auto_connect_out[0]] = 0,
				[auto_connect_out[1]] = 1,
				[auto_connect_out[2]] = 2,
			},
			default = auto_connect_out[ARDOUR.config():get_output_auto_connect ()]
		},
	}

	local dlg = LuaDialog.Dialog ("Template Setup", dialog_options)
	local rv = dlg:run()
	if (not rv) then return end

	if rv['master'] > 0 then
		local count = ARDOUR.ChanCount ( ARDOUR.DataType("audio"), rv['master'])
		Session:add_master_bus (count)
	end

	if rv['monitor'] then
		ARDOUR.config():set_use_monitor_bus (true)
	end

	ARDOUR.config():set_input_auto_connect (rv['ac_input'])
	ARDOUR.config():set_output_auto_connect (rv['ac_output'])

	Session:save_state("");
end end
