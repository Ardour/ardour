/*
    Copyright (C) 1999-2002 Paul Davis 

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

/* see gdither.cc for why we have to do this */

#define	_ISOC9X_SOURCE	1
#define _ISOC99_SOURCE	1
#include <cmath>
#undef  _ISOC99_SOURCE
#undef  _ISOC9X_SOURCE
#undef  __USE_SVID 
#define __USE_SVID 1
#include <cstdlib>
#undef  __USE_SVID

#include <unistd.h>
#include <inttypes.h>
#include <float.h>

#include <sigc++/bind.h>

#include <pbd/error.h>
#include <glibmm/thread.h>

#include <ardour/gdither.h>
#include <ardour/timestamps.h>
#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/export.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/port.h>
#include <ardour/audio_port.h>
#include <ardour/audioengine.h>
#include <ardour/audio_diskstream.h>
#include <ardour/panner.h>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static int
convert_spec_to_info (AudioExportSpecification& spec, SF_INFO& sfinfo)
{
	if (spec.path.length() == 0) {
		error << _("Export: no output file specified") << endmsg;
		return -1;
	}

	/* XXX add checks that the directory path exists, and also 
	   check if we are overwriting an existing file...
	*/

	sfinfo.format = spec.format;
	sfinfo.samplerate = spec.sample_rate;
	sfinfo.frames = spec.end_frame - spec.start_frame + 1;
	sfinfo.channels = min (spec.channels, 2U);

	return 0;
}

AudioExportSpecification::AudioExportSpecification ()
{
	init ();
}

AudioExportSpecification::~AudioExportSpecification ()
{
	clear ();
}

void
AudioExportSpecification::init ()
{
	src_state = 0;
	pos = 0;
	total_frames = 0;
	out = 0;
	channels = 0;
	running = false;
	stop = false;
	progress = 0.0;
	status = 0;
	dither = 0;
	start_frame = 0;
	end_frame = 0;
	dataF = 0;
	dataF2 = 0;
 	leftoverF = 0;
 	max_leftover_frames = 0;
 	leftover_frames = 0;
	output_data = 0;
	out_samples_max = 0;
	data_width = 0;
	do_freewheel = false;
}

void
AudioExportSpecification::clear ()
{
	if (out) {
		sf_close (out);
		out = 0;
	}

	if (src_state) {
		src_delete (src_state);
		src_state = 0;
	}

	if (dither) {
		gdither_free (dither);
		dither = 0;
	}

	if (output_data) {
		free (output_data);
		output_data = 0;
	}
	if (dataF) {
		delete [] dataF;
		dataF = 0;
	}
	if (dataF2) {
		delete [] dataF2;
		dataF2 = 0;
	}
 	if (leftoverF) {
 		delete [] leftoverF;
 		leftoverF = 0;
 	}

	freewheel_connection.disconnect ();

	init ();
}

int
AudioExportSpecification::prepare (nframes_t blocksize, nframes_t frate)
{
	char errbuf[256];
	GDitherSize dither_size;

	frame_rate = frate;

	if (channels == 0) {
		error << _("illegal frame range in export specification") << endmsg;
		return -1;
	}

	if (start_frame >= end_frame) {
		error << _("illegal frame range in export specification") << endmsg;
		return -1;
	}

	if ((data_width = sndfile_data_width(format)) == 0) {
		error << _("Bad data width size.  Report me!") << endmsg;
		return -1;
	}

	switch (data_width) {
	case 8:
		dither_size = GDither8bit;
		break;

	case 16:
		dither_size = GDither16bit;
		break;

	case 24:
		dither_size = GDither32bit;
		break;

	default:
		dither_size = GDitherFloat;
		break;
	}

	if (convert_spec_to_info (*this, sfinfo)) {
		return -1;
	}

	/* XXX make sure we have enough disk space for the output */
	
	if ((out = sf_open (path.c_str(), SFM_WRITE, &sfinfo)) == 0) {
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		error << string_compose(_("Export: cannot open output file \"%1\" (%2)"), path, errbuf) << endmsg;
		return -1;
	}

	dataF = new float[blocksize * channels];

	if (sample_rate != frame_rate) {
		int err;

		if ((src_state = src_new (src_quality, channels, &err)) == 0) {
			error << string_compose (_("cannot initialize sample rate conversion: %1"), src_strerror (err)) << endmsg;
			return -1;
		}
		
		src_data.src_ratio = sample_rate / (double) frame_rate;
		out_samples_max = (nframes_t) ceil (blocksize * src_data.src_ratio * channels);
		dataF2 = new float[out_samples_max];

 		max_leftover_frames = 4 * blocksize;
 		leftoverF = new float[max_leftover_frames * channels];
 		leftover_frames = 0;

	} else {
		out_samples_max = blocksize * channels;
	}

	dither = gdither_new (dither_type, channels, dither_size, data_width);

	/* allocate buffers where dithering and output will occur */

	switch (data_width) {
	case 8:
		sample_bytes = 1;
		break;

	case 16:
		sample_bytes = 2;
		break;

	case 24:
	case 32:
		sample_bytes = 4;
		break;

	default:
		sample_bytes = 0; // float format
		break;
	}

	if (sample_bytes) {
		output_data = (void*) malloc (sample_bytes * out_samples_max);
	}

	return 0;
}

