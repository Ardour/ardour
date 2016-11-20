ardour { ["type"] = "EditorAction", name = "Fan Out Instrument",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Create Busses for every Instrument Output on selected Tracks]]
}

function factory () return function ()

	local outputs = 2
	local mst = Session:master_out();
	if not mst:isnil() then
		outputs = mst:n_inputs():n_audio()
	end
	mst = nil -- drop reference

	local sel = Editor:get_selection ()
	for r in sel.tracks:routelist ():iter () do
		local proc = r:the_instrument ():to_insert()
		if proc:isnil () then
			print ("Track", r:name(), "does not have an instrument plugin")
			goto next
		end
		local plugin = proc:plugin(0);

		if (r:n_outputs ():n_audio() ~= proc:output_streams():n_audio()) then
			print ("Instrument ", proc:name(), "outputs", proc:output_streams():n_audio(), "do not match track outputs", r:n_outputs ():n_audio())
			goto next
		end

		-- collect port-group information, count target bus width
		local targets = {}
		for i = 1, proc:output_streams():n_audio() do
			local pd = plugin:describe_io_port (ARDOUR.DataType("Audio"), false, i - 1)
			local nn = proc:name() .. " " .. pd.group_name; -- TODO use track-name prefix?
			targets[nn] = targets[nn] or 0
			targets[nn] = targets[nn] + 1
		end

		if #targets < 2 then
			print ("Instrument ", proc:name(), "has only 1 output bus. Nothing to fan out.")
			goto next
		end

		-- create busses ; TODO retain order
		for t,c in pairs (targets) do
			local rt = Session:route_by_name (t)
			if rt:isnil () then
				Session:new_audio_route (c, outputs, nil, 1, t, ARDOUR.PresentationInfo.Flag.AudioBus, ARDOUR.PresentationInfo.max_order)
			end
		end

		r:output():disconnect_all (nil)
		r:panner_shell():set_bypassed (true)

		-- connect the busses
		for i = 1, proc:output_streams():n_audio() do
			local pd = plugin:describe_io_port (ARDOUR.DataType("Audio"), false, i - 1)
			local nn = proc:name() .. " " .. pd.group_name;
			local rt = Session:route_by_name (nn)
			assert (rt)

			local op =  r:output():audio (i - 1)
			local ip = rt:input():audio (pd.group_channel)
			op:connect (ip:name())
		end

		::next::
	end
end end
