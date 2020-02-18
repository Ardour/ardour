/*
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Nick Mainsbridge <mainsbridge@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __SYSEX_H__
#define __SYSEX_H__

#include "midi_region_view.h"

namespace ArdourCanvas {
	class Flag;
}

class SysEx
{
public:
	SysEx (
		MidiRegionView&             region,
		ArdourCanvas::Container*    parent,
		std::string&                text,
		double                      height,
		double                      x,
		double                      y,
		ARDOUR::MidiModel::SysExPtr sysex);

	~SysEx ();

	void hide ();
	void show ();

	void set_height (ArdourCanvas::Distance h) { _flag->set_height (h); }

	ArdourCanvas::Item& item() const { return *_flag; }

private:
	bool event_handler (GdkEvent* ev);
	SysEx(const SysEx& rhs){}
	ArdourCanvas::Flag* _flag;
	ARDOUR::MidiModel::SysExPtr _sysex;
};

#endif /* __SYSEX_H__ */
