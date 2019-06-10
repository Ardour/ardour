/*
    Copyright (C) 2019 Nikolaus Gullotta

    This program is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public License
    as published by the Free Software Foundation; either version 2
    of the License, or (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <iostream>

#include <glibmm.h>
#include <glibmm/fileutils.h>

#include "pbd/file_utils.h"
#include "pbd/i18n.h"
#include "pbd/memento_command.h"
#include "pbd/types_convert.h"
#include "pbd/stl_delete.h"
#include "pbd/strsplit.h"
#include "pbd/xml++.h"
#include "pbd/enumwriter.h"

#include "ardour/mixer_snapshot.h"
#include "ardour/route_group.h"
#include "ardour/vca_manager.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/session_state_utils.h"
#include "ardour/revision.h"
#include "ardour/session_directory.h"
#include "ardour/types_convert.h"

namespace PBD {
    DEFINE_ENUM_CONVERT(ARDOUR::MixerSnapshot::RecallFlags)
}

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MixerSnapshot::MixerSnapshot(Session* s)
    : id(0)
    , favorite(false)
    , label("snapshot")
    , timestamp(time(0))
    , last_modified_with(string_compose("%1 %2", PROGRAM_NAME, revision))
    , _flags(RecallFlags(127))
{
    if(s) {
        _session = s;
    }
}

MixerSnapshot::MixerSnapshot(Session* s, string file_path)
    : id(0)
    , favorite(false)
    , label("snapshot")
    , timestamp(time(0))
    , last_modified_with(string_compose("%1 %2", PROGRAM_NAME, revision))
    , _flags(RecallFlags(127))
{
    if(s) {
        _session = s;
    }

    if(Glib::file_test(file_path.c_str(), Glib::FILE_TEST_IS_DIR)) {
        load_from_session(file_path);
        return;
    }

    string suffix = "." + get_suffix(file_path);
    if(suffix == statefile_suffix) {
        load_from_session(file_path);
        return;
    }

    if(suffix == template_suffix) {
        load_from_session(file_path);
        return;
    }

    if(suffix == ".xml") {
        load(file_path);
        return;
    }
}

bool MixerSnapshot::set_flag(bool yn, RecallFlags flag)
{
    if (yn) {
        if (!(_flags & flag)) {
            _flags = RecallFlags (_flags | flag);
            return true;
        }
    } else {
        if (_flags & flag) {
            _flags = RecallFlags (_flags & ~flag);
            return true;
        }
    }
    return false;
}

#ifdef MIXBUS
bool MixerSnapshot::set_recall_eq(bool yn)    { return set_flag(yn, RecallEQ);   };
bool MixerSnapshot::set_recall_sends(bool yn) { return set_flag(yn, RecallSends);};
bool MixerSnapshot::set_recall_comp(bool yn)  { return set_flag(yn, RecallComp); };
#endif
bool MixerSnapshot::set_recall_pan(bool yn)     { return set_flag(yn, RecallPan);   };
bool MixerSnapshot::set_recall_plugins(bool yn) { return set_flag(yn, RecallPlugs); };
bool MixerSnapshot::set_recall_groups(bool yn)  { return set_flag(yn, RecallGroups);};
bool MixerSnapshot::set_recall_vcas(bool yn)    { return set_flag(yn, RecallVCAs);  };

bool MixerSnapshot::has_specials()
{
    if(empty()) {
        return false;
    }

    for(vector<State>::const_iterator it = route_states.begin(); it != route_states.end(); it++) {
        if((*it).name == "Monitor" || "Auditioner" || "Master") {
            return true;
        }
    }
    return false;
}

void MixerSnapshot::clear()
{
    timestamp = time(0);
    route_states.clear();
    group_states.clear();
    vca_states.clear();
}

void MixerSnapshot::snap(boost::shared_ptr<Route> route)
{
    if(!route) {
        return;
    }

    string name = route->name();
    XMLNode& original = route->get_template();
    XMLNode copy (original);

    RouteGroup* group = route->route_group();
    if(group) {
        XMLNode* group_node = copy.add_child(X_("Group"));
        group_node->set_property(X_("name"), group->name());
        snap(group);
    }

    XMLNode* slavable = find_named_node(copy, "Slavable");

    if(slavable) {
        XMLNodeList nlist = slavable->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
            string number;
            (*niter)->get_property(X_("number"), number);

            int i = atoi(number.c_str());
            boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_number(i);

            if(vca) {
                //we will need this for later recollection
                (*niter)->set_property(X_("name"), vca->name());
                snap(vca);
            }
        }
    }

    State state {route->id().to_s(), route->name(), copy};
    route_states.push_back(state);
}

void MixerSnapshot::snap(RouteGroup* group)
{
    if(!group) {
        return;
    }

    string name = group->name();
    XMLNode& original = group->get_state();
    XMLNode  copy (original);

    State state {group->id().to_s(), group->name(), copy};
    group_states.push_back(state);
}

void MixerSnapshot::snap(boost::shared_ptr<VCA> vca)
{
    if(!vca) {
        return;
    }

    string name = vca->name();
    XMLNode& original = vca->get_state();
    XMLNode  copy (original);

    State state {vca->id().to_s(), vca->name(), copy};
    vca_states.push_back(state);
}



void MixerSnapshot::snap(RouteList rl)
{
    if(!_session) {
        return;
    }

    clear();

    if(rl.empty()) {
        return;
    }

    for(RouteList::const_iterator it = rl.begin(); it != rl.end(); it++) {
        snap((*it));
    }
}

void MixerSnapshot::snap()
{
    if(!_session) {
        return;
    }

    clear();

    RouteList rl = _session->get_routelist();
    if(rl.empty()) {
        return;
    } else {
        snap(rl);
    }
}

void MixerSnapshot::reassign_masters(boost::shared_ptr<Slavable> slv, XMLNode node)
{
    if(!slv) {
        return;
    }

    XMLNode* slavable = find_named_node(node, "Slavable");

    if(!slavable) {
        return;
    }

    XMLNodeList nlist = slavable->children();

    for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
        string name;
        (*niter)->get_property(X_("name"), name);

        boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_name(name);

        if(vca) {
            slv->assign(vca);
        }
    }
}

void MixerSnapshot::recall()
{

    if(!_session) {
        return;
    }

    _session->begin_reversible_command(_("mixer-snapshot recall"));

    //vcas
    for(vector<State>::const_iterator i = vca_states.begin(); i != vca_states.end(); i++) {
        if(!get_recall_vcas()) {
            break;
        }

        State state = (*i);

        boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_name(state.name);

        if(!vca) {
           VCAList vl = _session->vca_manager().create_vca(1, state.name);
           boost::shared_ptr<VCA> vca = vl.front();

           if(vca) {
               vca->set_state(state.node, Stateful::loading_state_version);
           }

        } else {
            vca->set_state(state.node, Stateful::loading_state_version);
        }
    }


    //routes
    for(vector<State>::const_iterator i = route_states.begin(); i != route_states.end(); i++) {
        State state = (*i);

        boost::shared_ptr<Route> route = _session->route_by_name(state.name);

        if(route) {
            if(route->is_auditioner() || route->is_master() || route->is_monitor()) {
                /*  we need to special case this but I still
                    want to be able to set some state info here
                    skip... for now */
                continue;
            }
        }

        if(route) {
            PresentationInfo::order_t order = route->presentation_info().order();
            string                    name  = route->name();
            XMLNode&                  node  = sanitize_node(state.node);
            PlaylistDisposition       disp  = CopyPlaylist;

            //we need the route's playlist id before it dissapears
            XMLNode& route_node = route->get_state();
            string playlist_id;
            route_node.get_property (X_("audio-playlist"), playlist_id);

            node.set_property(X_("audio-playlist"), playlist_id);

            _session->remove_route(route);
            route = 0; //explicitly drop reference

            RouteList rl = _session->new_route_from_template(1, order, node, name, disp);
            boost::shared_ptr<Route> route = rl.front();

            if(!route) {
                continue;
            }

            if(get_recall_groups()) {
                XMLNode* group_node = find_named_node(node, X_("Group"));
                if(group_node) {
                    string name;
                    group_node->get_property(X_("name"), name);
                    RouteGroup* rg = _session->route_group_by_name(name);
                    if(!rg) {
                        //this might've been destroyed earlier
                        rg = _session->new_route_group(name);
                    }
                    rg->add(route);
                }
            }

            // this is no longer possible due to using new_from_route_template
            // _session->add_command(new MementoCommand<Route>((*route), &bfr, &route->get_state()));

            reassign_masters(route, node);
        }
    }

    //groups
    for(vector<State>::const_iterator i = group_states.begin(); i != group_states.end(); i++) {
        if(!get_recall_groups()) {
            break;
        }

        State state = (*i);

        RouteGroup* group = _session->route_group_by_name(state.name);

        if(!group) {
            group = _session->new_route_group(state.name);
        }

        if(group) {
            Stateful::ForceIDRegeneration fid;
            
            uint32_t color;
            state.node.get_property(X_("rgba"), color);

            bool gain, mute, solo, recenable, select, route_active, monitoring;
            state.node.get_property(X_("used-to-share-gain"), gain);
            state.node.get_property(X_("mute"), mute);
            state.node.get_property(X_("recenable"), recenable);
            state.node.get_property(X_("select"), select);
            state.node.get_property(X_("route-active"), route_active);
            state.node.get_property(X_("monitoring"), monitoring);
            group->set_gain(gain);
            group->set_mute(mute);
            group->set_solo(solo);
            group->set_recenable(recenable);
            group->set_select(select);
            group->set_route_active(route_active);
            group->set_monitoring(monitoring);
            group->set_color(color);
        }
    }

    _session->commit_reversible_command();
}

