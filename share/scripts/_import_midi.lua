ardour {
	["type"]    = "EditorAction",
	name        = "Import MIDI File",
	author      = "Ronan Keryell",
	description = [[
Import a Standard MIDI File into the current session, creating one new
MIDI track per SMF track and placing the imported region at the session
start.

This is a thin example wrapper around the libardour Lua binding:

    ARDOUR.LuaAPI.import_midi (session, path,
                               with_tempo_map,   -- import the SMF tempo map
                               with_markers,     -- import SMF markers / text meta-events
                               split_channels)   -- one track per channel instead of per SMF track

The binding lives in libardour (not in the GTK layer), so the very same
call also works headless from the `ardour6-lua` / `ardour9-lua`
interpreter, e.g.:

    local s = create_session ("/tmp/Demo", "Demo", 48000)
    ARDOUR.LuaAPI.import_midi (s, "/path/to/song.mid", true, true, false)
    s:save_state ("")

It returns the list of newly created MIDI tracks (a MidiTrackList), or an
empty list on failure (no session, not a MIDI file, file missing, or the
import produced no sources).

Unlike Editor:do_import(), this binding needs no GTK Editor: it performs
the SMF tempo-map import, calls Session:import_files(), and creates the
tracks/regions itself, so it is usable for fully automated, headless
session setup.
]]
}

function factory () return function ()

	-- Ask the user for a MIDI file and the import options. The "file"
	-- control opens a native file chooser; the checkboxes map directly to
	-- the boolean arguments of ARDOUR.LuaAPI.import_midi().
	local dialog_options = {
		{ type = "file",     key = "path",       title = "MIDI file to import" },
		{ type = "checkbox", key = "tempo_map",  title = "Import tempo map",            default = true  },
		{ type = "checkbox", key = "markers",    title = "Import markers / meta-events", default = true  },
		{ type = "checkbox", key = "split",      title = "Split channels into tracks",   default = false },
	}

	local rv = LuaDialog.Dialog ("Import MIDI File", dialog_options):run ()
	if not rv or rv['path'] == nil or rv['path'] == "" then
		return -- cancelled, or no file chosen
	end

	-- The actual import. `Session` is the live editor session global.
	local tracks = ARDOUR.LuaAPI.import_midi (Session, rv['path'],
	                                          rv['tempo_map'],
	                                          rv['markers'],
	                                          rv['split'])

	-- `tracks` is a MidiTrackList; #...:table() gives its length.
	local n = #tracks:table ()
	if n == 0 then
		LuaDialog.Message ("Import MIDI File",
		                   "No tracks were created. Is \"" .. rv['path'] .. "\" a valid MIDI file?",
		                   LuaDialog.MessageType.Warning, LuaDialog.ButtonType.Close):run ()
	else
		LuaDialog.Message ("Import MIDI File",
		                   string.format ("Imported %d MIDI track(s) from\n%s", n, rv['path']),
		                   LuaDialog.MessageType.Info, LuaDialog.ButtonType.Close):run ()
	end

end end
