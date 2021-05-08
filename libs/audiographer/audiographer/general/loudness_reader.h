/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
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

#ifndef AUDIOGRAPHER_LOUDNESS_READER_H
#define AUDIOGRAPHER_LOUDNESS_READER_H

#include <vector>

#include <vamp-hostsdk/PluginLoader.h>

#include "audiographer/visibility.h"
#include "audiographer/sink.h"
#include "audiographer/routines.h"
#include "audiographer/utils/listed_source.h"

namespace AudioGrapher
{

class LIBAUDIOGRAPHER_API LoudnessReader : public ListedSource<float>, public Sink<float>
{
  public:
	LoudnessReader (float sample_rate, unsigned int channels, samplecnt_t bufsize);
	~LoudnessReader ();

	void reset ();

	float calc_peak (float target_lufs = -23, float target_dbtp = -1) const;
	bool  get_loudness (float* integrated, float* short_term = NULL, float* momentary = NULL) const;

	virtual void process (ProcessContext<float> const & c);

	using Sink<float>::process;

  protected:
	Vamp::Plugin*              _ebur_plugin;
	std::vector<Vamp::Plugin*> _dbtp_plugins;

	float        _sample_rate;
	unsigned int _channels;
	samplecnt_t   _bufsize;
	samplecnt_t   _pos;
	float*       _bufs[2];
};

} // namespace


#endif // AUDIOGRAPHER_LOUDNESS_READER_H
