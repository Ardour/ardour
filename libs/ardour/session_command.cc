#include <ardour/session.h>
#include <ardour/route.h>
#include <pbd/memento_command.h>
#include <ardour/diskstream.h>
#include <ardour/playlist.h>
#include <ardour/tempo.h>
#include <ardour/audiosource.h>
#include <ardour/audioregion.h>
#include <pbd/error.h>
using namespace PBD;
#include "i18n.h"


namespace ARDOUR {

static map<PBD::ID, Stateful*> registry;

void Session::register_with_memento_command_factory(PBD::ID id, Stateful *ptr)
{
    registry[id] = ptr;
}
    
Command *Session::memento_command_factory(XMLNode *n)
{
    PBD::ID id;
    XMLNode *before = 0, *after = 0;
    XMLNode *child;

    /* get id */
    id = PBD::ID(n->property("obj_id")->value());

    /* get before/after */
    if (n->name() == "MementoCommand")
    {
        before = new XMLNode(*n->children().front());
        after = new XMLNode(*n->children().back());
	child = before;
    } else if (n->name() == "MementoUndoCommand")
    {
        before = new XMLNode(*n->children().front());
	child = before;
    }
    else if (n->name() == "MementoRedoCommand")
    {
        after = new XMLNode(*n->children().front());
	child = after;
    }

    if (!child)
    {
	error << _("Tried to reconstitute a MementoCommand with no contents, failing. id=") << id.to_s() << endmsg;
	return 0;
    }


    /* create command */
    string obj_T = n->children().front()->name();
    if (obj_T == "AudioRegion" || obj_T == "Region")
    {
        if (audio_regions.count(id))
            return new MementoCommand<AudioRegion>(*audio_regions[id], before, after);
    }
    else if (obj_T == "AudioSource")
    {
        if (audio_sources.count(id))
            return new MementoCommand<AudioSource>(*audio_sources[id], before, after);
    }
    else if (obj_T == "Location")
        return new MementoCommand<Location>(*_locations.get_location_by_id(id), before, after);
    else if (obj_T == "Locations")
        return new MementoCommand<Locations>(_locations, before, after);
    else if (obj_T == "TempoMap")
        return new MementoCommand<TempoMap>(*_tempo_map, before, after);
    else if (obj_T == "Playlist" || obj_T == "AudioPlaylist")
    {
        if (Playlist *pl = playlist_by_name(child->property("name")->value()))
            return new MementoCommand<Playlist>(*pl, before, after);
    }
    else if (obj_T == "Route") // inlcudes AudioTrack
        return new MementoCommand<Route>(*route_by_id(id), before, after);
    else if (obj_T == "Curve")
    {
        if (curves.count(id))
            return new MementoCommand<Curve>(*curves[id], before, after);
    }
    else if (obj_T == "AutomationList")
    {
        if (automation_lists.count(id))
            return new MementoCommand<AutomationList>(*automation_lists[id], before, after);
    }
    // For Editor and AutomationLine which are off-limits here
    else if (registry.count(id))
        return new MementoCommand<Stateful>(*registry[id], before, after);

    /* we failed */
    error << _("could not reconstitute MementoCommand from XMLNode. id=") << id.to_s() << endmsg;
    return 0;
}

// solo
Session::GlobalSoloStateCommand::GlobalSoloStateCommand(Session &sess, void *src)
    : sess(sess), src(src)
{
    after = before = sess.get_global_route_boolean(&Route::soloed);
}
void Session::GlobalSoloStateCommand::mark()
{
    after = sess.get_global_route_boolean(&Route::soloed);
}
void Session::GlobalSoloStateCommand::operator()()
{
    sess.set_global_solo(after, src);
}
void Session::GlobalSoloStateCommand::undo()
{
    sess.set_global_solo(before, src);
}
XMLNode &Session::GlobalSoloStateCommand::get_state()
{
    XMLNode *node = new XMLNode("GlobalSoloStateCommand");
    return *node;
}

// mute
Session::GlobalMuteStateCommand::GlobalMuteStateCommand(Session &sess, void *src)
    : sess(sess), src(src)
{
    after = before = sess.get_global_route_boolean(&Route::muted);
}
void Session::GlobalMuteStateCommand::mark()
{
    after = sess.get_global_route_boolean(&Route::muted);
}
void Session::GlobalMuteStateCommand::operator()()
{
    sess.set_global_mute(after, src);
}
void Session::GlobalMuteStateCommand::undo()
{
    sess.set_global_mute(before, src);
}
XMLNode &Session::GlobalMuteStateCommand::get_state()
{
    XMLNode *node = new XMLNode("GlobalMuteStateCommand");
    return *node;
}

// record enable
Session::GlobalRecordEnableStateCommand::GlobalRecordEnableStateCommand(Session &sess, void *src) 
    : sess(sess), src(src)
{
    after = before = sess.get_global_route_boolean(&Route::record_enabled);
}
void Session::GlobalRecordEnableStateCommand::mark()
{
    after = sess.get_global_route_boolean(&Route::record_enabled);
}
void Session::GlobalRecordEnableStateCommand::operator()()
{
    sess.set_global_record_enable(after, src);
}
void Session::GlobalRecordEnableStateCommand::undo()
{
    sess.set_global_record_enable(before, src);
}
XMLNode &Session::GlobalRecordEnableStateCommand::get_state()
{
    XMLNode *node = new XMLNode("GlobalRecordEnableStateCommand");
    return *node;
}

// metering
Session::GlobalMeteringStateCommand::GlobalMeteringStateCommand(Session &sess, void *src) 
    : sess(sess), src(src)
{
    after = before = sess.get_global_route_metering();
}
void Session::GlobalMeteringStateCommand::mark()
{
    after = sess.get_global_route_metering();
}
void Session::GlobalMeteringStateCommand::operator()()
{
    sess.set_global_route_metering(after, src);
}
void Session::GlobalMeteringStateCommand::undo()
{
    sess.set_global_route_metering(before, src);
}
XMLNode &Session::GlobalMeteringStateCommand::get_state()
{
    XMLNode *node = new XMLNode("GlobalMeteringStateCommand");
    return *node;
}

} // namespace ARDOUR
