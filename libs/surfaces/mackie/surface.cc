#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdio>

#include "ardour/debug.h"

#include "mackie_button_handler.h"
#include "surface_port.h"
#include "surface.h"

using namespace std;
using namespace PBD;
using namespace Mackie;

Surface::Surface (uint32_t max_strips, uint32_t unit_strips)
	: _max_strips (max_strips)
	, _unit_strips( unit_strips )
{
}

void Surface::init ()
{
	DEBUG_TRACE (DEBUG::MackieControl, "Surface::init\n");

	strips.resize (_max_strips);
	init_controls ();
	init_strips ();

	DEBUG_TRACE (DEBUG::MackieControl, "Surface::init finish\n");
}

Surface::~Surface ()
{
	// delete groups
	for (Groups::iterator it = groups.begin(); it != groups.end(); ++it) {
		delete it->second;
	}
	
	// delete controls
	for (Controls::iterator it = controls.begin(); it != controls.end(); ++it) {
		delete *it;
	}
}

static GlobalControlDefinition mackie_global_controls[] = {
	{ "jog", 0x3c, Jog::factory, "none" },
	{ "external", 0x2e, Pot::factory, "none" },
	{ "io", 0x28, Button::factory, "assignment" },
	{ "sends", 0x29, Button::factory, "assignment" },
	{ "pan", 0x2a, Button::factory, "assignment" },
	{ "plugin", 0x2b, Button::factory, "assignment" },
	{ "eq", 0x2c, Button::factory, "assignment" },
	{ "dyn", 0x2d, Button::factory, "assignment" },
	{ "left", 0x2e, Button::factory, "bank" },
	{ "right", 0x2f, Button::factory, "bank" },
	{ "channel_left", 0x30, Button::factory, "bank" },
	{ "channel_right", 0x31, Button::factory, "bank" },
	{ "flip", 0x32, Button::factory, "none" },
	{ "edit", 0x33, Button::factory, "none" },
	{ "name_value", 0x34, Button::factory, "display" },
	{ "timecode_beats", 0x35, Button::factory, "display" },
	{ "F1", 0x36, Button::factory, "none" },
	{ "F2", 0x37, Button::factory, "none" },
	{ "F3", 0x38, Button::factory, "none" },
	{ "F4", 0x39, Button::factory, "none" },
	{ "F5", 0x3a, Button::factory, "none" },
	{ "F6", 0x3b, Button::factory, "none" },
	{ "F7", 0x3c, Button::factory, "none" },
	{ "F8", 0x3d, Button::factory, "none" },
	{ "F9", 0x3e, Button::factory, "none" },
	{ "F10", 0x3f, Button::factory, "none" },
	{ "F11", 0x40, Button::factory, "none" },
	{ "F12", 0x41, Button::factory, "none" },
	{ "F13", 0x42, Button::factory, "none" },
	{ "F14", 0x43, Button::factory, "none" },
	{ "F15", 0x44, Button::factory, "none" },
	{ "F16", 0x45, Button::factory, "none" },
	{ "shift", 0x46, Button::factory, "modifiers" },
	{ "option", 0x47, Button::factory, "modifiers" },
	{ "control", 0x48, Button::factory, "modifiers" },
	{ "cmd_alt", 0x49, Button::factory, "modifiers" },
	{ "on", 0x4a, Button::factory, "automation" },
	{ "rec_ready", 0x4b, Button::factory, "automation" },
	{ "undo", 0x4c, Button::factory, "functions" },
	{ "snapshot", 0x4d, Button::factory, "automation" },
	{ "touch", 0x4e, Button::factory, "automation" },
	{ "redo", 0x4f, Button::factory, "functions" },
	{ "marker", 0x50, Button::factory, "functions" },
	{ "enter", 0x51, Button::factory, "functions" },
	{ "cancel", 0x52, Button::factory, "functions" },
	{ "mixer", 0x53, Button::factory, "functions" },
	{ "frm_left", 0x54, Button::factory, "transport" },
	{ "frm_right", 0x55, Button::factory, "transport" },
	{ "loop", 0x56, Button::factory, "transport" },
	{ "punch_in", 0x57, Button::factory, "transport" },
	{ "punch_out", 0x58, Button::factory, "transport" },
	{ "home", 0x59, Button::factory, "transport" },
	{ "end", 0x5a, Button::factory, "transport" },
	{ "rewind", 0x5b, Button::factory, "transport" },
	{ "ffwd", 0x5c, Button::factory, "transport" },
	{ "stop", 0x5d, Button::factory, "transport" },
	{ "play", 0x5e, Button::factory, "transport" },
	{ "record", 0x5f, Button::factory, "transport" },
	{ "cursor_up", 0x60, Button::factory, "cursor" },
	{ "cursor_down", 0x61, Button::factory, "cursor" },
	{ "cursor_left", 0x62, Button::factory, "cursor" },
	{ "cursor_right", 0x63, Button::factory, "cursor" },
	{ "zoom", 0x64, Button::factory, "none" },
	{ "scrub", 0x65, Button::factory, "none" },
	{ "user_a", 0x66, Button::factory, "user" },
	{ "user_b", 0x67, Button::factory, "user" },
	{ "fader_touch", 0x70, Led::factory, "master" },
	{ "timecode", 0x71, Led::factory, "none" },
	{ "beats", 0x72, Led::factory, "none" },
	{ "solo", 0x73, Led::factory, "none" },
	{ "relay_click", 0x73, Led::factory, "none" },
	{ "", 0, Button::factory, "" }
};
	
