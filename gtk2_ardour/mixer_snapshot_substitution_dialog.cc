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
#include <cstdio>

#include <gtkmm/comboboxtext.h>
#include <gtkmm/table.h>
#include <gtkmm/stock.h>

#include "mixer_snapshot_substitution_dialog.h"

#include "ardour/mixer_snapshot.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace PBD;
using namespace std;

MixerSnapshotSubstitutionDialog::MixerSnapshotSubstitutionDialog(MixerSnapshot* snapshot)
    : ArdourDialog (_("Substitutions"), true)
    , _snapshot(snapshot)
{
    RouteList rl = _snapshot->get_session()->get_routelist();

    Table* table = manage(new Table(rl.size(), 1));

    int n = 0;
    Label* dst = manage(new Label(_("Destination: "), ALIGN_CENTER, ALIGN_CENTER));
    Label* src = manage(new Label(_("Source: "),      ALIGN_CENTER, ALIGN_CENTER));
    table->attach(*dst, 0, 1, n, n+1);
    table->attach(*src, 1, 2, n, n+1);
    n++;


    for(RouteList::const_iterator it = rl.begin(); it != rl.end(); it++) {
        boost::shared_ptr<Route> route = (*it);

        if(route->is_monitor() || route->is_master() || route->is_auditioner()) {
            //skip for now
            continue;
        }

        if(route) {
            ComboBoxText* combo = manage(new ComboBoxText());
            fill_combo_box(combo, route->name());

            Label* l = manage(new Label(route->name(), ALIGN_LEFT, ALIGN_CENTER, false));
            table->attach(*l,     0, 1, n, n+1);
            table->attach(*combo, 1, 2, n, n+1);

            substitutions.push_back(route_combo(route, combo));
        }
        n++;
    }

    ComboBoxText* sel_combo = manage(new ComboBoxText());
    Label* sel = manage(new Label(_("All Selected: ")));
    fill_combo_box(sel_combo, "");
    table->attach(*sel,       0, 1, n, n+1);
    table->attach(*sel_combo, 1, 2, n, n+1);
    n++;


    add_button (Stock::CANCEL, RESPONSE_CANCEL);
    add_button (Stock::APPLY, RESPONSE_ACCEPT);
    set_default_response(RESPONSE_ACCEPT);

    get_vbox()->pack_start(*table, true, true);
}

void MixerSnapshotSubstitutionDialog::fill_combo_box(ComboBoxText* box, const string route_name)
{
    box->append(" --- ");
    box->set_active_text(" --- ");

    vector<MixerSnapshot::State> routes = _snapshot->get_routes();

    for(vector<MixerSnapshot::State>::iterator i = routes.begin(); i != routes.end(); i++) {
        string state_name = (*i).name;

        box->append(state_name);
        if(state_name == route_name) {
            box->set_active_text(state_name);
        }
    }
}

bool MixerSnapshotSubstitutionDialog::state_exists(const string name)
{
    vector<MixerSnapshot::State> routes = _snapshot->get_routes();

    for(vector<MixerSnapshot::State>::iterator i = routes.begin(); i != routes.end(); i++) {
        if((*i).name == name) {
            return true;
        }
    }
    return false;
}

MixerSnapshot::State MixerSnapshotSubstitutionDialog::get_state_by_name(const string name) {
    vector<MixerSnapshot::State> routes = _snapshot->get_routes();
    for(vector<MixerSnapshot::State>::iterator i = routes.begin(); i != routes.end(); i++) {
        if((*i).name == name) {
            return (*i);
        }
    }
}
