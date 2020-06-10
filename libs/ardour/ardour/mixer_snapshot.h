/*
    Copyright (C) 2020 Nikolaus Gullotta

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

#ifndef __ardour_mixer_snapshot_h__
#define __ardour_mixer_snapshot_h__

#include <vector>
#include <ctime>

#include "ardour/session.h"
#include "ardour/route.h"
#include "ardour/vca.h"
#include "ardour/route_group.h"

#include "pbd/signals.h"

namespace ARDOUR {

class LIBARDOUR_API MixerSnapshot
{
    public:
        enum RecallFlags {
            RecallEQ = 0x1,
            RecallSends = 0x2,
            RecallComp = 0x4,
            RecallPan = 0x8,
            RecallPlugs = 0x10,
            RecallGroups = 0x20,
            RecallVCAs = 0x40
        };

        MixerSnapshot();
        MixerSnapshot(const std::string&);

        void snap();
        void snap(ARDOUR::RouteList);
        void snap(ARDOUR::RouteGroup*);
        void snap(boost::shared_ptr<ARDOUR::VCA>);
        void snap(boost::shared_ptr<ARDOUR::Route>);
        void recall(bool make_tracks = false);
        void clear();
        void write(const std::string);
        bool has_specials();

        ARDOUR::Session* get_session() {return _session;};

        struct State {
            std::string id;
            std::string name;
            XMLNode     node;
        };

        bool empty() {
            return (
                routes.empty() &&
                groups.empty() &&
                vcas.empty()
            );
        };

        MixerSnapshot::State get_route_state_by_name(const std::string&);
        bool route_state_exists(const std::string&);

        std::vector<State> get_routes() {return routes;};
        std::vector<State> get_groups() {return groups;};
        std::vector<State> get_vcas()   {return vcas;};
#ifdef MIXBUS
        bool get_recall_eq()    const { return _flags & RecallEQ;};
        bool get_recall_sends() const { return _flags & RecallSends;};
        bool get_recall_comp()  const { return _flags & RecallComp;};
#endif
        bool get_recall_pan()     const { return _flags & RecallPan;};
        bool get_recall_plugins() const { return _flags & RecallPlugs;};
        bool get_recall_groups()  const { return _flags & RecallGroups;};
        bool get_recall_vcas()    const { return _flags & RecallVCAs;};

#ifdef MIXBUS
        bool set_recall_eq(bool);
        bool set_recall_sends(bool);
        bool set_recall_comp(bool);
#endif
        bool set_recall_pan(bool);
        bool set_recall_plugins(bool);
        bool set_recall_groups(bool);
        bool set_recall_vcas(bool);

        unsigned int get_id() {return id;};
        void set_id(unsigned int new_id) {id = new_id;};

        std::string get_label() {return label;};
        void set_label(const std::string& new_label) {label = new_label; LabelChanged(this);};

        std::string get_description() {return _description;}
        void set_description(const std::string& new_desc) {_description = new_desc; DescriptionChanged();};

        std::string get_path() {return _path;};
        void set_path(const std::string& new_path) {_path = new_path; PathChanged(this);};

        bool get_favorite() {return favorite;};
        void set_favorite(bool yn) {favorite = yn;};

        std::time_t get_timestamp() {return timestamp;};
        void set_timestamp(std::time_t new_timestamp) {timestamp = new_timestamp;};

        std::string get_last_modified_with() {return last_modified_with;};
        void set_last_modified_with(std::string new_modified_with) {last_modified_with = new_modified_with;};

        void set_routes(std::vector<State> states) { routes = states;};

        //signals
        PBD::Signal1<void, ARDOUR::MixerSnapshot*> LabelChanged;
        PBD::Signal0<void>                         DescriptionChanged;
        PBD::Signal1<void, ARDOUR::MixerSnapshot*> PathChanged;
    private:
        ARDOUR::Session* _session;

        XMLNode& sanitize_node(XMLNode&);

        void reassign_masters(boost::shared_ptr<ARDOUR::Slavable>, XMLNode);
        bool load(const std::string&);
        bool set_flag(bool, RecallFlags);

        const std::string allowed[6] = {
            "lv2",
            "windows-vst",
            "lxvst",
            "mac-vst",
            "audiounit",
            "luaproc"
        };

        unsigned int id;
        bool favorite;
        std::string  label;
        std::string _description;
        std::time_t timestamp;
        std::string last_modified_with;
        std::string suffix;
        RecallFlags _flags;
        std::string _path;

        std::vector<State> routes;
        std::vector<State> groups;
        std::vector<State> vcas;

};

} // namespace ARDOUR

#endif /* __ardour_mixer_snapshot_h__ */