void 
Surface::init_controls()
{
	Group* group;

	groups["assignment"] = new Group  ("assignment");
	groups["automation"] = new Group  ("automation");
	groups["bank"] = new Group  ("bank");
	groups["cursor"] = new Group  ("cursor");
	groups["display"] = new Group  ("display");
	groups["functions"] = new Group  ("functions");
	groups["modifiers"] = new Group  ("modifiers");
	groups["none"] = new Group  ("none");
	groups["transport"] = new Group  ("transport");
	groups["user"] = new Group  ("user");

	group = new MasterStrip  ("master", 0);
	groups["master"] = group;
	strips[0] = dynamic_cast<Strip*> (group);

	for (uint32_t n = 0; mackie_global_controls[n].name[0]; ++n) {
		group = groups[mackie_global_controls[n].group_name];
		Control* control = mackie_global_controls[n].factory (*this, mackie_global_controls[n].id, 1, mackie_global_controls[n].name, *group);
		controls_by_name[mackie_global_controls[n].name] = control;
		group->add (*control);
	}
}

static StripControlDefinition mackie_strip_controls[] = {
	{ "gain", Control::fader_base_id, Fader::factory, },
	{ "vpot", Control::pot_base_id, Pot::factory, },
	{ "recenable", Control::recenable_button_base_id, Button::factory, },
	{ "solo", Control::solo_button_base_id, Button::factory, },
	{ "mute", Control::mute_button_base_id, Button::factory, },
	{ "select", Control::select_button_base_id, Button::factory, },
	{ "vselect", Control::vselect_button_base_id, Button::factory, },
	{ "fader_touch", Control::fader_touch_button_base_id, Button::factory, },
	{ "", 0, Button::factory, }
};

void 
Surface::init_strips ()
{
	for (uint32_t i = 0; i < _max_strips; ++i) {

		char name[32];
		
		uint32_t unit_index = i % _unit_strips;
		
		snprintf (name, sizeof (name), "strip_%d", unit_index+1);
		
		Strip* strip = new Strip (*this, name, i, unit_index, mackie_strip_controls);
		
		groups[name] = strip;
		strips[i] = strip;
	}
}

