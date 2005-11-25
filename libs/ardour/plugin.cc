/*
    Copyright (C) 2000-2002 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <vector>
#include <string>

#include <cstdlib>
#include <cstdio> // so libraptor doesn't complain
#include <cmath>
#include <dirent.h>
#include <sys/stat.h>
#include <cerrno>

#include <lrdf.h>

#include <pbd/compose.h>
#include <pbd/error.h>
#include <pbd/pathscanner.h>
#include <pbd/xml++.h>

#include <midi++/manager.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audioengine.h>
#include <ardour/plugin.h>

#include <pbd/stl_delete.h>

#include "i18n.h"
#include <locale.h>

using namespace ARDOUR;

Plugin::Plugin (AudioEngine& e, Session& s)
	: _engine (e), _session (s)
{
}

Plugin::Plugin (const Plugin& other)
	: _engine (other._engine), _session (other._session), _info (other._info)
{
}

void
Plugin::setup_midi_controls ()
{
	uint32_t port_cnt;

	port_cnt = parameter_count();

	/* set up a vector of null pointers for the MIDI controls.
	   we'll fill this in on an as-needed basis.
	*/

	for (uint32_t i = 0; i < port_cnt; ++i) {
		midi_controls.push_back (0);
	}
}

Plugin::~Plugin ()
{
	for (vector<MIDIPortControl*>::iterator i = midi_controls.begin(); i != midi_controls.end(); ++i) {
		if (*i) {
			delete *i;
		}
	}
}

MIDI::Controllable *
Plugin::get_nth_midi_control (uint32_t n)
{
	if (n >= parameter_count()) {
		return 0;
	}

	if (midi_controls[n] == 0) {

		Plugin::ParameterDescriptor desc;

		get_parameter_descriptor (n, desc);

		midi_controls[n] = new MIDIPortControl (*this, n, _session.midi_port(), desc.lower, desc.upper, desc.toggled, desc.logarithmic);
	} 

	return midi_controls[n];
}

Plugin::MIDIPortControl::MIDIPortControl (Plugin& p, uint32_t port_id, MIDI::Port *port,
					  float low, float up, bool t, bool loga)
	: MIDI::Controllable (port, 0), plugin (p), absolute_port (port_id)
{
	toggled = t;
	logarithmic = loga;
	lower = low;
	upper = up;
	range = upper - lower;
	last_written = 0; /* XXX need a good out-of-bound-value */
	setting = false;
}

void
Plugin::MIDIPortControl::set_value (float value)
{
	if (toggled) {
		if (value > 0.5) {
			value = 1.0;
		} else {
			value = 0.0;
		}
	} else {
		value = lower + (range * value);
		
		if (logarithmic) {
			value = exp(value);
		}
	}

	setting = true;
	plugin.set_parameter (absolute_port, value);
	setting = false;
}

void
Plugin::MIDIPortControl::send_feedback (float value)
{

	if (!setting && get_midi_feedback()) {
		MIDI::byte val;
		MIDI::channel_t ch = 0;
		MIDI::eventType ev = MIDI::none;
		MIDI::byte additional = 0;
		MIDI::EventTwoBytes data;

		if (toggled) {
			val = (MIDI::byte) (value * 127.0f);
		} else {
			if (logarithmic) {
				value = log(value);
			}

			val = (MIDI::byte) (((value - lower) / range) * 127.0f);
		}
		
		if (get_control_info (ch, ev, additional)) {
			data.controller_number = additional;
			data.value = val;

			plugin.session().send_midi_message (get_port(), ev, ch, data);
		}
	}
	
}

MIDI::byte*
Plugin::MIDIPortControl::write_feedback (MIDI::byte* buf, int32_t& bufsize, float value, bool force)
{
	if (get_midi_feedback() && bufsize > 2) {
		MIDI::channel_t ch = 0;
		MIDI::eventType ev = MIDI::none;
		MIDI::byte additional = 0;

		if (get_control_info (ch, ev, additional)) {

			MIDI::byte val;

			if (toggled) {

				val = (MIDI::byte) (value * 127.0f);

			} else {

				if (logarithmic) {
					value = log(value);
				}
				
				val = (MIDI::byte) (((value - lower) / range) * 127.0f);
			}

			if (val != last_written || force)  {
				*buf++ = MIDI::controller & ch;
				*buf++ = additional; /* controller number */
				*buf++ = val;
				last_written = val;
				bufsize -= 3;
			}
		}
	}

	return buf;
}


