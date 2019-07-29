/*
    Copyright (C) 2000-2019 Paul Davis

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
#include <vector>

#include <stdio.h>
#include <glibmm.h>

#include "ardour/directory_names.h"
#include "ardour/mixer_snapshot_manager.h"
#include "ardour/mixer_snapshot.h"
#include "ardour/session_directory.h"
#include "ardour/template_utils.h"

#include "pbd/basename.h"

using namespace ARDOUR;
using namespace std;

MixerSnapshotManager::MixerSnapshotManager (Session* s)
{
    if(!s) {
        throw failed_constructor();
    }
    _session = s;

    _global_path = user_route_template_directory();
    _local_path  = Glib::build_filename(_session->session_directory().root_path(), route_templates_dir_name);

    refresh();
}

void MixerSnapshotManager::refresh()
{
    clear();
    vector<TemplateInfo> global_templates;
    find_route_templates(global_templates);

    for(vector<TemplateInfo>::const_iterator it = global_templates.begin(); it != global_templates.end(); it++) {
        TemplateInfo info = (*it);

        MixerSnapshot* snap = new MixerSnapshot(_session, info.path);
        snap->set_label(info.name);
        _global_snapshots.insert(snap);
        
        printf("Snapshot name: %s\n", snap->get_label().c_str());
        printf("Path: %s\n", info.path.c_str());
    }

    //TODO: the local part of this discovery
}

void MixerSnapshotManager::create_snapshot(std::string const& label, RouteList& rl, bool global)
{
    const string path = global?_global_path:_local_path;
    MixerSnapshot* snapshot = new MixerSnapshot(_session);
    snapshot->snap(rl);
    snapshot->set_label(label);
    snapshot->write(path);

    if(global) {
        _global_snapshots.insert(snapshot);
    } else {
        _local_snapshots.insert(snapshot);
    }
}