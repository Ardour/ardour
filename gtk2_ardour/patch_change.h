/*
    Copyright (C) 2000-2010 Paul Davis

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
	PatchChange(
		MidiRegionView& region,
		ArdourCanvas::Group* parent,
		const string&   text,
		double          height,
		double          x,
		double          y,
		ARDOUR::InstrumentInfo& info,
		ARDOUR::MidiModel::PatchChangePtr patch
		);

	~PatchChange();

	ARDOUR::MidiModel::PatchChangePtr patch () const { return _patch; }

	void initialize_popup_menus();

	void on_patch_menu_selected(const MIDI::Name::PatchPrimaryKey& key);

	ArdourCanvas::Item* canvas_item () const {
		return _flag;
	}

	void move (ArdourCanvas::Duple);
	void set_height (ArdourCanvas::Distance);
	void hide ();
	void show ();

        ArdourCanvas::Item& item() const { return *_flag; }

private:
	bool event_handler (GdkEvent *);

	MidiRegionView& _region;
        ARDOUR::InstrumentInfo& _info;
	ARDOUR::MidiModel::PatchChangePtr _patch;
	Gtk::Menu     _popup;
	bool          _popup_initialized;
	ArdourCanvas::Flag* _flag;
};

#endif /* __PATCH_CHANGE_H__ */
