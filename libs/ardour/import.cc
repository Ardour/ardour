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

#include <evoral/SMF.hpp>

#include <ardour/ardour.h>
#include <ardour/session.h>
#include <ardour/session_directory.h>
#include <ardour/audio_diskstream.h>
#include <ardour/audioengine.h>
#include <ardour/sndfilesource.h>
#include <ardour/sndfile_helpers.h>
#include <ardour/audioregion.h>
#include <ardour/region_factory.h>
#include <ardour/source_factory.h>
#include <ardour/resampled_source.h>
#include <ardour/sndfileimportable.h>
#include <ardour/analyser.h>
#include <ardour/smf_source.h>
#include <ardour/tempo.h>

#ifdef HAVE_COREAUDIO
#ifdef USE_COREAUDIO_FOR_FILE_IO
#include <ardour/caimportable.h>
#endif
#endif

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;


static boost::shared_ptr<ImportableSource>
open_importable_source (const string& path, nframes_t samplerate, ARDOUR::SrcQuality quality)
{
#ifdef HAVE_COREAUDIO
#ifdef USE_COREAUDIO_FOR_FILE_IO

	/* see if we can use CoreAudio to handle the IO */
	
	try { 
		CAImportableSource* src = new CAImportableSource(path);
		boost::shared_ptr<CAImportableSource> source (src);
		
		if (source->samplerate() == samplerate) {
			return source;
		}
		
		/* rewrap as a resampled source */

		return boost::shared_ptr<ImportableSource>(new ResampledImportableSource(source, samplerate, quality));
	}

	catch (...) {

		/* fall back to SndFile */
#endif	
#endif

		try { 
			boost::shared_ptr<SndFileImportableSource> source(new SndFileImportableSource(path));
			
			if (source->samplerate() == samplerate) {
				return source;
			}
			
			/* rewrap as a resampled source */
			
			return boost::shared_ptr<ImportableSource>(new ResampledImportableSource(source, samplerate, quality));
		}
		
		catch (...) {
			throw; // rethrow
		}
		
#ifdef HAVE_COREAUDIO		
#ifdef USE_COREAUDIO_FOR_FILE_IO
	}
#endif
#endif
}

