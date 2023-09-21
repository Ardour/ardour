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
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <stdint.h>

#include <sndfile.h>
#include <samplerate.h>

#include "pbd/gstdio_compat.h"
#include <glibmm.h>

#include <boost/scoped_array.hpp>
#include <boost/scoped_ptr.hpp>
#include <boost/shared_array.hpp>

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
                                    vector<string> const & smf_names)

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
				string mchn_name = string_compose ("%1.%2", basename, smf_names[n]);
				filepath = new_midi_source_path (mchn_name);
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

	boost::scoped_array<float> data(new float[nframes * channels]);
	vector<boost::shared_array<Sample> > channel_data;

	for (uint32_t n = 0; n < channels; ++n) {
		channel_data.push_back(boost::shared_array<Sample>(new Sample[nframes]));
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
write_midi_data_to_new_files (Evoral::SMF* source, ImportStatus& status,
                              vector<std::shared_ptr<Source> >& newfiles,
                              bool split_midi_channels)
{
	uint32_t buf_size = 4;
	uint8_t* buf      = (uint8_t*) malloc (buf_size);

	status.progress = 0.0f;

	bool type0 = source->smf_format()==0;

	int total_files = newfiles.size();

	try {
		vector<std::shared_ptr<Source> >::iterator s = newfiles.begin();

		int cur_chan = 0;

		for (int i = 0; i < total_files; ++i) {

			int cur_track = i+1;  //first Track of a type-1 file is metadata only. Start importing sourcefiles at Track index 1

			if (split_midi_channels) {  //if splitting channels we will need to fill 16x sources.  empties will be disposed-of later
				cur_track = 1 + (int) floor((float)i/16.f);  //calculate the Track needed for this sourcefile (offset by 1)
			}

			std::shared_ptr<SMFSource> smfs = std::dynamic_pointer_cast<SMFSource> (*s);
			if (!smfs) {
				continue;  //should never happen.  The calling code should provide exactly the number of tracks&channels we need
			}

			Source::WriterLock source_lock(smfs->mutex());

			smfs->drop_model (source_lock);
			if (type0) {
				source->seek_to_start ();
			} else {
				source->seek_to_track (cur_track);
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

				/* if requested by user, each sourcefile gets only a single channel's data */
				if (split_midi_channels) {
					uint8_t type = buf[0] & 0xf0;
					uint8_t chan = buf[0] & 0x0f;
					if (type >= 0x80 && type <= 0xE0) {
						if (chan != cur_chan) {
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
					Evoral::Event<Temporal::Beats>(
						Evoral::MIDI_EVENT,
						Temporal::Beats::ticks_at_rate(t, source->ppqn()),
						size,
						buf));

				if (status.progress < 0.99) {
					status.progress += 0.01;
				}
			}

			if (!first) {

				/* we wrote something */

				/* try to guess at the meter, for 5/4 midi loop oddballs */
				int pulses_per_bar = 4;
				Evoral::SMF::Tempo *tempo = source->nth_tempo (0);
				if (tempo && (tempo->numerator>0) ) {
					pulses_per_bar = tempo->numerator;
				}

				/* extend the length of the region to the end of a bar */
				const Temporal::Beats  length_beats = Temporal::Beats::ticks_at_rate(t, source->ppqn());
				smfs->update_length (timepos_t (length_beats.round_up_to_multiple(Temporal::Beats(pulses_per_bar,0))));

				smfs->mark_streaming_write_completed (source_lock);

				/* the streaming write that we've just finished
				 * only wrote data to the SMF object, which is
				 * ultimately an on-disk data structure. So now
				 * we pull the data back from disk to build our
				 * in-memory MidiModel version.
				 */

				smfs->load_model (source_lock, true);

				/* Now that there is a model, we can set interpolation of parameters. */
				smfs->mark_streaming_write_completed (source_lock);

				if (status.cancel) {
					break;
				}
			} else {
				info << string_compose (_("Track %1 of %2 contained no usable MIDI data"), i, total_files) << endmsg;
			}

			++s; // next source

			++cur_chan;
			if (cur_chan > 15) {
				cur_chan=0;
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
		boost::scoped_ptr<Evoral::SMF> smf_reader;
		smf_reader.reset (new Evoral::SMF());
		if (smf_reader->open (source_path)) {
			throw Evoral::SMF::FileError (source_path);
		}

		/* create new file paths for 16 potential channels of midi data */
		vector<string> smf_names;
		for (int i = 0; i<16; i++) {
			smf_names.push_back(string_compose("-ch%1", i+1));
		}
		vector<string> new_paths = get_paths_for_new_sources (false, source_path, 16, smf_names);

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
	Sources all_new_sources;
	std::shared_ptr<AudioFileSource> afs;
	std::shared_ptr<SMFSource> smfs;
	uint32_t num_channels = 0;
	vector<string> smf_names;

	status.sources.clear ();

	for (vector<string>::const_iterator p = status.paths.begin(); p != status.paths.end() && !status.cancel; ++p) {

		std::shared_ptr<ImportableSource> source;

		const DataType type = SMFSource::safe_midi_file_extension (*p) ? DataType::MIDI : DataType::AUDIO;
		boost::scoped_ptr<Evoral::SMF> smf_reader;

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
						for (uint32_t i = 0; i<num_channels; i++) {
							smf_names.push_back( string_compose ("ch%1", 1+i ) ); //chanX
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

		vector<string> new_paths = get_paths_for_new_sources (status.replace_existing_source, *p, num_channels, smf_names);
		Sources newfiles;
		samplepos_t natural_position = source ? source->natural_position() : 0;


		if (status.replace_existing_source) {
			fatal << "THIS IS NOT IMPLEMENTED YET, IT SHOULD NEVER GET CALLED!!! DYING!" << endmsg;
			status.cancel = !map_existing_mono_sources (new_paths, *this, sample_rate(), newfiles, this);
		} else {
			status.cancel = !create_mono_sources_for_writing (new_paths, *this, sample_rate(), newfiles, natural_position, true);
		}

		// copy on cancel/failure so that any files that were created will be removed below
		std::copy (newfiles.begin(), newfiles.end(), std::back_inserter(all_new_sources));

		if (status.cancel) {
			break;
		}

		for (Sources::iterator i = newfiles.begin(); i != newfiles.end(); ++i) {
			if ((afs = std::dynamic_pointer_cast<AudioFileSource>(*i)) != 0) {
				afs->prepare_for_peakfile_writes ();
			}
		}

		if (source) { // audio
			status.doing_what = compose_status_message (*p, source->samplerate(),
			                                            sample_rate(), status.current, status.total);
			write_audio_data_to_new_files (source.get(), status, newfiles);
		} else if (smf_reader) { // midi
			status.doing_what = string_compose(_("Loading MIDI file %1"), *p);
			write_midi_data_to_new_files (smf_reader.get(), status, newfiles, status.split_midi_channels);

			if (status.import_markers) {
				smf_reader->load_markers ();
				for (auto const& m : smf_reader->markers ()) {
					Temporal::Beats beats = Temporal::Beats::from_double (m.time_pulses / (double) smf_reader->ppqn ());
					// XXX import to all sources (in case split_midi_channels is set)?
					newfiles.front()->add_cue_marker (CueMarker (m.text, timepos_t (beats)));
				}
			}
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
				x = all_new_sources.erase(x);
			} else {
				++x;
			}
		}

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
