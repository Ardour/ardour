/*
    Copyright (C) 2006 Paul Davis 

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

#ifndef __ardour_track_h__
#define __ardour_track_h__

#include <ardour/route.h>

namespace ARDOUR {

class Session;
class Diskstream;
class Playlist;
class RouteGroup;

class Track : public Route
{
  public:
	Track (Session&, string name, Route::Flag f = Route::Flag (0), TrackMode m = Normal, Buffer::Type default_type = Buffer::AUDIO);

	virtual ~Track () {}
	
	virtual int set_name (string str, void *src) = 0;

	virtual int roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, 
		jack_nframes_t offset, int declick, bool can_record, bool rec_monitors_input) = 0;
	
	virtual int no_roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, 
		jack_nframes_t offset, bool state_changing, bool can_record, bool rec_monitors_input) = 0;
	
	virtual int silent_roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, 
		jack_nframes_t offset, bool can_record, bool rec_monitors_input) = 0;

	void toggle_monitor_input ();

	bool can_record() const { return true; }
	virtual void set_record_enable (bool yn, void *src) = 0;

	Diskstream& diskstream() const { return *_diskstream; }

	virtual int use_diskstream (string name) = 0;
	virtual int use_diskstream (const PBD::ID& id) = 0;

	TrackMode    mode() const { return _mode; }
	virtual void set_mode (TrackMode m) = 0;

	jack_nframes_t update_total_latency();
	virtual void   set_latency_delay (jack_nframes_t) = 0;

	enum FreezeState {
		NoFreeze,
		Frozen,
		UnFrozen
	};

	FreezeState freeze_state() const;
 
	virtual void freeze (InterThreadInfo&) = 0;
	virtual void unfreeze () = 0;

	virtual void bounce (InterThreadInfo&) = 0;
	virtual void bounce_range (jack_nframes_t start, jack_nframes_t end, InterThreadInfo&) = 0;

	XMLNode&    get_state();
	XMLNode&    get_template();
	virtual int set_state(const XMLNode& node) = 0;

	PBD::Controllable& rec_enable_control() { return _rec_enable_control; }

	virtual bool record_enabled() const = 0;
	void set_meter_point (MeterPoint, void* src);
	
	sigc::signal<void>       ModeChanged;
	sigc::signal<void,void*> DiskstreamChanged;
	sigc::signal<void>       FreezeChange;

  protected:
	Track (Session& sess, const XMLNode& node, Buffer::Type default_type = Buffer::AUDIO);

	virtual XMLNode& state (bool full) = 0;

	virtual void passthru_silence (jack_nframes_t start_frame, jack_nframes_t end_frame, 
		jack_nframes_t nframes, jack_nframes_t offset, int declick, bool meter) = 0;

	virtual uint32_t n_process_buffers () = 0;
	
	Diskstream *_diskstream;
	MeterPoint  _saved_meter_point;
	TrackMode   _mode;

	//private:
	struct FreezeRecordInsertInfo {
	    FreezeRecordInsertInfo(XMLNode& st) 
		    : state (st), insert (0) {}

	    XMLNode    state;
	    Insert*    insert;
	    PBD::ID    id;
	    UndoAction memento;
	};

	struct FreezeRecord {
	    FreezeRecord()
		: playlist(0)
		, have_mementos(false)
	    {}

	    ~FreezeRecord();

	    Playlist*                       playlist;
	    vector<FreezeRecordInsertInfo*> insert_info;
	    bool                            have_mementos;
	    FreezeState                     state;
	};

	struct RecEnableControllable : public PBD::Controllable {
	    RecEnableControllable (Track&);
	    
	    void set_value (float);
	    float get_value (void) const;

	    Track& track;
	};

	//virtual void diskstream_record_enable_changed (void *src) = 0;
	//virtual void diskstream_input_channel_changed (void *src) = 0;

	//virtual void input_change_handler (void *src) = 0;

	virtual void set_state_part_two () = 0;

	FreezeRecord          _freeze_record;
	XMLNode*              pending_state;
	sigc::connection      recenable_connection;
	sigc::connection      ic_connection;
	RecEnableControllable _rec_enable_control;
	bool                  _destructive;
};

}; /* namespace ARDOUR*/

#endif /* __ardour_track_h__ */
