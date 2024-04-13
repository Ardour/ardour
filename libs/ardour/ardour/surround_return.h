/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_surround_return_h__
#define __ardour_surround_return_h__

#ifdef HAVE_LV2_1_18_6
#include <lv2/atom/atom.h>
#include <lv2/atom/forge.h>
#else
#include <lv2/lv2plug.in/ns/ext/atom/atom.h>
#include <lv2/lv2plug.in/ns/ext/atom/forge.h>
#endif

#ifdef __APPLE__
#include <CoreServices/CoreServices.h>
#include <CoreAudio/CoreAudio.h>
#include <AudioUnit/AudioUnit.h>
#endif

#include "ardour/chan_mapping.h"
#include "ardour/fixed_delay.h"
#include "ardour/lufs_meter.h"
#include "ardour/monitor_processor.h"
#include "ardour/processor.h"

namespace ARDOUR
{
class Amp;
class Session;
class SurroundSend;
class SurroundPannable;
class LV2Plugin;

class LIBARDOUR_API SurroundReturn : public Processor
{
public:
	SurroundReturn (Session&, Route*);
	virtual ~SurroundReturn ();

	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);
	void run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool);
	int  set_block_size (pframes_t);
	void flush ();
	void set_playback_offset (samplecnt_t cnt);
	bool display_to_user () const { return false; }

	void setup_export (std::string const&, samplepos_t, samplepos_t);
	void finalize_export ();

	size_t n_channels () const {
		return _current_n_channels;
	}

	size_t total_n_channels (bool with_beds = true) const {
		return _total_n_channels - (with_beds ? 0 : 10);
	}

	std::shared_ptr<LV2Plugin> surround_processor () const {
		return _surround_processor;
	}

	bool have_au_renderer () const {
		return _have_au_renderer;
	}

	bool load_au_preset (size_t);
	bool set_au_param (size_t, float);

	std::shared_ptr<PBD::Controllable> binaural_render_controllable () const {
		return _binaural_render_control;
	}

	enum MainOutputFormat {
		OUTPUT_FORMAT_5_1 = 2,
		OUTPUT_FORMAT_7_1_4 = 6
	};

	MainOutputFormat output_format () const {
		return _current_output_format;
	}

	std::shared_ptr<PBD::Controllable> output_format_controllable () const {
		return _output_format_control;
	}

	/* a value <= -200 indicates that no data is available */
	float integrated_loudness () const;
	float max_momentary () const;
	float momentary () const;
	float max_dbtp () const;

	samplecnt_t signal_latency () const;

	/* XXX this is only for testing */
	void set_bed_mix (bool on, std::string const& ref, int* cmap = NULL);
	void set_sync_and_align (bool on);
	void set_ffoa (float);
	void set_with_all_metadata (bool);

	int set_state (XMLNode const&, int version);

protected:
	XMLNode& state () const;

private:
	static const size_t max_object_id = 128; // happens to be the same as a constant in a well known surround system
	static const size_t num_pan_parameters = 8; // X, Y, Z, Size, Snap [ElevEn, Ramp, Zones]

	void forge_int_msg (uint32_t obj_id, uint32_t key, int val, uint32_t key2 = 0, int val2 = 0);
	void maybe_send_metadata (size_t id, pframes_t frame, pan_t const v[num_pan_parameters], bool force = false);
	void evaluate (size_t id, std::shared_ptr<SurroundPannable> const&, timepos_t const& , pframes_t, bool force = false);

	void reset_object_map ();
	void latency_changed ();

	std::shared_ptr<LV2Plugin> _surround_processor;

	LUFSMeter _lufs_meter;

	std::shared_ptr<Amp> _trim;

	class OutputFormatControl : public MPControl<bool>
	{
	public:
		OutputFormatControl (bool v, std::string const& n, PBD::Controllable::Flag f);
		virtual std::string get_user_string () const;
	};

	std::shared_ptr<OutputFormatControl> _output_format_control;

	class BinauralRenderControl : public MPControl<bool>
	{
	public:
		BinauralRenderControl (bool v, std::string const& n, PBD::Controllable::Flag f);
		virtual std::string get_user_string () const;
	};

	std::shared_ptr<BinauralRenderControl> _binaural_render_control;

#ifdef __APPLE__
	::AudioUnit      _au;
	AudioBufferList* _au_buffers;
	samplecnt_t      _au_samples_processed;
	float*           _au_data[12];

	struct LIBARDOUR_API AUParameter {
		AudioUnitParameterID id;
		AudioUnitScope scope;
		AudioUnitElement element;
		std::string label;
		float lower, upper, normal;
	};

	std::vector<AUParameter> _au_params;
	std::vector<AUPreset>    _au_presets;

	static OSStatus _render_callback(void*, AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
	OSStatus render_callback(AudioUnitRenderActionFlags*, const AudioTimeStamp*, UInt32, UInt32, AudioBufferList*);
#endif

	bool             _have_au_renderer;
	LV2_Atom_Forge   _forge;
	uint8_t          _atom_buf[8192];
	pan_t            _current_value[max_object_id][num_pan_parameters];
	int              _current_render_mode[max_object_id];
	size_t           _channel_id_map[max_object_id];
	size_t           _current_n_channels;
	size_t           _total_n_channels;
	MainOutputFormat _current_output_format;
	BufferSet        _surround_bufs;
	ChanMapping      _in_map;
	ChanMapping      _out_map;
	bool             _exporting;
	samplepos_t      _export_start;
	samplepos_t      _export_end;
	bool             _rolling;
	bool             _with_bed;
	bool             _sync_and_align;
	bool             _with_all_metadata;
	bool             _content_creation;
	float            _ffoa;
	std::string      _export_reference;
	FixedDelay       _delaybuffers;
	std::atomic<int> _flush;
};

} // namespace ARDOUR

#endif /* __ardour_surround_return_h__ */
