/*
    Copyright (C) 2000-2001 Paul Davis 

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

#ifndef __ardour_audio_region_h__
#define __ardour_audio_region_h__

#include <vector>

#include <pbd/fastlog.h>
#include <pbd/undo.h>

#include <ardour/ardour.h>
#include <ardour/source.h>
#include <ardour/gain.h>
#include <ardour/region.h>
#include <ardour/export.h>

class XMLNode;

namespace ARDOUR {

class Route;
class Playlist;
class Session;
class AudioFilter;

struct AudioRegionState : public RegionState 
{
    AudioRegionState (std::string why);

    Curve _fade_in;
    Curve _fade_out;
    Curve _envelope;
    gain_t _scale_amplitude;
    uint32_t _fade_in_disabled;
    uint32_t _fade_out_disabled;
};

class AudioRegion : public Region
{
  public:
	typedef vector<Source *> SourceList;

	static Change FadeInChanged;
	static Change FadeOutChanged;
	static Change FadeInActiveChanged;
	static Change FadeOutActiveChanged;
	static Change EnvelopeActiveChanged;
	static Change ScaleAmplitudeChanged;
	static Change EnvelopeChanged;

	AudioRegion (Source&, jack_nframes_t start, jack_nframes_t length, bool announce = true);
	AudioRegion (Source&, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	AudioRegion (SourceList &, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	AudioRegion (const AudioRegion&, jack_nframes_t start, jack_nframes_t length, const string& name, layer_t = 0, Region::Flag flags = Region::DefaultFlags, bool announce = true);
	AudioRegion (const AudioRegion&);
	AudioRegion (Source&, const XMLNode&);
	AudioRegion (SourceList &, const XMLNode&);
	~AudioRegion();

	bool region_list_equivalent (const AudioRegion&);
	bool source_equivalent (const AudioRegion&);
	bool equivalent (const AudioRegion&);
	bool size_equivalent (const AudioRegion&);

	void lock_sources ();
	void unlock_sources ();
	Source& source (uint32_t n=0) const { if (n < sources.size()) return *sources[n]; else return *sources[0]; } 

	void set_scale_amplitude (gain_t);
	gain_t scale_amplitude() const { return _scale_amplitude; }
	
	void normalize_to (float target_in_dB = 0.0f);

	uint32_t n_channels() { return sources.size(); }
	vector<string> master_source_names();
	
	bool envelope_active () const { return _flags & Region::EnvelopeActive; }
	bool fade_in_active () const { return _flags & Region::FadeIn; }
	bool fade_out_active () const { return _flags & Region::FadeOut; }
	bool captured() const { return !(_flags & (Region::Flag (Region::Import|Region::External))); }

	Curve& fade_in()  { return _fade_in; }
	Curve& fade_out() { return _fade_out; }
	Curve& envelope() { return _envelope; }

	jack_nframes_t read_peaks (PeakData *buf, jack_nframes_t npeaks, jack_nframes_t offset, jack_nframes_t cnt, uint32_t chan_n=0, double samples_per_unit= 1.0) const;

	virtual jack_nframes_t read_at (Sample *buf, Sample *mixdown_buffer, 
					float *gain_buffer, jack_nframes_t position, jack_nframes_t cnt, 
					uint32_t chan_n = 0,
					jack_nframes_t read_frames = 0,
					jack_nframes_t skip_frames = 0) const;

	jack_nframes_t master_read_at (Sample *buf, Sample *mixdown_buffer, 
				       float *gain_buffer, jack_nframes_t position, jack_nframes_t cnt, uint32_t chan_n=0) const;


	XMLNode& state (bool);
	XMLNode& get_state ();
	int      set_state (const XMLNode&);

	static void set_default_fade (float steepness, jack_nframes_t len);

	enum FadeShape {
		Linear,
		Fast,
		Slow,
		LogA,
		LogB,

	};

	void set_fade_in_active (bool yn);
	void set_fade_in_shape (FadeShape);
	void set_fade_in_length (jack_nframes_t);
	void set_fade_in (FadeShape, jack_nframes_t);

	void set_fade_out_active (bool yn);
	void set_fade_out_shape (FadeShape);
	void set_fade_out_length (jack_nframes_t);
	void set_fade_out (FadeShape, jack_nframes_t);

	void set_envelope_active (bool yn);

	int separate_by_channel (ARDOUR::Session&, vector<AudioRegion*>&) const;

	uint32_t read_data_count() const { return _read_data_count; }

	ARDOUR::Playlist* playlist() const { return _playlist; }

	UndoAction get_memento() const;

	/* filter */

	int apply (AudioFilter&);

	/* export */

	int exportme (ARDOUR::Session&, ARDOUR::AudioExportSpecification&);

	Region* get_parent();

	/* xfade/fade interactions */

	void suspend_fade_in ();
	void suspend_fade_out ();
	void resume_fade_in ();
	void resume_fade_out ();

  private:
	friend class Playlist;

  private:
	SourceList        sources;
	SourceList        master_sources; /* used when timefx are applied, so 
					     we can always use the original
					     source.
					  */
	mutable Curve  	  _fade_in;
	FadeShape         _fade_in_shape;
	mutable Curve  	  _fade_out;
	FadeShape         _fade_out_shape;
	mutable Curve 	  _envelope;
	gain_t            _scale_amplitude;
	uint32_t          _fade_in_disabled;
	uint32_t          _fade_out_disabled;

	void set_default_fades ();
	void set_default_fade_in ();
	void set_default_fade_out ();
	void set_default_envelope ();

	StateManager::State* state_factory (std::string why) const;
	Change restore_state (StateManager::State&);

	void recompute_gain_at_end ();
	void recompute_gain_at_start ();

	bool copied() const { return _flags & Copied; }
	void maybe_uncopy ();
	void rename_after_first_edit ();

	jack_nframes_t _read_at (const SourceList&, Sample *buf, Sample *mixdown_buffer, 
				 float *gain_buffer, jack_nframes_t position, jack_nframes_t cnt, 
				 uint32_t chan_n = 0,
				 jack_nframes_t read_frames = 0,
				 jack_nframes_t skip_frames = 0) const;

	bool verify_start (jack_nframes_t position);
	bool verify_length (jack_nframes_t position);
	bool verify_start_mutable (jack_nframes_t& start);
	bool verify_start_and_length (jack_nframes_t start, jack_nframes_t length);
	void recompute_at_start ();
	void recompute_at_end ();

	void envelope_changed (Change);

	void source_deleted (Source*);
};

} /* namespace ARDOUR */

/* access from C objects */

extern "C" {
	int    region_read_peaks_from_c   (void *arg, uint32_t npeaks, uint32_t start, uint32_t length, intptr_t data, uint32_t n_chan, double samples_per_unit);
	uint32_t region_length_from_c (void *arg);
	uint32_t sourcefile_length_from_c (void *arg);
}

#endif /* __ardour_audio_region_h__ */
