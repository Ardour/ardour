/*
 * Copyright (C) 2018-2021 Robin Gareus <robin@gareus.org>
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
#ifndef _ardour_convolver_h_
#define _ardour_convolver_h_

#include <vector>

#include "zita-convolver/zita-convolver.h"

#include "ardour/libardour_visibility.h"

#include "ardour/buffer_set.h"
#include "ardour/chan_mapping.h"
#include "ardour/readable.h"

namespace ARDOUR { namespace DSP {

class LIBARDOUR_API Convolution : public SessionHandleRef
{
public:
	Convolution (Session&, uint32_t n_in, uint32_t n_out);
	virtual ~Convolution () {}

	bool add_impdata (
	    uint32_t                    c_in,
	    uint32_t                    c_out,
	    boost::shared_ptr<AudioReadable> r,
	    float                       gain      = 1.0,
	    uint32_t                    pre_delay = 0,
	    sampleoffset_t              offset    = 0,
	    samplecnt_t                 length    = 0,
	    uint32_t                    channel   = 0);

	bool     ready () const;
	uint32_t latency () const   { return _n_samples; }
	uint32_t n_inputs () const  { return _n_inputs; }
	uint32_t n_outputs () const { return _n_outputs; }

	void restart ();
	void run (BufferSet&, ChanMapping const&, ChanMapping const&, pframes_t, samplecnt_t);

protected:
	ArdourZita::Convproc _convproc;

	uint32_t _n_samples;
	uint32_t _max_size;
	uint32_t _offset;
	bool     _configured;
	bool     _threaded;

private:
	class ImpData : public AudioReadable
	{
	public:
		ImpData (uint32_t ci, uint32_t co, boost::shared_ptr<AudioReadable> r, float g, float d, sampleoffset_t s = 0, samplecnt_t l = 0, uint32_t c = 0)
		    : c_in (ci)
		    , c_out (co)
		    , gain (g)
		    , delay (d)
		    , _readable (r)
		    , _offset (s)
		    , _length (l)
		    , _channel (c)
		{}

		uint32_t c_in;
		uint32_t c_out;
		float    gain;
		uint32_t delay;

		samplecnt_t read (Sample* s, samplepos_t pos, samplecnt_t cnt, int c = -1) const {
			return _readable->read (s, pos + _offset, cnt, _channel);
		}

		samplecnt_t readable_length_samples () const {
			samplecnt_t rl = _readable->readable_length_samples ();
			if (rl < _offset) {
				return 0;
			} else if (_length > 0) {
				return std::min (rl - _offset, _length);
			} else {
				return rl - _offset;
			}
		}

		uint32_t n_channels () const {
			return _readable->n_channels ();
		}

	private:
		boost::shared_ptr<AudioReadable> _readable;

		sampleoffset_t _offset;
		samplecnt_t    _length;
		uint32_t       _channel;
	};

	std::vector<ImpData> _impdata;
	uint32_t             _n_inputs;
	uint32_t             _n_outputs;
};

class LIBARDOUR_API Convolver : public Convolution
{
public:
	enum IRChannelConfig {
		Mono,         ///< 1 in, 1 out; 1ch IR
		MonoToStereo, ///< 1 in, 2 out, stereo IR  M -> L, M -> R
		Stereo,       ///< 2 in, 2 out, stereo IR  L -> L, R -> R || 4 chan IR  L -> L, L -> R, R -> R, R -> L
	};

	static uint32_t ircc_in (IRChannelConfig irc) {
		return irc < Stereo ? 1 : 2;
	}

	static uint32_t ircc_out (IRChannelConfig irc) {
		return irc == Mono ? 1 : 2;
	}

	struct IRSettings {
		IRSettings ()
		{
			gain      = 1.0;
			pre_delay = 0.0;
			channel_gain[0]  = channel_gain[1] = channel_gain[2] = channel_gain[3] = 1.0;
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

	void run_mono_buffered (float*, uint32_t);
	void run_stereo_buffered (float* L, float* R, uint32_t);

	void run_mono_no_latency (float*, uint32_t);
	void run_stereo_no_latency (float* L, float* R, uint32_t);

private:
	std::vector<boost::shared_ptr<AudioReadable> > _readables;

	IRChannelConfig _irc;
	IRSettings      _ir_settings;
};

} } /* namespace */
#endif
