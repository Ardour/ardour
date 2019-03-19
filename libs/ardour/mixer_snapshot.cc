#include <iostream>
#include <ctime>

#include "ardour/mixer_snapshot.h"
#include "ardour/audioengine.h"
#include "ardour/route_group.h"
#include "ardour/vca_manager.h"
#include "ardour/vca.h"
#include "ardour/session_directory.h"
#include "ardour/filesystem_paths.h"

#include "pbd/stateful.h"
#include "pbd/id.h"
#include "pbd/i18n.h"
#include "pbd/xml++.h"

#include <glib.h>
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
        XMLNodeList nlist;
        XMLNodeConstIterator niter;
        nlist = slavable->children();

        for(niter = nlist.begin(); niter != nlist.end(); ++niter) {
            string number;
            (*niter)->get_property(X_("number"), number);
            
            int i = atoi(number.c_str());
            boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_number(i);

            if(vca) {
                //we will need this later recollection
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
    cout << "capturing " << "group " << group->name() << endl;
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
    cout << "capturing " << "vca " << vca->name() << endl;
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

void MixerSnapshot::reassign_masters(boost::shared_ptr<Route> slv, XMLNode node)
{
    if(!slv)
        return;

    XMLNode* slavable = find_named_node(node, "Slavable");

    if(!slavable)
        return;

    XMLNodeList nlist = slavable->children();

    for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
        string name;
        (*niter)->get_property(X_("name"), name);

        boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_name(name);
        
        if(vca)
            slv->assign(vca);
    }
}

void MixerSnapshot::recall() 
{    
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
            route->set_state(state.node, PBD::Stateful::loading_state_version);
            reassign_masters(route, state.node);
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
}

void MixerSnapshot::write()
{
    XMLNode* node = new XMLNode("MixerSnapshot");
	XMLNode* child;

    child = node->add_child ("Routes");
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

void MixerSnapshot::load()
{
    clear();

    string snap = Glib::build_filename(user_config_directory(-1), label + ".xml");
    XMLTree tree;
    tree.read(snap);
   
    XMLNode* root = tree.root();
    if(!root) {
        return;
    }

    XMLNode* route_node = find_named_node(*root, "Routes");
    XMLNode* group_node = find_named_node(*root, "Groups");
    XMLNode* vca_node   = find_named_node(*root, "VCAS");

    if(route_node) {
        XMLNodeList nlist = route_node->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);
            
            State state {id, name, (**niter)};
            route_states.push_back(state);
        }
    }

    if(group_node) {
        XMLNodeList nlist = group_node->children();       
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);
            
            State state {id, name, (**niter)};
            group_states.push_back(state);
        }
    }

    if(vca_node) {
        XMLNodeList nlist = vca_node->children();
        for(XMLNodeConstIterator niter = nlist.begin(); niter != nlist.end(); ++niter) {
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);
            
            State state {id, name, (**niter)};
            vca_states.push_back(state);
        }
    }
}