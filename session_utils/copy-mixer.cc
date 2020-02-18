/*
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

#include <iostream>
#include <cstdlib>
#include <getopt.h>
#include <glibmm.h>

#include "pbd/basename.h"
#include "pbd/stateful.h"
#include "ardour/filename_extensions.h"
#include "ardour/send.h"
#include "ardour/track.h"

#include "common.h"

#define X_(Text) Text

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

static bool opt_debug_dump = false;
static bool opt_copy_busses = false;
static bool opt_verbose = false;
static bool opt_log = false;

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
					(*i)->set_property ("type", "dangling-aux-send");
					continue;
				}
				boost::shared_ptr<Route> r = s->route_by_name (target->value ());
				if (!r || boost::dynamic_pointer_cast<Track> (r)) {
					(*i)->set_property ("type", "dangling-aux-send");
					continue;
				}
				(*i)->set_property ("target", r->id ().to_s ());
			}
			if (role && role->value () == X_("Listen")) {
				(*i)->remove_property (X_("bitslot"));
			}
			else if (role && (role->value () == X_("Send") || role->value () == X_("Aux"))) {
				Delivery::Role xrole;
				uint32_t bitslot = 0;
				xrole = Delivery::Role (string_2_enum (role->value (), xrole));
				std::string name = Send::name_and_id_new_send (*s, xrole, bitslot, false);
				(*i)->remove_property (X_("bitslot"));
				(*i)->remove_property (X_("name"));
				(*i)->set_property ("bitslot", bitslot);
				(*i)->set_property ("name", name);
			}
			else if (type && type->value () == X_("intreturn")) {
				// ignore, in case bus existed in old session,
				// tracks in old session may be connected to it.
				// if the bus is new, new_route_from_template()
				// will have re-created an ID.
				(*i)->set_property ("type", "ignore-aux-return");
			}
			else if (type && type->value () == X_("return")) {
				// Return::set_state() generates a new one
				(*i)->remove_property (X_("bitslot"));
			}
			else if (type && type->value () == X_("port")) {
				// PortInsert::set_state() handles the bitslot
				(*i)->remove_property (X_("bitslot"));
				(*i)->set_property ("ignore-name", "1");
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
	if (opt_debug_dump) {
		state.dump (cout);
	}

	dst->set_state (state, PBD::Stateful::loading_state_version);
}

static int
copy_session_routes (
		const std::string& src_path, const std::string& src_name,
		const std::string& dst_path, const std::string& dst_load, const std::string& dst_save)
{
	SessionUtils::init (opt_log);
	Session* s = 0;

	typedef std::map<std::string,XMLNode*> StateMap;
	StateMap routestate;
	StateMap buslist;

	s = SessionUtils::load_session (src_path, src_name, false);

	if (!s) {
		fprintf (stderr, "Cannot load source session %s/%s.\n", src_path.c_str (), src_name.c_str ());
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
		fprintf (stderr, "Cannot load target session %s/%s.\n", dst_path.c_str (), dst_load.c_str ());
		SessionUtils::cleanup ();
		return -1;
	}

	/* iterate over all busses in the src session, add missing ones to target */
	if (opt_copy_busses) {
		rl = s->get_routes ();
		for (StateMap::const_iterator i = buslist.begin (); i != buslist.end (); ++i) {
			if (s->route_by_name (i->first)) {
				continue;
			}
			XMLNode& rs (*(i->second));
			s->new_route_from_template (1, PresentationInfo::max_order, rs, rs.property (X_("name"))->value (), NewPlaylist);
		}
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
			if (opt_verbose) {
				printf (" -- no match for '%s'\n", (*i)->name ().c_str ());
			}
			continue;
		}
		if (opt_verbose) {
			printf ("-- found match '%s'\n", (*i)->name ().c_str ());
		}
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
			if (opt_verbose) {
				printf (" -- no match for '%s'\n", (*i)->name ().c_str ());
			}
			continue;
		}
		if (opt_verbose) {
			printf ("-- found match '%s'\n", (*i)->name ().c_str ());
		}
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


