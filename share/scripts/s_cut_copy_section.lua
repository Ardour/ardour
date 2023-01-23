ardour { ["type"] = "Snippet", name = "Move Section" }

function factory (params) return function ()
	local start = Temporal.timepos_t(96000)
	local _end = Temporal.timepos_t(144000)
	local to = Temporal.timepos_t(480000)
	Session:cut_copy_section (start, _end, to , true)
end end
