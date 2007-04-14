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

#include <ardour/session.h>
#include <ardour/route.h>
#include <pbd/memento_command.h>
#include <ardour/diskstream.h>
#include <ardour/playlist.h>
#include <ardour/audioplaylist.h>
#include <ardour/audio_track.h>
#include <ardour/tempo.h>
#include <ardour/audiosource.h>
#include <ardour/audioregion.h>
#include <ardour/midi_source.h>
#include <ardour/midi_region.h>
#include <pbd/error.h>
#include <pbd/id.h>
#include <pbd/statefuldestructible.h>
#include <pbd/failed_constructor.h>

using namespace PBD;
using namespace ARDOUR;

#include "i18n.h"

void Session::register_with_memento_command_factory(PBD::ID id, PBD::StatefulThingWithGoingAway *ptr)
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
    id = PBD::ID(n->property("obj_id")->value());

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
		    
    if (!child)
    {
	error << _("Tried to reconstitute a MementoCommand with no contents, failing. id=") << id.to_s() << endmsg;
	return 0;
    }

    /* create command */
    string obj_T = n->property ("type_name")->value();
    if (obj_T == typeid (AudioRegion).name() || obj_T == typeid (MidiRegion).name() || obj_T == typeid (Region).name()) {
	    if (regions.count(id))
		    return new MementoCommand<Region>(*regions[id], before, after);
    } else if (obj_T == typeid (AudioSource).name() || obj_T == typeid (MidiSource).name()) {
	    if (sources.count(id))
		    return new MementoCommand<Source>(*sources[id], before, after);
    } else if (obj_T == typeid (Location).name()) {
	    return new MementoCommand<Location>(*_locations.get_location_by_id(id), before, after);
    } else if (obj_T == typeid (Locations).name()) {
	    return new MementoCommand<Locations>(_locations, before, after);
    } else if (obj_T == typeid (TempoMap).name()) {
	    return new MementoCommand<TempoMap>(*_tempo_map, before, after);
    } else if (obj_T == typeid (Playlist).name() || obj_T == typeid (AudioPlaylist).name()) {
	    if (boost::shared_ptr<Playlist> pl = playlist_by_name(child->property("name")->value())) {
		    return new MementoCommand<Playlist>(*(pl.get()), before, after);
	    }
    } else if (obj_T == typeid (Route).name() || obj_T == typeid (AudioTrack).name()) { 
	    return new MementoCommand<Route>(*route_by_id(id), before, after);
    } else if (obj_T == typeid (Curve).name() || obj_T == typeid (AutomationList).name()) {
	    if (automation_lists.count(id))
		    return new MementoCommand<AutomationList>(*automation_lists[id], before, after);
    } else if (registry.count(id)) { // For Editor and AutomationLine which are off-limits here
	    return new MementoCommand<PBD::StatefulThingWithGoingAway>(*registry[id], before, after);
    }

    /* we failed */
    error << string_compose (_("could not reconstitute MementoCommand from XMLNode. object type = %1 id = %2"), obj_T, id.to_s()) << endmsg;
    return 0 ;
}

Command *
Session::global_state_command_factory (const XMLNode& node)
{
	const XMLProperty* prop;
	Command* command = 0;

	if ((prop = node.property ("type")) == 0) {
		error << _("GlobalRouteStateCommand has no \"type\" node, ignoring") << endmsg;
		return 0;
	}
	
	try {

		if (prop->value() == "solo") {
			command = new GlobalSoloStateCommand (*this, node);
		} else if (prop->value() == "mute") {
			command = new GlobalMuteStateCommand (*this, node);
		} else if (prop->value() == "rec-enable") {
			command = new GlobalRecordEnableStateCommand (*this, node);
		} else if (prop->value() == "metering") {
			command = new GlobalMeteringStateCommand (*this, node);
		} else {
			error << string_compose (_("unknown type of GlobalRouteStateCommand (%1), ignored"), prop->value()) << endmsg;
		}
	}

	catch (failed_constructor& err) {
		return 0;
	}

	return command;
}

Session::GlobalRouteStateCommand::GlobalRouteStateCommand (Session& s, void* p)
	: sess (s), src (p)
{
}

Session::GlobalRouteStateCommand::GlobalRouteStateCommand (Session& s, const XMLNode& node)
	: sess (s), src (this)
{
	if (set_state (node)) {
		throw failed_constructor ();
	}
}

