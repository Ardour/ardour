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

#include <glibmm.h>

#include <pbd/basename.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audio_diskstream.h>
#include <ardour/sndfilesource.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/audioregion.h>
#include <ardour/region_factory.h>
#include <ardour/source_factory.h>


#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

#define BLOCKSIZE 4096U

class ImportableSource {
   public:
    ImportableSource (SNDFILE* sf, SF_INFO* info) : in (sf), sf_info (info) {}
    virtual ~ImportableSource() {}

    virtual nframes_t read (Sample* buffer, nframes_t nframes) {
	    nframes_t per_channel = nframes / sf_info->channels;
	    per_channel = sf_readf_float (in, buffer, per_channel);
	    return per_channel * sf_info->channels;
    }

    virtual float ratio() const { return 1.0f; }

protected:
       SNDFILE* in;
       SF_INFO* sf_info;
};

class ResampledImportableSource : public ImportableSource {
   public:
    ResampledImportableSource (SNDFILE* sf, SF_INFO* info, nframes_t rate) : ImportableSource (sf, info) {
	    int err;

	    sf_seek (in, 0, SEEK_SET) ;
	    
	    /* Initialize the sample rate converter. */
	    
	    if ((src_state = src_new (SRC_SINC_BEST_QUALITY, sf_info->channels, &err)) == 0) {	
		    error << string_compose(_("Import: src_new() failed : %1"), src_strerror (err)) << endmsg ;
		    throw failed_constructor ();
	    }
	    
	    src_data.end_of_input = 0 ; /* Set this later. */
	    
	    /* Start with zero to force load in while loop. */
	    
	    src_data.input_frames = 0 ;
	    src_data.data_in = input ;
	    
	    src_data.src_ratio = ((float) rate) / sf_info->samplerate ;

    }

    ~ResampledImportableSource () { 
	src_state = src_delete (src_state) ;
    }

    nframes_t read (Sample* buffer, nframes_t nframes);
    
    float ratio() const { return src_data.src_ratio; }

   private:
	float input[BLOCKSIZE];
	SRC_STATE*	src_state;
	SRC_DATA	src_data;
};