int
AudioExportSpecification::process (nframes_t nframes)
{
	float* float_buffer = 0;
	uint32_t chn;
	uint32_t x;
	uint32_t i;
	sf_count_t written;
	char errbuf[256];
	nframes_t to_write = 0;
	int cnt = 0;
	
	do {

		/* now do sample rate conversion */
	
		if (sample_rate != frame_rate) {
		
			int err;
		
			src_data.output_frames = out_samples_max / channels;
			src_data.end_of_input = ((pos + nframes) >= end_frame);
			src_data.data_out = dataF2;

			if (leftover_frames > 0) {

				/* input data will be in leftoverF rather than dataF */

				src_data.data_in = leftoverF;

				if (cnt == 0) {
					
					/* first time, append new data from dataF into the leftoverF buffer */

					memcpy (leftoverF + (leftover_frames * channels), dataF, nframes * channels * sizeof(float));
					src_data.input_frames = nframes + leftover_frames;
				} else {
					
					/* otherwise, just use whatever is still left in leftoverF; the contents
					   were adjusted using memmove() right after the last SRC call (see
					   below)
					*/

					src_data.input_frames = leftover_frames;
				}
					
			} else {

				src_data.data_in = dataF;
				src_data.input_frames = nframes;

			}

			++cnt;

			if ((err = src_process (src_state, &src_data)) != 0) {
				error << string_compose (_("an error occured during sample rate conversion: %1"),
						  src_strerror (err))
				      << endmsg;
				return -1;
			}
		
			to_write = src_data.output_frames_gen;
			leftover_frames = src_data.input_frames - src_data.input_frames_used;

			if (leftover_frames > 0) {
				if (leftover_frames > max_leftover_frames) {
					error << _("warning, leftover frames overflowed, glitches might occur in output") << endmsg;
					leftover_frames = max_leftover_frames;
				}
				memmove (leftoverF, (char *) (src_data.data_in + (src_data.input_frames_used * channels)),
					 leftover_frames * channels * sizeof(float));
			}

			float_buffer = dataF2;
		
		} else {

			/* no SRC, keep it simple */
		
			to_write = nframes;
			leftover_frames = 0;
			float_buffer = dataF;
		}
	
		if (output_data) {
			memset (output_data, 0, sample_bytes * to_write * channels);
		}
	
		switch (data_width) {
		case 8:
		case 16:
		case 24:
			for (chn = 0; chn < channels; ++chn) { 
				gdither_runf (dither, chn, to_write, float_buffer, output_data);
			}
			break;
		
		case 32:
			for (chn = 0; chn < channels; ++chn) {
			
				int *ob = (int *) output_data;
				const double int_max = (float) INT_MAX;
				const double int_min = (float) INT_MIN;
			
				for (x = 0; x < to_write; ++x) {
					i = chn + (x * channels);
				
					if (float_buffer[i] > 1.0f) {
						ob[i] = INT_MAX;
					} else if (float_buffer[i] < -1.0f) {
						ob[i] = INT_MIN;
					} else {
						if (float_buffer[i] >= 0.0f) {
							ob[i] = lrintf (int_max * float_buffer[i]);
						} else {
							ob[i] = - lrintf (int_min * float_buffer[i]);
						}
					}
				}
			}
			break;
		
		default:
			for (x = 0; x < to_write * channels; ++x) {
				if (float_buffer[x] > 1.0f) {
					float_buffer[x] = 1.0f;
				} else if (float_buffer[x] < -1.0f) {
					float_buffer[x] = -1.0f;
				} 
			}
			break;
		}
	
		/* and export to disk */
	
		switch (data_width) {
		case 8:
			/* XXXX no way to deliver 8 bit audio to libsndfile */
			written = to_write;
			break;
		
		case 16:
			written = sf_writef_short (out, (short*) output_data, to_write);
			break;
		
		case 24:
		case 32:
			written = sf_writef_int (out, (int*) output_data, to_write);
			break;
		
		default:
			written = sf_writef_float (out, float_buffer, to_write);
			break;
		}
	
		if ((nframes_t) written != to_write) {
			sf_error_str (out, errbuf, sizeof (errbuf) - 1);
			error << string_compose(_("Export: could not write data to output file (%1)"), errbuf) << endmsg;
			return -1;
		}


	} while (leftover_frames >= nframes);

	return 0;
}

