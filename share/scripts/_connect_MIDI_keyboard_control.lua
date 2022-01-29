ardour {
	["type"]    = "EditorAction",
	name        = "Connect Keyboard Transport to Ardour",
	license     = "MIT",
	author      = "Vincent Tassy",
	description = [[Connects Alesis Q49 MKII MIDI Transport to Ardour]]
}

function factory () return function ()
	_, t = Session:engine ():get_backend_ports ("", ARDOUR.DataType.midi (),ARDOUR.PortFlags.IsOutput | ARDOUR.PortFlags.IsPhysical, C.StringVector ())
	local found = 0
	local i = 1
	repeat
		local p = t[4]:table()[i]
		if  (p) then
			print(p, " -> ", Session:engine (): get_pretty_name_by_name(p))
			if Session:engine (): get_pretty_name_by_name(p) == "Q49 MKII" then
				found = found + 1
				if found == 2 then
					Session:engine (): connect (p, "ardour:MIDI Control In")
				end
			end
			i = i + 1
		end
	until (found == 2 or p == nil)
end end