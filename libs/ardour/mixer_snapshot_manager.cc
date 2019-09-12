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

#include <stdio.h>
#include <glibmm.h>
#include <glibmm/miscutils.h>

#include "ardour/directory_names.h"
#include "ardour/filename_extensions.h"
#include "ardour/filesystem_paths.h"
#include "ardour/mixer_snapshot_manager.h"
#include "ardour/search_paths.h"
#include "ardour/session_directory.h"

#include "pbd/basename.h"
#include "pbd/file_utils.h"
#include "pbd/gstdio_compat.h"

using namespace ARDOUR;
using namespace PBD;
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

void MixerSnapshotManager::ensure_snapshot_dir(bool global)
{
    const string path = global ? _global_path : _local_path;
    if(!Glib::file_test(path.c_str(), Glib::FILE_TEST_EXISTS & Glib::FILE_TEST_IS_DIR)) {
        ::g_mkdir(path.c_str(), 0775);
    }
}

void MixerSnapshotManager::find_templates(vector<TemplateInfo>& template_info, bool global)
{
    if(!_session) {
        return;
    }

    if(!global) {
        Searchpath searchpath (_session->session_directory().root_path());
        searchpath.add_subdirectory_to_paths(route_templates_dir_name);

        vector<string> files;
        const string pattern = "*" + string(template_suffix);
        find_files_matching_pattern(files, searchpath, pattern);

        if(!files.empty()) {
            for(vector<string>::const_iterator it = files.begin(); it != files.end(); it++) {
                const string path = (*it);
                const string name = basename_nosuffix(path);

                MixerSnapshot* snapshot = new MixerSnapshot(_session, path);
                const string description = snapshot->get_description();
                const string modified    = snapshot->get_last_modified_with();

                TemplateInfo info {name, path, description, modified};
                template_info.push_back(info);
                delete snapshot;
            }
        }
    } else if(global) {
        find_route_templates(template_info);
    }
}

void MixerSnapshotManager::refresh()
{
    clear();

    vector<TemplateInfo> global_templates, local_templates;
    find_templates(global_templates, true);
    find_templates(local_templates, false);

    vector<TemplateInfo>::const_iterator it;
    if(!global_templates.empty()) {
        for(it = global_templates.begin(); it != global_templates.end(); it++) {
            TemplateInfo info = (*it);

            MixerSnapshot* snap = new MixerSnapshot(_session, info.path);
            _global_snapshots.insert(snap);
        }
    }

    if(!local_templates.empty()) {
        for(it = local_templates.begin(); it != local_templates.end(); it++) {
            TemplateInfo info = (*it);

            MixerSnapshot* snap = new MixerSnapshot(_session, info.path);
            _local_snapshots.insert(snap);
        }
    }
}

bool MixerSnapshotManager::erase(MixerSnapshot* snapshot) {
    if(!snapshot) {
        return false;
    }

    set<MixerSnapshot*>::const_iterator iter;

    iter = _global_snapshots.find(snapshot);
    if(iter != _global_snapshots.end()) {
        delete (*iter);
        _global_snapshots.erase(iter);
        return true;
    }

    iter = _local_snapshots.find(snapshot);
    if(iter != _local_snapshots.end()) {
        delete (*iter);
        _local_snapshots.erase(iter);
        return true;
    }
    return false;
}

bool MixerSnapshotManager::move(MixerSnapshot* snapshot, const string& to_path) {
    if(!snapshot) {
        return false;
    }

    const string path = snapshot->get_path();
    if(Glib::file_test(path.c_str(), Glib::FILE_TEST_EXISTS)) {
        const string dir = Glib::path_get_dirname(path);

        const string file     = snapshot->get_label() + string(template_suffix);
        const string new_path = Glib::build_filename(to_path, file);

        //already there
        if (Glib::file_test(new_path.c_str(), Glib::FILE_TEST_EXISTS)) {
            return false;
        }

        //local snapshots have no description
        if(to_path == _local_path) {
            snapshot->set_description("");
        }
        //write this to the new path
        snapshot->write(to_path);

        snapshot->set_path(new_path);
        return true;
    }
    return false;
}

bool MixerSnapshotManager::promote(MixerSnapshot* snapshot) {
    if(!snapshot) {
        return false;
    }

    //build the new file path
    const string file = snapshot->get_label() + string(template_suffix);
    const string path = Glib::build_filename(_global_path, file);

    //write the snapshot to the new path, and erase the old one
    if(move(snapshot, _global_path) && erase(snapshot)) {
        //insert the new snapshot
        _global_snapshots.insert(new MixerSnapshot(_session, path));
        PromotedSnapshot(*_global_snapshots.end()); /* EMIT SIGNAL */
        return true;
    }

    return false;
}