void
Plugin::reset_midi_control (MIDI::Port* port, bool on)
{
	MIDI::channel_t chn;
	MIDI::eventType ev;
	MIDI::byte extra;
	
	for (vector<MIDIPortControl*>::iterator i = midi_controls.begin(); i != midi_controls.end(); ++i) {
		if (*i == 0)
			continue;
		(*i)->get_control_info (chn, ev, extra);
		if (!on) {
			chn = -1;
		}
		(*i)->midi_rebind (port, chn);
	}
}

void
Plugin::send_all_midi_feedback ()
{
	if (_session.get_midi_feedback()) {
		float val = 0.0;
		uint32_t n = 0;
		
		for (vector<MIDIPortControl*>::iterator i = midi_controls.begin(); i != midi_controls.end(); ++i, ++n) {
			if (*i == 0) {
				continue;
			}

			val = (*i)->plugin.get_parameter (n);
			(*i)->send_feedback (val);
		}
		
	}
}

MIDI::byte*
Plugin::write_midi_feedback (MIDI::byte* buf, int32_t& bufsize)
{
	if (_session.get_midi_feedback()) {
		float val = 0.0;
		uint32_t n = 0;
		
		for (vector<MIDIPortControl*>::iterator i = midi_controls.begin(); i != midi_controls.end(); ++i, ++n) {
			if (*i == 0) {
				continue;
			}

			val = (*i)->plugin.get_parameter (n);
			buf = (*i)->write_feedback (buf, bufsize, val);
		}
	}

	return buf;
}

vector<string>
Plugin::get_presets()
{
	vector<string> labels;
	lrdf_uris* set_uris = lrdf_get_setting_uris(unique_id());

	if (set_uris) {
		for (uint32_t i = 0; i < set_uris->count; ++i) {
			if (char* label = lrdf_get_label(set_uris->items[i])) {
				labels.push_back(label);
				presets[label] = set_uris->items[i];
			}
		}
		lrdf_free_uris(set_uris);
	}

	// GTK2FIX find an equivalent way to do this with a vector (needed by GUI apis)
	// labels.unique();

	return labels;
}

bool
Plugin::load_preset(const string preset_label)
{
	lrdf_defaults* defs = lrdf_get_setting_values(presets[preset_label].c_str());

	if (defs) {
		for (uint32_t i = 0; i < defs->count; ++i) {
			// The defs->items[i].pid < defs->count check is to work around 
			// a bug in liblrdf that saves invalid values into the presets file.
			if (((uint32_t) defs->items[i].pid < defs->count) && parameter_is_input (defs->items[i].pid)) {
				set_parameter(defs->items[i].pid, defs->items[i].value);
			}
		}
		lrdf_free_setting_values(defs);
	}

	return true;
}

bool
Plugin::save_preset (string name, string domain)
{
	lrdf_portvalue portvalues[parameter_count()];
	lrdf_defaults defaults;
	defaults.count = parameter_count();
	defaults.items = portvalues;

	for (uint32_t i = 0; i < parameter_count(); ++i) {
		if (parameter_is_input (i)) {
			portvalues[i].pid = i;
			portvalues[i].value = get_parameter(i);
		}
	}

	char* envvar;
	if ((envvar = getenv ("HOME")) == 0) {
		warning << _("Could not locate HOME.  Preset not saved.") << endmsg;
		return false;
	}
	
	string source(string_compose("file:%1/.%2/rdf/ardour-presets.n3", envvar, domain));

	free(lrdf_add_preset(source.c_str(), name.c_str(), unique_id(), &defaults));

	string path = string_compose("%1/.%2", envvar, domain);
	if (mkdir(path.c_str(), 0775) && errno != EEXIST) {
		warning << string_compose(_("Could not create %1.  Preset not saved. (%2)"), path, strerror(errno)) << endmsg;
		return false;
	}
	
	path += "/rdf";
	if (mkdir(path.c_str(), 0775) && errno != EEXIST) {
		warning << string_compose(_("Could not create %1.  Preset not saved. (%2)"), path, strerror(errno)) << endmsg;
		return false;
	}
	
	if (lrdf_export_by_source(source.c_str(), source.substr(5).c_str())) {
		warning << string_compose(_("Error saving presets file %1."), source) << endmsg;
		return false;
	}

	return true;
}
