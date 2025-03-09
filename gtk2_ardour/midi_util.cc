/*
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2017 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2017 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include <string>

#include <sigc++/bind.h>

#include <ytkmm/menu.h>

#include "ardour/event_type_map.h"
#include "ardour/instrument_info.h"
#include "ardour/midi_patch_manager.h"

#include "midi++/events.h"

#include "midi_util.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace std;


void
build_controller_menu (Gtk::Menu& menu, InstrumentInfo const & instrument_info, uint16_t channel_mask,
                       std::function<void (Menu_Helpers::MenuList&, int, const std::string&)> add_single,
                       std::function<void (Menu_Helpers::MenuList&, uint16_t, int, const std::string&)> add_multi)
{
	using namespace Menu_Helpers;

	/* create several "top level" menu items for sets of controllers (16 at a
	 * time), and populate each one with a submenu for each controller+channel
	 * combination covering the currently selected channels for this track
	 */

	MenuList& items (menu.items());

	size_t total_ctrls = instrument_info.master_controller_count ();
	if (total_ctrls > 0) {
		/* Controllers names available in midnam file, generate fancy menu */
		using namespace MIDI::Name;

		unsigned n_items  = 0;
		unsigned n_groups = 0;

		/* keep track of CC numbers that are added */
		uint16_t ctl_start = 1;
		uint16_t ctl_end   = 1;

		MasterDeviceNames::ControlNameLists const& ctllist (instrument_info.master_device_names ()->controls ());

		bool per_name_list = ctllist.size () > 1;
		bool to_top_level = total_ctrls < 32 && !per_name_list;

		/* reverse lookup which "ChannelNameSet" has "UsesControlNameList <this list>"
		 * then check for which channels it is valid "AvailableForChannels"
		 */

		for (MasterDeviceNames::ControlNameLists::const_iterator l = ctllist.begin(); l != ctllist.end(); ++l) {

			uint16_t channels  = instrument_info.channels_for_control_list (l->first);
			bool multi_channel = (0 != (channels & (channels - 1)));

			std::shared_ptr<ControlNameList> name_list = l->second;
			Menu* ctl_menu = nullptr;

			for (ControlNameList::Controls::const_iterator c = name_list->controls().begin();
			     c != name_list->controls().end();) {

				const uint16_t ctl = c->second->number();

				/* Skip bank select controllers since they're handled specially */
				if (ctl != MIDI_CTL_MSB_BANK && ctl != MIDI_CTL_LSB_BANK) {

					if (to_top_level) {
						ctl_menu = &menu;
					} else if (!ctl_menu) {
						/* Create a new submenu */
						ctl_menu = manage (new Menu);
						ctl_start = ctl;
					}

					MenuList& ctl_items (ctl_menu->items());
					if (multi_channel) {
						add_multi (ctl_items, channels, ctl, c->second->name());
					} else {
						add_single (ctl_items, ctl, c->second->name());
					}
					ctl_end = ctl;
				}

				++c;

				if (!ctl_menu || to_top_level) {
					continue;
				}

				if (++n_items == 32 || ctl < ctl_start || c == name_list->controls().end()) {
					/* Submenu has 32 items or we're done, or a new name-list started:
					 * add it to controller menu and reset */
					items.push_back (MenuElem (string_compose ("%1 %2-%3",
									(per_name_list ? l->first.c_str() : _("Controllers")),
									ctl_start, ctl_end), *ctl_menu));
					ctl_menu = nullptr;
					n_items  = 0;
					++n_groups;
				}
			}
		}
	} else {
		/* No controllers names, generate generic numeric menu */

		bool multi_channel = (0 != (channel_mask & (channel_mask - 1)));

		/* count the number of selected channels because we will build a different menu
		 * structure if there is more than 1 selected.
		 */

		for (int i = 0; i < 127; i += 32) {
			Menu*     ctl_menu = manage (new Menu);
			MenuList& ctl_items (ctl_menu->items());

			for (int ctl = i; ctl < i + 32; ++ctl) {
				if (ctl == MIDI_CTL_MSB_BANK || ctl == MIDI_CTL_LSB_BANK) {
					/* Skip bank select controllers since they're handled specially */
					continue;
				}

				if (multi_channel) {
					add_multi (ctl_items, channel_mask, ctl, string_compose(_("Controller %1"), ctl));
				} else {
					add_single (ctl_items, ctl, string_compose(_("Controller %1"), ctl));
				}
			}

			/* Add submenu for this block of controllers to controller menu */
			switch (i) {
				case 0:
				case 32:
					/* skip 0x00 and 0x20 (bank-select) */
					items.push_back (MenuElem (string_compose (_("Controllers %1-%2"), i + 1, i + 31), *ctl_menu));
					break;
				default:
					items.push_back (MenuElem (string_compose (_("Controllers %1-%2"), i, i + 31), *ctl_menu));
					break;
			}
		}
	}
}