static void usage () {
	// help2man compatible format (standard GNU help-text)
	printf (UTILNAME " - copy mixer settings from one session to another.\n\n");
	printf ("Usage: " UTILNAME " [ OPTIONS ] <src> <dst>\n\n");

	printf ("Options:\n\
  -h, --help                 display this help and exit\n\
  -b, --bus-copy             add busses present in src to dst\n\
  -d, --debug                print pre-processed XML for each route\n\
  -l, --log-messages         display libardour log messages\n\
  -s, --snapshot <name>      create a new snapshot in dst\n\
  -v, --verbose              show performed copy operations\n\
  -V, --version              print version information and exit\n\
\n");

	printf ("\n\
This utility copies mixer-settings from the src-session to the dst-session.\n\
Both <src> and <dst> are paths to .ardour session files.\n\
If --snapshot is not given, the <dst> session file is overwritten.\n\
When --snapshot is set, a new snaphot in the <dst> session is created.\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
	        "Website: <http://ardour.org/>\n");
	::exit (EXIT_SUCCESS);
}

static bool ends_with (std::string const& value, std::string const& ending)
{
	if (ending.size() > value.size()) return false;
	return std::equal(ending.rbegin(), ending.rend(), value.rbegin());
}

int main (int argc, char* argv[])
{
	const char *optstring = "bhls:Vv";

	const struct option longopts[] = {
		{ "bus-copy",     required_argument, 0, 'b' },
		{ "debug",        no_argument,       0, 'd' },
		{ "help",         no_argument,       0, 'h' },
		{ "log-messages", no_argument,       0, 'l' },
		{ "snapshot",     no_argument,       0, 's' },
		{ "version",      no_argument,       0, 'V' },
		{ "vebose",       no_argument,       0, 'v' },
	};

	int c = 0;
	std::string dst_snapshot_name = "";

	while (EOF != (c = getopt_long (argc, argv,
					optstring, longopts, (int *) 0))) {
		switch (c) {
			case 'b':
				opt_copy_busses = true;
				break;

			case 'd':
				opt_debug_dump = true;
				break;

			case 'h':
				usage ();
				break;

			case 'l':
				opt_log = true;
				break;

			case 's':
				dst_snapshot_name = optarg;
				break;

			case 'V':
				printf ("ardour-utils version %s\n\n", VERSIONSTRING);
				printf ("Copyright (C) GPL 2016 Robin Gareus <robin@gareus.org>\n");
				exit (EXIT_SUCCESS);
				break;

			case 'v':
				opt_verbose = true;
				break;

			default:
				cerr << "Error: unrecognized option. See --help for usage information.\n";
				::exit (EXIT_FAILURE);
				break;
		}
	}

	// TODO parse path/name  from a single argument.

	if (optind + 2 > argc) {
		cerr << "Error: Missing parameter. See --help for usage information.\n";
		::exit (EXIT_FAILURE);
	}

	std::string src = argv[optind];
	std::string dst = argv[optind + 1];

	// statefile_suffix

	if (!ends_with (src, statefile_suffix)) {
		fprintf (stderr, "source is not a .ardour session file.\n");
		exit (EXIT_FAILURE);
	}
	if (!ends_with (dst, statefile_suffix)) {
		fprintf (stderr, "target is not a .ardour session file.\n");
		exit (EXIT_FAILURE);
	}
	if (!Glib::file_test (src, Glib::FILE_TEST_IS_REGULAR)) {
		fprintf (stderr, "source is not a regular file.\n");
		exit (EXIT_FAILURE);
	}
	if (!Glib::file_test (dst, Glib::FILE_TEST_IS_REGULAR)) {
		fprintf (stderr, "target is not a regular file.\n");
		exit (EXIT_FAILURE);
	}

	std::string src_path = Glib::path_get_dirname (src);
	std::string src_name = PBD::basename_nosuffix (src);
	std::string dst_path = Glib::path_get_dirname (dst);
	std::string dst_name = PBD::basename_nosuffix (dst);

	// TODO check if src != dst ..
	return copy_session_routes (
			src_path, src_name,
			dst_path, dst_name,
			dst_snapshot_name);
}
