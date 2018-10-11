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

	Convolver (Session&, std::string const&, IRChannelConfig irc = Mono, uint32_t pre_delay = 0);

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
	uint32_t _initial_delay;

	uint32_t _n_samples;
	uint32_t _max_size;
	uint32_t _offset;
	bool     _configured;
};

} } /* namespace */
#endif
