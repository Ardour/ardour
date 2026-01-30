/*
 * Copyright (C) 2000-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2006-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <cstdio>
#include <cstdlib>
#include <string>
#include <climits>
#include <cerrno>
#include <sys/stat.h>
#include <time.h>
#include <stdint.h>

#include <sndfile.h>
#include <samplerate.h>

#include "pbd/gstdio_compat.h"
#include <glibmm.h>

#include "pbd/basename.h"
#include "pbd/convert.h"

#include "evoral/SMF.h"

#include "ardour/analyser.h"
#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/ffmpegfileimportable.h"
#include "ardour/import_status.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/mp3fileimportable.h"
#include "ardour/playlist.h"
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

static std::shared_ptr<ImportableSource>
open_importable_source (const string& path, samplecnt_t samplerate, ARDOUR::SrcQuality quality)
{
	/* try libsndfile first, because it can get BWF info from .wav, which ExtAudioFile cannot.
	 * We don't necessarily need that information in an ImportableSource, but it keeps the
	 * logic the same as in SourceFactory::create()
	 */

	try {
		std::shared_ptr<SndFileImportableSource> source(new SndFileImportableSource(path));

		if (source->samplerate() == samplerate) {
			return source;
		}

		/* rewrap as a resampled source */
		return std::shared_ptr<ImportableSource>(new ResampledImportableSource(source, samplerate, quality));
	} catch (...) { }

	/* libsndfile failed, see if we can use CoreAudio to handle the IO */
#ifdef HAVE_COREAUDIO
	try {
		CAImportableSource* src = new CAImportableSource(path);
		std::shared_ptr<CAImportableSource> source (src);

		if (source->samplerate() == samplerate) {
			return source;
		}

		/* rewrap as a resampled source */
		return std::shared_ptr<ImportableSource>(new ResampledImportableSource(source, samplerate, quality));
	} catch (...) { }
#endif

	/* libsndfile and CoreAudioFile failed, try minimp3-decoder */
	try {
		std::shared_ptr<Mp3FileImportableSource> source(new Mp3FileImportableSource(path));

		if (source->samplerate() == samplerate) {
			return source;
		}

		/* rewrap as a resampled source */
		return std::shared_ptr<ImportableSource>(new ResampledImportableSource(source, samplerate, quality));
	} catch (...) { }

	/* finally try FFMPEG */
	try {
		std::shared_ptr<FFMPEGFileImportableSource> source(new FFMPEGFileImportableSource(path));

		if (source->samplerate() == samplerate) {
			return source;
		}

		/* rewrap as a resampled source */
		return std::shared_ptr<ImportableSource>(new ResampledImportableSource(source, samplerate, quality));
	} catch (...) { }

	throw failed_constructor ();
}

vector<string>
Session::get_paths_for_new_sources (bool /*allow_replacing*/, const string& import_file_path, uint32_t channels,
                                    vector<string> const & smf_names, bool use_smf_file_names)

