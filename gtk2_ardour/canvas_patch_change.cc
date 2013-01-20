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

#include <boost/algorithm/string.hpp>

#include "pbd/stacktrace.h"

#include "gtkmm2ext/keyboard.h"
#include "ardour/instrument_info.h"
#include "midi++/midnam_patch.h"

#include "ardour_ui.h"
#include "midi_region_view.h"
#include "canvas_patch_change.h"
#include "editor.h"
#include "editor_drag.h"

using namespace Gnome::Canvas;
using namespace MIDI::Name;
using namespace Gtkmm2ext;
using namespace std;

/** @param x x position in pixels.
 */
CanvasPatchChange::CanvasPatchChange(
	MidiRegionView&                   region,
	Group&                            parent,
	const string&                     text,
	double                            height,
	double                            x,
	double                            y,
	ARDOUR::InstrumentInfo&           info,
	ARDOUR::MidiModel::PatchChangePtr patch,
	bool                              active_channel)
	: CanvasFlag(
		region,
		parent,
		height,
		(active_channel
		 ? ARDOUR_UI::config()->canvasvar_MidiPatchChangeOutline.get()
		 : ARDOUR_UI::config()->canvasvar_MidiPatchChangeInactiveChannelOutline.get()),
		(active_channel
		 ? ARDOUR_UI::config()->canvasvar_MidiPatchChangeFill.get()
		 : ARDOUR_UI::config()->canvasvar_MidiPatchChangeInactiveChannelFill.get()),
		x,
		y)
	, _info (info)
	, _patch (patch)
	, _popup_initialized(false)
{
	set_text (text);
}

CanvasPatchChange::~CanvasPatchChange()
{
}

void
CanvasPatchChange::initialize_popup_menus()
{
	boost::shared_ptr<ChannelNameSet> channel_name_set = _info.get_patches (_patch->channel());

	if (!channel_name_set) {
		return;
	}

	const ChannelNameSet::PatchBanks& patch_banks = channel_name_set->patch_banks();

	if (patch_banks.size() > 1) {
		// fill popup menu:
		Gtk::Menu::MenuList& patch_bank_menus = _popup.items();
		
		for (ChannelNameSet::PatchBanks::const_iterator bank = patch_banks.begin();
		     bank != patch_banks.end();
		     ++bank) {
			Gtk::Menu& patch_bank_menu = *manage(new Gtk::Menu());
			
			const PatchNameList& patches = (*bank)->patch_name_list();
			Gtk::Menu::MenuList& patch_menus = patch_bank_menu.items();
			
			for (PatchNameList::const_iterator patch = patches.begin();
			     patch != patches.end();
			     ++patch) {
				std::string name = (*patch)->name();
				boost::replace_all (name, "_", " ");
				
				patch_menus.push_back(
					Gtk::Menu_Helpers::MenuElem(
						name,
						sigc::bind(sigc::mem_fun(*this, &CanvasPatchChange::on_patch_menu_selected),
						           (*patch)->patch_primary_key())) );
			}
			
			std::string name = (*bank)->name();
			boost::replace_all (name, "_", " ");
			
			patch_bank_menus.push_back(
				Gtk::Menu_Helpers::MenuElem(
					name,
					patch_bank_menu) );
		}
	} else {
		/* only one patch bank, so make it the initial menu */

		const PatchNameList& patches = patch_banks.front()->patch_name_list();
		Gtk::Menu::MenuList& patch_menus = _popup.items();
		
		for (PatchNameList::const_iterator patch = patches.begin();
		     patch != patches.end();
		     ++patch) {
			std::string name = (*patch)->name();
			boost::replace_all (name, "_", " ");
			
			patch_menus.push_back (
				Gtk::Menu_Helpers::MenuElem (
					name,
					sigc::bind (sigc::mem_fun(*this, &CanvasPatchChange::on_patch_menu_selected),
					            (*patch)->patch_primary_key())));
		}
	}
}
	
void
CanvasPatchChange::on_patch_menu_selected(const PatchPrimaryKey& key)
{
	_region.change_patch_change (*this, key);
}

static bool
in_edit_mode(Editor* editor)
{
	return (editor->internal_editing() &&
	        (editor->current_mouse_mode() == Editing::MouseObject ||
	         editor->current_mouse_mode() == Editing::MouseDraw));
}

bool
CanvasPatchChange::on_event (GdkEvent* ev)
{
	Editor* e;

	switch (ev->type) {
	case GDK_BUTTON_PRESS:
		/* XXX: icky dcast */
		e = dynamic_cast<Editor*> (&_region.get_time_axis_view().editor());
		if (in_edit_mode(e)) {

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
			if (!_popup.items().empty()) {
				_popup.popup(ev->button.button, ev->button.time);
			}
			return true;
		}
		break;

	case GDK_KEY_PRESS:
		switch (ev->key.keyval) {
		case GDK_Up:
		case GDK_KP_Up:
		case GDK_uparrow:
			if (Keyboard::modifier_state_contains (ev->key.state, Keyboard::PrimaryModifier)) {
				_region.previous_bank (*this);
			} else {
				_region.previous_patch (*this);
			}
			break;
		case GDK_Down:
		case GDK_KP_Down:
		case GDK_downarrow:
			if (Keyboard::modifier_state_contains (ev->key.state, Keyboard::PrimaryModifier)) {
				_region.next_bank (*this);
			} else {
				_region.next_patch (*this);
			}
			break;
		case GDK_Delete:
		case GDK_BackSpace:
			_region.delete_patch_change (this);
			break;
		default:
			break;
		}
		break;

	case GDK_SCROLL:
		/* XXX: icky dcast */
		e = dynamic_cast<Editor*> (&_region.get_time_axis_view().editor());
		if (in_edit_mode(e)) {
			if (ev->scroll.direction == GDK_SCROLL_UP) {
				if (Keyboard::modifier_state_contains (ev->scroll.state, Keyboard::PrimaryModifier)) {
					_region.previous_bank (*this);
				} else {
					_region.previous_patch (*this);
				}
			} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
				if (Keyboard::modifier_state_contains (ev->scroll.state, Keyboard::PrimaryModifier)) {
					_region.next_bank (*this);
				} else {
					_region.next_patch (*this);
				}
			}
			return true;
			break;
		}
		break;

	case GDK_ENTER_NOTIFY:
		_region.patch_entered (this);
		return true;
		break;

	case GDK_LEAVE_NOTIFY:
		_region.patch_left (this);
		return true;
		break;

	default:
		break;
	}

	return false;
}
