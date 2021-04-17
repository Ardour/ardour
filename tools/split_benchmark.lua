-- cd gtk2_ardour; ./arlua < ../tools/split_benchmark.lua

reclen = 30 -- seconds

backend = AudioEngine:set_backend("None (Dummy)", "", "")
backend:set_device_name ("Uniform White Noise")

os.execute('rm -rf /tmp/luabench')
s = create_session ("/tmp/luabench", "luabench", 48000)
assert (s)

s:new_audio_track (1, 2, nil, 16, "",  ARDOUR.PresentationInfo.max_order, ARDOUR.TrackMode.Normal)

for t in s:get_tracks():iter() do
	t:rec_enable_control():set_value(1, PBD.GroupControlDisposition.UseGroup)
end

ARDOUR.LuaAPI.usleep (100000)

s:goto_start()
s:maybe_enable_record()

s:request_roll (4)
ARDOUR.LuaAPI.usleep (1000000 * reclen)
s:request_stop (false, false, 4);

for t in s:get_tracks():iter() do
	t:rec_enable_control():set_value(0, PBD.GroupControlDisposition.UseGroup)
end

ARDOUR.LuaAPI.usleep (100000)

s:goto_start()
s:save_state("")

function split_at (pos)
	local add_undo = false -- keep track if something has changed
	Session:begin_reversible_command ("Auto Region Split")
	for route in Session:get_tracks():iter() do
		local playlist = route:to_track():playlist ()
		playlist:to_stateful ():clear_changes ()
		for region in playlist:regions_at (pos):iter () do
			playlist:split_region (region, ARDOUR.MusicSample (pos, 0))
		end
		if not Session:add_stateful_diff_command (playlist:to_statefuldestructible ()):empty () then
			add_undo = true
		end
	end
	if add_undo then
		Session:commit_reversible_command (nil)
	else
		Session:abort_reversible_command ()
	end
end

function count_regions ()
	local total = 0
	for route in Session:get_tracks():iter() do
		total = total + route:to_track():playlist():region_list():size()
	end
	return total
end

stepsize = Session:samples_per_timecode_frame()
fps = Session:nominal_sample_rate () / stepsize
n_steps = 10
cnt = reclen * fps / n_steps

for x = 2, cnt do
	local playhead = Session:transport_sample ()

	local t_start = ARDOUR.LuaAPI.monotonic_time ()
	for i = 1, n_steps do
		split_at (playhead + stepsize * i)
	end
	local t_end = ARDOUR.LuaAPI.monotonic_time ()

	Session:request_locate((playhead + stepsize * n_steps), false, 5)
	print (count_regions (), (t_end - t_start) / 1000 / n_steps)
	collectgarbage ();
	ARDOUR.LuaAPI.usleep(500000)
end

s:save_state("")
close_session()
quit()
