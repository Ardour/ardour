ardour {
	["type"]    = "session",
	name        = "Stop at Marker",
	license     = "MIT",
	author      = "Ardour Team",
	description = [[Stop the transport on every location marker when rolling forward.]]
}

function factory ()
	return function (n_samples)
		if (not Session:transport_rolling ()) then
			-- not rolling, nothing to do.
			return
		end

		local pos = Session:transport_sample () -- current playhead position
		local loc = Session:locations () -- all marker locations

		-- find first marker after the current playhead position, ignore loop + punch ranges
		-- (this only works when rolling forward, to extend this example see
		-- http://manual.ardour.org/lua-scripting/class_reference/#ARDOUR:Locations )
		--
		local m = loc:first_mark_after (pos, false)

		if (m == -1) then
			-- no marker was found
			return
		end

		-- due to `first_mark_after(pos)` "m" is always > "pos":
		-- assert(pos < m)
		--
		-- This callback happens from within the process callback:
		--
		-- this cycle's end = next cycle start = pos + n_samples.
		--
		-- Note that if "m" is exactly at cycle's end, that marker
		-- will be at "pos" in the next cycle. Since we ask for
		-- "first_mark_after pos", the marker would not be found.
		--
		-- So even though "pos + n_samples" is barely reached,
		-- we need to stop at "m" in the cycle that crosses or ends at "m".
		if (pos + n_samples >= m) then
			-- asking to locate to "m" ensures that playback continues at "m"
			-- and the same marker will not be taken into account.
			Session:request_locate (m, ARDOUR.LocateTransportDisposition.MustStop, ARDOUR.TransportRequestSource.TRS_Engine)
		end
	end
end
