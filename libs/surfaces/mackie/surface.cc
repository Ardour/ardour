#include <sstream>
#include <iomanip>
#include <iostream>
#include <cstdio>

#include "ardour/debug.h"

#include "control_group.h"
#include "surface_port.h"
#include "surface.h"
#include "strip.h"

#include "strip.h"
#include "button.h"
#include "led.h"
#include "ledring.h"
#include "pot.h"
#include "fader.h"
#include "jog.h"
#include "meter.h"

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
	{ "meter", 0, Meter::factory, },
	{ "", 0, Button::factory, }
};

void 
Surface::init_strips ()
{
	for (uint32_t i = 0; i < _max_strips; ++i) {

		char name[32];
		
		uint32_t unit_index = i % _unit_strips;
		
		snprintf (name, sizeof (name), "strip_%d", unit_index+1);

		cerr << "Register strip " << i << " unit index " << unit_index << endl;
		
		Strip* strip = new Strip (*this, name, i/8, i, unit_index, mackie_strip_controls);
		
		groups[name] = strip;
		strips[i] = strip;
	}
}

