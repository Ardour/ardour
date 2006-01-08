/*
    Copyright (C) 2002-2006 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_audio_track_h__
#define __ardour_audio_track_h__

#include <ardour/route.h>

namespace ARDOUR {

class Session;
class DiskStream;
class AudioPlaylist;

class AudioTrack : public Route
{
  public:
	AudioTrack (Session&, string name, Route::Flag f = Route::Flag (0));
	AudioTrack (Session&, const XMLNode&);
	~AudioTrack ();
	
	int set_name (string str, void *src);

	int  roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, 

		   jack_nframes_t offset, int declick, bool can_record, bool rec_monitors_input);
	int  no_roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, 
		      jack_nframes_t offset, bool state_changing, bool can_record, bool rec_monitors_input);
	int  silent_roll (jack_nframes_t nframes, jack_nframes_t start_frame, jack_nframes_t end_frame, 
			  jack_nframes_t offset, bool can_record, bool rec_monitors_input);

	void toggle_monitor_input ();

	bool can_record() const { return true; }
	void set_record_enable (bool yn, void *src);

	DiskStream& disk_stream() const { return *diskstream; }
	int set_diskstream (DiskStream&, void *);
	int use_diskstream (string name);
	int use_diskstream (id_t id);

	bool destructive() const { return _destructive; }
	void set_destructive (bool yn);
	sigc::signal<void> DestructiveChanged;

	jack_nframes_t update_total_latency();
	void set_latency_delay (jack_nframes_t);
	
	int export_stuff (vector<Sample*>& buffers, uint32_t nbufs, jack_nframes_t nframes, jack_nframes_t end_frame);

	sigc::signal<void,void*> diskstream_changed;

	enum FreezeState {
		NoFreeze,
		Frozen,
		UnFrozen
	};

	FreezeState freeze_state() const;

	sigc::signal<void> FreezeChange;
 
	void freeze (InterThreadInfo&);
	void unfreeze ();

	void bounce (InterThreadInfo&);
	void bounce_range (jack_nframes_t start, jack_nframes_t end, InterThreadInfo&);

	XMLNode& get_state();
	int set_state(const XMLNode& node);

	MIDI::Controllable& midi_rec_enable_control() {
		return _midi_rec_enable_control;
	}

	void reset_midi_control (MIDI::Port*, bool);
	void send_all_midi_feedback ();

	bool record_enabled() const;
	void set_meter_point (MeterPoint, void* src);

  protected:
	DiskStream *diskstream;
	MeterPoint _saved_meter_point;

	void passthru_silence (jack_nframes_t start_frame, jack_nframes_t end_frame, 
			       jack_nframes_t nframes, jack_nframes_t offset, int declick,
			       bool meter);

	uint32_t n_process_buffers ();

  private:
	struct FreezeRecordInsertInfo {
	    FreezeRecordInsertInfo(XMLNode& st) 
		    : state (st), insert (0) {}

	    XMLNode  state;
	    Insert*  insert;
	    id_t     id;
	    UndoAction memento;
	};

	struct FreezeRecord {
	    FreezeRecord() {
		    playlist = 0;
		    have_mementos = false;
	    }

	    ~FreezeRecord();

	    AudioPlaylist* playlist;
	    vector<FreezeRecordInsertInfo*> insert_info;
	    bool have_mementos;
	    FreezeState state;
	};

	FreezeRecord _freeze_record;
	XMLNode* pending_state;

	void diskstream_record_enable_changed (void *src);
	void diskstream_input_channel_changed (void *src);

	void input_change_handler (void *src);

	sigc::connection recenable_connection;
	sigc::connection ic_connection;

	XMLNode& state(bool);

	int deprecated_use_diskstream_connections ();
	void set_state_part_two ();
	void set_state_part_three ();

	struct MIDIRecEnableControl : public MIDI::Controllable {
		MIDIRecEnableControl (AudioTrack&, MIDI::Port *);
		void set_value (float);
		void send_feedback (bool);
	        MIDI::byte* write_feedback (MIDI::byte* buf, int32_t& bufsize, bool val, bool force = false);
		AudioTrack& track;
		bool setting;
  	        bool last_written;
	};

	MIDIRecEnableControl _midi_rec_enable_control;

	bool _destructive;
};

}; /* namespace ARDOUR*/

#endif /* __ardour_audio_track_h__ */
