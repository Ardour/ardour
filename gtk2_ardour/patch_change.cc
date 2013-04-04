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

#include <iostream>

#include <glibmm/regex.h>

#include "gtkmm2ext/keyboard.h"
#include "ardour/midi_patch_manager.h"
#include "ardour_ui.h"
#include "midi_region_view.h"
#include "patch_change.h"
#include "editor.h"
#include "editor_drag.h"

using namespace MIDI::Name;
using namespace std;

/** @param x x position in pixels.
 */
PatchChange::PatchChange(
		MidiRegionView& region,
		ArdourCanvas::Group* parent,
		const string&   text,
		double          height,
		double          x,
		double          y,
		string&         model_name,
		string&         custom_device_mode,
		ARDOUR::MidiModel::PatchChangePtr patch)
	: _region (region)
	, _model_name(model_name)
	, _custom_device_mode(custom_device_mode)
	, _patch (patch)
	, _popup_initialized(false)
{
	_flag = new ArdourCanvas::Flag (
		parent,
		height,
		ARDOUR_UI::config()->canvasvar_MidiPatchChangeOutline.get(),
		ARDOUR_UI::config()->canvasvar_MidiPatchChangeFill.get(),
		ArdourCanvas::Duple (x, y)
		);
	
	_flag->Event.connect (sigc::mem_fun (*this, &PatchChange::event_handler));
	_flag->set_text(text);
}

PatchChange::~PatchChange()
{
}

void
PatchChange::initialize_popup_menus()
{
	boost::shared_ptr<ChannelNameSet> channel_name_set =
		MidiPatchManager::instance()
		.find_channel_name_set(_model_name, _custom_device_mode, _patch->channel());

	if (!channel_name_set) {
		return;
	}

	const ChannelNameSet::PatchBanks& patch_banks = channel_name_set->patch_banks();

	// fill popup menu:
	Gtk::Menu::MenuList& patch_bank_menus = _popup.items();

	for (ChannelNameSet::PatchBanks::const_iterator bank = patch_banks.begin();
	     bank != patch_banks.end();
	     ++bank) {
		Glib::RefPtr<Glib::Regex> underscores = Glib::Regex::create("_");
		std::string replacement(" ");

		Gtk::Menu& patch_bank_menu = *manage(new Gtk::Menu());

		const PatchBank::PatchNameList& patches = (*bank)->patch_name_list();
		Gtk::Menu::MenuList& patch_menus = patch_bank_menu.items();

		for (PatchBank::PatchNameList::const_iterator patch = patches.begin();
		     patch != patches.end();
		     ++patch) {
			std::string name = underscores->replace((*patch)->name().c_str(), -1, 0, replacement);

			patch_menus.push_back(
				Gtk::Menu_Helpers::MenuElem(
					name,
					sigc::bind(
						sigc::mem_fun(*this, &PatchChange::on_patch_menu_selected),
						(*patch)->patch_primary_key())) );
		}


		std::string name = underscores->replace((*bank)->name().c_str(), -1, 0, replacement);

		patch_bank_menus.push_back(
			Gtk::Menu_Helpers::MenuElem(
				name,
				patch_bank_menu) );
	}
}

void
PatchChange::on_patch_menu_selected(const PatchPrimaryKey& key)
{
	_region.change_patch_change (*this, key);
}

bool
PatchChange::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
	{
		/* XXX: icky dcast */
		Editor* e = dynamic_cast<Editor*> (&_region.get_time_axis_view().editor());
		if (e->current_mouse_mode() == Editing::MouseObject && e->internal_editing()) {

			if (Gtkmm2ext::Keyboard::is_delete_event (&ev->button)) {

				_region.delete_patch_change (this);
				return true;

			} else if (Gtkmm2ext::Keyboard::is_edit_event (&ev->button)) {

				_region.edit_patch_change (this);
				return true;
				
			} else if (ev->button.button == 1) {
				e->drags()->set (new PatchChangeDrag (e, this, &_region), ev);
				return true;
			}
		}

		if (ev->button.button == 3) {
			if (!_popup_initialized) {
				initialize_popup_menus();
				_popup_initialized = true;
			}
			_popup.popup(ev->button.button, ev->button.time);
			return true;
		}
		break;
	}

	case GDK_KEY_PRESS:
		switch (ev->key.keyval) {
		case GDK_Up:
		case GDK_KP_Up:
		case GDK_uparrow:
			_region.previous_patch (*this);
			break;
		case GDK_Down:
		case GDK_KP_Down:
		case GDK_downarrow:
			_region.next_patch (*this);
			break;
		default:
			break;
		}
		break;

	case GDK_SCROLL:
		if (ev->scroll.direction == GDK_SCROLL_UP) {
			_region.previous_patch (*this);
			return true;
		} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
			_region.next_patch (*this);
			return true;
		}
		break;

	case GDK_ENTER_NOTIFY:
		_region.patch_entered (this);
		break;

	case GDK_LEAVE_NOTIFY:
		_region.patch_left (this);
		break;

	default:
		break;
	}

	return false;
}

void
PatchChange::move (ArdourCanvas::Duple d)
{
	_flag->move (d);
}

void
PatchChange::set_height (ArdourCanvas::Distance height)
{
	_flag->set_height (height);
}

void
PatchChange::hide ()
{
	_flag->hide ();
}

void
PatchChange::show ()
{
	_flag->show ();
}
