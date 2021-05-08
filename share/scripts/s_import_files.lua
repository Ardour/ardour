ardour { ["type"] = "Snippet", name = "Import File(s) Example" }

function factory (params) return function ()
	local files = C.StringVector();

	files:push_back("/tmp/test.wav")

	local pos = -1
	Editor:do_import (files,
		Editing.ImportDistinctFiles, Editing.ImportAsTrack, ARDOUR.SrcQuality.SrcBest,
		ARDOUR.MidiTrackNameSource.SMFTrackName, ARDOUR.MidiTempoMapDisposition.SMFTempoIgnore,
		pos, ARDOUR.PluginInfo(), false)

end end
