/*
    Copyright (C) 20XX Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_mixer_snapshot_h__
#define __ardour_mixer_snapshot_h__

#include <utility>
#include <vector>
#include <ctime>

#include "pbd/stateful.h"
#include "pbd/xml++.h"
#include "pbd/id.h"

#include "ardour/session.h"
#include "ardour/route.h"

class MixerSnapshot //: public PBD::Stateful
{
    public:
        MixerSnapshot(ARDOUR::Session*);
        ~MixerSnapshot();

        // void snap(ARDOUR::Route*);
        void snap();
        void recall();

        int id;
        char label[255];
        std::time_t timestamp;

    private:
        ARDOUR::Session* _session;

        void clear();

        struct State {
            PBD::ID     id;
            std::string name;
            XMLNode     node;
        };

        std::vector<State> route_states;
        std::vector<State> group_states;
        std::vector<State> vca_states;
};

#endif /* __ardour_mixer_snapshot_h__ */