void 
Surface::handle_button (MackieButtonHandler & mbh, ButtonState bs, Button & button)
{
	if  (bs != press && bs != release) {
		mbh.update_led (button, none);
		return;
	}
	
	LedState ls;
	switch  (button.id()) {
	case 0x9028: // io
		switch  (bs) {
		case press: ls = mbh.io_press (button); break;
		case release: ls = mbh.io_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x9029: // sends
		switch  (bs) {
		case press: ls = mbh.sends_press (button); break;
		case release: ls = mbh.sends_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x902a: // pan
		switch  (bs) {
		case press: ls = mbh.pan_press (button); break;
		case release: ls = mbh.pan_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x902b: // plugin
		switch  (bs) {
		case press: ls = mbh.plugin_press (button); break;
		case release: ls = mbh.plugin_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x902c: // eq
		switch  (bs) {
		case press: ls = mbh.eq_press (button); break;
		case release: ls = mbh.eq_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x902d: // dyn
		switch  (bs) {
		case press: ls = mbh.dyn_press (button); break;
		case release: ls = mbh.dyn_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x902e: // left
		switch  (bs) {
		case press: ls = mbh.left_press (button); break;
		case release: ls = mbh.left_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x902f: // right
		switch  (bs) {
		case press: ls = mbh.right_press (button); break;
		case release: ls = mbh.right_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x9030: // channel_left
		switch  (bs) {
		case press: ls = mbh.channel_left_press (button); break;
		case release: ls = mbh.channel_left_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x9031: // channel_right
		switch  (bs) {
		case press: ls = mbh.channel_right_press (button); break;
		case release: ls = mbh.channel_right_release (button); break;
		case neither: break;
		}
		break;
		
	case 0x9032: // flip
		switch  (bs) {
		case press: ls = mbh.flip_press (button); break;
		case release: ls = mbh.flip_release (button); break;
		case neither: break;
		}
		break;

	case 0x9033: // edit
		switch  (bs) {
		case press: ls = mbh.edit_press (button); break;
		case release: ls = mbh.edit_release (button); break;
		case neither: break;
		}
		break;

	case 0x9034: // name_value
		switch  (bs) {
		case press: ls = mbh.name_value_press (button); break;
		case release: ls = mbh.name_value_release (button); break;
		case neither: break;
		}
		break;

	case 0x9035: // timecode_beats
		switch  (bs) {
		case press: ls = mbh.timecode_beats_press (button); break;
		case release: ls = mbh.timecode_beats_release (button); break;
		case neither: break;
		}
		break;

	case 0x9036: // F1
		switch  (bs) {
		case press: ls = mbh.F1_press (button); break;
		case release: ls = mbh.F1_release (button); break;
		case neither: break;
		}
		break;

	case 0x9037: // F2
		switch  (bs) {
		case press: ls = mbh.F2_press (button); break;
		case release: ls = mbh.F2_release (button); break;
		case neither: break;
		}
		break;

	case 0x9038: // F3
		switch  (bs) {
		case press: ls = mbh.F3_press (button); break;
		case release: ls = mbh.F3_release (button); break;
		case neither: break;
		}
		break;

	case 0x9039: // F4
		switch  (bs) {
		case press: ls = mbh.F4_press (button); break;
		case release: ls = mbh.F4_release (button); break;
		case neither: break;
		}
		break;

	case 0x903a: // F5
		switch  (bs) {
		case press: ls = mbh.F5_press (button); break;
		case release: ls = mbh.F5_release (button); break;
		case neither: break;
		}
		break;

	case 0x903b: // F6
		switch  (bs) {
		case press: ls = mbh.F6_press (button); break;
		case release: ls = mbh.F6_release (button); break;
		case neither: break;
		}
		break;

	case 0x903c: // F7
		switch  (bs) {
		case press: ls = mbh.F7_press (button); break;
		case release: ls = mbh.F7_release (button); break;
		case neither: break;
		}
		break;

	case 0x903d: // F8
		switch  (bs) {
		case press: ls = mbh.F8_press (button); break;
		case release: ls = mbh.F8_release (button); break;
		case neither: break;
		}
		break;

	case 0x903e: // F9
		switch  (bs) {
		case press: ls = mbh.F9_press (button); break;
		case release: ls = mbh.F9_release (button); break;
		case neither: break;
		}
		break;

	case 0x903f: // F10
		switch  (bs) {
		case press: ls = mbh.F10_press (button); break;
		case release: ls = mbh.F10_release (button); break;
		case neither: break;
		}
		break;

	case 0x9040: // F11
		switch  (bs) {
		case press: ls = mbh.F11_press (button); break;
		case release: ls = mbh.F11_release (button); break;
		case neither: break;
		}
		break;

	case 0x9041: // F12
		switch  (bs) {
		case press: ls = mbh.F12_press (button); break;
		case release: ls = mbh.F12_release (button); break;
		case neither: break;
		}
		break;

	case 0x9042: // F13
		switch  (bs) {
		case press: ls = mbh.F13_press (button); break;
		case release: ls = mbh.F13_release (button); break;
		case neither: break;
		}
		break;

	case 0x9043: // F14
		switch  (bs) {
		case press: ls = mbh.F14_press (button); break;
		case release: ls = mbh.F14_release (button); break;
		case neither: break;
		}
		break;

	case 0x9044: // F15
		switch  (bs) {
		case press: ls = mbh.F15_press (button); break;
		case release: ls = mbh.F15_release (button); break;
		case neither: break;
		}
		break;

	case 0x9045: // F16
		switch  (bs) {
		case press: ls = mbh.F16_press (button); break;
		case release: ls = mbh.F16_release (button); break;
		case neither: break;
		}
		break;

	case 0x9046: // shift
		switch  (bs) {
		case press: ls = mbh.shift_press (button); break;
		case release: ls = mbh.shift_release (button); break;
		case neither: break;
		}
		break;

	case 0x9047: // option
		switch  (bs) {
		case press: ls = mbh.option_press (button); break;
		case release: ls = mbh.option_release (button); break;
		case neither: break;
		}
		break;

	case 0x9048: // control
		switch  (bs) {
		case press: ls = mbh.control_press (button); break;
		case release: ls = mbh.control_release (button); break;
		case neither: break;
		}
		break;

	case 0x9049: // cmd_alt
		switch  (bs) {
		case press: ls = mbh.cmd_alt_press (button); break;
		case release: ls = mbh.cmd_alt_release (button); break;
		case neither: break;
		}
		break;

	case 0x904a: // on
		switch  (bs) {
		case press: ls = mbh.on_press (button); break;
		case release: ls = mbh.on_release (button); break;
		case neither: break;
		}
		break;

	case 0x904b: // rec_ready
		switch  (bs) {
		case press: ls = mbh.rec_ready_press (button); break;
		case release: ls = mbh.rec_ready_release (button); break;
		case neither: break;
		}
		break;

	case 0x904c: // undo
		switch  (bs) {
		case press: ls = mbh.undo_press (button); break;
		case release: ls = mbh.undo_release (button); break;
		case neither: break;
		}
		break;

	case 0x904d: // snapshot
		switch  (bs) {
		case press: ls = mbh.snapshot_press (button); break;
		case release: ls = mbh.snapshot_release (button); break;
		case neither: break;
		}
		break;

	case 0x904e: // touch
		switch  (bs) {
		case press: ls = mbh.touch_press (button); break;
		case release: ls = mbh.touch_release (button); break;
		case neither: break;
		}
		break;

	case 0x904f: // redo
		switch  (bs) {
		case press: ls = mbh.redo_press (button); break;
		case release: ls = mbh.redo_release (button); break;
		case neither: break;
		}
		break;

	case 0x9050: // marker
		switch  (bs) {
		case press: ls = mbh.marker_press (button); break;
		case release: ls = mbh.marker_release (button); break;
		case neither: break;
		}
		break;

	case 0x9051: // enter
		switch  (bs) {
		case press: ls = mbh.enter_press (button); break;
		case release: ls = mbh.enter_release (button); break;
		case neither: break;
		}
		break;

	case 0x9052: // cancel
		switch  (bs) {
		case press: ls = mbh.cancel_press (button); break;
		case release: ls = mbh.cancel_release (button); break;
		case neither: break;
		}
		break;

	case 0x9053: // mixer
		switch  (bs) {
		case press: ls = mbh.mixer_press (button); break;
		case release: ls = mbh.mixer_release (button); break;
		case neither: break;
		}
		break;

	case 0x9054: // frm_left
		switch  (bs) {
		case press: ls = mbh.frm_left_press (button); break;
		case release: ls = mbh.frm_left_release (button); break;
		case neither: break;
		}
		break;

	case 0x9055: // frm_right
		switch  (bs) {
		case press: ls = mbh.frm_right_press (button); break;
		case release: ls = mbh.frm_right_release (button); break;
		case neither: break;
		}
		break;

	case 0x9056: // loop
		switch  (bs) {
		case press: ls = mbh.loop_press (button); break;
		case release: ls = mbh.loop_release (button); break;
		case neither: break;
		}
		break;

	case 0x9057: // punch_in
		switch  (bs) {
		case press: ls = mbh.punch_in_press (button); break;
		case release: ls = mbh.punch_in_release (button); break;
		case neither: break;
		}
		break;

	case 0x9058: // punch_out
		switch  (bs) {
		case press: ls = mbh.punch_out_press (button); break;
		case release: ls = mbh.punch_out_release (button); break;
		case neither: break;
		}
		break;

	case 0x9059: // home
		switch  (bs) {
		case press: ls = mbh.home_press (button); break;
		case release: ls = mbh.home_release (button); break;
		case neither: break;
		}
		break;

	case 0x905a: // end
		switch  (bs) {
		case press: ls = mbh.end_press (button); break;
		case release: ls = mbh.end_release (button); break;
		case neither: break;
		}
		break;

	case 0x905b: // rewind
		switch  (bs) {
		case press: ls = mbh.rewind_press (button); break;
		case release: ls = mbh.rewind_release (button); break;
		case neither: break;
		}
		break;

	case 0x905c: // ffwd
		switch  (bs) {
		case press: ls = mbh.ffwd_press (button); break;
		case release: ls = mbh.ffwd_release (button); break;
		case neither: break;
		}
		break;

	case 0x905d: // stop
		switch  (bs) {
		case press: ls = mbh.stop_press (button); break;
		case release: ls = mbh.stop_release (button); break;
		case neither: break;
		}
		break;

	case 0x905e: // play
		switch  (bs) {
		case press: ls = mbh.play_press (button); break;
		case release: ls = mbh.play_release (button); break;
		case neither: break;
		}
		break;

	case 0x905f: // record
		switch  (bs) {
		case press: ls = mbh.record_press (button); break;
		case release: ls = mbh.record_release (button); break;
		case neither: break;
		}
		break;

	case 0x9060: // cursor_up
		switch  (bs) {
		case press: ls = mbh.cursor_up_press (button); break;
		case release: ls = mbh.cursor_up_release (button); break;
		case neither: break;
		}
		break;

	case 0x9061: // cursor_down
		switch  (bs) {
		case press: ls = mbh.cursor_down_press (button); break;
		case release: ls = mbh.cursor_down_release (button); break;
		case neither: break;
		}
		break;

	case 0x9062: // cursor_left
		switch  (bs) {
		case press: ls = mbh.cursor_left_press (button); break;
		case release: ls = mbh.cursor_left_release (button); break;
		case neither: break;
		}
		break;

	case 0x9063: // cursor_right
		switch  (bs) {
		case press: ls = mbh.cursor_right_press (button); break;
		case release: ls = mbh.cursor_right_release (button); break;
		case neither: break;
		}
		break;

	case 0x9064: // zoom
		switch  (bs) {
		case press: ls = mbh.zoom_press (button); break;
		case release: ls = mbh.zoom_release (button); break;
		case neither: break;
		}
		break;

	case 0x9065: // scrub
		switch  (bs) {
		case press: ls = mbh.scrub_press (button); break;
		case release: ls = mbh.scrub_release (button); break;
		case neither: break;
		}
		break;

	case 0x9066: // user_a
		switch  (bs) {
		case press: ls = mbh.user_a_press (button); break;
		case release: ls = mbh.user_a_release (button); break;
		case neither: break;
		}
		break;

	case 0x9067: // user_b
		switch  (bs) {
		case press: ls = mbh.user_b_press (button); break;
		case release: ls = mbh.user_b_release (button); break;
		case neither: break;
		}
		break;

	}
	mbh.update_led (button, ls);
}
