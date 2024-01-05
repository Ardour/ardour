/*
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2016-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#ifndef __PATCH_CHANGE_H__
#define __PATCH_CHANGE_H__

#include "canvas/flag.h"

class MidiRegionView;

namespace MIDI {
	namespace Name {
		struct PatchPrimaryKey;
	}
}

class PatchChange
{
public:
	PatchChange (MidiView&                         region,
	             ArdourCanvas::Item*               parent,
	             double                            height,
	             double                            x,
	             double                            y,
	             ARDOUR::InstrumentInfo&           info,
	             ARDOUR::MidiModel::PatchChangePtr patch,
	             Gtkmm2ext::Color                  outline_color,
	             Gtkmm2ext::Color                  fill_color);

	~PatchChange ();

	void initialize_popup_menus ();

	void on_patch_menu_selected (const MIDI::Name::PatchPrimaryKey& key);

	void move (ArdourCanvas::Duple);
	void set_height (ArdourCanvas::Distance);
	void hide ();
	void show ();

	void update_name ();

	double                            width ()       const { return _flag->width (); }
	ARDOUR::MidiModel::PatchChangePtr patch ()       const { return _patch; }
	ArdourCanvas::Item*               canvas_item () const { return _flag; }
	ArdourCanvas::Item&               item ()        const { return *_flag; }

private:
	bool event_handler (GdkEvent*);

	MidiView&                         _region;
	ARDOUR::InstrumentInfo&           _info;
	ARDOUR::MidiModel::PatchChangePtr _patch;
	Gtk::Menu                         _popup;
	bool                              _popup_initialized;
	ArdourCanvas::Flag*               _flag;
};

#endif /* __PATCH_CHANGE_H__ */
