#include <iostream>
#include <ctime>

#include "ardour/mixer_snapshot.h"
#include "ardour/audioengine.h"
#include "ardour/route_group.h"
#include "ardour/vca_manager.h"
#include "ardour/vca.h"

#include "pbd/stateful.h"
#include "pbd/id.h"
#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;

MixerSnapshot::MixerSnapshot(Session* s)
    : id(0)
    , label("")
    , timestamp(time(0))
{
    cout << s->name() << endl;
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

// void MixerSnapshot::snap(Route* route) 
// {
//     clear();

//     if(route) {
//         string name = route->name();
//         XMLNode previous_state (route->get_state());

//         string group_name;
//         get_property(X_("route-group"), group_name);
        
//         State state {name, previous_state};
//         states.push_back(state);
//         cout << timestamp << " " << state.name << endl;
//     }
//     return;
// }

void MixerSnapshot::snap() 
{
    clear();

    if(!_session)
        return;
    
    RouteList rl = _session->get_routelist();

    if(rl.empty())
        return;
    // for(RouteList::iterator i = rl.begin(); i != rl.end(); i++) {
    //     string route_name = (*i)->name();
        
    //     XMLNode& current_state  = (*i)->get_state();
    //     XMLNode previous_state (current_state);

    //     string group_name = "";
    //     current_state.get_property(X_("route-group"), group_name);

    //     RouteGroup* group = _session->route_group_by_name(group_name);

    //     XMLNode group_state = XMLNode("");
    //     if(group) {
    //         group_state = XMLNode(group->get_state());
    //     }
        
    //     State state {(*i)->id(), route_name, group_name, "", previous_state, group_state, XMLNode("")};
    //     states.push_back(state);
    //     cout << timestamp << ": " << state.route_name << endl;
    // }

    for(RouteList::const_iterator i = rl.begin(); i != rl.end(); i++) {
        //copy current state to node obj
        XMLNode node ((*i)->get_state());
        
        State state {(*i)->id(), (string) (*i)->name(), node};
        route_states.push_back(state);
    }

    //push back groups
    list<RouteGroup*> gl = _session->route_groups();
    for(list<RouteGroup*>::const_iterator i = gl.begin(); i != gl.end(); i++) {
        //copy current state to node obj
        XMLNode node ((*i)->get_state());

        State state {(*i)->id(), (string) (*i)->name(), node};
        group_states.push_back(state);
    }
    
    //push back VCA's
    VCAList _vcas = _session->vca_manager().vcas();
    for(VCAList::const_iterator i = _vcas.begin(); i != _vcas.end(); i++) {
        cout << (*i)->name() << endl;
    }

    return;
}

void MixerSnapshot::recall() {
    
    for(vector<State>::const_iterator i = route_states.begin(); i != route_states.end(); i++) {
        State state = (*i);
        
        boost::shared_ptr<Route> route = _session->route_by_id(state.id);
        
        if(!route)
            route = _session->route_by_name(state.name);

        if(route)
            route->set_state(state.node, PBD::Stateful::loading_state_version);
        else
            cout << "couldn't find " << state.name << " in session ... we won't be creating this." << endl;

        // //establish group association if applicable
        // if((*i).group_name != "") {
        //     RouteGroup* group = _session->route_group_by_name((*i).group_name);
            
        //     if(!group) {
        //         group = new RouteGroup(*_session, (*i).group_name);
        //         _session->add_route_group(group);
        //     }

        //     if(group) {
        //         group->set_state((*i).group_state, PBD::Stateful::loading_state_version);
        //         group->changed();
        //     }
        // }
    }

    for(vector<State>::const_iterator i = group_states.begin(); i != group_states.end(); i++) {
        State state = (*i);

        RouteGroup* group = _session->route_group_by_name(state.name);

        if(!group){
            group = new RouteGroup(*_session, state.name);
            //notify session
            _session->add_route_group(group);
        }

        if(group) {
            group->set_state(state.node, PBD::Stateful::loading_state_version);
            group->changed()
        } else {
            cout << "not found" << endl;
        }
    }
    return;
}