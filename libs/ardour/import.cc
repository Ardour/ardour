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

#include <boost/scoped_array.hpp>
#include <boost/shared_array.hpp>

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
#include <ardour/analyser.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

static std::auto_ptr<ImportableSource>
open_importable_source (const string& path, nframes_t samplerate, ARDOUR::SrcQuality quality)
{
	std::auto_ptr<ImportableSource> source(new ImportableSource(path));

	if (source->samplerate() == samplerate) {
		return source;
	}
	
	return std::auto_ptr<ImportableSource>(new ResampledImportableSource(path, samplerate, quality));
}

static std::string
get_non_existent_filename (const bool allow_replacing, const std::string destdir, const std::string& basename, uint channel, uint channels)
{
	char buf[PATH_MAX+1];
	bool goodfile = false;
	string base(basename);

	do {
		if (channels == 2) {
			if (channel == 0) {
				snprintf (buf, sizeof(buf), "%s-L.wav", base.c_str());
			} else {
				snprintf (buf, sizeof(buf), "%s-R.wav", base.c_str());
			}
		} else if (channels > 1) {
			snprintf (buf, sizeof(buf), "%s-c%d.wav", base.c_str(), channel+1);
		} else {
			snprintf (buf, sizeof(buf), "%s.wav", base.c_str());
		}
		

		string tempname = destdir + "/" + buf;
		if (!allow_replacing && Glib::file_test (tempname, Glib::FILE_TEST_EXISTS)) {

			/* if the file already exists, we must come up with
			 *  a new name for it.  for now we just keep appending
			 *  _ to basename
			 */

			base += "_";

		} else {

			goodfile = true;
		}

	} while ( !goodfile);

	return buf;
}

static vector<string>
get_paths_for_new_sources (const bool allow_replacing, const string& import_file_path, const string& session_dir, uint channels)
{
	vector<string> new_paths;
	const string basename = basename_nosuffix (import_file_path);

	for (uint n = 0; n < channels; ++n) {

		std::string filepath;

		filepath = session_dir;
		filepath += '/';
		filepath += get_non_existent_filename (allow_replacing, session_dir, basename, n, channels); 

		new_paths.push_back (filepath);
	}

	return new_paths;
}

static bool
map_existing_mono_sources (const vector<string>& new_paths, Session& sess,
		           uint samplerate, vector<boost::shared_ptr<AudioFileSource> >& newfiles, Session *session)
{
	for (vector<string>::const_iterator i = new_paths.begin();
			i != new_paths.end(); ++i)
	{
		boost::shared_ptr<Source> source = session->source_by_path_and_channel(*i, 0);

		if (source == 0) {
			error << string_compose(_("Could not find a source for %1 even though we are updating this file!"), (*i)) << endl;
			return false;
		}

		newfiles.push_back(boost::dynamic_pointer_cast<AudioFileSource>(source));
	}
	return true;
}

static bool
create_mono_sources_for_writing (const vector<string>& new_paths, Session& sess,
		uint samplerate, vector<boost::shared_ptr<AudioFileSource> >& newfiles)
{
	for (vector<string>::const_iterator i = new_paths.begin();
			i != new_paths.end(); ++i)
	{
		boost::shared_ptr<Source> source;

		try
		{
			source = SourceFactory::createWritable (
				DataType::AUDIO,
				sess,
				i->c_str(),
				false, // destructive
				samplerate
				);
		}
		catch (const failed_constructor& err)
		{
			error << string_compose (_("Unable to create file %1 during import"), *i) << endmsg;
			return false;
		}

		newfiles.push_back(boost::dynamic_pointer_cast<AudioFileSource>(source));
	}
	return true;
}

static Glib::ustring
compose_status_message (const string& path,
			uint file_samplerate,
			uint session_samplerate,
			uint current_file,
			uint total_files)
{
	if (file_samplerate != session_samplerate) {
		return string_compose (_("converting %1\n(resample from %2KHz to %3KHz)\n(%4 of %5)"),
				       Glib::path_get_basename (path),
				       file_samplerate/1000.0f,
				       session_samplerate/1000.0f,
				       current_file, total_files);
	}

	return  string_compose (_("converting %1\n(%2 of %3)"), 
				Glib::path_get_basename (path),
				current_file, total_files);
}