{
	vector<string> new_paths;
	const string basename = basename_nosuffix (import_file_path);

	for (uint32_t n = 0; n < channels; ++n) {

		const DataType type = SMFSource::safe_midi_file_extension (import_file_path) ? DataType::MIDI : DataType::AUDIO;
		string filepath;

		switch (type) {
		case DataType::MIDI:
			if (channels > 1) {
				assert (smf_names.size() == channels);
				if (use_smf_file_names) {
					string mchn_name = string_compose ("%1.%2", basename, smf_names[n]);
					filepath = new_midi_source_path (mchn_name);
				} else {
					filepath = new_midi_source_path (smf_names[n]);
				}
			} else {
				filepath = new_midi_source_path (basename);
			}
			break;
		case DataType::AUDIO:
			filepath = new_audio_source_path (basename, channels, n, false);
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
                           uint32_t /*samplerate*/, vector<std::shared_ptr<Source> >& newfiles, Session *session)
{
	for (vector<string>::const_iterator i = new_paths.begin();
	     i != new_paths.end(); ++i)
	{
		std::shared_ptr<Source> source = session->audio_source_by_path_and_channel(*i, 0);

		if (source == 0) {
			error << string_compose(_("Could not find a source for %1 even though we are updating this file!"), (*i)) << endl;
			return false;
		}

		newfiles.push_back(std::dynamic_pointer_cast<Source>(source));
	}
	return true;
}

static bool
create_mono_sources_for_writing (const vector<string>& new_paths,
                                 Session& sess, uint32_t samplerate,
                                 vector<std::shared_ptr<Source> >& newfiles,
                                 samplepos_t natural_position, bool announce)
{
	for (vector<string>::const_iterator i = new_paths.begin(); i != new_paths.end(); ++i) {

		std::shared_ptr<Source> source;

		try {
			const DataType type = SMFSource::safe_midi_file_extension (*i) ? DataType::MIDI : DataType::AUDIO;

			source = SourceFactory::createWritable (type, sess, i->c_str(), samplerate, announce);
		}

		catch (const failed_constructor& err) {
			error << string_compose (_("Unable to create file %1 during import"), *i) << endmsg;
			return false;
		}

		newfiles.push_back(std::dynamic_pointer_cast<Source>(source));

		/* for audio files, reset the timeline position so that any BWF-ish
		   information in the original files we are importing from is maintained.
		*/

		std::shared_ptr<AudioFileSource> afs;
		if ((afs = std::dynamic_pointer_cast<AudioFileSource>(source)) != 0) {
			afs->set_natural_position (timepos_t (natural_position));
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
                               vector<std::shared_ptr<Source> >& newfiles)
{
	const samplecnt_t nframes = ResampledImportableSource::blocksize;
	std::shared_ptr<AudioFileSource> afs;
	uint32_t channels = source->channels();
	if (channels == 0) {
		return;
	}

	std::unique_ptr<float[]> data(new float[nframes * channels]);
	vector<std::shared_ptr<Sample[]> > channel_data;

	for (uint32_t n = 0; n < channels; ++n) {
		channel_data.push_back(std::shared_ptr<Sample[]>(new Sample[nframes]));
	}

	float gain = 1;

	std::shared_ptr<AudioSource> s = std::dynamic_pointer_cast<AudioSource> (newfiles[0]);
	assert (s);

	status.progress = 0.0f;
	float progress_multiplier = 1;
	float progress_base = 0;
	const float progress_length = source->ratio() * source->length();

	if (!source->clamped_at_unity() && s->clamped_at_unity()) {

		/* The source we are importing from can return sample values with a magnitude greater than 1,
		   and the file we are writing the imported data to cannot handle such values.  Compute the gain
		   factor required to normalize the input sources to have a magnitude of less than 1.
		*/

		float peak = 0;
		uint32_t read_count = 0;

		while (!status.cancel) {
			samplecnt_t const nread = source->read (data.get(), nframes * channels);
			if (nread == 0) {
				break;
			}

			peak = compute_peak (data.get(), nread, peak);

			read_count += nread / channels;
			status.progress = 0.5 * read_count / progress_length;
		}

		if (peak >= 1) {
			/* we are out of range: compute a gain to fix it */
			gain = (1 - FLT_EPSILON) / peak;
		}

		source->seek (0);
		progress_multiplier = 0.5;
		progress_base = 0.5;
	}

	samplecnt_t read_count = 0;

	while (!status.cancel) {

		samplecnt_t nread, nfread;
		uint32_t x;
		uint32_t chn;

		if ((nread = source->read (data.get(), nframes * channels)) == 0) {
#ifdef PLATFORM_WINDOWS
			/* Flush the data once we've finished importing the file. Windows can  */
			/* cache the data for very long periods of time (perhaps not writing   */
			/* it to disk until Ardour closes). So let's force it to flush now.    */
			for (chn = 0; chn < channels; ++chn)
				if ((afs = std::dynamic_pointer_cast<AudioFileSource>(newfiles[chn])) != 0)
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

			samplecnt_t n;
			for (x = chn, n = 0; n < nfread; x += channels, ++n) {
				channel_data[chn][n] = (Sample) data[x];
			}
		}

		/* flush to disk */

		for (chn = 0; chn < channels; ++chn) {
			if ((afs = std::dynamic_pointer_cast<AudioFileSource>(newfiles[chn])) != 0) {
				afs->write (channel_data[chn].get(), nfread);
			}
		}

		read_count += nfread;
		status.progress = progress_base + progress_multiplier * read_count / progress_length;
	}
}

static void
write_midi_type0_data_to_one_file (Evoral::SMF* source, ImportStatus& status, size_t n, size_t nfiles, std::shared_ptr<SMFSource> smfs, bool split_midi_channels, int channel)
{
	uint32_t bufsize = 4;
	uint8_t* buf     = (uint8_t*) malloc (bufsize);
	Evoral::event_id_t ignored_note_id; /* imported files either don't have noted IDs or we ignore them */

	Source::WriterLock target_lock (smfs->mutex());
	smfs->mark_streaming_write_started (target_lock);
	smfs->drop_model (target_lock);

	try {

		source->seek_to_start();

		uint64_t t       = 0;
		uint32_t delta_t = 0;
		uint32_t size    = 0;
		uint32_t written = 0;

		while (!status.cancel) {

			size = bufsize;

			/* ret will be:

			   < 0 : error/end-of-track
			   0   : metadata event, size gives the byte count
			   > 0 : regular event for our consideration

			*/

			int ret = source->read_event (&delta_t, &size, &buf, &ignored_note_id);

			if (ret < 0) { // EOT
				break;
			}

			t += delta_t;

			if (size == 0) {
				/* metadata not meant for us */
				continue;
			}

			if (ret == 0) {
				/* set note id, but we ignored it */
				continue;
			}

			if (size > bufsize) {
				bufsize = size;
			}

			/* if requested by user, each sourcefile gets only a single channel's data */

			if (ret > 0 && split_midi_channels) {
				uint8_t type = buf[0] & 0xf0;
				uint8_t chan = buf[0] & 0x0f;
				if (type >= 0x80 && type <= 0xE0) {
					if (chan != channel) {
						continue;
					}
				}
			}

			smfs->append_event_beats (
				target_lock,
				Evoral::Event<Temporal::Beats>(
					Evoral::MIDI_EVENT,
					Temporal::Beats::ticks_at_rate(t, source->ppqn()),
					size,
					buf));

			written++;

			if (status.progress < 0.99) {
				status.progress += 0.01;
			}
		}

		if (written) {

			/* we wrote something */

			smfs->mark_streaming_write_completed (target_lock, timecnt_t (source->duration()));
			smfs->round_length_to_bars (t);

			/* the streaming write that we've just finished
			 * only wrote data to the SMF object, which is
			 * ultimately an on-disk data structure. So now
			 * we pull the data back from disk to build our
			 * in-memory MidiModel version.
			 */

			smfs->load_model (target_lock, true);

		} else {
			info << string_compose (_("No usable MIDI data found for file %1 of %2"), n, nfiles) << endmsg;
		}

	} catch (exception& e) {
		error << string_compose (_("MIDI file could not be written (best guess: %1)"), e.what()) << endmsg;
	}

	free (buf);

}

static bool
track_contains_tempo_or_key_metadata (Evoral::SMF* source, int track)
{
	if (source->seek_to_track (track+1) != 0) {
		return false;
	}

	uint8_t* buf     = (uint8_t*) malloc (4);
	uint32_t delta_t = 0;
	uint32_t size    = 4;
	bool seen = false;
	Evoral::event_id_t ignored_note_id; /* imported files either don't have noted IDs or we ignore them */

	while (true) {
		int ret = source->read_event (&delta_t, &size, &buf, &ignored_note_id);

		if (ret < 0) { // EOT
			break;
		}

		if (size == 0) {
			/* meta event that is not for us */
			continue;
		}

		if (Evoral::SMF::is_tempo_or_meter_related (buf, size)) {
			seen  = true;
			break;
		}
	}

	free (buf);
	return seen;
}

/* return true if only meta-data was found */
static bool
write_midi_type1_data_to_one_file (Evoral::SMF* source, ImportStatus& status, std::shared_ptr<SMFSource> smfs,
                                   int track, bool split_midi_channels, int channel, int meta_track)
{
	uint32_t bufsize = 4;
	uint8_t* buf     = (uint8_t*) malloc (bufsize);
	bool meta_in_file  = false;
	bool meta_in_track = false;
	uint32_t written = 0;
	Evoral::event_id_t ignored_note_id; /* imported files either don't have noted IDs or we ignore them */

	/* libsmf starts counting tracks at one, not zero */
	track++;
	meta_track++;

	/* Check track number is legal.
	 */
	if (track > source->num_tracks()) {
		return false;
	}

	Source::WriterLock target_lock (smfs->mutex());
	smfs->mark_streaming_write_started (target_lock);
	smfs->drop_model (target_lock);

	try {
		/* Get metadata first */

		if (meta_track > 0 && source->seek_to_track (meta_track) == 0) {

			uint64_t t       = 0;
			uint32_t delta_t = 0;
			uint32_t size    = 0;

			while (!status.cancel) {

				size = bufsize;

				int ret = source->read_event (&delta_t, &size, &buf, &ignored_note_id);

				if (ret < 0) { // EOT
					break;
				}

				t += delta_t;

				if (size == 0) {
					/* meta event that is not for us */
					continue;
				}

				if (size > bufsize) {
					bufsize = size;
				}

				if (ret == 0) { // meta event

					meta_in_file = true;

					smfs->append_event_beats (
						target_lock,
						Evoral::Event<Temporal::Beats>(
							Evoral::MIDI_EVENT,
							Temporal::Beats::ticks_at_rate(t, source->ppqn()),
							size,
							buf), true); /* allow meta-events */
				}

				if (status.progress < 0.99) {
					status.progress += 0.01;
				}
			}

			if (meta_in_file || meta_in_track) {
				smfs->end_track (target_lock);
			}
		}

		/* Now the actual track we're actually trying to write */

		uint64_t t = 0;
		uint64_t our_t = 0;

		if (source->seek_to_track (track) == 0) {

			uint32_t delta_t = 0;
			uint32_t size    = 0;

			while (!status.cancel) {
				gint note_id_ignored; // imported files either don't have NoteID's or we ignore them.

				size = bufsize;

				int ret = source->read_event (&delta_t, &size, &buf, &note_id_ignored);

				if (ret < 0) { // EOT
					break;
				}

				t += delta_t;

				if (size == 0) {
					/* meta event, not for us */
					continue;
				}

				if (size > bufsize) {
					bufsize = size;
				}

				if (ret > 0) { // non-meta event

					/* if requested by user, each sourcefile gets only a single channel's data */

					if (split_midi_channels) {
						uint8_t type = buf[0] & 0xf0;
						uint8_t chan = buf[0] & 0x0f;
						if (type >= 0x80 && type <= 0xE0) {
							if (chan != channel) {
								continue;
							}
						}
					}
					smfs->append_event_beats (
						target_lock,
						Evoral::Event<Temporal::Beats>(
							Evoral::MIDI_EVENT,
							Temporal::Beats::ticks_at_rate(t, source->ppqn()),
							size,
							buf));

					written++;
					our_t = t;

				}  else if (ret == 0 && track != meta_track) {

					/* meta event on this track that was
					 * not handled by the meta "pre-write"
					 * above.
					 */

					meta_in_track = true;

					smfs->append_event_beats (
						target_lock,
						Evoral::Event<Temporal::Beats>(
							Evoral::MIDI_EVENT,
							Temporal::Beats::ticks_at_rate(t, source->ppqn()),
							size,
							buf), true); /* allow meta-events */
					our_t = t;
				}

				if (status.progress < 0.99) {
					status.progress += 0.01;
				}
			}

		}

		if (written == 0) {
			our_t = 0;
		}

		smfs->mark_streaming_write_completed (target_lock, timecnt_t (Temporal::Beats::ticks_at_rate (our_t, source->ppqn())));
		smfs->round_length_to_bars (our_t);

		if (written) {

			/* we wrote something other than meta-data */

			/* the streaming write that we've just finished
			 * only wrote data to the SMF object, which is
			 * ultimately an on-disk data structure. So now
			 * we pull the data back from disk to build our
			 * in-memory MidiModel version.
			 */

			smfs->load_model (target_lock, true);

		} else {
			info << string_compose (_("Track %1 contained no usable MIDI data"), track) << endmsg;
		}

	} catch (exception& e) {
		error << string_compose (_("MIDI file could not be written (best guess: %1)"), e.what()) << endmsg;
	}

	free (buf);

	return (meta_in_track || meta_in_file) && (written == 0);
}

static void
write_midi_data_to_new_files (Evoral::SMF* source, ImportStatus& status,
                              vector<std::shared_ptr<Source> >& newsrcs,
                              bool split_midi_channels)
{
	int channel;

	status.progress = 0.0f;
	size_t nfiles = newsrcs.size();
	size_t n = 0;
	int32_t meta_track = -1;
	vector<std::shared_ptr<Source>>::iterator nsi;

	/* If we're splitting channels, do the inner loop once for each
	   channel; otherwise, just do it once.
	*/
	int channel_limit = (split_midi_channels ? 16 : 1);

	switch (source->smf_format()) {
	case 0:
		channel = 0;

		for (auto & newsrc : newsrcs) {
			std::shared_ptr<SMFSource> smfs = std::dynamic_pointer_cast<SMFSource> (newsrc);
			assert (smfs);

			write_midi_type0_data_to_one_file (source, status, n, nfiles, smfs, split_midi_channels, channel);

			if (split_midi_channels) {
				channel = (channel + 1) % 16;
			}

			if (status.cancel) {
				break;
			}

			++n;
		}
		break;

	case 1:

		for (uint16_t n = 0; n < source->num_tracks(); ++n) {
			if (track_contains_tempo_or_key_metadata (source, n)) {
				meta_track = n;
				break;
			}
		}

		for (n = 0, nsi = newsrcs.begin(); n < source->num_tracks(); ++n) {
			for (int channel = 0; channel < channel_limit; ++channel) {

				std::shared_ptr<SMFSource> smfs = std::dynamic_pointer_cast<SMFSource> (*nsi);
				assert (smfs);

				/* Note that "channel" is irrelevant if
				 * split_midi_channels is false - all events
				 * for this track will be written to the new
				 * file. It matters only if split_midi_channels
				 * is true, and if so, we'll run this inner
				 * loop for every channel.
				 */

				bool meta_only = write_midi_type1_data_to_one_file (source, status, smfs, n, split_midi_channels, channel, meta_track);

				if (meta_only) {
					std::shared_ptr<FileSource> fs (std::dynamic_pointer_cast<FileSource>(*nsi));
					assert (fs);
					fs->mark_removable ();
					nsi = newsrcs.erase (nsi);
				} else {
					++nsi;
				}

				if (status.cancel) {
					break;
				}
			}
		}
		break;

	default:
		error << string_compose (_("MIDI file has unsupported SMF format type %1"), source->smf_format()) << endmsg;
		return;
	}
}

static void
remove_file_source (std::shared_ptr<Source> source)
{
	std::shared_ptr<FileSource> fs = std::dynamic_pointer_cast<FileSource> (source);

	fs->DropReferences ();

	if (fs) {
		::g_unlink (fs->path().c_str());
	}
}

void
Session::deinterlace_midi_region (std::shared_ptr<MidiRegion> mr)
{
	typedef vector<std::shared_ptr<Source> > Sources;
	Sources newfiles;

	try {
		std::shared_ptr<SMFSource> smf = std::dynamic_pointer_cast<SMFSource> (mr->midi_source(0));  //ToDo: handle compound sources?
		string source_path = smf->path();

		/* Write_midi_data_to_new_files expects to find raw midi on-disk (SMF*).
		 * this means that a split looks like a no-op if the file wasn't written to disk yet.
		 * I've chosen to flush the file to disk, rather than reimplement
		 * write_midi_data_to_new_files for a Source
		 */
		smf->session_saved(); //TODO:  should we just expose flush_midi() instead?

		/* open the SMF file for reading */
		const std::unique_ptr<Evoral::SMF> smf_reader (new Evoral::SMF());
		if (smf_reader->open (source_path)) {
			throw Evoral::SMF::FileError (source_path);
		}

		/* create new file paths for 16 potential channels of midi data */
		vector<string> smf_names;
		for (int i = 0; i < 16; i++) {
			smf_names.push_back (string_compose ("-ch%1", i+1));
		}
		vector<string> new_paths = get_paths_for_new_sources (false, source_path, 16, smf_names, true);

		/* create source files and write 1 channel of midi data to each of them */
		if (create_mono_sources_for_writing (new_paths, *this, sample_rate(), newfiles, 0, false)) {
			ImportStatus status;
			write_midi_data_to_new_files (smf_reader.get(), status, newfiles, true /*split*/);
		} else {
			error << _("deinterlace_midi_region: failed to create sources") << endmsg;
		}

	} catch (...) {
		error << _("deinterlace_midi_region: error opening MIDI file for splitting") << endmsg;
		return;
	}

	/* not all 16 channels will have midi data; delete any sources that turned up empty */
	for (Sources::iterator x = newfiles.begin(); x != newfiles.end(); ) {
		std::shared_ptr<SMFSource> smfs;
		if ((smfs = std::dynamic_pointer_cast<SMFSource>(*x)) != 0 && smfs->is_empty()) {
			x = newfiles.erase(x);
		} else {
			++x;
		}
	}

	/* insert new regions with the properties of the source region */
	for (Sources::iterator x = newfiles.begin(); x != newfiles.end(); x++) {

		/* hand over the new Source to the session*/
		add_source(*x);

		/* create a whole-file region for this new source, so it shows up in the Source List...*/
		PropertyList plist (mr->properties ());
		plist.add (Properties::whole_file, true);
		plist.add (Properties::opaque, true);
		plist.add (Properties::name, (*x)->name());
		plist.add (Properties::tags, string_compose ("%1%2%3", _("(split-chans)"), mr->tags ().empty() ? "" : " ", mr->tags ()));
		std::shared_ptr<Region> whole = RegionFactory::create (*x, plist);

		/* ... and insert a discrete copy into the playlist*/
		PropertyList plist2;
		plist2.add (ARDOUR::Properties::whole_file, false);
		std::shared_ptr<Region> copy (RegionFactory::create (whole, plist2));
		mr->playlist()->add_region (copy, mr->position());
	}
}

static vector<string>
unique_track_names (const vector<string>& n)
{
	set<string>    uniq;
	vector<string> rv;

	for (auto tn : n) {
		while (uniq.find (tn) != uniq.end()) {
			if (tn.empty ()) {
				tn = "MIDI";
			}
			/* not not use '-' as separator because that is used by
			 * new_midi_source_path, new_audio_source_path
			 * when checking for existing files.
			 */
			tn = bump_name_once (tn, '.');
		}
		uniq.insert (tn);
		rv.push_back (tn);
	}
	return rv;
}

// This function is still unable to cleanly update an existing source, even though
// it is possible to set the ImportStatus flag accordingly. The functionality
// is disabled at the GUI until the Source implementations are able to provide
// the necessary API.
void
Session::import_files (ImportStatus& status)
{
	typedef vector<std::shared_ptr<Source> > Sources;
	Sources delete_if_cancelled;
	Sources successful_imports;
	std::shared_ptr<AudioFileSource> afs;
	std::shared_ptr<SMFSource> smfs;
	uint32_t num_channels = 0;
	vector<string> smf_names;
	bool smf_keep_filename = false;

	status.sources.clear ();

	for (vector<string>::const_iterator p = status.paths.begin(); p != status.paths.end() && !status.cancel; ++p) {

		std::shared_ptr<ImportableSource> source;

		const DataType type = SMFSource::safe_midi_file_extension (*p) ? DataType::MIDI : DataType::AUDIO;
		std::unique_ptr<Evoral::SMF> smf_reader;

		if (type == DataType::AUDIO) {
			try {
				source = open_importable_source (*p, sample_rate(), status.quality);
				num_channels = source->channels();
			} catch (const failed_constructor& err) {
				error << string_compose(_("Import: cannot open input sound file \"%1\""), (*p)) << endmsg;
				status.done = status.cancel = true;
				return;
			}

		} else {
			try {
				smf_reader.reset (new Evoral::SMF());

				if (smf_reader->open(*p)) {
					throw Evoral::SMF::FileError (*p);
				}

				if (smf_reader->smf_format()==0) {
					/* Type0: we should prepare filenames for up to 16 channels in the file; we will throw out the empty ones later */
					if (status.split_midi_channels) {
						num_channels = 16;
						for (uint32_t i = 0; i < num_channels; i++) {
							smf_names.push_back (string_compose ("ch%1", 1+i ) ); //chanX
						}
					} else {
						num_channels = 1;
						smf_names.push_back("");
					}
				} else {
					/* we should prepare filenames for up to 16 channels in each Track; we will throw out the empty ones later*/
					num_channels = status.split_midi_channels ? smf_reader->num_tracks()*16 : smf_reader->num_tracks();
					switch (status.midi_track_name_source) {
					case SMFTrackNumber:
						if (status.split_midi_channels) {
							for (uint32_t i = 0; i<num_channels; i++) {
								smf_names.push_back( string_compose ("t%1.ch%2", 1+i/16, 1+i%16 ) );  //trackX.chanX
							}
						} else {
							for (uint32_t i = 0; i<num_channels;i++) {
								smf_names.push_back( string_compose ("t%1", i+1 ) );  //trackX
							}
						}
						break;
					case SMFFileAndTrackName:
						smf_keep_filename = true;
						/*FALLTHRU*/
					case SMFTrackName:
						if (status.split_midi_channels) {
							vector<string> temp;
							smf_reader->track_names (temp);
							temp = unique_track_names (temp);
							for (uint32_t i = 0; i<num_channels;i++) {
								smf_names.push_back( string_compose ("%1.ch%2", temp[i/16], 1+i%16 ) );  //trackname.chanX
							}
						} else {
							vector<string> temp;
							smf_reader->track_names (temp);
							smf_names = unique_track_names (temp);
						}
						break;
					case SMFInstrumentName:
						if (status.split_midi_channels) {
							vector<string> temp;
							smf_reader->instrument_names (temp);
							for (uint32_t i = 0; i<num_channels;i++) {
								smf_names.push_back( string_compose ("%1.ch%2", temp[i/16], 1+i%16 ) );  //instrument.chanX
							}
						} else {
							smf_reader->instrument_names (smf_names);
						}
						break;
					}
				}
			} catch (...) {
				error << _("Import: error opening MIDI file") << endmsg;
				status.done = status.cancel = true;
				return;
			}
		}

		if (num_channels == 0) {
			error << _("Import: file contains no channels.") << endmsg;
			continue;
		}

		vector<string> new_paths = get_paths_for_new_sources (status.replace_existing_source, *p, num_channels, smf_names, smf_keep_filename);
		samplepos_t natural_position = source ? source->natural_position() : 0;

		if (status.replace_existing_source) {
			fatal << "THIS IS NOT IMPLEMENTED YET, IT SHOULD NEVER GET CALLED!!! DYING!" << endmsg;
			status.cancel = !map_existing_mono_sources (new_paths, *this, sample_rate(), successful_imports, this);
		} else {
			status.cancel = !create_mono_sources_for_writing (new_paths, *this, sample_rate(), successful_imports, natural_position, false);
		}

		// copy on cancel/failure so that any files that were created will be removed below
		std::copy (successful_imports.begin(), successful_imports.end(), std::back_inserter(delete_if_cancelled));

		if (status.cancel) {
			break;
		}

		for (Sources::iterator i = successful_imports.begin(); i != successful_imports.end(); ++i) {
			if ((afs = std::dynamic_pointer_cast<AudioFileSource>(*i)) != 0) {
				afs->prepare_for_peakfile_writes ();
			}
		}

		if (source) { // audio
			status.doing_what = compose_status_message (*p, source->samplerate(),
			                                            sample_rate(), status.current, status.total);
			write_audio_data_to_new_files (source.get(), status, successful_imports);
		} else if (smf_reader) { // midi
			status.doing_what = string_compose(_("Loading MIDI file %1"), *p);
			write_midi_data_to_new_files (smf_reader.get(), status, successful_imports, status.split_midi_channels);

			if (status.import_markers) {
				smf_reader->load_markers ();
				for (auto const& m : smf_reader->markers ()) {
					Temporal::Beats beats = Temporal::Beats::from_double (m.time_pulses / (double) smf_reader->ppqn ());
					// XXX import to all sources (in case split_midi_channels is set)?
					successful_imports.front()->add_cue_marker (CueMarker (m.text, timepos_t (beats)));
				}
			}
		}

		std::copy (successful_imports.begin(), successful_imports.end(), std::back_inserter(status.sources));
		successful_imports.clear ();

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

		for (Sources::iterator x = status.sources.begin(); x != status.sources.end(); ) {

			if ((afs = std::dynamic_pointer_cast<AudioFileSource>(*x)) != 0) {
				afs->update_header((*x)->natural_position().samples(), *now, xnow);
				afs->done_with_peakfile_writes ();

				/* now that there is data there, requeue the file for analysis */

				if (Config->get_auto_analyse_audio()) {
					Analyser::queue_source_for_analysis (std::static_pointer_cast<Source>(*x), false);
				}
			}

			/* imported, copied files cannot be written or removed
			 */

			std::shared_ptr<FileSource> fs = std::dynamic_pointer_cast<FileSource>(*x);
			if (fs) {
				/* Only audio files should be marked as
				   immutable - we may need to rewrite MIDI
				   files at any time.
				*/
				if (std::dynamic_pointer_cast<AudioFileSource> (fs)) {
					fs->mark_immutable ();
				} else {
					fs->mark_immutable_except_write ();
				}
				fs->mark_nonremovable ();
			}

			/* don't create tracks for empty MIDI sources (channels) */

			if ((smfs = std::dynamic_pointer_cast<SMFSource>(*x)) != 0 && smfs->is_empty()) {
				x = status.sources.erase(x);
			} else {
				++x;
			}
		}

		/* Now, and only now, announce the newly created and to-be-used sources */

		for (auto & src : successful_imports) {
			SourceFactory::SourceCreated (src);
		}

	} else {
		try {
			std::for_each (delete_if_cancelled.begin(), delete_if_cancelled.end(), remove_file_source);
		} catch (...) {
			error << _("Failed to remove some files after failed/cancelled import operation") << endmsg;
		}

	}

	status.done = true;
}
