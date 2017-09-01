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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <climits>
#include <cerrno>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdint.h>

#include <sndfile.h>
#include <samplerate.h>

#include "pbd/gstdio_compat.h"
#include <glibmm.h>

#include <boost/scoped_array.hpp>
#include <boost/shared_array.hpp>

#include "pbd/basename.h"
#include "pbd/convert.h"

#include "evoral/SMF.hpp"

#include "ardour/analyser.h"
#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/import_status.h"
#include "ardour/region_factory.h"
#include "ardour/resampled_source.h"
#include "ardour/runtime_functions.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"
#include "ardour/smf_source.h"
#include "ardour/sndfile_helpers.h"
#include "ardour/sndfileimportable.h"
#include "ardour/sndfilesource.h"
#include "ardour/source_factory.h"
#include "ardour/tempo.h"

#ifdef HAVE_COREAUDIO
#include "ardour/caimportable.h"
#endif

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

static boost::shared_ptr<ImportableSource>
open_importable_source (const string& path, framecnt_t samplerate, ARDOUR::SrcQuality quality)
{
	/* try libsndfile first, because it can get BWF info from .wav, which ExtAudioFile cannot.
	   We don't necessarily need that information in an ImportableSource, but it keeps the
	   logic the same as in SourceFactory::create()
	*/

	try {
		boost::shared_ptr<SndFileImportableSource> source(new SndFileImportableSource(path));

		if (source->samplerate() == samplerate) {
			return source;
		}

		/* rewrap as a resampled source */

		return boost::shared_ptr<ImportableSource>(new ResampledImportableSource(source, samplerate, quality));
	}

	catch (...) {

#ifdef HAVE_COREAUDIO

		/* libsndfile failed, see if we can use CoreAudio to handle the IO */

		CAImportableSource* src = new CAImportableSource(path);
		boost::shared_ptr<CAImportableSource> source (src);

		if (source->samplerate() == samplerate) {
			return source;
		}

		/* rewrap as a resampled source */

		return boost::shared_ptr<ImportableSource>(new ResampledImportableSource(source, samplerate, quality));

#else
		throw; // rethrow
#endif

	}
}

vector<string>
Session::get_paths_for_new_sources (bool /*allow_replacing*/, const string& import_file_path, uint32_t channels,
                                    vector<string> const & smf_track_names)

{
	vector<string> new_paths;
	const string basename = basename_nosuffix (import_file_path);

	for (uint32_t n = 0; n < channels; ++n) {

		const DataType type = SMFSource::safe_midi_file_extension (import_file_path) ? DataType::MIDI : DataType::AUDIO;
		string filepath;

		switch (type) {
		case DataType::MIDI:
			assert (smf_track_names.empty() || smf_track_names.size() == channels);
			if (channels > 1) {
				string mchn_name;
				if (smf_track_names.empty() || smf_track_names[n].empty()) {
					mchn_name = string_compose ("%1-t%2", basename, n);
				} else {
					mchn_name = string_compose ("%1-%2", basename, smf_track_names[n]);
				}
				filepath = new_midi_source_path (mchn_name);
			} else {
				filepath = new_midi_source_path (basename);
			}
			break;
		case DataType::AUDIO:
			filepath = new_audio_source_path (basename, channels, n, false, false);
			break;
		}

		if (filepath.empty()) {
			error << string_compose (_("Cannot find new filename for imported file %1"), import_file_path) << endmsg;
			return vector<string>();
		}

		new_paths.push_back (filepath);
	}

	return new_paths;
}

static bool
map_existing_mono_sources (const vector<string>& new_paths, Session& /*sess*/,
                           uint32_t /*samplerate*/, vector<boost::shared_ptr<Source> >& newfiles, Session *session)
{
	for (vector<string>::const_iterator i = new_paths.begin();
	     i != new_paths.end(); ++i)
	{
		boost::shared_ptr<Source> source = session->audio_source_by_path_and_channel(*i, 0);

		if (source == 0) {
			error << string_compose(_("Could not find a source for %1 even though we are updating this file!"), (*i)) << endl;
			return false;
		}

		newfiles.push_back(boost::dynamic_pointer_cast<Source>(source));
	}
	return true;
}

