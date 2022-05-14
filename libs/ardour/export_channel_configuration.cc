/*
 * Copyright (C) 2008-2009 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2012 Sakari Bergen <sakari.bergen@beatwaves.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#include "ardour/export_channel_configuration.h"

#include "pbd/convert.h"
#include "pbd/enumwriter.h"
#include "pbd/pthread_utils.h"

using namespace PBD;

namespace ARDOUR
{
/* ExportChannelConfiguration */

ExportChannelConfiguration::ExportChannelConfiguration (Session& session)
	: session (session)
	, split (false)
	, region_type (RegionExportChannelFactory::None)
{
}

XMLNode&
ExportChannelConfiguration::get_state () const
{
	XMLNode* root = new XMLNode ("ExportChannelConfiguration");
	XMLNode* channel;

	root->set_property ("split", get_split ());
	root->set_property ("channels", get_n_chans ());

	switch (region_type) {
		case RegionExportChannelFactory::None:
			// Do nothing
			break;
		default:
			root->set_property ("region-processing", enum_2_string (region_type));
			break;
	}

	uint32_t i = 1;
	for (auto const& c : channels) {
		channel = root->add_child ("ExportChannel");
		channel->set_property ("type", c->state_node_name ());
		channel->set_property ("number", i);
		c->get_state (channel);
		++i;
	}

	return *root;
}

int
ExportChannelConfiguration::set_state (const XMLNode& root)
{
	bool yn;
	if (root.get_property ("split", yn)) {
		set_split (yn);
	}

	std::string str;
	if (root.get_property ("region-processing", str)) {
		set_region_processing_type ((RegionExportChannelFactory::Type) string_2_enum (str, RegionExportChannelFactory::Type));
	}

	/* load old state, if any */
	XMLNodeList channels = root.children ("Channel");
	for (auto const& n : channels) {
		ExportChannelPtr channel (new PortExportChannel ());
		channel->set_state (n, session);
		register_channel (channel);
	}

	XMLNodeList export_channels = root.children ("ExportChannel");
	for (auto const& n : export_channels) {
		std::string type;
		if (!n->get_property ("type", type)) {
			assert (0);
			continue;
		}
		ExportChannelPtr channel;
		if (type == "PortExportChannel") {
			channel = ExportChannelPtr (new PortExportChannel ());
		} else if (type == "PortExportMIDI") {
			channel = ExportChannelPtr (new PortExportMIDI ());
		} else if (type == "RouteExportChannel") {
			std::list<ExportChannelPtr> list;
			RouteExportChannel::create_from_state (list, session, n);
			if (list.size () > 0) {
				register_channels (list);
			}
			continue;
		} else if (type == "RegionExportChannel") {
			/* no state */
			continue;
		} else {
			assert (0);
			continue;
		}

		channel->set_state (n, session);
		register_channel (channel);
	}

	return 0;
}

bool
ExportChannelConfiguration::all_channels_have_ports () const
{
	for (auto const& c : channels) {
		if (c->empty ()) {
			return false;
		}
	}

	return true;
}

void
ExportChannelConfiguration::configurations_for_files (std::list<boost::shared_ptr<ExportChannelConfiguration>>& configs)
{
	configs.clear ();

	if (!split) {
		configs.push_back (shared_from_this ());
		return;
	}

	for (auto const& c : channels) {
		boost::shared_ptr<ExportChannelConfiguration> config (new ExportChannelConfiguration (session));
		config->set_name (_name);
		config->register_channel (c);
		configs.push_back (config);
	}
}

} // namespace ARDOUR