void MixerSnapshot::write(const string path)
{
    if(empty()) {
        return;
    }

    XMLNode* node = new XMLNode("MixerSnapshot");
    node->set_property(X_("flags"), _flags);
    node->set_property(X_("favorite"), favorite);
    node->set_property(X_("modified-with"), last_modified_with);
    XMLNode* child;

    child = node->add_child("Routes");
    for(vector<State>::iterator i = route_states.begin(); i != route_states.end(); i++) {
        child->add_child_copy((*i).node);
    }

    child = node->add_child("Groups");
    for(vector<State>::iterator i = group_states.begin(); i != group_states.end(); i++) {
        child->add_child_copy((*i).node);
    }

    child = node->add_child("VCAS");
    for(vector<State>::iterator i = vca_states.begin(); i != vca_states.end(); i++) {
        child->add_child_copy((*i).node);
    }

    XMLTree tree;
    tree.set_root(node);
    tree.write(path);
}

void MixerSnapshot::load(const string path)
{
    clear();

    if(!Glib::file_test(path.c_str(), Glib::FILE_TEST_EXISTS))
        return;

    XMLTree tree;
    tree.read(path);

    XMLNode* root = tree.root();
    if(!root) {
        return;
    }

    string fav;
    root->get_property(X_("flags"), _flags);
    root->get_property(X_("favorite"), fav);
    root->get_property(X_("modified-with"), last_modified_with);
    set_favorite(atoi(fav.c_str()));

    XMLNode* route_node = find_named_node(*root, "Routes");
    XMLNode* group_node = find_named_node(*root, "Groups");
    XMLNode* vca_node   = find_named_node(*root, "VCAS");

    if(route_node) {
        XMLNodeList nlist = route_node->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);

            State state {id, name, (**niter)};
            route_states.push_back(state);
        }
    }

    if(group_node) {
        XMLNodeList nlist = group_node->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);

            State state {id, name, (**niter)};
            group_states.push_back(state);
        }
    }

    if(vca_node) {
        XMLNodeList nlist = vca_node->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);

            State state {id, name, (**niter)};
            vca_states.push_back(state);
        }
    }
}