static bool
create_mono_sources_for_writing (const vector<string>& new_paths,
                                 Session& sess, uint32_t samplerate,
                                 vector<boost::shared_ptr<Source> >& newfiles,
                                 framepos_t timeline_position)
{
	for (vector<string>::const_iterator i = new_paths.begin(); i != new_paths.end(); ++i) {

		boost::shared_ptr<Source> source;

		try {
			const DataType type = SMFSource::safe_midi_file_extension (*i) ? DataType::MIDI : DataType::AUDIO;

			source = SourceFactory::createWritable (type, sess,
			                                        i->c_str(),
			                                        false, // destructive
			                                        samplerate);
		}

		catch (const failed_constructor& err) {
			error << string_compose (_("Unable to create file %1 during import"), *i) << endmsg;
			return false;
		}

		newfiles.push_back(boost::dynamic_pointer_cast<Source>(source));

		/* for audio files, reset the timeline position so that any BWF-ish
		   information in the original files we are importing from is maintained.
		*/

		boost::shared_ptr<AudioFileSource> afs;
		if ((afs = boost::dynamic_pointer_cast<AudioFileSource>(source)) != 0) {
			afs->set_timeline_position(timeline_position);
		}
	}
	return true;
}

static string
compose_status_message (const string& path,
                        uint32_t file_samplerate,
                        uint32_t session_samplerate,
                        uint32_t /* current_file */,
                        uint32_t /* total_files */)
{
	if (file_samplerate != session_samplerate) {
		return string_compose (_("Resampling %1 from %2kHz to %3kHz"),
		                       Glib::path_get_basename (path),
		                       file_samplerate/1000.0f,
		                       session_samplerate/1000.0f);
	}

	return string_compose (_("Copying %1"), Glib::path_get_basename (path));
}

