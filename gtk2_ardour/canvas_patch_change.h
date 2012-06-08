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

#ifndef CANVAS_PATCH_CHANGE_H_
#define CANVAS_PATCH_CHANGE_H_

#include "canvas-flag.h"

class MidiRegionView;

namespace MIDI {
	namespace Name {
		struct PatchPrimaryKey;
	}
}

namespace Gnome {
namespace Canvas {

class CanvasPatchChange : public CanvasFlag
{
public:
	CanvasPatchChange(
		MidiRegionView& region,
		Group&          parent,
		const string&   text,
		double          height,
		double          x,
		double          y,
		string&         model_name,
		string&         custom_device_mode,
		ARDOUR::MidiModel::PatchChangePtr patch,
		bool
		);

	virtual ~CanvasPatchChange();

	virtual bool on_event(GdkEvent* ev);

	string model_name () const { return _model_name; }
	string custom_device_mode () const { return _custom_device_mode; }
	ARDOUR::MidiModel::PatchChangePtr patch () const { return _patch; }

	void initialize_popup_menus();

	void on_patch_menu_selected(const MIDI::Name::PatchPrimaryKey& key);

private:
	string        _model_name;
	string        _custom_device_mode;
	ARDOUR::MidiModel::PatchChangePtr _patch;
	Gtk::Menu     _popup;
	bool          _popup_initialized;
};

} // namespace Canvas
} // namespace Gnome

#endif /*CANVASPROGRAMCHANGE_H_*/
