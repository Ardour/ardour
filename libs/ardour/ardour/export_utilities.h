/*
    Copyright (C) 2008 Paul Davis
    Author: Sakari Bergen

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

#ifndef __ardour_export_utilities_h__
#define __ardour_export_utilities_h__

#include <samplerate.h>

#include "ardour/graph.h"
#include "ardour/types.h"
#include "ardour/ardour.h"
#include "ardour/export_format_base.h"
#include "ardour/runtime_functions.h"

namespace ARDOUR
{

/* Processors */

/* Sample rate converter */

class SampleRateConverter : public GraphSinkVertex<float, float>
{
  public:
	SampleRateConverter (uint32_t channels, nframes_t in_rate, nframes_t out_rate, int quality);
	~SampleRateConverter ();

  protected:
	nframes_t process (float * data, nframes_t frames);
	
  private:
	bool           active;
	uint32_t       channels;
	
	nframes_t      leftover_frames;
	nframes_t      max_leftover_frames;
	nframes_t      frames_in;
	nframes_t      frames_out;
	
	float *        data_in;
	float *        leftover_data;
	
	float *        data_out;
	nframes_t      data_out_size;
	
	SRC_DATA       src_data;
	SRC_STATE*     src_state;
};

/* Sample format converter */

template <typename TOut>
class SampleFormatConverter : public GraphSinkVertex<float, TOut>
{
  public:
	SampleFormatConverter (uint32_t channels, ExportFormatBase::DitherType type = ExportFormatBase::D_None, int data_width_ = 0);
	~SampleFormatConverter ();
	
	void set_clip_floats (bool yn) { clip_floats = yn; }
	
  protected:
	nframes_t process (float * data, nframes_t frames);
	
  private:
	uint32_t     channels;
	int          data_width;
	GDither      dither;
	nframes_t    data_out_size;
	TOut *       data_out;
	
	bool         clip_floats;
	
};

/* Peak reader */

class PeakReader : public GraphSinkVertex<float, float>
{
  public:
	PeakReader (uint32_t channels) : channels (channels), peak (0) {}
	~PeakReader () {}
	
	float get_peak () { return peak; }
	
  protected:
	nframes_t process (float * data, nframes_t frames)
	{
		peak = compute_peak (data, channels * frames, peak);
		return piped_to->write (data, frames);
	}
	
  private:
	uint32_t    channels;
	float       peak;
};

/* Normalizer */

class Normalizer : public GraphSinkVertex<float, float>
{
  public:
	Normalizer (uint32_t channels, float target_dB);
	~Normalizer ();
	
	void set_peak (float peak);
	
  protected:
	nframes_t process (float * data, nframes_t frames);
	
  private:
	uint32_t    channels;
	
	bool        enabled;
	gain_t      target;
	gain_t      gain;
};

/* Other */

class NullSink : public GraphSink<float>
{
  public:
	nframes_t write (float * data, nframes_t frames) { return frames; }
};


} // namespace ARDOUR

#endif /* __ardour_export_utilities_h__ */
