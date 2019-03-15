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
    if(route) {
        string name = route->name();
        XMLNode node (route->get_state());
        vector<string> slaves;

        State state {route->id().to_s(), (string) route->name(), node, slaves};
        route_states.push_back(state);

        //is it in a group?
        string group_name;
        node.get_property(X_("route-group"), group_name);

        RouteGroup* group = _session->route_group_by_name(group_name);

        if(group) {
            XMLNode node (group->get_state());
            State state {group->id().to_s(), group->name(), node, slaves};
            group_states.push_back(state);
        }

        //push back VCA's connected to this route
        VCAList vl = _session->vca_manager().vcas();
        for(VCAList::const_iterator i = vl.begin(); i != vl.end(); i++) {
            
            //only store this particular VCA once
            bool already_exists = false;
            for(vector<State>::iterator s = vca_states.begin(); s != vca_states.end(); s++) {
                if((*s).name == (*i)->name()) {
                    already_exists = !already_exists;
                    break;
                }
            }
            
            if(already_exists)
                continue;

            if(route->slaved_to((*i))) {

                XMLNode node ((*i)->get_state());
                vector<string> slaves;
                
                RouteList rl = _session->get_routelist();
                for(RouteList::iterator r = rl.begin(); r != rl.end(); r++){
                    if((*r)->slaved_to((*i))) {
                        slaves.push_back((*r)->name());
                    }
                }

                State state {(*i)->id().to_s(), (*i)->name(), node, slaves};
                vca_states.push_back(state);
            }
        }
    }
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

void MixerSnapshot::recall() {
    
    //routes
    for(vector<State>::const_iterator i = route_states.begin(); i != route_states.end(); i++) {
        State state = (*i);
        
        boost::shared_ptr<Route> route = _session->route_by_id(PBD::ID(state.id));
        
        if(!route)
            route = _session->route_by_name(state.name);

        if(route)
            route->set_state(state.node, PBD::Stateful::loading_state_version);
    }

    //groups
    for(vector<State>::const_iterator i = group_states.begin(); i != group_states.end(); i++) {
        State state = (*i);

        RouteGroup* group = _session->route_group_by_name(state.name);

        if(!group) {
            group = new RouteGroup(*_session, state.name);
            //notify session
            _session->add_route_group(group);
        }

        if(group) {
            group->set_state(state.node, PBD::Stateful::loading_state_version);
            group->changed();
        }
    }

    //vcas
    for(vector<State>::const_iterator i = vca_states.begin(); i != vca_states.end(); i++) {
        State state = (*i);

        boost::shared_ptr<VCA> vca = _session->vca_manager().vca_by_name(state.name);

        if(!vca) {
           VCAList vl = _session->vca_manager().create_vca(1, state.name);
           boost::shared_ptr<VCA> vca = vl.front();

           if(vca) {
               vca->set_state(state.node, PBD::Stateful::loading_state_version);
               for(vector<string>::const_iterator s = state.slaves.begin(); s != state.slaves.end(); s++) {
                   boost::shared_ptr<Route> route = _session->route_by_name((*s));
                   if(route) {route->assign(vca);}
                   continue;
               }
           }
        } else {
            vca->set_state(state.node, PBD::Stateful::loading_state_version);
            for(vector<string>::const_iterator s = state.slaves.begin(); s != state.slaves.end(); s++) {
                boost::shared_ptr<Route> route = _session->route_by_name((*s));
                if(route) {route->assign(vca);}
            }
        }
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
        XMLNode* ch;
        ch = find_named_node((*i).node, "Slaves");

        if(!ch)
            ch = (*i).node.add_child_copy(XMLNode("Slaves"));
        else
            ch->remove_nodes("Slave");
        
        for(vector<string>::const_iterator sl = (*i).slaves.begin(); sl != (*i).slaves.end(); sl++) {
            XMLNode n ("Slave");
            n.set_property(X_("name"), *(sl));
            ch->add_child_copy(n);
        }
        
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
        XMLNodeList nlist;
        XMLNodeConstIterator niter;
        nlist = route_node->children();
        
        for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
            
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);
            
            vector<string> slaves;
            
            State state {id, name, **niter, slaves};
            route_states.push_back(state);
        }
    }

    if(group_node) {
        XMLNodeList nlist;
        XMLNodeConstIterator niter;
        nlist = group_node->children();
        
        for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
            
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);

            vector<string> slaves;
            
            State state {id, name, **niter, slaves};
            group_states.push_back(state);
        }
    }

    if(vca_node) {
        XMLNodeList nlist;
        XMLNodeConstIterator niter;
        nlist = vca_node->children();
        
        for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
            
            string name, id;
            (*niter)->get_property(X_("name"), name);
            (*niter)->get_property(X_("id"), id);
            
            vector<string> slaves;
            
            XMLNodeList slist;
            XMLNodeConstIterator siter;
            XMLNode* slave_node = find_named_node((**niter), "Slaves");
            if(slave_node) {
                slist = slave_node->children();
                for(siter = slist.begin(); siter != slist.end(); siter++) {
                    string sname;
                    (*siter)->get_property(X_("name"), sname);
                    slaves.push_back(sname);
                }
            }
            
            State state {id, name, **niter, slaves};
            vca_states.push_back(state);
        }
    }
}