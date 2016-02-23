ardour {
	["type"]    = "session",
	name        = "Example",
	license     = "MIT",
	author      = "Robin Gareus",
	email       = "robin@gareus.org",
	site        = "http://gareus.org",
	description = [[
	An Example Ardour Session Process Plugin.
	Install a 'hook' that is called on every process cycle
	(before doing any processing).
	This example stops the transport after rolling for a specific time.]]
}

function sess_params ()
	return
	{
		["print"]  = { title = "Debug Print (yes/no)", default = "no", optional = true },
		["time"] = { title = "Timeout (sec)", default = "90", optional = false },
	}
end

function factory (params)
	return function (n_samples)
		local p = params["print"] or "no"
		local timeout = params["time"] or 90
		a = a or 0
		if p ~= "no" then print (a, n_samples, Session:frame_rate (), Session:transport_rolling ()) end -- debug output (not rt safe)
		if (not Session:transport_rolling()) then
			a = 0
			return
		end
		a = a + n_samples
		if (a > timeout * Session:frame_rate()) then
			Session:request_transport_speed(0.0, true)
		end
	end
end
