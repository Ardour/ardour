/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

#include "ardour/automation_list.h"
#include "ardour/location.h"
#include "ardour/midi_automation_list_binder.h"
#include "ardour/playlist.h"
#include "ardour/region.h"
#include "ardour/region_factory.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/source.h"
#include "ardour/tempo.h"
#include "evoral/Curve.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/id.h"
#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/statefuldestructible.h"
#include "pbd/types_convert.h"

class Command;

using namespace PBD;
using namespace ARDOUR;
using namespace Temporal;

#include "pbd/i18n.h"

void Session::register_with_memento_command_factory(PBD::ID id, PBD::StatefulDestructible *ptr)
{
    registry[id] = ptr;
}

Command *
Session::memento_command_factory(XMLNode *n)
{
    PBD::ID id;
    XMLNode *before = 0, *after = 0;
    XMLNode *child = 0;

    /* XXX: HACK! */
    bool have_id = n->get_property ("obj-id", id);

    /* get before/after */

    if (n->name() == "MementoCommand") {
	    before = new XMLNode(*n->children().front());
	    after = new XMLNode(*n->children().back());
	    child = before;
    } else if (n->name() == "MementoUndoCommand") {
	    before = new XMLNode(*n->children().front());
	    child = before;
    } else if (n->name() == "MementoRedoCommand") {
	    after = new XMLNode(*n->children().front());
	    child = after;
    } else if (n->name() == "PlaylistCommand") {
	    before = new XMLNode(*n->children().front());
	    after = new XMLNode(*n->children().back());
	    child = before;
    }

    if (!child) {
	    info << string_compose (_("Tried to reconstitute a MementoCommand with no contents, failing. id=%1"), id.to_s()) << endmsg;
	    return 0;
    }

    /* create command */
    std::string type_name;
    n->get_property ("type-name", type_name);

    if (type_name == "ARDOUR::AudioRegion" || type_name == "ARDOUR::MidiRegion" || type_name == "ARDOUR::Region") {
	    boost::shared_ptr<Region> r = RegionFactory::region_by_id (id);
	    if (r) {
		    return new MementoCommand<Region>(*r, before, after);
	    }

    } else if (type_name == "ARDOUR::AudioSource" || type_name == "ARDOUR::MidiSource") {
	    if (sources.count(id))
		    return new MementoCommand<Source>(*sources[id], before, after);

    } else if (type_name == "ARDOUR::Location") {
	    Location* loc = _locations->get_location_by_id(id);
	    if (loc) {
		    return new MementoCommand<Location>(*loc, before, after);
	    }

    } else if (type_name == "ARDOUR::Locations") {
	    return new MementoCommand<Locations>(*_locations, before, after);

    } else if (type_name == "Temporal::TempoMap") {
	    return new MementoCommand<TempoMap>(*TempoMap::use(), before, after);

    } else if (type_name == "ARDOUR::Playlist" || type_name == "ARDOUR::AudioPlaylist" || type_name == "ARDOUR::MidiPlaylist") {
	    if (boost::shared_ptr<Playlist> pl = _playlists->by_name(child->property("name")->value())) {
		    return new MementoCommand<Playlist>(*(pl.get()), before, after);
	    }

    } else if (type_name == "ARDOUR::Route" || type_name == "ARDOUR::AudioTrack" || type_name == "ARDOUR::MidiTrack") {
		if (boost::shared_ptr<Route> r = route_by_id(id)) {
			return new MementoCommand<Route>(*r, before, after);
		} else {
			error << string_compose (X_("Route %1 not found in session"), id) << endmsg;
		}

    } else if (type_name == "Evoral::Curve" || type_name == "ARDOUR::AutomationList") {
	    if (have_id) {
		    std::map<PBD::ID, AutomationList*>::iterator i = automation_lists.find(id);
		    if (i != automation_lists.end()) {
			    return new MementoCommand<AutomationList>(*i->second, before, after);
		    }
	    } else {
		    return new MementoCommand<AutomationList> (
			    new MidiAutomationListBinder (n, sources),
			    before, after
			    );
	    }

	    std::cerr << "Alist " << id << " not found\n";

    } else if (registry.count(id)) { // For Editor and AutomationLine which are off-limits herea
	    return new MementoCommand<PBD::StatefulDestructible>(*registry[id], before, after);
    }

    /* we failed */
    info << string_compose (_("Could not reconstitute MementoCommand from XMLNode. object type = %1 id = %2"), type_name, id.to_s()) << endmsg;

    return 0 ;
}

Command *
Session::stateful_diff_command_factory (XMLNode* n)
{
	PBD::ID id;
	std::string type_name;
	if (!n->get_property ("obj-id", id) || !n->get_property ("type-name", type_name)) {
		error << _("Could get object ID and type name for StatefulDiffCommand from XMLNode.")
		      << endmsg;
		      return 0;
	}

	if ((type_name == "ARDOUR::AudioRegion" || type_name == "ARDOUR::MidiRegion")) {
		boost::shared_ptr<Region> r = RegionFactory::region_by_id (id);
		if (r) {
			return new StatefulDiffCommand (r, *n);
		}

	} else if (type_name == "ARDOUR::AudioPlaylist" ||  type_name == "ARDOUR::MidiPlaylist") {
		boost::shared_ptr<Playlist> p = _playlists->by_id (id);
		if (p) {
			return new StatefulDiffCommand (p, *n);
		} else {
			std::cerr << "Playlist with ID = " << id << " not found\n";
		}
	}

	/* we failed */

	info << string_compose (
		_("Could not reconstitute StatefulDiffCommand from XMLNode. object type = %1 id = %2"), type_name, id.to_s())
	      << endmsg;

	return 0;
}
