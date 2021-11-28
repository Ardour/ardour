ardour {
	["type"]    = "session",
	name        = "Good Night",
	author      = "Ardour Team",
	description = [[
	Example Ardour Session Script.
	Session scripts are called at the beginning of every process-callback (before doing any audio processing).
	This example stops the transport after rolling for a configurable time which can be set when instantiating the script.]]
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
		if p ~= "no" then print (a, n_samples, Session:sample_rate (), Session:transport_rolling ()) end -- debug output (not rt safe)
		if (not Session:transport_rolling()) then
			a = 0
			return
		end
		a = a + n_samples
		if (a > timeout * Session:sample_rate()) then
			Session:request_stop (false, false, ARDOUR.TransportRequestSource.TRS_UI);
		end
	end
end
