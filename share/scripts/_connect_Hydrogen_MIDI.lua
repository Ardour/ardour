ardour {
	["type"]    = "EditorAction",
	name        = "Connect MIDI Track to Hydrogen",
	license     = "MIT",
	author      = "Vincent Tassy",
	description = [[Connects the current track to Hydrogen]]
}

function factory () return function ()
	local sel = Editor:get_selection ()
	-- for each selected track/bus
	for r in sel.tracks:routelist ():iter () do
			if not r:to_track ():isnil () and not r:to_track ():to_midi_track ():isnil () then
				local inputmidiport = r:input():midi(0)
				print(inputmidiport:name())
				inputmidiport:disconnect_all()
				inputmidiport:connect("Hydrogen-midi:TX")
			end
	end
end end
