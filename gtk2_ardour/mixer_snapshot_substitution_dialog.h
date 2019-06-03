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

#ifndef __ardour_mixer_snapshot_substitution_dialog_h__
#define __ardour_mixer_snapshot_substitution_dialog_h__

#include <string>
#include <vector>
#include <cstdio>

#include "ardour_dialog.h"

#include "ardour/mixer_snapshot.h"

typedef std::pair<boost::shared_ptr<ARDOUR::Route>, Gtk::ComboBoxText*> route_combo;

class MixerSnapshotSubstitutionDialog : public ArdourDialog
{
public:
    MixerSnapshotSubstitutionDialog(ARDOUR::MixerSnapshot*);
private:
    bool state_exists(const std::string);
    ARDOUR::MixerSnapshot::State get_state_by_name(const std::string);
    void fill_combo_box(Gtk::ComboBoxText*, const std::string);
    void on_response(int);

    std::vector<route_combo> substitutions;

    ARDOUR::MixerSnapshot* _snapshot;
};

#endif /* __ardour_mixer_snapshot_substitution_dialog_h__ */