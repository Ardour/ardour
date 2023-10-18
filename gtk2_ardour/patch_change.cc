/*
 * Copyright (C) 2013-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <iostream>

#include <boost/algorithm/string.hpp>

#include <glibmm/regex.h>

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"

#include "midi++/midnam_patch.h"

#include "canvas/debug.h"

#include "editor.h"
#include "editor_drag.h"
#include "midi_region_view.h"
#include "patch_change.h"
#include "ui_config.h"

using namespace MIDI::Name;
using namespace std;
using Gtkmm2ext::Keyboard;

/** @param x x position in pixels.
 */
PatchChange::PatchChange (MidiRegionView&                   region,
                          ArdourCanvas::Container*          parent,
                          double                            height,
                          double                            x,
                          double                            y,
                          ARDOUR::InstrumentInfo&           info,
                          ARDOUR::MidiModel::PatchChangePtr patch,
                          Gtkmm2ext::Color                  outline_color,
                          Gtkmm2ext::Color                  fill_color)
	: _region (region)
	, _info (info)
	, _patch (patch)
	, _popup_initialized (false)
{
	_flag = new ArdourCanvas::Flag (parent,
	                                height,
	                                outline_color,
	                                fill_color,
	                                ArdourCanvas::Duple (x, y),
	                                true);

	CANVAS_DEBUG_NAME (_flag, _info.get_patch_name (_patch->bank (), _patch->program (), _patch->channel ()));

	_flag->Event.connect (sigc::mem_fun (*this, &PatchChange::event_handler));
	_flag->set_font_description (UIConfiguration::instance ().get_SmallFont ());

	update_name ();
}

PatchChange::~PatchChange ()
{
	delete _flag;
}

void
PatchChange::update_name ()
{
	_flag->set_text (_info.get_patch_name (_patch->bank (), _patch->program (), _patch->channel ()));
}

void
PatchChange::initialize_popup_menus ()
{
	using namespace MIDI::Name;

	std::shared_ptr<ChannelNameSet> channel_name_set = _info.get_patches (_patch->channel ());

	if (!channel_name_set || channel_name_set->patch_banks ().size () == 0) {
		return;
	}

	const ChannelNameSet::PatchBanks& patch_banks = channel_name_set->patch_banks ();

	if (patch_banks.size () > 1) {
		// fill popup menu:
		Gtk::Menu::MenuList& patch_bank_menus = _popup.items ();

		for (ChannelNameSet::PatchBanks::const_iterator bank = patch_banks.begin (); bank != patch_banks.end (); ++bank) {
			Glib::RefPtr<Glib::Regex> underscores = Glib::Regex::create ("_");
			std::string const&        replacement (" ");

			Gtk::Menu& patch_bank_menu = *manage (new Gtk::Menu ());

			const PatchNameList& patches     = (*bank)->patch_name_list ();
			Gtk::Menu::MenuList& patch_menus = patch_bank_menu.items ();

			for (PatchNameList::const_iterator patch = patches.begin (); patch != patches.end (); ++patch) {

				std::string name = underscores->replace ((*patch)->name ().c_str (), -1, 0, replacement);

				patch_menus.push_back (
				    Gtk::Menu_Helpers::MenuElem (
				        name,
				        sigc::bind (sigc::mem_fun (*this, &PatchChange::on_patch_menu_selected), (*patch)->patch_primary_key ())));
			}

			std::string name = underscores->replace ((*bank)->name ().c_str (), -1, 0, replacement);

			patch_bank_menus.push_back (
			    Gtk::Menu_Helpers::MenuElem (
			        name,
			        patch_bank_menu));
		}

	} else {
		/* only one patch bank, so make it the initial menu */

		const PatchNameList& patches     = patch_banks.front ()->patch_name_list ();
		Gtk::Menu::MenuList& patch_menus = _popup.items ();

		for (PatchNameList::const_iterator patch = patches.begin ();
		     patch != patches.end ();
		     ++patch) {
			patch_menus.push_back (Gtkmm2ext::MenuElemNoMnemonic ((*patch)->name (),
			                                                      sigc::bind (sigc::mem_fun (*this, &PatchChange::on_patch_menu_selected), (*patch)->patch_primary_key ())));
		}
	}
}

void
PatchChange::on_patch_menu_selected (const PatchPrimaryKey& key)
{
	_region.change_patch_change (*this, key);
}

bool
PatchChange::event_handler (GdkEvent* ev)
{
	/* XXX: icky dcast */
	Editor* e = dynamic_cast<Editor*> (&_region.get_time_axis_view ().editor ());

	if (!e->internal_editing ()) {
		return false;
	}

	switch (ev->type) {
		case GDK_BUTTON_PRESS:
			if (e->current_mouse_mode () == Editing::MouseContent) {
				if (Gtkmm2ext::Keyboard::is_delete_event (&ev->button)) {
					_region.delete_patch_change (this);
					return true;

				} else if (Gtkmm2ext::Keyboard::is_edit_event (&ev->button)) {
					_region.edit_patch_change (this);
					return true;

				} else if (ev->button.button == 1) {
					e->drags ()->set (new PatchChangeDrag (*e, this, &_region), ev);
					return true;
				}
			}

			if (Gtkmm2ext::Keyboard::is_context_menu_event (&ev->button)) {
				if (!_popup_initialized) {
					initialize_popup_menus ();
					_popup_initialized = true;
				}
				_popup.popup (ev->button.button, ev->button.time);
				return true;
			}
			break;

		case GDK_KEY_PRESS:
			switch (ev->key.keyval) {
				case GDK_Up:
				case GDK_KP_Up:
				case GDK_uparrow:
					_region.step_patch (*this, Keyboard::modifier_state_contains (ev->key.state, Keyboard::TertiaryModifier), 1);
					return true;
				case GDK_Down:
				case GDK_KP_Down:
				case GDK_downarrow:
					_region.step_patch (*this, Keyboard::modifier_state_contains (ev->key.state, Keyboard::TertiaryModifier), -1);
					return true;
				default:
					break;
			}
			break;

		case GDK_KEY_RELEASE:
			switch (ev->key.keyval) {
				case GDK_BackSpace:
				case GDK_Delete:
					_region.delete_patch_change (this);
				default:
					break;
			}
			break;

		case GDK_SCROLL:
			if (ev->scroll.direction == GDK_SCROLL_UP) {
				_region.step_patch (*this, Keyboard::modifier_state_contains (ev->scroll.state, Keyboard::TertiaryModifier), 1);
				return true;
			} else if (ev->scroll.direction == GDK_SCROLL_DOWN) {
				_region.step_patch (*this, Keyboard::modifier_state_contains (ev->scroll.state, Keyboard::TertiaryModifier), -1);
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