bool MixerSnapshotManager::demote(MixerSnapshot* snapshot) {
    if(!snapshot) {
        return false;
    }

    //build the new file path
    const string file = snapshot->get_label() + string(template_suffix);
    const string path = Glib::build_filename(_local_path, file);

    //write the snapshot to the new path, and erase the old one
    if(move(snapshot, _local_path) && erase(snapshot)) {
        //insert the new snapshot
        _local_snapshots.insert(new MixerSnapshot(_session, path));
        return true;
    }

    return false;
}

bool MixerSnapshotManager::rename(MixerSnapshot* snapshot, const string& new_name) {
    if(!snapshot) {
        return false;
    }

    if(new_name.empty()) {
        return false;
    }

    snapshot->set_label(new_name);

    const string path = snapshot->get_path();
    const string dir = Glib::path_get_dirname(path);

    if (move(snapshot, dir)) {
        RenamedSnapshot(); /* EMIT SIGNAL */
        ::g_remove(path.c_str());
        //remove the old file
        return true;
    }

    return false;
}

bool MixerSnapshotManager::remove(MixerSnapshot* snapshot) {
    if(!snapshot) {
        return false;
    }

    const string path = snapshot->get_path();
    if (Glib::file_test(path.c_str(), Glib::FILE_TEST_EXISTS)) {
        if(erase(snapshot)) {
            ::g_remove(path.c_str());
            RemovedSnapshot(); /* EMIT SIGNAL */
            return true;
        }
    }

    return false;
}

MixerSnapshot* MixerSnapshotManager::get_snapshot_by_name(const string& name, bool global)
{
    set<MixerSnapshot*> snapshots_list = global ? _global_snapshots : _local_snapshots;
    MixerSnapshot* snapshot;

    set<MixerSnapshot*>::iterator it;
    for(it = snapshots_list.begin(); it != snapshots_list.end(); it++) {
        if((*it)->get_label() == name) {
            snapshot = (*it);
            break;
        }
    }
    return snapshot;
}

void MixerSnapshotManager::create_snapshot(const string& label, const string& desc, RouteList& rl, bool global)
{
    ensure_snapshot_dir(global);
    const string path = global ? _global_path : _local_path;
    MixerSnapshot* snapshot = new MixerSnapshot(_session);

    snapshot->set_description(desc);

    if(!rl.empty()) {
        //just this routelist
        snapshot->snap(rl);
    } else {
        //the whole session
        snapshot->snap();
    }

    //is this even possible? either way, sanity check
    if(snapshot->empty()) {
        delete snapshot;
        return;
    }

    snapshot->set_label(label);
    snapshot->write(path);

    const string full_path = Glib::build_filename(path, snapshot->get_label() + string(template_suffix));
    snapshot->set_path(full_path);

    MixerSnapshot* old_snapshot = get_snapshot_by_name(snapshot->get_label(), global);
    set<MixerSnapshot*>& snapshots_list = global ? _global_snapshots : _local_snapshots;
    set<MixerSnapshot*>::iterator iter = snapshots_list.find(old_snapshot);

    //remove it from it's set
    if(iter != snapshots_list.end()) {
        delete (*iter);
        snapshots_list.erase(iter);
    }
    //and insert the new one
    snapshots_list.insert(snapshot);
    CreatedSnapshot(snapshot); /* EMIT SIGNAL */
}

void MixerSnapshotManager::create_snapshot(const string& label, const string& desc, const string& from_path, bool global)
{
    ensure_snapshot_dir(global);
    const string path = global ? _global_path : _local_path;
    MixerSnapshot* snapshot = new MixerSnapshot(_session, from_path);

    //clearly from_path doesn't point to a parsable state file
    if(snapshot->empty()) {
        delete snapshot;
        return;
    }

    snapshot->set_label(label);
    snapshot->write(path);

    const string full_path = Glib::build_filename(path, snapshot->get_label() + string(template_suffix));
    snapshot->set_path(full_path);

    MixerSnapshot* old_snapshot = get_snapshot_by_name(snapshot->get_label(), global);
    set<MixerSnapshot*>& snapshots_list = global ? _global_snapshots : _local_snapshots;
    set<MixerSnapshot*>::iterator iter = snapshots_list.find(old_snapshot);

    //remove it from it's set
    if(iter != snapshots_list.end()) {
        delete (*iter);
        snapshots_list.erase(iter);
    }
    //and insert the new one
    snapshots_list.insert(snapshot);
    CreatedSnapshot(snapshot); /* EMIT SIGNAL */
}