ardour { ["type"] = "Snippet", name = "Region Transient List" }

function factory () return function ()
	local sel = Editor:get_selection ()
	for r in sel.regions:regionlist ():iter () do
		local ref = r:position() - r:start()
		print (r:name(), r:position(), r:start(), r:position():samples(), r:start():samples())
		local trans = r:transients() -- list of samplepos_t
		for t in trans:iter() do
			print (ref + Temporal.timecnt_t(t), ref:samples () + t)
		end
		print ("----")
	end
end end