int
Session::import_audiofile (import_status& status)
{
	SNDFILE *in;
	vector<boost::shared_ptr<AudioFileSource> > newfiles;
	SourceList sources;
	SF_INFO info;
	float *data = 0;
	Sample **channel_data = 0;
	long nfiles = 0;
	long n;
	string basepath;
	string sounds_dir;
	nframes_t so_far;
	char buf[PATH_MAX+1];
	int ret = -1;
	vector<string> new_paths;
	struct tm* now;
	ImportableSource* importable = 0;
	const nframes_t nframes = BLOCKSIZE;

	status.new_regions.clear ();

	if ((in = sf_open (status.paths.front().c_str(), SFM_READ, &info)) == 0) {
		error << string_compose(_("Import: cannot open input sound file \"%1\""), status.paths.front()) << endmsg;
		return -1;
	}

	if ((nframes_t) info.samplerate != frame_rate()) {
		importable = new ResampledImportableSource (in, &info, frame_rate());
	} else {
		importable = new ImportableSource (in, &info);
	}

	for (n = 0; n < info.channels; ++n) {
		newfiles.push_back (boost::shared_ptr<AudioFileSource>());
	}

	sounds_dir = discover_best_sound_dir ();
	basepath = PBD::basename_nosuffix (status.paths.front());

	for (n = 0; n < info.channels; ++n) {

		bool goodfile = false;

		do {
			if (info.channels == 2) {
				if (n == 0) {
					snprintf (buf, sizeof(buf), "%s/%s-L.wav", sounds_dir.c_str(), basepath.c_str());
				} else {
					snprintf (buf, sizeof(buf), "%s/%s-R.wav", sounds_dir.c_str(), basepath.c_str());
				}
			} else if (info.channels > 1) {
				snprintf (buf, sizeof(buf), "%s/%s-c%lu.wav", sounds_dir.c_str(), basepath.c_str(), n+1);
			} else {
				snprintf (buf, sizeof(buf), "%s/%s.wav", sounds_dir.c_str(), basepath.c_str());
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
			newfiles[n] = boost::dynamic_pointer_cast<AudioFileSource> (SourceFactory::createWritable (*this, buf, false, frame_rate()));
		}

		catch (failed_constructor& err) {
			error << string_compose(_("Session::import_audiofile: cannot open new file source for channel %1"), n+1) << endmsg;
			goto out;
		}

		new_paths.push_back (buf);
		nfiles++;
	}
	
	data = new float[nframes * info.channels];
	channel_data = new Sample * [ info.channels ];
	
	for (n = 0; n < info.channels; ++n) {
		channel_data[n] = new Sample[nframes];
	}

	so_far = 0;

	status.doing_what = _("converting audio");
	status.progress = 0.0;

	while (!status.cancel) {

		nframes_t nread;
		long x;
		long chn;
		
		if ((nread = importable->read (data, nframes)) == 0) {
			break;
		}

		/* de-interleave */
				
		for (chn = 0; chn < info.channels; ++chn) {
			
			for (x = chn, n = 0; n < nframes; x += info.channels, ++n) {
				channel_data[chn][n] = (Sample) data[x];
			}
		}

		/* flush to disk */

		for (chn = 0; chn < info.channels; ++chn) {
			newfiles[chn]->write (channel_data[chn], nread / info.channels);
		}

		so_far += nread;
		status.progress = so_far / (importable->ratio () * info.frames * info.channels);
	}

	if (status.cancel) {
		goto out;
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

	if (status.multichan) {
		/* all sources are used in a single multichannel region */

		for (n = 0; n < nfiles && !status.cancel; ++n) {
			/* flush the final length to the header */
			newfiles[n]->update_header(0, *now, xnow);
			sources.push_back(newfiles[n]);
		}

		bool strip_paired_suffixes = (newfiles.size() > 1);

		boost::shared_ptr<AudioRegion> r (boost::dynamic_pointer_cast<AudioRegion> 
						  (RegionFactory::create (sources, 0, 
									  newfiles[0]->length(), 
									  region_name_from_path (basepath, strip_paired_suffixes),
									  0, AudioRegion::Flag (AudioRegion::DefaultFlags | AudioRegion::WholeFile))));
		
		status.new_regions.push_back (r);

	} else {
		for (n = 0; n < nfiles && !status.cancel; ++n) {

			/* flush the final length to the header */

			newfiles[n]->update_header(0, *now, xnow);

			/* The sources had zero-length when created, which means that the Session
			   did not bother to create whole-file AudioRegions for them. Do it now.

			   Note: leave any trailing paired indicators from the file names as part
			   of the region name.
			*/
		
			status.new_regions.push_back (boost::dynamic_pointer_cast<AudioRegion> 
						      (RegionFactory::create (boost::static_pointer_cast<Source> (newfiles[n]), 0, newfiles[n]->length(), 
									      region_name_from_path (newfiles[n]->name(), false),
									      0, AudioRegion::Flag (AudioRegion::DefaultFlags | AudioRegion::WholeFile | AudioRegion::Import))));
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

		status.new_regions.clear ();

		for (vector<string>::iterator i = new_paths.begin(); i != new_paths.end(); ++i) {
			unlink ((*i).c_str());
		}
	}

	if (importable) {
		delete importable;
	}

	sf_close (in);	
	status.done = true;

	return ret;
}

nframes_t 
ResampledImportableSource::read (Sample* output, nframes_t nframes)
{
	int err;

	/* If the input buffer is empty, refill it. */
	
	if (src_data.input_frames == 0) {	

		src_data.input_frames = ImportableSource::read (input, BLOCKSIZE);

		/* The last read will not be a full buffer, so set end_of_input. */

		if ((nframes_t) src_data.input_frames < BLOCKSIZE) {
			src_data.end_of_input = SF_TRUE ;
		}		

		src_data.input_frames /= sf_info->channels;
		src_data.data_in = input ;
	} 
	
	src_data.data_out = output;

	if (!src_data.end_of_input) {
		src_data.output_frames = nframes / sf_info->channels ;
	} else {
		src_data.output_frames = src_data.input_frames;
	}

	if ((err = src_process (src_state, &src_data))) {
		error << string_compose(_("Import: %1"), src_strerror (err)) << endmsg ;
		return false ;
	} 
	
	/* Terminate if at end */
	
	if (src_data.end_of_input && src_data.output_frames_gen == 0) {
		return 0;
	}
	
	src_data.data_in += src_data.input_frames_used * sf_info->channels ;
	src_data.input_frames -= src_data.input_frames_used ;

	return nframes;
}

