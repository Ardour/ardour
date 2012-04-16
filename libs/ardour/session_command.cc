/*
    Copyright (C) 2000-2007 Paul Davis

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

#include "ardour/session.h"
#include "ardour/route.h"
#include "pbd/memento_command.h"
#include "ardour/diskstream.h"
#include "ardour/playlist.h"
#include "ardour/audioplaylist.h"
#include "ardour/audio_track.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_track.h"
#include "ardour/tempo.h"
#include "ardour/audiosource.h"
#include "ardour/audioregion.h"
#include "ardour/midi_source.h"
#include "ardour/midi_region.h"
#include "ardour/session_playlists.h"
#include "ardour/region_factory.h"
#include "ardour/midi_automation_list_binder.h"
#include "pbd/error.h"
#include "pbd/id.h"
#include "pbd/statefuldestructible.h"
#include "pbd/failed_constructor.h"
#include "pbd/stateful_diff_command.h"
#include "evoral/Curve.hpp"

using namespace PBD;
using namespace ARDOUR;

#include "i18n.h"

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

    /* get id */

    /* XXX: HACK! */
    bool have_id = n->property("obj-id") != 0;
    if (have_id) {
	    id = PBD::ID(n->property("obj-id")->value());
    }

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
	error << _("Tried to reconstitute a MementoCommand with no contents, failing. id=") << id.to_s() << endmsg;
	return 0;
    }

    /* create command */
    string obj_T = n->property ("type-name")->value();

    if (obj_T == "ARDOUR::AudioRegion" || obj_T == "ARDOUR::MidiRegion" || obj_T == "ARDOUR::Region") {
	    boost::shared_ptr<Region> r = RegionFactory::region_by_id (id);
	    if (r) {
		    return new MementoCommand<Region>(*r, before, after);
	    }

    } else if (obj_T == "ARDOUR::AudioSource" || obj_T == "ARDOUR::MidiSource") {
	    if (sources.count(id))
		    return new MementoCommand<Source>(*sources[id], before, after);

    } else if (obj_T == "ARDOUR::Location") {
	    Location* loc = _locations->get_location_by_id(id);
	    if (loc) {
		    return new MementoCommand<Location>(*loc, before, after);
	    }

    } else if (obj_T == "ARDOUR::Locations") {
	    return new MementoCommand<Locations>(*_locations, before, after);

    } else if (obj_T == "ARDOUR::TempoMap") {
	    return new MementoCommand<TempoMap>(*_tempo_map, before, after);

    } else if (obj_T == "ARDOUR::Playlist" || obj_T == "ARDOUR::AudioPlaylist" || obj_T == "ARDOUR::MidiPlaylist") {
	    if (boost::shared_ptr<Playlist> pl = playlists->by_name(child->property("name")->value())) {
		    return new MementoCommand<Playlist>(*(pl.get()), before, after);
	    }

    } else if (obj_T == "ARDOUR::Route" || obj_T == "ARDOUR::AudioTrack" || obj_T == "ARDOUR::MidiTrack") {
		if (boost::shared_ptr<Route> r = route_by_id(id)) {
			return new MementoCommand<Route>(*r, before, after);
		} else {
			error << string_compose (X_("Route %1 not found in session"), id) << endmsg;
		}

    } else if (obj_T == "Evoral::Curve" || obj_T == "ARDOUR::AutomationList") {
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

	    cerr << "Alist " << id << " not found\n";

    } else if (registry.count(id)) { // For Editor and AutomationLine which are off-limits herea
	    return new MementoCommand<PBD::StatefulDestructible>(*registry[id], before, after);
    }

    /* we failed */
    error << string_compose (_("could not reconstitute MementoCommand from XMLNode. object type = %1 id = %2"), obj_T, id.to_s()) << endmsg;

    return 0 ;
}

Command *
Session::stateful_diff_command_factory (XMLNode* n)
{
	PBD::ID const id (n->property("obj-id")->value ());

	string const obj_T = n->property ("type-name")->value ();
	if ((obj_T == "ARDOUR::AudioRegion" || obj_T == "ARDOUR::MidiRegion")) {
		boost::shared_ptr<Region> r = RegionFactory::region_by_id (id);
		if (r) {
			return new StatefulDiffCommand (r, *n);
		}

	} else if (obj_T == "ARDOUR::AudioPlaylist" ||  obj_T == "ARDOUR::MidiPlaylist") {
                boost::shared_ptr<Playlist> p = playlists->by_id (id);
                if (p) {
                        return new StatefulDiffCommand (p, *n);
                } else {
                        cerr << "Playlist with ID = " << id << " not found\n";
                }
        }

	/* we failed */

	error << string_compose (
		_("could not reconstitute StatefulDiffCommand from XMLNode. object type = %1 id = %2"), obj_T, id.to_s())
	      << endmsg;

	return 0;
}