int
Session::GlobalRouteStateCommand::set_state (const XMLNode& node)
{
	GlobalRouteBooleanState states;
	XMLNodeList nlist;
	const XMLProperty* prop;
	XMLNode* child;
	XMLNodeConstIterator niter;
	int loop;

	before.clear ();
	after.clear ();
	
	for (loop = 0; loop < 2; ++loop) {

		const char *str;

		if (loop) {
			str = "after";
		} else {
			str = "before";
		}
		
		if ((child = node.child (str)) == 0) {
			warning << string_compose (_("global route state command has no \"%1\" node, ignoring entire command"), str) << endmsg;
			return -1;
		}

		nlist = child->children();

		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			
			RouteBooleanState rbs;
			boost::shared_ptr<Route> route;
			ID id;
			
			prop = (*niter)->property ("id");
			id = prop->value ();
			
			if ((route = sess.route_by_id (id)) == 0) {
				warning << string_compose (_("cannot find track/bus \"%1\" while rebuilding a global route state command, ignored"), id.to_s()) << endmsg;
				continue;
			}
			
			rbs.first = boost::weak_ptr<Route> (route);
			
			prop = (*niter)->property ("yn");
			rbs.second = (prop->value() == "1");
			
			if (loop) {
				after.push_back (rbs);
			} else {
				before.push_back (rbs);
			}
		}
	}

	return 0;
}

XMLNode&
Session::GlobalRouteStateCommand::get_state ()
{
	XMLNode* node = new XMLNode (X_("GlobalRouteStateCommand"));
	XMLNode* nbefore = new XMLNode (X_("before"));
	XMLNode* nafter = new XMLNode (X_("after"));

	for (Session::GlobalRouteBooleanState::iterator x = before.begin(); x != before.end(); ++x) {
		XMLNode* child = new XMLNode ("s");
		boost::shared_ptr<Route> r = x->first.lock();

		if (r) {
			child->add_property (X_("id"), r->id().to_s());
			child->add_property (X_("yn"), (x->second ? "1" : "0"));
			nbefore->add_child_nocopy (*child);
		}
	}

	for (Session::GlobalRouteBooleanState::iterator x = after.begin(); x != after.end(); ++x) {
		XMLNode* child = new XMLNode ("s");
		boost::shared_ptr<Route> r = x->first.lock();

		if (r) {
			child->add_property (X_("id"), r->id().to_s());
			child->add_property (X_("yn"), (x->second ? "1" : "0"));
			nafter->add_child_nocopy (*child);
		}
	}

	node->add_child_nocopy (*nbefore);
	node->add_child_nocopy (*nafter);

	return *node;
}

// solo

Session::GlobalSoloStateCommand::GlobalSoloStateCommand(Session &sess, void *src)
	: GlobalRouteStateCommand (sess, src)
{
    after = before = sess.get_global_route_boolean(&Route::soloed);
}

Session::GlobalSoloStateCommand::GlobalSoloStateCommand (Session& sess, const XMLNode& node)
	: Session::GlobalRouteStateCommand (sess, node)
{
}

void 
Session::GlobalSoloStateCommand::mark()
{
    after = sess.get_global_route_boolean(&Route::soloed);
}

void 
Session::GlobalSoloStateCommand::operator()()
{
    sess.set_global_solo(after, src);
}

void 
Session::GlobalSoloStateCommand::undo()
{
    sess.set_global_solo(before, src);
}

XMLNode&
Session::GlobalSoloStateCommand::get_state()
{
	XMLNode& node = GlobalRouteStateCommand::get_state();
	node.add_property ("type", "solo");
	return node;
}

// mute
Session::GlobalMuteStateCommand::GlobalMuteStateCommand(Session &sess, void *src)
	: GlobalRouteStateCommand (sess, src)
{
    after = before = sess.get_global_route_boolean(&Route::muted);
}

Session::GlobalMuteStateCommand::GlobalMuteStateCommand (Session& sess, const XMLNode& node)
	: Session::GlobalRouteStateCommand (sess, node)
{
}

void 
Session::GlobalMuteStateCommand::mark()
{
	after = sess.get_global_route_boolean(&Route::muted);
}

void 
Session::GlobalMuteStateCommand::operator()()
{
	sess.set_global_mute(after, src);
}

void 
Session::GlobalMuteStateCommand::undo()
{
	sess.set_global_mute(before, src);
}

XMLNode&
Session::GlobalMuteStateCommand::get_state()
{
	XMLNode& node = GlobalRouteStateCommand::get_state();
	node.add_property ("type", "mute");
	return node;
}

// record enable
Session::GlobalRecordEnableStateCommand::GlobalRecordEnableStateCommand(Session &sess, void *src) 
	: GlobalRouteStateCommand (sess, src)
{
	after = before = sess.get_global_route_boolean(&Route::record_enabled);
}

Session::GlobalRecordEnableStateCommand::GlobalRecordEnableStateCommand (Session& sess, const XMLNode& node)
	: Session::GlobalRouteStateCommand (sess, node)
{
}

void 
Session::GlobalRecordEnableStateCommand::mark()
{
	after = sess.get_global_route_boolean(&Route::record_enabled);
}

