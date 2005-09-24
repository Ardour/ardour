/*
    Copyright (C) 2000 Paul Davis 

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

#include <cstdio>
#include <cstdlib>
#include <string>
#include <climits>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>

#include <sndfile.h>
#include <samplerate.h>

#include <pbd/basename.h>
#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/diskstream.h>
#include <ardour/filesource.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/audioregion.h>

#include "i18n.h"

using namespace ARDOUR;

#define BLOCKSIZE 2048U

int
Session::import_audiofile (import_status& status)
{
	SNDFILE *in;
	FileSource **newfiles = 0;
	ARDOUR::AudioRegion::SourceList sources;
	SF_INFO info;
	float *data = 0;
	Sample **channel_data = 0;
	long nfiles = 0;
	long n;
	string basepath;
	string sounds_dir;
	jack_nframes_t so_far;
	char buf[PATH_MAX+1];
	int ret = -1;
	vector<AudioRegion *> new_regions;
	vector<string> new_paths;
	struct tm* now;
	string tmp_convert_file;
	
	if ((in = sf_open (status.pathname.c_str(), SFM_READ, &info)) == 0) {
		error << compose(_("Import: cannot open input sound file \"%1\""), status.pathname) << endmsg;
		return -1;
	} else {
		if ((uint32_t) info.samplerate != frame_rate()) {
			sf_close(in);
			status.doing_what = _("resampling audio");
			// resample to session frame_rate
			if (sample_rate_convert(status, status.pathname, tmp_convert_file)) {
				if ((in = sf_open (tmp_convert_file.c_str(), SFM_READ, &info)) == 0) {
					error << compose(_("Import: cannot open converted sound file \"%1\""), tmp_convert_file) << endmsg;
					return -1;
				}
			} else if (!status.cancel){
				// error
				error << compose(_("Import: error while resampling sound file \"%1\""), status.pathname) << endmsg;
				return -1;
			} else {
				// canceled
				goto out;
			}
		}
	}

	newfiles = new FileSource *[info.channels];
	for (n = 0; n < info.channels; ++n) {
		newfiles[n] = 0;
	}
	
	sounds_dir = discover_best_sound_dir ();
	basepath = PBD::basename_nosuffix (status.pathname);

	for (n = 0; n < info.channels; ++n) {

		bool goodfile = false;

		do {
			if (info.channels == 2) {
				if (n == 0) {
					snprintf (buf, sizeof(buf), "%s%s-L.wav", sounds_dir.c_str(), basepath.c_str());
				} else {
					snprintf (buf, sizeof(buf), "%s%s-R.wav", sounds_dir.c_str(), basepath.c_str());
				}
			} else if (info.channels > 1) {
				snprintf (buf, sizeof(buf), "%s%s-c%lu.wav", sounds_dir.c_str(), basepath.c_str(), n+1);
			} else {
				snprintf (buf, sizeof(buf), "%s%s.wav", sounds_dir.c_str(), basepath.c_str());
			}

			if (::access (buf, F_OK) == 0) {

				/* if the file already exists, we must come up with
				 *  a new name for it.  for now we just keep appending
				 *  _ to basepath
				 */
				
				basepath += "_";

			} else {

				goodfile = true;
			}

		} while ( !goodfile);

			
		try { 
			newfiles[n] = new FileSource (buf, frame_rate());
		}

		catch (failed_constructor& err) {
			error << compose(_("Session::import_audiofile: cannot open new file source for channel %1"), n+1) << endmsg;
			goto out;
		}

		new_paths.push_back (buf);
		nfiles++;
	}
	
	
	data = new float[BLOCKSIZE * info.channels];
	channel_data = new Sample * [ info.channels ];

	for (n = 0; n < info.channels; ++n) {
		channel_data[n] = new Sample[BLOCKSIZE];
	}

	so_far = 0;

	status.doing_what = _("converting audio");
	status.progress = 0.0;

	while (!status.cancel) {

		long nread;
		long x;
		long chn;

		if ((nread = sf_readf_float (in, data, BLOCKSIZE)) == 0) {
			break;
		}

		/* de-interleave */
				
		for (chn = 0; chn < info.channels; ++chn) {
			for (x = chn, n = 0; n < nread; x += info.channels, ++n) {
				channel_data[chn][n] = (Sample) data[x];
			}
		}

		/* flush to disk */

		for (chn = 0; chn < info.channels; ++chn) {
			newfiles[chn]->write (channel_data[chn], nread);
		}

		so_far += nread;
		status.progress = so_far / (float) (info.frames * info.channels);
	}

	if (status.multichan) {
		status.doing_what = _("building region");
	} else {
		status.doing_what = _("building regions");
	}

	status.freeze = true;

	time_t xnow;
	time (&xnow);
	now = localtime (&xnow);

	if (status.cancel) {
		goto out;
	}

	if (status.multichan) {
		/* all sources are used in a single multichannel region */
		for (n = 0; n < nfiles && !status.cancel; ++n) {
			/* flush the final length to the header */
			newfiles[n]->update_header(0, *now, xnow);
			sources.push_back(newfiles[n]);
		}

		AudioRegion *r = new AudioRegion (sources, 0, newfiles[0]->length(), region_name_from_path (PBD::basename(basepath)),
					0, AudioRegion::Flag (AudioRegion::DefaultFlags | AudioRegion::WholeFile));
		
		new_regions.push_back (r);

	} else {
		for (n = 0; n < nfiles && !status.cancel; ++n) {

			/* flush the final length to the header */

			newfiles[n]->update_header(0, *now, xnow);

			/* The sources had zero-length when created, which means that the Session
			   did not bother to create whole-file AudioRegions for them. Do it now.
			*/
		
			AudioRegion *r = new AudioRegion (*newfiles[n], 0, newfiles[n]->length(), region_name_from_path (PBD::basename (newfiles[n]->name())),
						0, AudioRegion::Flag (AudioRegion::DefaultFlags | AudioRegion::WholeFile | AudioRegion::Import));

			new_regions.push_back (r);
		}
	}
	
	/* save state so that we don't lose these new Sources */

	if (!status.cancel) {
		save_state (_name);
	}

	ret = 0;

  out:

	if (data) {
		delete [] data;
	}

	if (channel_data) {
		for (n = 0; n < info.channels; ++n) {
			delete [] channel_data[n];
		}
		delete [] channel_data;
	}

	if (status.cancel) {
		for (vector<AudioRegion *>::iterator i = new_regions.begin(); i != new_regions.end(); ++i) {
			delete *i;
		}

		for (vector<string>::iterator i = new_paths.begin(); i != new_paths.end(); ++i) {
			unlink ((*i).c_str());
		}
	}

	if (newfiles) {
		delete [] newfiles;
	}

	if (tmp_convert_file.length()) {
		unlink(tmp_convert_file.c_str());
	}
	
	sf_close (in);
	status.done = true;
	return ret;
}

