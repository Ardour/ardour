#include <iostream>

#include "ardour/mixer_snapshot.h"
#include "ardour/route_group.h"
#include "ardour/vca_manager.h"
#include "ardour/session_state.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/session_state_utils.h"

#include "pbd/i18n.h"
#include "pbd/xml++.h"
#include "pbd/memento_command.h"
#include "pbd/file_utils.h"

#include <glibmm.h>
#include <glibmm/fileutils.h>

using namespace std;
using namespace ARDOUR;

MixerSnapshot::MixerSnapshot(Session* s)
    : id(0)
    , label("snapshot")
    , timestamp(time(0))
{   
    if(s)
        _session = s;
}

MixerSnapshot::MixerSnapshot(Session* s, string file_path)
    : id(0)
    , label("snapshot")
    , timestamp(time(0))
{   
    if(s)
        _session = s;

    load_from_session(file_path);
}

MixerSnapshot::~MixerSnapshot()
{
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
    if(!route)
        return;
    
    string name = route->name();
    XMLNode& original = route->get_state();
    XMLNode copy (original);

    RouteGroup* group = route->route_group();
    if(group) {
        snap(group);
    }

    XMLNode* slavable = find_named_node(copy, "Slavable");
    
    if(slavable)
    {
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

    State state {
        route->id().to_s(), 
        route->name(), 
        copy
    };

    route_states.push_back(state);
}

void MixerSnapshot::snap(RouteGroup* group)
{
    if(!group)
        return;

    for(vector<State>::const_iterator it = group_states.begin(); it != group_states.end(); it++)
        if((it)->name == group->name())
            return;

    string name = group->name();
    XMLNode& original = group->get_state();
    XMLNode  copy (original);
    
    State state {
        group->id().to_s(), 
        group->name(), 
        copy
    };

    group_states.push_back(state);
}

void MixerSnapshot::snap(boost::shared_ptr<VCA> vca)
{
    if(!vca)
        return;

    for(vector<State>::const_iterator it = vca_states.begin(); it != vca_states.end(); it++)
        if((it)->name == vca->name())
            return;

    string name = vca->name();
    XMLNode& original = vca->get_state();
    XMLNode  copy (original);
    
    State state {
        vca->id().to_s(), 
        vca->name(), 
        copy
    };

    vca_states.push_back(state);
}


void MixerSnapshot::snap() 
{
    if(!_session)
        return;

    clear();
    
    RouteList rl = _session->get_routelist();
    if(rl.empty())
        return;

    for(RouteList::const_iterator it = rl.begin(); it != rl.end(); it++)
        snap((*it));
}

void MixerSnapshot::snap(RouteList rl) 
{
    if(!_session)
        return;

    clear();

    if(rl.empty())
        return;

    for(RouteList::const_iterator it = rl.begin(); it != rl.end(); it++)
        snap((*it));
}

void MixerSnapshot::reassign_masters(boost::shared_ptr<Slavable> slv, XMLNode node)
{
    if(!slv)
        return;

    XMLNode* slavable = find_named_node(node, "Slavable");

    if(!slavable)
        return;

    XMLNodeList nlist = slavable->children();

    for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
        string name;
        (*niter)->get_property(X_("name"), name);

        boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_name(name);
        
        if(vca)
            slv->assign(vca);
    }
}

void MixerSnapshot::recall()
{
    _session->begin_reversible_command(_("mixer-snapshot recall"));
    
    //vcas
    for(vector<State>::const_iterator i = vca_states.begin(); i != vca_states.end(); i++) {
        State state = (*i);

        boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_name(state.name);

        if(!vca) {
           VCAList vl = _session->vca_manager().create_vca(1, state.name);
           boost::shared_ptr<VCA> vca = vl.front();

           if(vca)
               vca->set_state(state.node, PBD::Stateful::loading_state_version);

        } else {
            vca->set_state(state.node, PBD::Stateful::loading_state_version);
        }
    }
    
    //routes
    for(vector<State>::const_iterator i = route_states.begin(); i != route_states.end(); i++) {
        State state = (*i);
        
        boost::shared_ptr<Route> route = _session->route_by_id(PBD::ID(state.id));
        
        if(!route)
            route = _session->route_by_name(state.name);

        if(route) {
            XMLNode& bfr = route->get_state();
            route->set_state(state.node, PBD::Stateful::loading_state_version);
            reassign_masters(route, state.node);
            _session->add_command(new MementoCommand<Route>((*route), &bfr, &route->get_state()));
        }
    }

    //groups
    for(vector<State>::const_iterator i = group_states.begin(); i != group_states.end(); i++) {
        State state = (*i);

        RouteGroup* group = _session->route_group_by_name(state.name);

        if(!group)
            group = _session->new_route_group(state.name);

        if(group)
            group->set_state(state.node, PBD::Stateful::loading_state_version);
    }

    _session->commit_reversible_command();
}

void MixerSnapshot::write()
{
    //_session->mixer_settings_dir()
    
    XMLNode* node = new XMLNode("MixerSnapshot");
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

    string snap = Glib::build_filename(user_config_directory(-1), label + ".xml");
    XMLTree tree;
	tree.set_root(node);
    tree.write(snap.c_str());
}

void MixerSnapshot::load(string path)
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
    if(Glib::file_test(path.c_str(), Glib::FILE_TEST_IS_DIR))
        get_state_files_in_directory(path, states);

    if(!states.empty())
        load_from_session(states.front());

    //final sanity check
    if(!("." + PBD::get_suffix(path) == statefile_suffix))
        return;

    XMLTree tree;
    tree.read(path);

    XMLNode* root = tree.root();
    if(!root) {
        return;
    }
}

void MixerSnapshot::load_from_session(XMLNode& node)
{
    clear();

    XMLNode* route_node = find_named_node(node, "Routes");
    XMLNode* group_node = find_named_node(node, "RouteGroups");
    XMLNode* vca_node   = find_named_node(node, "VCAManager");

    vector<pair<int,string>> number_name_pairs;
    if(vca_node) {
        XMLNodeList nlist = vca_node->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); niter++) {
            string name, number, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("number"), number);
            (*niter)->get_property(X_("id"), id);

            pair<int, string> pair (atoi(number.c_str()), name) ;
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
            reverse look-up the "number", and then 
            add it to a copy of the node. */
            XMLNode copy (**niter);
            XMLNode* slavable = find_named_node(copy, "Slavable");
            if(slavable) {
                XMLNodeList nlist = slavable->children();
                for(XMLNodeConstIterator siter = nlist.begin(); siter != nlist.end(); siter++) {
                    string number;
                    (*siter)->get_property(X_("number"), number);

                    for(vector<pair<int,string>>::const_iterator p = number_name_pairs.begin(); p != number_name_pairs.end(); p++) {
                        if((*p).first == atoi(number.c_str()))
                            (*siter)->set_property(X_("name"), (*p).second);
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
            
            State state {id, name, (**niter)};
            group_states.push_back(state);
        }
    }
}