void 
Session::GlobalRecordEnableStateCommand::operator()()
{
	sess.set_global_record_enable(after, src);
}

void 
Session::GlobalRecordEnableStateCommand::undo()
{
	sess.set_global_record_enable(before, src);
}

XMLNode& 
Session::GlobalRecordEnableStateCommand::get_state()
{
	XMLNode& node = GlobalRouteStateCommand::get_state();
	node.add_property ("type", "rec-enable");
	return node;
}

// metering
Session::GlobalMeteringStateCommand::GlobalMeteringStateCommand(Session &s, void *p) 
	: sess (s), src (p)
{
	after = before = sess.get_global_route_metering();
}

Session::GlobalMeteringStateCommand::GlobalMeteringStateCommand (Session& s, const XMLNode& node)
	: sess (s), src (this)
{
	if (set_state (node)) {
		throw failed_constructor();
	}
}

void 
Session::GlobalMeteringStateCommand::mark()
{
	after = sess.get_global_route_metering();
}

void 
Session::GlobalMeteringStateCommand::operator()()
{
	sess.set_global_route_metering(after, src);
}

void 
Session::GlobalMeteringStateCommand::undo()
{
	sess.set_global_route_metering(before, src);
}

XMLNode&
Session::GlobalMeteringStateCommand::get_state()
{
	XMLNode* node = new XMLNode (X_("GlobalRouteStateCommand"));
	XMLNode* nbefore = new XMLNode (X_("before"));
	XMLNode* nafter = new XMLNode (X_("after"));

	for (Session::GlobalRouteMeterState::iterator x = before.begin(); x != before.end(); ++x) {
		XMLNode* child = new XMLNode ("s");
		boost::shared_ptr<Route> r = x->first.lock();

		if (r) {
			child->add_property (X_("id"), r->id().to_s());

			const char* meterstr = 0;
			
			switch (x->second) {
			case MeterInput:
				meterstr = X_("input");
				break;
			case MeterPreFader:
				meterstr = X_("pre");
				break;
			case MeterPostFader:
				meterstr = X_("post");
				break;
			default:
				fatal << string_compose (_("programming error: %1") , "no meter state in Session::GlobalMeteringStateCommand::get_state") << endmsg;
			}

			child->add_property (X_("meter"), meterstr);
			nbefore->add_child_nocopy (*child);
		}
	}

	for (Session::GlobalRouteMeterState::iterator x = after.begin(); x != after.end(); ++x) {
		XMLNode* child = new XMLNode ("s");
		boost::shared_ptr<Route> r = x->first.lock();

		if (r) {
			child->add_property (X_("id"), r->id().to_s());

			const char* meterstr;
			
			switch (x->second) {
			case MeterInput:
				meterstr = X_("input");
				break;
			case MeterPreFader:
				meterstr = X_("pre");
				break;
			case MeterPostFader:
				meterstr = X_("post");
				break;
			default: meterstr = "";
			}

			child->add_property (X_("meter"), meterstr);
			nafter->add_child_nocopy (*child);
		}
	}

	node->add_child_nocopy (*nbefore);
	node->add_child_nocopy (*nafter);

	node->add_property ("type", "metering");

	return *node;
}

int
Session::GlobalMeteringStateCommand::set_state (const XMLNode& node)
{
	GlobalRouteBooleanState states;
	XMLNodeList nlist;
	const XMLProperty* prop;
	XMLNode* child;
	XMLNodeConstIterator niter;
	int loop;

	before.clear ();
	after.clear ();
	
	for (loop = 0; loop < 2; ++loop) {

		const char *str;

		if (loop) {
			str = "after";
		} else {
			str = "before";
		}
		
		if ((child = node.child (str)) == 0) {
			warning << string_compose (_("global route meter state command has no \"%1\" node, ignoring entire command"), str) << endmsg;
			return -1;
		}

		nlist = child->children();

		for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
			
			RouteMeterState rms;
			boost::shared_ptr<Route> route;
			ID id;
			
			prop = (*niter)->property ("id");
			id = prop->value ();
			
			if ((route = sess.route_by_id (id)) == 0) {
				warning << string_compose (_("cannot find track/bus \"%1\" while rebuilding a global route state command, ignored"), id.to_s()) << endmsg;
				continue;
			}
			
			rms.first = boost::weak_ptr<Route> (route);
			
			prop = (*niter)->property ("meter");

			if (prop->value() == X_("pre")) {
				rms.second = MeterPreFader;
			} else if (prop->value() == X_("post")) {
				rms.second = MeterPostFader;
			} else {
				rms.second = MeterInput;
			}
			
			if (loop) {
				after.push_back (rms);
			} else {
				before.push_back (rms);
			}
		}
	}

	return 0;
}
