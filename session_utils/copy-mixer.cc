#include <iostream>
#include <cstdlib>
#include <getopt.h>

#include "pbd/stateful.h"
#include "ardour/send.h"
#include "ardour/track.h"

#include "common.h"

#define X_(Text) Text

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

/* this is copied from  Session::new_route_from_template */
static void
trim_state_for_mixer_copy (Session*s, XMLNode& node)
{
	/* trim bitslots from listen sends so that new ones are used */
	XMLNodeList children = node.children ();
	for (XMLNodeList::iterator i = children.begin (); i != children.end (); ++i) {
		if ((*i)->name() == X_("Processor")) {
			/* ForceIDRegeneration does not catch the following */
			XMLProperty const * role = (*i)->property (X_("role"));
			XMLProperty const * type = (*i)->property (X_("type"));
			if (role && role->value () == X_("Aux")) {
				/* check if the target bus exists,
				 * HERE: we use the bus-name (not target-id)
				 */
				XMLProperty const * target = (*i)->property (X_("name"));
				if (!target) {
					(*i)->add_property ("type", "dangling-aux-send");
					continue;
				}
				boost::shared_ptr<Route> r = s->route_by_name (target->value ());
				if (!r || boost::dynamic_pointer_cast<Track> (r)) {
					(*i)->add_property ("type", "dangling-aux-send");
					continue;
				}
				(*i)->add_property ("target", r->id ().to_s ());
			}
			if (role && role->value () == X_("Listen")) {
				(*i)->remove_property (X_("bitslot"));
			}
			else if (role && (role->value () == X_("Send") || role->value () == X_("Aux"))) {
				char buf[32];
				Delivery::Role xrole;
				uint32_t bitslot = 0;
				xrole = Delivery::Role (string_2_enum (role->value (), xrole));
				std::string name = Send::name_and_id_new_send (*s, xrole, bitslot, false);
				snprintf (buf, sizeof (buf), "%" PRIu32, bitslot);
				(*i)->remove_property (X_("bitslot"));
				(*i)->remove_property (X_("name"));
				(*i)->add_property ("bitslot", buf);
				(*i)->add_property ("name", name);
			}
			else if (type && type->value () == X_("intreturn")) {
				// ignore, in case bus existed in old session,
				// tracks in old session may be connected to it.
				// if the bus is new, new_route_from_template()
				// will have re-created an ID.
				(*i)->add_property ("type", "ignore-aux-return");
			}
			else if (type && type->value () == X_("return")) {
				// Return::set_state() generates a new one
				(*i)->remove_property (X_("bitslot"));
			}
			else if (type && type->value () == X_("port")) {
				// PortInsert::set_state() handles the bitslot
				(*i)->remove_property (X_("bitslot"));
				(*i)->add_property ("ignore-name", "1");
			}
		}
	}
}

static void
copy_mixer_settings (Session*s, boost::shared_ptr<Route> dst, XMLNode& state)
{
	PBD::Stateful::ForceIDRegeneration force_ids;

	trim_state_for_mixer_copy (s, state);
	state.remove_nodes_and_delete ("Diskstream");
	state.remove_nodes_and_delete ("Automation");
	state.dump (cerr);

	dst->set_state (state, PBD::Stateful::loading_state_version);
}

