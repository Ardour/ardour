#include <iostream>
#include <ctime>

#include "ardour/mixer_snapshot.h"
#include "ardour/audioengine.h"

#include "pbd/stateful.h"

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
    states.clear();
}

// void MixerSnapshot::snap(Route* route) 
// {
//     clear();

//     if(route) {
//         string name = route->name();
//         XMLNode previous_state (route->get_state());
        
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
    for(RouteList::iterator i = rl.begin(); i != rl.end(); i++) {
        string name = (*i)->name();
        
        XMLNode& current_state  = (*i)->get_state();
        XMLNode previous_state (current_state);
        
        State state {name, previous_state};
        states.push_back(state);
        cout << timestamp << ": " << state.name << endl;
    }
    return;
}

void MixerSnapshot::recall() {
    for(vector<State>::const_iterator i = states.begin(); i != states.end(); i++) {
        string name = (*i).name;
        boost::shared_ptr<Route> route = _session->route_by_name(name);
        
        if(route) {
            route->set_state((*i).node, PBD::Stateful::loading_state_version);
        } else {
            cout << "couldn't find " << name << " in session" << endl;
            continue;
        }
        cout << timestamp << ": " << name << endl;
    }
    return;
}