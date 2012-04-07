#include <cmath>
#include <sstream>
#include <string>

#include "mackie_surface.h"
#include "surface_port.h"
#include "mackie_midi_builder.h"

using namespace Mackie;

void 
MackieSurface::display_timecode( SurfacePort & port, MackieMidiBuilder & builder, const std::string & timecode, const std::string & timecode_last )
{
	port.write( builder.timecode_display( port, timecode, timecode_last ) );
}

float 
MackieSurface::scaled_delta( const ControlState & state, float current_speed )
{
	return state.sign * ( std::pow( float(state.ticks + 1), 2 ) + current_speed ) / 100.0;
}

void 
Mackie::MackieSurface::init_controls()
{
	Pot* pot = 0;
	Button* button = 0;
	Led* led = 0;

	// intialise groups and strips
	Group * group = 0;
	
	group = new Group  ("user");
	groups["user"] = group;
	
	group = new Group  ("assignment");
	groups["assignment"] = group;
	
	group = new Group  ("none");
	groups["none"] = group;
	
	group = new MasterStrip  ("master", 0);
	groups["master"] = group;
	strips[0] = dynamic_cast<Strip*> (group);
	
	group = new Group  ("cursor");
	groups["cursor"] = group;
	

	group = new Group  ("functions");
	groups["functions"] = group;
	
	group = new Group  ("automation");
	groups["automation"] = group;
	

	group = new Group  ("display");
	groups["display"] = group;
		
	group = new Group  ("transport");
	groups["transport"] = group;
	
	group = new Group  ("modifiers");
	groups["modifiers"] = group;
	
	group = new Group  ("bank");
	groups["bank"] = group;
	
	group = groups["none"];
	pot = new Jog  (1, "jog", *group);
	pots[0x3c] = pot;
	controls.push_back (pot);
	controls_by_name["jog"] = pot;
	group->add (*pot);

	group = groups["none"];
	pot = new Pot  (1, "external", *group);
	pots[0x2e] = pot;
	controls.push_back (pot);
	controls_by_name["external"] = pot;
	group->add (*pot);

	group = groups["assignment"];
	button = new Button (1, "io", *group);
	buttons[0x28] = button;
	controls.push_back (button);
	controls_by_name["io"] = button;
	group->add (*button);

	group = groups["assignment"];
	button = new Button (1, "sends", *group);
	buttons[0x29] = button;
	controls.push_back (button);
	controls_by_name["sends"] = button;
	group->add (*button);

	group = groups["assignment"];
	button = new Button (1, "pan", *group);
	buttons[0x2a] = button;
	controls.push_back (button);
	controls_by_name["pan"] = button;
	group->add (*button);

	group = groups["assignment"];
	button = new Button (1, "plugin", *group);
	buttons[0x2b] = button;
	controls.push_back (button);
	controls_by_name["plugin"] = button;
	group->add (*button);

	group = groups["assignment"];
	button = new Button (1, "eq", *group);
	buttons[0x2c] = button;
	controls.push_back (button);
	controls_by_name["eq"] = button;
	group->add (*button);

	group = groups["assignment"];
	button = new Button (1, "dyn", *group);
	buttons[0x2d] = button;
	controls.push_back (button);
	controls_by_name["dyn"] = button;
	group->add (*button);

	group = groups["bank"];
	button = new Button (1, "left", *group);
	buttons[0x2e] = button;
	controls.push_back (button);
	controls_by_name["left"] = button;
	group->add (*button);

	group = groups["bank"];
	button = new Button (1, "right", *group);
	buttons[0x2f] = button;
	controls.push_back (button);
	controls_by_name["right"] = button;
	group->add (*button);

	group = groups["bank"];
	button = new Button (1, "channel_left", *group);
	buttons[0x30] = button;
	controls.push_back (button);
	controls_by_name["channel_left"] = button;
	group->add (*button);

	group = groups["bank"];
	button = new Button (1, "channel_right", *group);
	buttons[0x31] = button;
	controls.push_back (button);
	controls_by_name["channel_right"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "flip", *group);
	buttons[0x32] = button;
	controls.push_back (button);
	controls_by_name["flip"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "edit", *group);
	buttons[0x33] = button;
	controls.push_back (button);
	controls_by_name["edit"] = button;
	group->add (*button);

	group = groups["display"];
	button = new Button (1, "name_value", *group);
	buttons[0x34] = button;
	controls.push_back (button);
	controls_by_name["name_value"] = button;
	group->add (*button);

	group = groups["display"];
	button = new Button (1, "timecode_beats", *group);
	buttons[0x35] = button;
	controls.push_back (button);
	controls_by_name["timecode_beats"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F1", *group);
	buttons[0x36] = button;
	controls.push_back (button);
	controls_by_name["F1"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F2", *group);
	buttons[0x37] = button;
	controls.push_back (button);
	controls_by_name["F2"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F3", *group);
	buttons[0x38] = button;
	controls.push_back (button);
	controls_by_name["F3"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F4", *group);
	buttons[0x39] = button;
	controls.push_back (button);
	controls_by_name["F4"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F5", *group);
	buttons[0x3a] = button;
	controls.push_back (button);
	controls_by_name["F5"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F6", *group);
	buttons[0x3b] = button;
	controls.push_back (button);
	controls_by_name["F6"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F7", *group);
	buttons[0x3c] = button;
	controls.push_back (button);
	controls_by_name["F7"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F8", *group);
	buttons[0x3d] = button;
	controls.push_back (button);
	controls_by_name["F8"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F9", *group);
	buttons[0x3e] = button;
	controls.push_back (button);
	controls_by_name["F9"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F10", *group);
	buttons[0x3f] = button;
	controls.push_back (button);
	controls_by_name["F10"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F11", *group);
	buttons[0x40] = button;
	controls.push_back (button);
	controls_by_name["F11"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F12", *group);
	buttons[0x41] = button;
	controls.push_back (button);
	controls_by_name["F12"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F13", *group);
	buttons[0x42] = button;
	controls.push_back (button);
	controls_by_name["F13"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F14", *group);
	buttons[0x43] = button;
	controls.push_back (button);
	controls_by_name["F14"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F15", *group);
	buttons[0x44] = button;
	controls.push_back (button);
	controls_by_name["F15"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "F16", *group);
	buttons[0x45] = button;
	controls.push_back (button);
	controls_by_name["F16"] = button;
	group->add (*button);

	group = groups["modifiers"];
	button = new Button (1, "shift", *group);
	buttons[0x46] = button;
	controls.push_back (button);
	controls_by_name["shift"] = button;
	group->add (*button);

	group = groups["modifiers"];
	button = new Button (1, "option", *group);
	buttons[0x47] = button;
	controls.push_back (button);
	controls_by_name["option"] = button;
	group->add (*button);

	group = groups["modifiers"];
	button = new Button (1, "control", *group);
	buttons[0x48] = button;
	controls.push_back (button);
	controls_by_name["control"] = button;
	group->add (*button);

	group = groups["modifiers"];
	button = new Button (1, "cmd_alt", *group);
	buttons[0x49] = button;
	controls.push_back (button);
	controls_by_name["cmd_alt"] = button;
	group->add (*button);

	group = groups["automation"];
	button = new Button (1, "on", *group);
	buttons[0x4a] = button;
	controls.push_back (button);
	controls_by_name["on"] = button;
	group->add (*button);

	group = groups["automation"];
	button = new Button (1, "rec_ready", *group);
	buttons[0x4b] = button;
	controls.push_back (button);
	controls_by_name["rec_ready"] = button;
	group->add (*button);

	group = groups["functions"];
	button = new Button (1, "undo", *group);
	buttons[0x4c] = button;
	controls.push_back (button);
	controls_by_name["undo"] = button;
	group->add (*button);

	group = groups["automation"];
	button = new Button (1, "snapshot", *group);
	buttons[0x4d] = button;
	controls.push_back (button);
	controls_by_name["snapshot"] = button;
	group->add (*button);

	group = groups["automation"];
	button = new Button (1, "touch", *group);
	buttons[0x4e] = button;
	controls.push_back (button);
	controls_by_name["touch"] = button;
	group->add (*button);

	group = groups["functions"];
	button = new Button (1, "redo", *group);
	buttons[0x4f] = button;
	controls.push_back (button);
	controls_by_name["redo"] = button;
	group->add (*button);

	group = groups["functions"];
	button = new Button (1, "marker", *group);
	buttons[0x50] = button;
	controls.push_back (button);
	controls_by_name["marker"] = button;
	group->add (*button);

	group = groups["functions"];
	button = new Button (1, "enter", *group);
	buttons[0x51] = button;
	controls.push_back (button);
	controls_by_name["enter"] = button;
	group->add (*button);

	group = groups["functions"];
	button = new Button (1, "cancel", *group);
	buttons[0x52] = button;
	controls.push_back (button);
	controls_by_name["cancel"] = button;
	group->add (*button);

	group = groups["functions"];
	button = new Button (1, "mixer", *group);
	buttons[0x53] = button;
	controls.push_back (button);
	controls_by_name["mixer"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "frm_left", *group);
	buttons[0x54] = button;
	controls.push_back (button);
	controls_by_name["frm_left"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "frm_right", *group);
	buttons[0x55] = button;
	controls.push_back (button);
	controls_by_name["frm_right"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "loop", *group);
	buttons[0x56] = button;
	controls.push_back (button);
	controls_by_name["loop"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "punch_in", *group);
	buttons[0x57] = button;
	controls.push_back (button);
	controls_by_name["punch_in"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "punch_out", *group);
	buttons[0x58] = button;
	controls.push_back (button);
	controls_by_name["punch_out"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "home", *group);
	buttons[0x59] = button;
	controls.push_back (button);
	controls_by_name["home"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "end", *group);
	buttons[0x5a] = button;
	controls.push_back (button);
	controls_by_name["end"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "rewind", *group);
	buttons[0x5b] = button;
	controls.push_back (button);
	controls_by_name["rewind"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "ffwd", *group);
	buttons[0x5c] = button;
	controls.push_back (button);
	controls_by_name["ffwd"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "stop", *group);
	buttons[0x5d] = button;
	controls.push_back (button);
	controls_by_name["stop"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "play", *group);
	buttons[0x5e] = button;
	controls.push_back (button);
	controls_by_name["play"] = button;
	group->add (*button);

	group = groups["transport"];
	button = new Button (1, "record", *group);
	buttons[0x5f] = button;
	controls.push_back (button);
	controls_by_name["record"] = button;
	group->add (*button);

	group = groups["cursor"];
	button = new Button (1, "cursor_up", *group);
	buttons[0x60] = button;
	controls.push_back (button);
	controls_by_name["cursor_up"] = button;
	group->add (*button);

	group = groups["cursor"];
	button = new Button (1, "cursor_down", *group);
	buttons[0x61] = button;
	controls.push_back (button);
	controls_by_name["cursor_down"] = button;
	group->add (*button);

	group = groups["cursor"];
	button = new Button (1, "cursor_left", *group);
	buttons[0x62] = button;
	controls.push_back (button);
	controls_by_name["cursor_left"] = button;
	group->add (*button);

	group = groups["cursor"];
	button = new Button (1, "cursor_right", *group);
	buttons[0x63] = button;
	controls.push_back (button);
	controls_by_name["cursor_right"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "zoom", *group);
	buttons[0x64] = button;
	controls.push_back (button);
	controls_by_name["zoom"] = button;
	group->add (*button);

	group = groups["none"];
	button = new Button (1, "scrub", *group);
	buttons[0x65] = button;
	controls.push_back (button);
	controls_by_name["scrub"] = button;
	group->add (*button);

	group = groups["user"];
	button = new Button (1, "user_a", *group);
	buttons[0x66] = button;
	controls.push_back (button);
	controls_by_name["user_a"] = button;
	group->add (*button);

	group = groups["user"];
	button = new Button (1, "user_b", *group);
	buttons[0x67] = button;
	controls.push_back (button);
	controls_by_name["user_b"] = button;
	group->add (*button);

	group = groups["master"];
	button = new Button (1, "fader_touch", *group);
	buttons[0x70] = button;
	controls.push_back (button);
	group->add (*button);

	group = groups["none"];
	led = new Led  (1, "timecode", *group);
	leds[0x71] = led;
	controls.push_back (led);
	controls_by_name["timecode"] = led;
	group->add (*led);

	group = groups["none"];
	led = new Led  (1, "beats", *group);
	leds[0x72] = led;
	controls.push_back (led);
	controls_by_name["beats"] = led;
	group->add (*led);

	group = groups["none"];
	led = new Led  (1, "solo", *group);
	leds[0x73] = led;
	controls.push_back (led);
	controls_by_name["solo"] = led;
	group->add (*led);

	group = groups["none"];
	led = new Led  (1, "relay_click", *group);
	leds[0x76] = led;
	controls.push_back (led);
	controls_by_name["relay_click"] = led;
	group->add (*led);
}

void MackieSurface::init_strips ()
{
	Fader* fader = 0;
	Pot* pot = 0;
	Button* button = 0;

	for (uint32_t i = 0; i < _max_strips; ++i) {

		std::ostringstream os;
		uint32_t unit_index = i % _unit_strips;
		uint32_t unit_ordinal = unit_index + 1;

		os << "strip_" << unit_ordinal;
		std::string name = os.str();
		
		Strip* strip = new Strip (name, i);
		
		groups[name] = strip;
		strips[i] = strip;

		fader = new Fader (unit_ordinal, "gain", *strip);
		faders[0x00+unit_index] = fader;
		controls.push_back (fader);
		strip->add (*fader);

		pot = new Pot  (unit_ordinal, "vpot", *strip);
		pots[0x10+unit_index] = pot;
		controls.push_back (pot);
		strip->add (*pot);

		button = new Button  (unit_ordinal, "recenable", *strip);
		buttons[0x00+unit_index] = button;
		controls.push_back (button);
		strip->add (*button);

		button = new Button  (unit_ordinal, "solo", *strip);
		buttons[0x08+unit_index] = button;
		controls.push_back (button);
		strip->add (*button);

		button = new Button  (unit_ordinal, "mute", *strip);
		buttons[0x10+unit_index] = button;
		controls.push_back (button);
		strip->add (*button);

		button = new Button  (unit_ordinal, "select", *strip);
		buttons[0x18+unit_index] = button;
		controls.push_back (button);
		strip->add (*button);

		button = new Button  (unit_ordinal, "vselect", *strip);
		buttons[0x20+unit_index] = button;
		controls.push_back (button);
		strip->add (*button);

		button = new Button  (unit_ordinal, "fader_touch", *strip);
		buttons[0x68+unit_index] = button;
		controls.push_back (button);
		strip->add (*button);
	}
}	
