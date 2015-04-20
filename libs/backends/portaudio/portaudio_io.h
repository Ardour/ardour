/*
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#ifndef __libbackend_portaudio_pcmio_h__
#define __libbackend_portaudio_pcmio_h__

#include <map>
#include <vector>
#include <string>
#include <boost/shared_ptr.hpp>

#include <stdint.h>

#include <portaudio.h>

namespace ARDOUR {

class PortAudioIO {
public:
	PortAudioIO (void);
	~PortAudioIO (void);

	int      state (void) const { return _state; }

	void     discover();
	void     device_list (std::map<int, std::string> &devices) const;

	int      available_sample_rates (int device_id, std::vector<float>& sampleRates);
	int      available_buffer_sizes (int device_id, std::vector<uint32_t>& sampleRates);


	void     pcm_stop (void);
	int      pcm_start (void);

	int      pcm_setup (
			int device_input,
			int device_output,
			double   sample_rate,
			uint32_t samples_per_period
			);

	uint32_t n_playback_channels (void) const { return _playback_channels; }
	uint32_t n_capture_channels (void) const { return _capture_channels; }

	double   sample_rate (void) const { return _cur_sample_rate; }
	uint32_t capture_latency (void) const { return _cur_input_latency; }
	uint32_t playback_latency (void) const { return _cur_output_latency; }
	double   stream_time(void) const { if (_stream) return Pa_GetStreamTime (_stream); return 0; }

	int      next_cycle(uint32_t n_samples);
	int      get_capture_channel (uint32_t chn, float *input, uint32_t n_samples);
	int      set_playback_channel (uint32_t chn, const float *input, uint32_t n_samples);

private:
	int  _state;
	bool _initialized;

	uint32_t _capture_channels;
	uint32_t _playback_channels;

	PaStream *_stream;

	float *_input_buffer;
	float *_output_buffer;

	double _cur_sample_rate;
	uint32_t _cur_input_latency;
	uint32_t _cur_output_latency;


	struct paDevice {
		std::string name;
		uint32_t n_inputs;
		uint32_t n_outputs;

		paDevice (std::string n, uint32_t i, uint32_t o)
			: name (n)
			, n_inputs (i)
			, n_outputs (o)
		{}
	};

	std::map<int, paDevice *> _devices;
};

} // namespace

#endif /* __libbackend_portaudio_pcmio_h__ */
