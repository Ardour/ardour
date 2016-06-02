#include <iostream>
#include <cstdlib>

#include "pbd/stateful.h"
#include "ardour/send.h"
#include "ardour/track.h"

#include "common.h"

#define X_(Text) Text

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;

/* this is copied from  Session::new_route_from_template */
void trim_state_for_mixer_copy (Session*s, XMLNode& node)
{
	/* trim bitslots from listen sends so that new ones are used */
	XMLNodeList children = node.children ();
	for (XMLNodeList::iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == X_("Processor")) {
			/* ForceIDRegeneration does not catch the following */
			XMLProperty const * role = (*i)->property (X_("role"));
			XMLProperty const * type = (*i)->property (X_("type"));
			if (role && role->value() == X_("Aux")) {
				/* check if the target bus exists.
				 * we should not save aux-sends in templates.
				 */
				XMLProperty const * target = (*i)->property (X_("target"));
				if (!target) {
					(*i)->add_property ("type", "dangling-aux-send");
					continue;
				}
				boost::shared_ptr<Route> r = s->route_by_id (target->value());
				if (!r || boost::dynamic_pointer_cast<Track>(r)) {
					(*i)->add_property ("type", "dangling-aux-send");
					continue;
				}
			}
			if (role && role->value() == X_("Listen")) {
				(*i)->remove_property (X_("bitslot"));
			}
			else if (role && (role->value() == X_("Send") || role->value() == X_("Aux"))) {
				char buf[32];
				Delivery::Role xrole;
				uint32_t bitslot = 0;
				xrole = Delivery::Role (string_2_enum (role->value(), xrole));
				std::string name = Send::name_and_id_new_send(*s, xrole, bitslot, false);
				snprintf (buf, sizeof (buf), "%" PRIu32, bitslot);
				(*i)->remove_property (X_("bitslot"));
				(*i)->remove_property (X_("name"));
				(*i)->add_property ("bitslot", buf);
				(*i)->add_property ("name", name);
			}
			else if (type && type->value() == X_("return")) {
				// Return::set_state() generates a new one
				(*i)->remove_property (X_("bitslot"));
			}
			else if (type && type->value() == X_("port")) {
				// PortInsert::set_state() handles the bitslot
				(*i)->remove_property (X_("bitslot"));
				(*i)->add_property ("ignore-name", "1");
			}
		}
	}
}


static void copy_mixer_settings (Session*s, boost::shared_ptr<Route> dst, XMLNode& state)
{
	PBD::Stateful::ForceIDRegeneration force_ids;

	trim_state_for_mixer_copy (s, state);
	state.remove_nodes_and_delete ("Diskstream");
	state.remove_nodes_and_delete ("Automation");
	//state.dump (cerr);

	dst->set_state (state, PBD::Stateful::loading_state_version);
}

int main (int argc, char* argv[])
{
	if (argc < 5) {
		printf ("Usage: copy-mix <session1-dir> <session1-name> <session2-dir> <session2-name>\n");
		return -1;
	}

	std::string session1_dir (argv[1]);
	std::string session1_name (argv[2]);

	std::string session2_dir = (argv[3]);
	std::string session2_name = (argv[4]);


	SessionUtils::init();
	Session* s = 0;
	std::map<std::string,XMLNode*> routestate;

	s = SessionUtils::load_session ( session1_dir, session1_name);

	if (!s) {
		printf("Cannot load source session.\n");
		SessionUtils::cleanup();
		return 0;
	}

	// get route state from first session
	boost::shared_ptr<RouteList> rl = s->get_routes ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		XMLNode& state ((*i)->get_state());
		routestate[(*i)->name()] = &state;
	}
	rl.reset();
	SessionUtils::unload_session(s);


	// open target session
	s = SessionUtils::load_session (session2_dir, session2_name);
	if (!s) {
		printf("Cannot load target session.\n");
		SessionUtils::cleanup();
		return 0;
	}

	// iterate over all routes in the target session..
	rl = s->get_routes ();
	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Route> r = *i;
		// skip special busses
		if (r->is_master() || r->is_monitor() || r->is_auditioner()) {
			continue;
		}
		// find matching route by name
		std::map<std::string,XMLNode*>::iterator it = routestate.find (r->name ());
		if (it == routestate.end()) {
			printf (" -- no match for '%s'\n", (*i)->name().c_str());
			continue;
		}
		printf ("-- found match '%s'\n", (*i)->name().c_str());
		XMLNode *state = it->second;
		// copy state
		copy_mixer_settings (s, r, *state);
	}

	s->save_state ("");

	rl.reset();
	SessionUtils::unload_session(s);

	// clean up.
	for (std::map<std::string,XMLNode*>::iterator i = routestate.begin(); i != routestate.end(); ++i) {
		XMLNode *state = i->second;
		delete state;
	}

	SessionUtils::cleanup();
	return 0;
}