static void
write_audio_data_to_new_files (ImportableSource* source, Session::import_status& status,
			       vector<boost::shared_ptr<AudioFileSource> >& newfiles)
{
	const nframes_t nframes = ResampledImportableSource::blocksize;
	uint channels = source->channels();

	boost::scoped_array<float> data(new float[nframes * channels]);
	vector<boost::shared_array<Sample> > channel_data;

	for (uint n = 0; n < channels; ++n) {
		channel_data.push_back(boost::shared_array<Sample>(new Sample[nframes]));
	}
	
	uint read_count = 0;
	status.progress = 0.0f;

	while (!status.cancel) {

		nframes_t nread, nfread;
		uint x;
		uint chn;

		if ((nread = source->read (data.get(), nframes)) == 0) {
			break;
		}
		nfread = nread / channels;

		/* de-interleave */

		for (chn = 0; chn < channels; ++chn) {

			nframes_t n;
			for (x = chn, n = 0; n < nfread; x += channels, ++n) {
				channel_data[chn][n] = (Sample) data[x];
			}
		}

		/* flush to disk */

		for (chn = 0; chn < channels; ++chn) {
			newfiles[chn]->write (channel_data[chn].get(), nfread);
		}

		read_count += nread;
		status.progress = read_count / (source->ratio () * source->length() * channels);
	}
}

static void
remove_file_source (boost::shared_ptr<AudioFileSource> file_source)
{
	::unlink (file_source->path().c_str());
}

// This function is still unable to cleanly update an existing source, even though
// it is possible to set the import_status flag accordingly. The functinality
// is disabled at the GUI until the Source implementations are able to provide
// the necessary API.
void
Session::import_audiofiles (import_status& status)
{
	uint32_t cnt = 1;
	typedef vector<boost::shared_ptr<AudioFileSource> > AudioSources;
	AudioSources all_new_sources;

	status.sources.clear ();
	
	for (vector<Glib::ustring>::iterator p = status.paths.begin();
			p != status.paths.end() && !status.cancel;
			++p, ++cnt)
	{
		std::auto_ptr<ImportableSource> source;

		try
		{
			source = open_importable_source (*p, frame_rate(), status.quality);
		}
		catch (const failed_constructor& err)
		{
			error << string_compose(_("Import: cannot open input sound file \"%1\""), (*p)) << endmsg;
			status.done = status.cancel = true;
			return;
		}

		vector<string> new_paths = get_paths_for_new_sources (status.replace_existing_source,
								      *p,
				get_best_session_directory_for_new_source (),
				source->channels());
		
		AudioSources newfiles;

		if (status.replace_existing_source) {
			fatal << "THIS IS NOT IMPLEMENTED YET, IT SHOULD NEVER GET CALLED!!! DYING!" << endl;
			status.cancel = !map_existing_mono_sources (new_paths, *this, frame_rate(), newfiles, this);
		} else {
			status.cancel = !create_mono_sources_for_writing (new_paths, *this, frame_rate(), newfiles);
		}

		// copy on cancel/failure so that any files that were created will be removed below
		std::copy (newfiles.begin(), newfiles.end(), std::back_inserter(all_new_sources));

		if (status.cancel) break;

		for (AudioSources::iterator i = newfiles.begin(); i != newfiles.end(); ++i) {
			(*i)->prepare_for_peakfile_writes ();
		}

		status.doing_what = compose_status_message (*p, source->samplerate(),
				frame_rate(), cnt, status.paths.size());

		write_audio_data_to_new_files (source.get(), status, newfiles);
	}

	if (!status.cancel) {
		struct tm* now;
		time_t xnow;
		time (&xnow);
		now = localtime (&xnow);
		status.freeze = true;

		/* flush the final length(s) to the header(s) */

		for (AudioSources::iterator x = all_new_sources.begin(); x != all_new_sources.end(); ++x)
		{
			(*x)->update_header(0, *now, xnow);
			(*x)->done_with_peakfile_writes ();
			
			/* now that there is data there, requeue the file for analysis */
			
			Analyser::queue_source_for_analysis (boost::static_pointer_cast<Source>(*x), false);
		}

		/* save state so that we don't lose these new Sources */

		save_state (_name);

		std::copy (all_new_sources.begin(), all_new_sources.end(),
				std::back_inserter(status.sources));
	} else {
		// this can throw...but it seems very unlikely
		std::for_each (all_new_sources.begin(), all_new_sources.end(), remove_file_source);
	}

	status.done = true;
}

