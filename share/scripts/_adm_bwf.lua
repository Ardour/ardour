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

	function parse_zone (zone)
		if zone == "No Back" then return 1 end
		if zone == "No Sides" then return 2 end
		if zone == "Center Back" then return 3 end
		if zone == "Screen Only" then return 4 end
		if zone == "Surround Only" then return 5 end
		return 0
	end

	function parse_ramp (ramp)
		if tonumber(ramp) == 0 then return 0 end
		return 1
	end

	local rv = LuaDialog.Dialog ("Load ADM/BWF File",
	{
		{ type = "file", key = "file", title = "Choose ADM/BWF File", path = "" },
	}):run()

	if (not rv or not ARDOUR.LuaAPI.file_test (rv['file'], ARDOUR.LuaAPI.FileTest.Exists)) then
		return
	end

	-- place `Dolby_Atmos_Storage_SIDK_v2.3.2/Tools/linux/lin64_fpic/master_info` in $PATH
	os.execute ("master_info -printMetadata \"" .. rv['file'] .. "\" > /tmp/adm.info")

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
	local chan_map = {}
	local last_chan = 0

	for line in io.lines('/tmp/adm.info') do

		local rv, _, chn = string.find (line, "channel (%d+) descriptor")
		if rv then
			last_chan = tonumber (chn)
			goto next
		end
		local rv, _, idx = string.find (line, "source channel index: (%d+)")
		if rv then
			chan_map[last_chan] = tonumber(idx)
			goto next
		end

		local rv, _, obj, tme, pan_ramp, pan_x, pan_y, pan_z, pan_snap, pan_eleven, pan_zone, pan_size, bin_mode = string.find (line,
		"Metadata.* index: (%d+) offset: (%d+) .* ramp: (%d+) pos: %(([%d%.]+),([%d%.]+),([%d%.-]+)%) snap: (%d) elevation: (%d) zone: '([%a ]+)' size: %(([%d%.]+),.* binaural: '(%a+)'")

		if not rv then goto next end

		if not meta[obj] then
			meta[obj] = {x = {}, y = {}, z = {}, sz = {}, sn = {}, el = {}, rp = {}, zn = {}}
		end
		tme = tonumber(tme)
		meta[obj]['x'][tme] = tonumber(pan_x)
		meta[obj]['y'][tme] = tonumber(pan_y)
		meta[obj]['z'][tme] = tonumber(pan_z)
		meta[obj]['sz'][tme] = tonumber(pan_size)
		meta[obj]['sn'][tme] = tonumber(pan_snap)
		meta[obj]['el'][tme] = tonumber(pan_eleven)
		meta[obj]['rp'][tme] = parse_ramp(pan_ramp)
		meta[obj]['zn'][tme] = parse_zone(pan_zone)
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

		local thin = 0.00001

		ARDOUR.LuaAPI.set_automation_data (p.pan_pos_x, d['x'], thin)
		ARDOUR.LuaAPI.set_automation_data (p.pan_pos_y, d['y'], thin)
		ARDOUR.LuaAPI.set_automation_data (p.pan_pos_z, d['z'], thin)
		ARDOUR.LuaAPI.set_automation_data (p.pan_size, d['sz'], thin)
		ARDOUR.LuaAPI.set_automation_data (p.pan_snap, d['sn'], thin)

		ARDOUR.LuaAPI.set_automation_data (p.sur_elevation_enable, d['el'], thin)
		ARDOUR.LuaAPI.set_automation_data (p.sur_zones,            d['zn'], thin)
		ARDOUR.LuaAPI.set_automation_data (p.sur_ramp,             d['rp'], thin)

		p.binaural_render_mode:set_value (parse_bin_mode (d['bin']), PBD.GroupControlDisposition.NoGroup)

		-- this changes all
		p.pan_pos_x:set_automation_state (ARDOUR.AutoState.Play)

		::skip::
	end

	local imap = C.IntVector()
	for k = 0, 127 do
		v = chan_map[k] or k
		imap:add ({v})
	end

	Session:surround_master():surround_return():set_bed_mix (true, rv['file'], imap:to_array ())
	print ("OK")
end end
