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
#include <pbd/convert.h>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/audio_diskstream.h>
#include <ardour/sndfilesource.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/audioregion.h>
#include <ardour/region_factory.h>
#include <ardour/source_factory.h>
#include <ardour/resampled_source.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

int
Session::import_audiofile (import_status& status)
{
	SNDFILE *in;
	vector<boost::shared_ptr<AudioFileSource> > newfiles;
	SF_INFO info;
	float *data = 0;
	Sample **channel_data = 0;
	int nfiles = 0;
	string basepath;
	string sounds_dir;
	nframes_t so_far;
	char buf[PATH_MAX+1];
	int ret = -1;
	vector<string> new_paths;
	struct tm* now;
	ImportableSource* importable = 0;
	const nframes_t nframes = ResampledImportableSource::blocksize;
	uint32_t cnt = 1;

	status.sources.clear ();

	for (vector<Glib::ustring>::iterator p = status.paths.begin(); p != status.paths.end(); ++p, ++cnt) {

		if ((in = sf_open ((*p).c_str(), SFM_READ, &info)) == 0) {
			error << string_compose(_("Import: cannot open input sound file \"%1\""), (*p)) << endmsg;
			status.done = 1;
			status.cancel = 1;
			return -1;
		}
		
		if ((nframes_t) info.samplerate != frame_rate()) {
			importable = new ResampledImportableSource (in, &info, frame_rate());
		} else {
			importable = new ImportableSource (in, &info);
		}
		
		newfiles.clear ();

		for (int n = 0; n < info.channels; ++n) {
			newfiles.push_back (boost::shared_ptr<AudioFileSource>());
		}
		
		sounds_dir = discover_best_sound_dir ();
		basepath = PBD::basename_nosuffix ((*p));
		
		for (int n = 0; n < info.channels; ++n) {
			
			bool goodfile = false;
			
			do {
				if (info.channels == 2) {
					if (n == 0) {
						snprintf (buf, sizeof(buf), "%s/%s-L.wav", sounds_dir.c_str(), basepath.c_str());
					} else {
						snprintf (buf, sizeof(buf), "%s/%s-R.wav", sounds_dir.c_str(), basepath.c_str());
					}
				} else if (info.channels > 1) {
					snprintf (buf, sizeof(buf), "%s/%s-c%d.wav", sounds_dir.c_str(), basepath.c_str(), n+1);
				} else {
					snprintf (buf, sizeof(buf), "%s/%s.wav", sounds_dir.c_str(), basepath.c_str());
				}

				if (Glib::file_test (buf, Glib::FILE_TEST_EXISTS)) {

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
	
		for (int n = 0; n < info.channels; ++n) {
			channel_data[n] = new Sample[nframes];
		}

		so_far = 0;

		if ((nframes_t) info.samplerate != frame_rate()) {
			status.doing_what = string_compose (_("converting %1\n(resample from %2KHz to %3KHz)\n(%4 of %5)"),
							    basepath,
							    info.samplerate/1000.0f,
							    frame_rate()/1000.0f,
							    cnt, status.paths.size());
							    
		} else {
			status.doing_what = string_compose (_("converting %1\n(%2 of %3)"), 
							    basepath,
							    cnt, status.paths.size());

		}

		status.progress = 0.0;

		while (!status.cancel) {

			nframes_t nread, nfread;
			long x;
			long chn;
		
			if ((nread = importable->read (data, nframes)) == 0) {
				break;
			}
			nfread = nread / info.channels;

			/* de-interleave */
				
			for (chn = 0; chn < info.channels; ++chn) {

				nframes_t n;
				for (x = chn, n = 0; n < nfread; x += info.channels, ++n) {
					channel_data[chn][n] = (Sample) data[x];
				}
			}

			/* flush to disk */

			for (chn = 0; chn < info.channels; ++chn) {
				newfiles[chn]->write (channel_data[chn], nfread);
			}

			so_far += nread;
			status.progress = so_far / (importable->ratio () * info.frames * info.channels);
		}

		if (status.cancel) {
			goto out;
		}
		
		for (int n = 0; n < info.channels; ++n) {
			status.sources.push_back (newfiles[n]);
		}

		if (status.cancel) {
			goto out;
		}
	}
	
	status.freeze = true;

	time_t xnow;
	time (&xnow);
	now = localtime (&xnow);

	/* flush the final length(s) to the header(s) */

	for (SourceList::iterator x = status.sources.begin(); x != status.sources.end() && !status.cancel; ++x) {
		boost::dynamic_pointer_cast<AudioFileSource>(*x)->update_header(0, *now, xnow);
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
		for (int n = 0; n < info.channels; ++n) {
			delete [] channel_data[n];
		}
		delete [] channel_data;
	}

	if (status.cancel) {

		status.sources.clear ();

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