static int
copy_session_routes (
		const std::string& src_path, const std::string& src_name,
		const std::string& dst_path, const std::string& dst_load, const std::string& dst_save)
{
	SessionUtils::init (false);
	Session* s = 0;

	typedef std::map<std::string,XMLNode*> StateMap;
	StateMap routestate;
	StateMap buslist;

	s = SessionUtils::load_session (src_path, src_name);

	if (!s) {
		printf ("Cannot load source session %s/%s.\n", src_path.c_str (), src_name.c_str ());
		SessionUtils::cleanup ();
		return -1;
	}

	/* get route state from first session */
	boost::shared_ptr<RouteList> rl = s->get_routes ();
	for (RouteList::iterator i = rl->begin (); i != rl->end (); ++i) {
		boost::shared_ptr<Route> r = *i;
		if (r->is_master () || r->is_monitor () || r->is_auditioner ()) {
			continue;
		}
		XMLNode& state (r->get_state ());
		routestate[r->name ()] = &state;
		if (boost::dynamic_pointer_cast<Track> (r)) {
			continue;
		}
		buslist[r->name ()] = &state;
	}
	rl.reset ();
	SessionUtils::unload_session (s);


	/* open target session */
	s = SessionUtils::load_session (dst_path, dst_load);
	if (!s) {
		printf ("Cannot load target session %s/%s.\n", dst_path.c_str (), dst_load.c_str ());
		SessionUtils::cleanup ();
		return -1;
	}

	/* iterate over all busses in the src session, add missing ones to target */
	// TODO: make optional
	rl = s->get_routes ();
	for (StateMap::const_iterator i = buslist.begin (); i != buslist.end (); ++i) {
		if (s->route_by_name (i->first)) {
			continue;
		}
		XMLNode& rs (*(i->second));
		s->new_route_from_template (1, rs, rs.property (X_("name"))->value (), NewPlaylist);
	}

	/* iterate over all *busses* in the target session.
	 * setup internal return targets.
	 */
	rl = s->get_routes ();
	for (RouteList::iterator i = rl->begin (); i != rl->end (); ++i) {
		boost::shared_ptr<Route> r = *i;
		/* skip special busses */
		if (r->is_master () || r->is_monitor () || r->is_auditioner ()) {
			continue;
		}
		if (boost::dynamic_pointer_cast<Track> (r)) {
			continue;
		}
		/* find matching route by name */
		std::map<std::string,XMLNode*>::iterator it = routestate.find (r->name ());
		if (it == routestate.end ()) {
			printf (" -- no match for '%s'\n", (*i)->name ().c_str ());
			continue;
		}
		printf ("-- found match '%s'\n", (*i)->name ().c_str ());
		XMLNode *state = it->second;
		// copy state
		copy_mixer_settings (s, r, *state);
	}

	/* iterate over all tracks in the target session.. */
	rl = s->get_routes ();
	for (RouteList::iterator i = rl->begin (); i != rl->end (); ++i) {
		boost::shared_ptr<Route> r = *i;
		/* skip special busses */
		if (r->is_master () || r->is_monitor () || r->is_auditioner ()) {
			continue;
		}
		if (!boost::dynamic_pointer_cast<Track> (r)) {
			continue;
		}

		/* find matching route by name */
		std::map<std::string,XMLNode*>::iterator it = routestate.find (r->name ());
		if (it == routestate.end ()) {
			printf (" -- no match for '%s'\n", (*i)->name ().c_str ());
			continue;
		}
		printf ("-- found match '%s'\n", (*i)->name ().c_str ());
		XMLNode *state = it->second;
		/* copy state */
		copy_mixer_settings (s, r, *state);
	}

	s->save_state (dst_save);

	rl.reset ();
	SessionUtils::unload_session (s);

	// clean up.
	for (StateMap::iterator i = routestate.begin (); i != routestate.end (); ++i) {
		XMLNode *state = i->second;
		delete state;
	}

	SessionUtils::cleanup ();
	return 0;
}


static void usage (int status) {
	// help2man compatible format (standard GNU help-text)
	printf ("copy-mixer - copy mixer settings from one session to another.\n\n");
	printf ("Usage: copy-mixer [ OPTIONS ] <src-path> <name> <dst-path> <name> [name]\n\n");
	printf ("Options:\n\
  -h, --help                 display this help and exit\n\
  -V, --version              print version information and exit\n\
\n");
	printf ("\n\
.. not yet documented..\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
	        "Website: <http://ardour.org/>\n");
	::exit (status);
}


int main (int argc, char* argv[])
{
	const char *optstring = "hV";

	// TODO add some arguments. (save snapshot, copy busses...)

	const struct option longopts[] = {
		{ "help",       0, 0, 'h' },
		{ "version",    0, 0, 'V' },
	};

	int c = 0;

	while (EOF != (c = getopt_long (argc, argv,
					optstring, longopts, (int *) 0))) {
		switch (c) {

			case 'V':
				printf ("ardour-utils version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2015 Robin Gareus <robin@gareus.org>\n");
				exit (0);
				break;

			case 'h':
				usage (0);
				break;

			default:
					usage (EXIT_FAILURE);
					break;
		}
	}

	// TODO parse path/name  from a single argument.

	if (optind + 4 > argc) {
		usage (EXIT_FAILURE);
	}

	return copy_session_routes (
			argv[optind], argv[optind + 1],
			argv[optind + 2], argv[optind + 3],
			(optind + 4 < argc) ? argv[optind + 4] : "");
}
