--[[

# Example script to prepare the Ardour session for recording

Usually there's a certain state needed to actually start the recording.  This example
script treats the situation of a podcast recording. When starting the recording the
following settings have to be ensured.

* Session has to be recenabled
* Tracks have to be recenabled
* Gain automation have to set on write in order to record events from mute buttons
* The playhead has to be at 00:00:00.000
* The last (failed) capture has to be cleared
* Location markers have to be cleared

So this script automizes away the task and lets the podcast moderator by just one
action (for example triggerd by a Wiimote) prepare the session for recording.

It can be used for example with the python script of the Linux podcasting hacks:
https://github.com/linux-podcasting-hacks/wiimote-recording-control

Not that this script is more meant as an demo script to demonstrate the
possibilities of the Lua interface.

--]]

ardour {
	["type"] = "EditorAction",
	name = "Prepare recording for podcast",
	author = "Johannes Mueller",
	description = [[
Prepares the Ardour session for podcast recording.

* Sets the gain automation to "Write" so that muting buttons work.
* Recenables all tracks.
* Clears all markers.
* Erases the last capture (assuming that it was a failed one)
* Rewinds the session to starting point.
* Recenables the session.
]]
}

function factory (unused) return function()
	if Session:actively_recording() then
		return end

	for t in Session:get_tracks():iter() do
		t:gain_control():set_automation_state(ARDOUR.AutoState.Write)
		t:rec_enable_control():set_value(1, PBD.GroupControlDisposition.UseGroup)
	end

	for l in Session:locations():list():iter() do
		if l:is_mark() then
			Session:locations():remove(l)
		end
	end

	Session:goto_start()
	Editor:remove_last_capture()
	Session:maybe_enable_record()

end end

function icon (params) return function (ctx, width, height)
	local x = width * .5
	local y = height * .5
	local r = math.min (x, y) * .55

	ctx:arc (x, y, r, 0, 2 * math.pi)
	ctx:set_source_rgba (.9, .3, .3, 1.)
	ctx:fill_preserve ()
	ctx:set_source_rgba (0, 0, 0, .8)
	ctx:set_line_width (1)
	ctx:stroke ()

	local txt = Cairo.PangoLayout (ctx, "ArdourMono ".. math.ceil(r * 1.5) .. "px")
	txt:set_text ("P")
	local tw, th = txt:get_pixel_size ()
	ctx:set_source_rgba (0, 0, 0, 1.0)
	ctx:move_to (.5 * (width - tw), .5 * (height - th))
	txt:show_in_cairo_context (ctx)
end end