void MixerSnapshot::load_from_session(string path)
{
    clear();

    vector<string> states;
    if(Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_DIR)) {
        get_state_files_in_directory(path, states);
    }

    if(!states.empty()) {
        load_from_session(states.front());
    }

    //final sanity check
    const string suffix = "." + get_suffix(path);
    if(!(suffix == statefile_suffix)) {
        if(!(suffix == template_suffix)) {
            return;
        }
    }

    XMLTree tree;
    tree.read(path);

    XMLNode* root = tree.root();
    if(!root) {
        return;
    }

    if(root->name() == "Route") {
        //must be a route template
        load_from_route_template(*(root));
        return;
    }

    load_from_session(*(root));
}

void MixerSnapshot::load_from_route_template(XMLNode& node) 
{
    string name, id, group_name;
    node.get_property(X_("name"), name);
    node.get_property(X_("id"), id);
    node.get_property(X_("route-group"), group_name);

    XMLNode* group = node.add_child(X_("Group"));
    if(group) {
        group->set_property(X_("name"), group_name);
    }
    State state {id, name, node};
    route_states.push_back(state);
}

void MixerSnapshot::load_from_session(XMLNode& node)
{
    clear();

    XMLNode* version_node = find_named_node(node, X_("ProgramVersion"));
    XMLNode* route_node   = find_named_node(node, X_("Routes"));
    XMLNode* group_node   = find_named_node(node, X_("RouteGroups"));
    XMLNode* vca_node     = find_named_node(node, X_("VCAManager"));

    if(version_node) {
        string version;
        version_node->get_property(X_("modified-with"), version);
        last_modified_with = version;
    }

    vector<pair<int,string> > number_name_pairs;
    if(vca_node) {
        XMLNodeList nlist = vca_node->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
            string name, number, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("number"), number);
            (*niter)->get_property(X_("id"), id);

            pair<int, string> pair (atoi(number.c_str()), name);
            number_name_pairs.push_back(pair);

            State state {id, name, (**niter)};
            vca_states.push_back(state);
        }
    }

    if(route_node) {
        XMLNodeList nlist = route_node->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);

            /* ugly workaround - recall() expects
            that a route's Slavable children has
            the "name" property. Normal session state
            files don't have this. So we stash it,
            reverse look-up the name based on its number,
            and then  add it to a copy of the node. */

            XMLNode copy (**niter);
            XMLNode* slavable = find_named_node(copy, "Slavable");
            if(slavable) {
                XMLNodeList nlist = slavable->children();
                for(XMLNodeConstIterator siter = nlist.begin(); siter != nlist.end(); siter++) {
                    string number;
                    (*siter)->get_property(X_("number"), number);

                    for(vector<pair<int,string> >::const_iterator p = number_name_pairs.begin(); p != number_name_pairs.end(); p++) {
                        int mst_number  = atoi(number.c_str());
                        int vca_number  = (*p).first;
                        string vca_name = (*p).second;

                        if(vca_number == mst_number) {
                            (*siter)->set_property(X_("name"), vca_name);
                        }
                    }
                }
            }

            State state {id, name, copy};
            route_states.push_back(state);
        }
    }

    if(group_node) {
        XMLNodeList nlist = group_node->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);
            
            /* reverse look-up the routes that belong to this group
             and notify them that they belong to this group name
             just like we do in a normal creation */
             
            string routes;
            if((*niter)->get_property(X_("routes"), routes)) {
                
                stringstream str (routes);
                vector<string> ids;
                split(str.str(), ids, ' ');

                for(vector<string>::iterator i = ids.begin(); i != ids.end(); i++) {
                    for(vector<State>::iterator j = route_states.begin(); j != route_states.end(); j++) {
                        //route state id matches id from vector
                        if((*j).id == (*i)) {
                            XMLNode* group = (*j).node.add_child(X_("Group"));
                            if(group) {
                                group->set_property(X_("name"), name);
                            }
                        }
                    }
                }
            }

            State state {id, name, (**niter)};
            group_states.push_back(state);
        }
    }
}

XMLNode& MixerSnapshot::sanitize_node(XMLNode& node)
{
    if(!get_recall_plugins()) {
        vector<string> types {"lv2", "windows-vst", "lxvst", "mac-vst", "audiounit", "luaproc"};
        for(vector<string>::iterator it = types.begin(); it != types.end(); it++) {
            node.remove_nodes_and_delete(X_("type"), (*it));
        }
    }
    return node;
}