static void
write_audio_data_to_new_files (ImportableSource* source, ImportStatus& status,
                               vector<boost::shared_ptr<Source> >& newfiles)
{
	const framecnt_t nframes = ResampledImportableSource::blocksize;
	boost::shared_ptr<AudioFileSource> afs;
	uint32_t channels = source->channels();
	if (channels == 0) {
		return;
	}

	boost::scoped_array<float> data(new float[nframes * channels]);
	vector<boost::shared_array<Sample> > channel_data;

	for (uint32_t n = 0; n < channels; ++n) {
		channel_data.push_back(boost::shared_array<Sample>(new Sample[nframes]));
	}

	float gain = 1;

	boost::shared_ptr<AudioSource> s = boost::dynamic_pointer_cast<AudioSource> (newfiles[0]);
	assert (s);

	status.progress = 0.0f;
	float progress_multiplier = 1;
	float progress_base = 0;

	if (!source->clamped_at_unity() && s->clamped_at_unity()) {

		/* The source we are importing from can return sample values with a magnitude greater than 1,
		   and the file we are writing the imported data to cannot handle such values.  Compute the gain
		   factor required to normalize the input sources to have a magnitude of less than 1.
		*/

		float peak = 0;
		uint32_t read_count = 0;

		while (!status.cancel) {
			framecnt_t const nread = source->read (data.get(), nframes * channels);
			if (nread == 0) {
				break;
			}

			peak = compute_peak (data.get(), nread * channels, peak);

			read_count += nread / channels;
			status.progress = 0.5 * read_count / (source->ratio() * source->length() * channels);
		}

		if (peak >= 1) {
			/* we are out of range: compute a gain to fix it */
			gain = (1 - FLT_EPSILON) / peak;
		}

		source->seek (0);
		progress_multiplier = 0.5;
		progress_base = 0.5;
	}

	framecnt_t read_count = 0;

	while (!status.cancel) {

		framecnt_t nread, nfread;
		uint32_t x;
		uint32_t chn;

		if ((nread = source->read (data.get(), nframes * channels)) == 0) {
#ifdef PLATFORM_WINDOWS
			/* Flush the data once we've finished importing the file. Windows can  */
			/* cache the data for very long periods of time (perhaps not writing   */
			/* it to disk until Ardour closes). So let's force it to flush now.    */
			for (chn = 0; chn < channels; ++chn)
				if ((afs = boost::dynamic_pointer_cast<AudioFileSource>(newfiles[chn])) != 0)
					afs->flush ();
#endif
			break;
		}

		if (gain != 1) {
			/* here is the gain fix for out-of-range sample values that we computed earlier */
			apply_gain_to_buffer (data.get(), nread, gain);
		}

		nfread = nread / channels;

		/* de-interleave */

		for (chn = 0; chn < channels; ++chn) {

			framecnt_t n;
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
		status.progress = progress_base + progress_multiplier * read_count / (source->ratio () * source->length() * channels);
	}
}

static void
write_midi_data_to_new_files (Evoral::SMF* source, ImportStatus& status,
                              vector<boost::shared_ptr<Source> >& newfiles,
                              bool split_type0)
{
	uint32_t buf_size = 4;
	uint8_t* buf      = (uint8_t*) malloc (buf_size);

	status.progress = 0.0f;
	uint16_t num_tracks;
	bool type0 = source->is_type0 () && split_type0;
	const std::set<uint8_t>& chn = source->channels ();

	if (type0) {
		num_tracks = source->channels().size();
	} else {
		num_tracks = source->num_tracks();
	}
	assert (newfiles.size() == num_tracks);

	try {
		vector<boost::shared_ptr<Source> >::iterator s = newfiles.begin();
		std::set<uint8_t>::const_iterator cur_chan = chn.begin();

		for (unsigned i = 1; i <= num_tracks; ++i) {

			boost::shared_ptr<SMFSource> smfs = boost::dynamic_pointer_cast<SMFSource> (*s);

			Glib::Threads::Mutex::Lock source_lock(smfs->mutex());

			smfs->drop_model (source_lock);
			if (type0) {
				source->seek_to_start ();
			} else {
				source->seek_to_track (i);
			}

			uint64_t t       = 0;
			uint32_t delta_t = 0;
			uint32_t size    = 0;
			bool first = true;

			while (!status.cancel) {
				gint note_id_ignored; // imported files either don't have NoteID's or we ignore them.

				size = buf_size;

				int ret = source->read_event (&delta_t, &size, &buf, &note_id_ignored);

				if (size > buf_size) {
					buf_size = size;
				}

				if (ret < 0) { // EOT
					break;
				}

				t += delta_t;

				if (ret == 0) { // Meta
					continue;
				}

				// type-0 files separate by channel
				if (type0) {
					uint8_t type = buf[0] & 0xf0;
					uint8_t chan = buf[0] & 0x0f;
					if (type >= 0x80 && type <= 0xE0) {
						if (chan != *cur_chan) {
							continue;
						}
					}
				}

				if (first) {
					smfs->mark_streaming_write_started (source_lock);
					first = false;
				}

				smfs->append_event_beats(
					source_lock,
					Evoral::Event<Evoral::Beats>(
						Evoral::MIDI_EVENT,
						Evoral::Beats::ticks_at_rate(t, source->ppqn()),
						size,
						buf));

				if (status.progress < 0.99) {
					status.progress += 0.01;
				}
			}

			if (!first) {

				/* we wrote something */

				const framepos_t     pos          = 0;
				const Evoral::Beats  length_beats = Evoral::Beats::ticks_at_rate(t, source->ppqn());
				BeatsFramesConverter converter(smfs->session().tempo_map(), pos);
				smfs->update_length(pos + converter.to(length_beats.round_up_to_beat()));
				smfs->mark_streaming_write_completed (source_lock);

				if (status.cancel) {
					break;
				}
			} else {
				info << string_compose (_("Track %1 of %2 contained no usable MIDI data"), i, num_tracks) << endmsg;
			}

			++s; // next source
			if (type0) {
				++cur_chan;
			}
		}

	} catch (exception& e) {
		error << string_compose (_("MIDI file could not be written (best guess: %1)"), e.what()) << endmsg;
	}

	if (buf) {
		free (buf);
	}
}

static void
remove_file_source (boost::shared_ptr<Source> source)
{
	boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource> (source);

	fs->DropReferences ();

	if (fs) {
		::g_unlink (fs->path().c_str());
	}
}

// This function is still unable to cleanly update an existing source, even though
// it is possible to set the ImportStatus flag accordingly. The functinality
// is disabled at the GUI until the Source implementations are able to provide
// the necessary API.
void
Session::import_files (ImportStatus& status)
{
	typedef vector<boost::shared_ptr<Source> > Sources;
	Sources all_new_sources;
	boost::shared_ptr<AudioFileSource> afs;
	boost::shared_ptr<SMFSource> smfs;
	uint32_t channels = 0;
	vector<string> smf_names;

	status.sources.clear ();

	for (vector<string>::const_iterator p = status.paths.begin();
	     p != status.paths.end() && !status.cancel;
	     ++p)
	{
		boost::shared_ptr<ImportableSource> source;
		std::auto_ptr<Evoral::SMF>          smf_reader;
		const DataType type = SMFSource::safe_midi_file_extension (*p) ? DataType::MIDI : DataType::AUDIO;

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

				if (smf_reader->open(*p)) {
					throw Evoral::SMF::FileError (*p);
				}

				if (smf_reader->is_type0 () && status.split_midi_channels) {
					channels = smf_reader->channels().size();
				} else {
					channels = smf_reader->num_tracks();
					switch (status.midi_track_name_source) {
					case SMFTrackNumber:
						break;
					case SMFTrackName:
						smf_reader->track_names (smf_names);
						break;
					case SMFInstrumentName:
						smf_reader->instrument_names (smf_names);
						break;
					}
				}
			} catch (...) {
				error << _("Import: error opening MIDI file") << endmsg;
				status.done = status.cancel = true;
				return;
			}
		}

		if (channels == 0) {
			error << _("Import: file contains no channels.") << endmsg;
			continue;
		}

		vector<string> new_paths = get_paths_for_new_sources (status.replace_existing_source, *p, channels, smf_names);
		Sources newfiles;
		framepos_t natural_position = source ? source->natural_position() : 0;


		if (status.replace_existing_source) {
			fatal << "THIS IS NOT IMPLEMENTED YET, IT SHOULD NEVER GET CALLED!!! DYING!" << endmsg;
			status.cancel = !map_existing_mono_sources (new_paths, *this, frame_rate(), newfiles, this);
		} else {
			status.cancel = !create_mono_sources_for_writing (new_paths, *this, frame_rate(), newfiles, natural_position);
		}

		// copy on cancel/failure so that any files that were created will be removed below
		std::copy (newfiles.begin(), newfiles.end(), std::back_inserter(all_new_sources));

		if (status.cancel) {
			break;
		}

		for (Sources::iterator i = newfiles.begin(); i != newfiles.end(); ++i) {
			if ((afs = boost::dynamic_pointer_cast<AudioFileSource>(*i)) != 0) {
				afs->prepare_for_peakfile_writes ();
			}
		}

		if (source) { // audio
			status.doing_what = compose_status_message (*p, source->samplerate(),
			                                            frame_rate(), status.current, status.total);
			write_audio_data_to_new_files (source.get(), status, newfiles);
		} else if (smf_reader.get()) { // midi
			status.doing_what = string_compose(_("Loading MIDI file %1"), *p);
			write_midi_data_to_new_files (smf_reader.get(), status, newfiles, status.split_midi_channels);
		}

		++status.current;
		status.progress = 0;
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
				afs->update_header((*x)->natural_position(), *now, xnow);
				afs->done_with_peakfile_writes ();

				/* now that there is data there, requeue the file for analysis */

				if (Config->get_auto_analyse_audio()) {
					Analyser::queue_source_for_analysis (boost::static_pointer_cast<Source>(*x), false);
				}
			}

			/* imported, copied files cannot be written or removed
			 */

			boost::shared_ptr<FileSource> fs = boost::dynamic_pointer_cast<FileSource>(*x);
			if (fs) {
				/* Only audio files should be marked as
				   immutable - we may need to rewrite MIDI
				   files at any time.
				*/
				if (boost::dynamic_pointer_cast<AudioFileSource> (fs)) {
					fs->mark_immutable ();
				} else {
					fs->mark_immutable_except_write ();
				}
				fs->mark_nonremovable ();
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
		try {
			std::for_each (all_new_sources.begin(), all_new_sources.end(), remove_file_source);
		} catch (...) {
			error << _("Failed to remove some files after failed/cancelled import operation") << endmsg;
		}

	}

	status.done = true;
}

