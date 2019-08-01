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
#ifndef __ardour_mixer_snapshot_manager_h__
#define __ardour_mixer_snapshot_manager_h__

#include <string>
#include <set>

#include <boost/utility.hpp>

#include "ardour/mixer_snapshot.h"
#include "ardour/session.h"

namespace ARDOUR {
typedef std::set<MixerSnapshot*> SnapshotList;

class LIBARDOUR_API MixerSnapshotManager : public boost::noncopyable
{
public:

    MixerSnapshotManager (ARDOUR::Session*);
    ~MixerSnapshotManager() {}

    void create_snapshot(std::string const& label, RouteList& rl, bool global);
    void create_snapshot(std::string const& label, std::string const& from_path, bool global);

    bool rename_snapshot(MixerSnapshot*, const std::string&);
    bool remove_snapshot(MixerSnapshot*);

    bool promote(ARDOUR::MixerSnapshot*);
    bool demote(ARDOUR::MixerSnapshot*);

    MixerSnapshot* get_snapshot_by_name(const std::string&, bool);

    std::string get_global_path() {return _global_path;}
    std::string get_local_path() {return _local_path;}
    
    ARDOUR::SnapshotList get_global_snapshots() {return _global_snapshots;}
    ARDOUR::SnapshotList get_local_snapshots() {return _local_snapshots;}

    void refresh();
    void clear() { _global_snapshots.clear(); _local_snapshots.clear(); };
private:
    void ensure_snapshot_dir(bool global);
    std::string _global_path;
    std::string _local_path;

    ARDOUR::SnapshotList _global_snapshots;
    ARDOUR::SnapshotList _local_snapshots;

    ARDOUR::Session* _session;
};
} /* namespace ARDOUR */

#endif /* __ardour_mixer_snapshot_manager_h__ */