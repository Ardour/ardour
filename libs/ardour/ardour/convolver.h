/*
 * Copyright (C) 2018 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 *
 */
#ifndef _ardour_convolver_h_
#define _ardour_convolver_h_

#include <vector>

#include "zita-convolver/zita-convolver.h"

#include "ardour/libardour_visibility.h"
#include "ardour/readable.h"

namespace ARDOUR { namespace DSP {

class LIBARDOUR_API Convolver : public SessionHandleRef {
public:

	enum IRChannelConfig {
		Mono,         ///< 1 in, 1 out; 1ch IR
		MonoToStereo, ///< 1 in, 2 out, stereo IR  M -> L, M -> R
		Stereo,       ///< 2 in, 2 out, stereo IR  L -> L, R -> R || 4 chan IR  L -> L, L -> R, R -> R, R -> L
	};

	struct IRSettings {
		IRSettings () {
			gain  = 1.0;
			pre_delay = 0.0;
			channel_gain[0] = channel_gain[1] = channel_gain[2] = channel_gain[3] = 1.0;
			channel_delay[0] = channel_delay[1] = channel_delay[2] = channel_delay[3] = 0;
		};

		float    gain;
		uint32_t pre_delay;
		float    channel_gain[4];
		uint32_t channel_delay[4];

		/* convenient array accessors for Lua bindings */
		float get_channel_gain (unsigned i) const {
			if (i < 4) { return channel_gain[i]; }
			return 0;
		}
		void set_channel_gain (unsigned i, float g) {
			if (i < 4) { channel_gain[i] = g; }
		}
		uint32_t get_channel_delay (unsigned i) const {
			if (i < 4) { return channel_delay[i]; }
			return 0;
		}
		void set_channel_delay (unsigned i, uint32_t d) {
			if (i < 4) { channel_delay[i] = d; }
		}
	};


	Convolver (Session&, std::string const&, IRChannelConfig irc = Mono, IRSettings irs = IRSettings ());

	void run (float*, uint32_t);
	void run_stereo (float* L, float* R, uint32_t);

	uint32_t latency () const { return _n_samples; }

	uint32_t n_inputs  () const { return _irc < Stereo ? 1 : 2; }
	uint32_t n_outputs () const { return _irc == Mono  ? 1 : 2; }

	bool ready () const;

private:
	void reconfigure ();
	std::vector<boost::shared_ptr<Readable> > _readables;
	ArdourZita::Convproc _convproc;

	IRChannelConfig _irc;
	IRSettings      _ir_settings;

	uint32_t _n_samples;
	uint32_t _max_size;
	uint32_t _offset;
	bool     _configured;
};

} } /* namespace */
#endif
