/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2010-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#pragma once

#include <string>

#include <glibmm/threads.h>

#include "evoral/PatchChange.h"

#include "ardour/ardour.h"
#include "ardour/plugin.h"
#include "ardour/track.h"

namespace ARDOUR {

class Session;
class AudioRegion;
class AudioPlaylist;
class MidiRegion;

class LIBARDOUR_API Auditioner : public Track
{
public:
	Auditioner (Session&);
	~Auditioner ();

	int init ();
	int connect ();

	bool auditioning() const;
	void audition_region (std::shared_ptr<Region>, bool loop = false);
	int play_audition (samplecnt_t nframes);
	void cancel_audition ();

	void seek_to_sample (sampleoffset_t pos);
	void seek_to_percent (float const pos);
	sampleoffset_t seek_sample() const { return _seeking ? _seek_sample : -1;}
	void seek_response(sampleoffset_t pos);

	void idle_synth_update ();

	Evoral::PatchChange<MidiBuffer::TimeType> const& patch_change (uint8_t chn) {
		return _patch_change[chn & 0xf];
	}

	MonitorState monitoring_state () const;

	bool needs_monitor() const { return via_monitor; }

	virtual ChanCount input_streams () const;

	PBD::Signal<void(ARDOUR::samplecnt_t, ARDOUR::samplecnt_t)> AuditionProgress;

	/* Track */
	int roll (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler);
	DataType data_type () const;

	int roll_audio (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler);
	int roll_midi (pframes_t nframes, samplepos_t start_sample, samplepos_t end_sample, bool& need_butler);

	/* fake track */
	void set_state_part_two () {}
	int set_state (const XMLNode&, int) { return 0; }
	bool bounceable (std::shared_ptr<Processor>, bool) const { return false; }
	void freeze_me (InterThreadInfo&) {}
	void unfreeze () {}

	/* MIDI Track -- listen to Bank/Patch */
	void update_controls (BufferSet const& bufs);

	std::shared_ptr<Region> bounce (InterThreadInfo&, std::string const& name) {
		return std::shared_ptr<Region> ();
	}

	std::shared_ptr<Region> bounce_range (samplepos_t, samplepos_t, InterThreadInfo&, std::shared_ptr<Processor>, bool, std::string const&, bool) {
		return std::shared_ptr<Region> ();
	}

	int export_stuff (BufferSet&, samplepos_t, samplecnt_t, std::shared_ptr<Processor>, bool, bool, bool, MidiNoteTracker&) { return -1; }

	void set_audition_synth_info(PluginInfoPtr in);
	PluginInfoPtr get_audition_synth_info() {return audition_synth_info;}

	samplecnt_t output_latency () const { return 0; }

private:

	PluginInfoPtr audition_synth_info;  //we will use this to create a new synth on-the-fly each time an audition is requested

	std::shared_ptr<AudioRegion> the_region;
	std::shared_ptr<MidiRegion> midi_region;
	samplepos_t current_sample;
	mutable std::atomic<int> _auditioning;
	Glib::Threads::Mutex lock;
	timecnt_t length;
	sampleoffset_t _seek_sample;
	bool _reload_synth;
	bool _seeking;
	bool _seek_complete;
	bool via_monitor;
	bool _midi_audition;
	bool _queue_panic;
	bool _loop;

	std::shared_ptr<Processor> asynth;

	Evoral::PatchChange<MidiBuffer::TimeType> _patch_change[16];

	PluginInfoPtr lookup_fallback_synth_plugin_info (std::string const&) const;
	void drop_ports ();
	void lookup_fallback_synth ();
	bool load_synth();
	void unload_synth (bool);
	static void*_drop_ports (void*);
	void actually_drop_ports ();
	void output_changed (IOChange, void*);
	timepos_t _import_position;
};

}; /* namespace ARDOUR */