static std::string
get_non_existent_filename (DataType type, const bool allow_replacing, const std::string destdir, const std::string& basename, uint channel, uint channels)
{
	char buf[PATH_MAX+1];
	bool goodfile = false;
	string base(basename);
	const char* ext = (type == DataType::AUDIO) ? "wav" : "mid";

	do {

		if (type == DataType::AUDIO && channels == 2) {
			if (channel == 0) {
				snprintf (buf, sizeof(buf), "%s-L.wav", base.c_str());
			} else {
				snprintf (buf, sizeof(buf), "%s-R.wav", base.c_str());
			}
		} else if (channels > 1) {
			snprintf (buf, sizeof(buf), "%s-c%d.%s", base.c_str(), channel, ext);
		} else {
			snprintf (buf, sizeof(buf), "%s.%s", base.c_str(), ext);
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

	SessionDirectory sdir(session_dir);

	for (uint n = 0; n < channels; ++n) {

		const DataType type = (import_file_path.rfind(".mid") != string::npos)
				? DataType::MIDI : DataType::AUDIO;

		std::string filepath = (type == DataType::MIDI)
				? sdir.midi_path().to_string() : sdir.sound_path().to_string();

		filepath += '/';
		filepath += get_non_existent_filename (type, allow_replacing, filepath, basename, n, channels); 
		new_paths.push_back (filepath);
	}

	return new_paths;
}

static bool
map_existing_mono_sources (const vector<string>& new_paths, Session& sess,
		           uint samplerate, vector<boost::shared_ptr<Source> >& newfiles, Session *session)
{
	for (vector<string>::const_iterator i = new_paths.begin();
			i != new_paths.end(); ++i)
	{
		boost::shared_ptr<Source> source = session->source_by_path_and_channel(*i, 0);

		if (source == 0) {
			error << string_compose(_("Could not find a source for %1 even though we are updating this file!"), (*i)) << endl;
			return false;
		}

		newfiles.push_back(boost::dynamic_pointer_cast<Source>(source));
	}
	return true;
}

static bool
create_mono_sources_for_writing (const vector<string>& new_paths, Session& sess,
		uint samplerate, vector<boost::shared_ptr<Source> >& newfiles)
{
	for (vector<string>::const_iterator i = new_paths.begin();
			i != new_paths.end(); ++i)
	{
		boost::shared_ptr<Source> source;

		try
		{
			const DataType type = ((*i).rfind(".mid") != string::npos)
				? DataType::MIDI : DataType::AUDIO;
				
			source = SourceFactory::createWritable (
					type,
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

		newfiles.push_back(boost::dynamic_pointer_cast<Source>(source));
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
write_audio_data_to_new_files (ImportableSource* source, Session::ImportStatus& status,
			       vector<boost::shared_ptr<Source> >& newfiles)
{
	const nframes_t nframes = ResampledImportableSource::blocksize;
	boost::shared_ptr<AudioFileSource> afs;
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
			if ((afs = boost::dynamic_pointer_cast<AudioFileSource>(newfiles[chn])) != 0) {
				afs->write (channel_data[chn].get(), nfread);
			}
		}

		read_count += nread;
		status.progress = read_count / (source->ratio () * source->length() * channels);
	}
}

static void
write_midi_data_to_new_files (Evoral::SMF* source, Session::ImportStatus& status,
		vector<boost::shared_ptr<Source> >& newfiles)
{
	uint32_t buf_size = 4;
	uint8_t* buf      = (uint8_t*)malloc(buf_size);

	status.progress = 0.0f;

	try {

	for (unsigned i = 1; i <= source->num_tracks(); ++i) {
		boost::shared_ptr<SMFSource> smfs = boost::dynamic_pointer_cast<SMFSource>(newfiles[i-1]);
		smfs->drop_model();
		
		source->seek_to_track(i);
	
		uint64_t t       = 0;
		uint32_t delta_t = 0;
		uint32_t size    = 0;
		
		while (!status.cancel) {
			size = buf_size;

			int ret = source->read_event(&delta_t, &size, &buf);
			if (size > buf_size)
				buf_size = size;

			if (ret < 0) { // EOT
				break;
			}
			
			t += delta_t;

			if (ret == 0) { // Meta
				continue;
			}

			smfs->append_event_unlocked_beats(Evoral::Event<double>(0,
					(double)t / (double)source->ppqn(),
					size,
					buf));

			if (status.progress < 0.99)
				status.progress += 0.01;
		}

		const double length_beats = ceil(t / (double)source->ppqn());
		smfs->update_length(0, smfs->time_converter().to(length_beats));
		smfs->end_write();

		if (status.cancel) {
			break;
		}
	}

	} catch (...) {
		error << "Corrupt MIDI file " << source->path() << endl;
	}
}

static void
remove_file_source (boost::shared_ptr<Source> source)
{
	::unlink (source->path().c_str());
}

// This function is still unable to cleanly update an existing source, even though
// it is possible to set the ImportStatus flag accordingly. The functinality
// is disabled at the GUI until the Source implementations are able to provide
// the necessary API.
void
Session::import_audiofiles (ImportStatus& status)
{
	uint32_t cnt = 1;
	typedef vector<boost::shared_ptr<Source> > Sources;
	Sources all_new_sources;
	boost::shared_ptr<AudioFileSource> afs;
	boost::shared_ptr<SMFSource> smfs;
	uint channels = 0;

	status.sources.clear ();
	
	for (vector<Glib::ustring>::iterator p = status.paths.begin();
			p != status.paths.end() && !status.cancel;
			++p, ++cnt)
	{
		boost::shared_ptr<ImportableSource> source;
		std::auto_ptr<Evoral::SMF>          smf_reader;
		const DataType type = ((*p).rfind(".mid") != string::npos) ? 
			DataType::MIDI : DataType::AUDIO;
		
		if (type == DataType::AUDIO) {
			try {
				source = open_importable_source (*p, frame_rate(), status.quality);
				channels = source->channels();
			} catch (const failed_constructor& err) {
				error << string_compose(_("Import: cannot open input sound file \"%1\""), (*p)) << endmsg;
				status.done = status.cancel = true;
				return;
			}

		} else {
			try {
				smf_reader = std::auto_ptr<Evoral::SMF>(new Evoral::SMF());
				smf_reader->open(*p);
				channels = smf_reader->num_tracks();
			} catch (...) {
				error << _("Import: error opening MIDI file") << endmsg;
				status.done = status.cancel = true;
				return;
			}
		}

		vector<string> new_paths = get_paths_for_new_sources (status.replace_existing_source, *p,
								      get_best_session_directory_for_new_source (),
								      channels);
		Sources newfiles;

		if (status.replace_existing_source) {
			fatal << "THIS IS NOT IMPLEMENTED YET, IT SHOULD NEVER GET CALLED!!! DYING!" << endl;
			status.cancel = !map_existing_mono_sources (new_paths, *this, frame_rate(), newfiles, this);
		} else {
			status.cancel = !create_mono_sources_for_writing (new_paths, *this, frame_rate(), newfiles);
		}

		// copy on cancel/failure so that any files that were created will be removed below
		std::copy (newfiles.begin(), newfiles.end(), std::back_inserter(all_new_sources));

		if (status.cancel) break;

		for (Sources::iterator i = newfiles.begin(); i != newfiles.end(); ++i) {
			if ((afs = boost::dynamic_pointer_cast<AudioFileSource>(*i)) != 0) {
				afs->prepare_for_peakfile_writes ();
			}
		}

		if (source) { // audio
			status.doing_what = compose_status_message (*p, source->samplerate(),
								    frame_rate(), cnt, status.total);
			write_audio_data_to_new_files (source.get(), status, newfiles);
		} else if (smf_reader.get()) { // midi
			status.doing_what = string_compose(_("loading MIDI file %1"), *p);
			write_midi_data_to_new_files (smf_reader.get(), status, newfiles);
		}
	}

	if (!status.cancel) {
		struct tm* now;
		time_t xnow;
		time (&xnow);
		now = localtime (&xnow);
		status.freeze = true;

		/* flush the final length(s) to the header(s) */

		for (Sources::iterator x = all_new_sources.begin(); x != all_new_sources.end(); ) {
			if ((afs = boost::dynamic_pointer_cast<AudioFileSource>(*x)) != 0) {
				afs->update_header(0, *now, xnow);
				afs->done_with_peakfile_writes ();
			
				/* now that there is data there, requeue the file for analysis */
				
				if (Config->get_auto_analyse_audio()) {
					Analyser::queue_source_for_analysis (boost::static_pointer_cast<Source>(*x), false);
				}
			}
			
			/* don't create tracks for empty MIDI sources (channels) */

			if ((smfs = boost::dynamic_pointer_cast<SMFSource>(*x)) != 0 && smfs->is_empty()) {
				x = all_new_sources.erase(x);
			} else {
				++x;
			}
		}

		/* save state so that we don't lose these new Sources */

		save_state (_name);

		std::copy (all_new_sources.begin(), all_new_sources.end(), std::back_inserter(status.sources));
	} else {
		// this can throw...but it seems very unlikely
		std::for_each (all_new_sources.begin(), all_new_sources.end(), remove_file_source);
	}

	status.done = true;
}