int
Session::start_audio_export (AudioExportSpecification& spec)
{
	if (spec.prepare (current_block_size, frame_rate())) {
		return -1;
	}

	spec.pos = spec.start_frame;
	spec.end_frame = spec.end_frame;
	spec.total_frames = spec.end_frame - spec.start_frame;
	spec.running = true; 
	spec.do_freewheel = false; /* force a call to ::prepare_to_export() before proceeding to normal operation */

	spec.freewheel_connection = _engine.Freewheel.connect (sigc::bind (mem_fun (*this, &Session::process_export), &spec));

	return _engine.freewheel (true);
}

int
Session::stop_audio_export (AudioExportSpecification& spec)
{
	/* don't stop freewheeling but do stop paying attention to it for now */

	spec.freewheel_connection.disconnect ();
	spec.clear (); /* resets running/stop etc */

	return 0;
}

int 
Session::prepare_to_export (AudioExportSpecification& spec)
{
	int ret = -1;

	wait_till_butler_finished ();

	/* take everyone out of awrite to avoid disasters */

	{
		boost::shared_ptr<RouteList> r = routes.reader ();

		for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
			(*i)->protect_automation ();
		}
	}

	/* get everyone to the right position */

	{
		boost::shared_ptr<DiskstreamList> dsl = diskstreams.reader();

		for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
			if ((*i)-> seek (spec.start_frame, true)) {
				error << string_compose (_("%1: cannot seek to %2 for export"),
						  (*i)->name(), spec.start_frame)
				      << endmsg;
				goto out;
			}
		}
	}

	/* make sure we are actually rolling */

	if (get_record_enabled()) {
		disable_record (false);
	}

	_exporting = true;
	
	/* no slaving */

	post_export_slave = Config->get_slave_source ();
	post_export_position = _transport_frame;

	Config->set_slave_source (None);

	/* get transport ready */

	set_transport_speed (1.0, false);
	butler_transport_work ();
	g_atomic_int_set (&butler_should_do_transport_work, 0);
	post_transport ();

	/* we are ready to go ... */

	ret = 0;

  out:
	return ret;
}

int
Session::process_export (nframes_t nframes, AudioExportSpecification* spec)
{
	uint32_t chn;
	uint32_t x;
	int ret = -1;
	nframes_t this_nframes;

	/* This is not required to be RT-safe because we are running while freewheeling */

	if (spec->do_freewheel == false) {
		
		/* first time in export function: get set up */

		if (prepare_to_export (*spec)) {
			spec->running = false;
			spec->status = -1;
			return -1;
		}
		
		spec->do_freewheel = true;
	}

	if (!_exporting) {
		/* finished, but still freewheeling */
		process_without_events (nframes);
		return 0;
	}
		
	if (!spec->running || spec->stop || (this_nframes = min ((spec->end_frame - spec->pos), nframes)) == 0) {
		process_without_events (nframes);
		return stop_audio_export (*spec);
	}

	/* make sure we've caught up with disk i/o, since
	   we're running faster than realtime c/o JACK.
	*/

	wait_till_butler_finished ();
	
	/* do the usual stuff */
	
	process_without_events (nframes);

	/* and now export the results */

	nframes = this_nframes;

	memset (spec->dataF, 0, sizeof (spec->dataF[0]) * nframes * spec->channels);

	/* foreach output channel ... */
	
	for (chn = 0; chn < spec->channels; ++chn) {
		
		AudioExportPortMap::iterator mi = spec->port_map.find (chn);
		
		if (mi == spec->port_map.end()) {
			/* no ports exported to this channel */
			continue;
		}
		
		vector<PortChannelPair>& mapped_ports ((*mi).second);
		
		for (vector<PortChannelPair>::iterator t = mapped_ports.begin(); t != mapped_ports.end(); ++t) {
			
			/* OK, this port's output is supposed to appear on this channel 
			 */

			AudioPort* const port = dynamic_cast<AudioPort*>((*t).first);
			if (port == 0) {
				cerr << "FIXME: Non-audio export" << endl;
				continue;
			}
			Sample* port_buffer = port->get_audio_buffer().data();

			/* now interleave the data from the channel into the float buffer */
				
			for (x = 0; x < nframes; ++x) {
				spec->dataF[chn+(x*spec->channels)] += (float) port_buffer[x];
			}
		}
	}

	if (spec->process (nframes)) {
		goto out;
	}
	
	spec->pos += nframes;
	spec->progress = 1.0 - (((float) spec->end_frame - spec->pos) / spec->total_frames);

	/* and we're good to go */

	ret = 0;

  out: 
	if (ret) {
		sf_close (spec->out);
		spec->out = 0;
		unlink (spec->path.c_str());
		spec->running = false;
		spec->status = ret;
		_exporting = false;
	}

	return ret;
}

void
Session::finalize_audio_export ()
{
	_engine.freewheel (false);
	_exporting = false;

	/* can't use stop_transport() here because we need
	   an immediate halt and don't require all the declick
	   stuff that stop_transport() implements.
	*/

	realtime_stop (true);
	schedule_butler_transport_work ();

	/* restart slaving */

	if (post_export_slave != None) {
		Config->set_slave_source (post_export_slave);
	} else {
		locate (post_export_position, false, false, false);
	}
}