string
Session::build_tmp_convert_name(string infile)
{
	string tmp_name(_path + "/." + PBD::basename (infile.c_str()) + "XXXXXX");
	char* tmp = new char[tmp_name.length() + 1];
	tmp_name.copy(tmp, string::npos);
	tmp[tmp_name.length()] = 0;
	mkstemp(tmp);
	string outfile = tmp;
	delete [] tmp;
	
	return outfile;
}

bool
Session::sample_rate_convert (import_status& status, string infile, string& outfile)
{	
	float input [BLOCKSIZE] ;
	float output [BLOCKSIZE] ;

	SF_INFO		sf_info;
	SRC_STATE*	src_state ;
	SRC_DATA	src_data ;
	int			err ;
	sf_count_t	output_count = 0 ;
	sf_count_t  input_count = 0;

	SNDFILE* in = sf_open(infile.c_str(), SFM_READ, &sf_info);
	sf_count_t total_input_frames = sf_info.frames;
	
	outfile = build_tmp_convert_name(infile);
	SNDFILE* out = sf_open(outfile.c_str(), SFM_RDWR, &sf_info);
	if(!out) {
		error << compose(_("Import: could not open temp file: %1"), outfile) << endmsg;
		return false;
	}
	
	sf_seek (in, 0, SEEK_SET) ;
	sf_seek (out, 0, SEEK_SET) ;

	/* Initialize the sample rate converter. */
	if ((src_state = src_new (SRC_SINC_BEST_QUALITY, sf_info.channels, &err)) == 0) {	
		error << compose(_("Import: src_new() failed : %1"), src_strerror (err)) << endmsg ;
		return false ;
	}

	src_data.end_of_input = 0 ; /* Set this later. */

	/* Start with zero to force load in while loop. */
	src_data.input_frames = 0 ;
	src_data.data_in = input ;

	src_data.src_ratio = (1.0 * frame_rate()) / sf_info.samplerate ;

	src_data.data_out = output ;
	src_data.output_frames = BLOCKSIZE / sf_info.channels ;

	while (!status.cancel) {
		/* If the input buffer is empty, refill it. */
		if (src_data.input_frames == 0) {	
			src_data.input_frames = sf_readf_float (in, input, BLOCKSIZE / sf_info.channels) ;
			src_data.data_in = input ;

			/* The last read will not be a full buffer, so snd_of_input. */
			if (src_data.input_frames < (int)BLOCKSIZE / sf_info.channels) {
				src_data.end_of_input = SF_TRUE ;
			}
		} 

		if ((err = src_process (src_state, &src_data))) {
			error << compose(_("Import: %1"), src_strerror (err)) << endmsg ;
			return false ;
		} 

		/* Terminate if at end */
		if (src_data.end_of_input && src_data.output_frames_gen == 0) {
			break ;
		}

		/* Write output. */
		sf_writef_float (out, output, src_data.output_frames_gen) ;
		output_count += src_data.output_frames_gen ;
		input_count += src_data.input_frames_used;

		src_data.data_in += src_data.input_frames_used * sf_info.channels ;
		src_data.input_frames -= src_data.input_frames_used ;
		
		status.progress = (float) input_count / total_input_frames;
	}

	src_state = src_delete (src_state) ;
	sf_close(in);
	sf_close(out);

	status.done = true;

	if (status.cancel) {
		return false;
	} else {
		return true ;
	}
}
