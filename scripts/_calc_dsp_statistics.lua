ardour { ["type"] = "Snippet", name = "Calculate DSP stats",
	license     = "MIT",
	author      = "Ardour Team",
}

function factory () return function ()

	for t in Session:get_routes ():iter () do
		local i = 0
		while true do
			local rv, stats
			local proc = t:nth_processor (i)
			if proc:isnil () then break end
			if proc:to_plugininsert():isnil() then goto continue end

			rv, stats = proc:to_plugininsert():get_stats (0, 0, 0, 0)
			if not rv then goto continue end

			print (string.format (" * %-28s | min: %.2f max: %.2f avg: %.3f std-dev: %.3f [ms]",
				string.sub (proc:name() .. '  (' .. t:name() .. ')', 0, 28),
				stats[1] / 1000.0, stats[2] / 1000.0, stats[3] / 1000.0, stats[4] / 1000.0))

			::continue::
			i = i + 1
		end
	end
end end
