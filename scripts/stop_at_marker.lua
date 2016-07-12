ardour {
	["type"]    = "session",
	name        = "Stop at Marker",
	license     = "MIT",
	author      = "Ardour Lua Task Force",
	description = [[An example session script which stops the transport on every location marker when rolling forward.]]
}

function factory ()
	return function (n_samples)
		if (not Session:transport_rolling ()) then
			-- not rolling, nothing to do.
			return
		end

		local pos = Session:transport_frame () -- current playhead position
		local loc = Session:locations () -- all marker locations

		-- find first marker after the current playhead position, ignore loop + punch ranges
		-- (this only works when rolling forward, to extend this example see
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Locations )
		local m = loc:first_mark_after (pos, false)

		if (m == -1) then
			-- no marker was found
			return
		end

		-- since ardour may split the process cycle for events,
		-- n_samples may be smaller.
		local blk = Session:get_block_size ()

		-- transport stop can only happen on a process-cycle boundary.
		-- This callback happens from within the process callback,
		-- so we need to queue it ahead of time.
		if (pos + n_samples + blk >= m and pos + n_samples < m) then
			Session:request_transport_speed (0.0, true)
		end
	end
end
