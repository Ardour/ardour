ardour {
	["type"]    = "EditorAction",
	name        = "Import ADM BWF File",
	author      = "Ardour Team",
	description = [[...]]
}

function factory () return function ()

	function parse_bin_mode (mode)
		if mode == "Off" then return 1 end
		if mode == "Near" then return 2 end
		if mode == "Far" then return 3 end
		return 0 -- "Undefined" ie "Mid"
	end

	local rv = LuaDialog.Dialog ("Load ADM/BWF File",
	{
		{ type = "file", key = "file", title = "Choose ADM/BWF File", path = "" },
	}):run()

	if (not rv or not ARDOUR.LuaAPI.file_test (rv['file'], ARDOUR.LuaAPI.FileTest.Exists)) then
		return
	end

	-- place `Dolby_Atmos_Storage_SIDK_v2.3.2/Tools/linux/lin64_fpic/master_info` in $PATH
	os.execute ("master_info -printProgramData -printMetadata \"" .. rv['file'] .. "\" > /tmp/adm.info")

	if Session:get_tracks():size() == 0 then
		print ("Importing Files ...")
		Session:cfg():set_use_surround_master (true)
		Session:cfg():set_use_region_fades (false)
		local files = C.StringVector()
		files:push_back (rv['file'])
		local pos = Temporal.timepos_t(0)
		Editor:do_import (files,
		Editing.ImportDistinctChannels, Editing.ImportAsTrack, ARDOUR.SrcQuality.SrcBest,
		ARDOUR.MidiTrackNameSource.SMFTrackName, ARDOUR.MidiTempoMapDisposition.SMFTempoIgnore,
		pos, ARDOUR.PluginInfo(), ARDOUR.Track(), false)
		print ("Files Imported")
	end

	local meta = {}
	local chan_type = {[0] = 0, [1] = 1, [2] = 2, [3] = 3, [4] = 6, [5] = 7, [6] = 8, [7] = 9, [8] = 12, [9] = 13}
	local chan_beds = {[0] = 0, [1] = 0, [2] = 0, [3] = 0, [4] = 0, [5] = 0, [6] = 0, [7] = 0, [8] = 0, [9] = 0}
	local last_chan = 0
	local ffoa_sec  = 0

	for line in io.lines('/tmp/adm.info') do

		local rv, _, chn = string.find (line, "channel (%d+) descriptor")
		if rv then
			last_chan = tonumber (chn)
			goto next
		end
		local rv, _, chn = string.find (line, "channel type: (%a+)")
		if rv then
			chan_type[last_chan] = chn
			goto next
		end
		local rv, _, bed = string.find (line, "object/bed ID: (%d+)")
		if rv then
			chan_beds[last_chan] = tonumber(bed)
			goto next
		end
		local rv, _, ffoa = string.find (line, "FFOA: ([%d%.-]+) sec")
		if rv then
			ffoa_sec = tonumber (ffoa)
			goto next
		end

		local rv, _, obj, tme, pan_x, pan_y, pan_z, pan_snap, pan_size, bin_mode = string.find (line,
		"Metadata.* index: (%d+) offset: (%d+) .* pos: %(([%d%.]+),([%d%.]+),([%d%.]+)%) snap: (%d) .* size: %(([%d%.]+),.* binaural: '(%a+)'")

		if not rv then goto next end

		if not meta[obj] then
			meta[obj] = {x = {}, y = {}, z = {}, sz = {}, sn = {}}
		end
		tme = tonumber(tme)
		meta[obj]['x'][tme] = tonumber(pan_x)
		meta[obj]['y'][tme] = tonumber(pan_y)
		meta[obj]['z'][tme] = tonumber(pan_z)
		meta[obj]['sz'][tme] = tonumber(pan_size)
		meta[obj]['sn'][tme] = tonumber(pan_snap)
		meta[obj]['bin'] = bin_mode

		::next::
	end

	print ("Setting Metadata")

	for obj, d in pairs (meta) do
		local r = Session:get_remote_nth_route (obj)
		if r:isnil () then goto skip end
		local s = r:surround_send ()
		if s:isnil () then goto skip end
		assert(1 == s:n_pannables ())
		local p = s:pannable (0)

		ARDOUR.LuaAPI.set_automation_data (p.pan_pos_x, d['x'], 0.00001)
		ARDOUR.LuaAPI.set_automation_data (p.pan_pos_y, d['y'], 0.00001)
		ARDOUR.LuaAPI.set_automation_data (p.pan_pos_z, d['z'], 0.00001)
		ARDOUR.LuaAPI.set_automation_data (p.pan_size, d['sz'], 0.00001)
		ARDOUR.LuaAPI.set_automation_data (p.pan_snap, d['sn'], 0.00001)
		p.binaural_render_mode:set_value (parse_bin_mode (d['bin']), PBD.GroupControlDisposition.NoGroup)

		-- this changes all
		p.pan_pos_x:set_automation_state (ARDOUR.AutoState.Play)

		::skip::
	end

	Session:surround_master():surround_return():set_bed_mix (true, rv['file'])
	print ("OK")
end end
