/*
 * Copyright (C) 2023 Adrien Gesta-Fline <dev.agfline@posteo.net>
 * Based on Robin Gareus <robin@gareus.org> session_utils files
 * Based on Damien Zammit ptformat
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

#include <cstdlib>
#include <getopt.h>
#include <glibmm.h>
#include <iostream>
#include <locale.h>
#include <unistd.h>

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <libxml/xpath.h>
#include <libxml/xpathInternals.h>

#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/session_directory.h"
#include "ardour/template_utils.h"

#include "ardour/audio_track.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioregion.h"
#include "ardour/import_status.h"
#include "ardour/playlist.h"
#include "ardour/region_factory.h"
#include "ardour/source_factory.h"

#include "common.h"
#include "pbd/file_utils.h"

#include "aaf/libaaf.h"
#include "aaf/utils.h"

using namespace std;
using namespace ARDOUR;
using namespace SessionUtils;
using namespace Timecode;
using namespace PBD;

#ifndef UTILNAME
#define UTILNAME "new_aaf_session"
#endif

/*
 *  TODO:
 *    - Track level
 *    - Track pan
 *    - Track level automation
 *    - Track pan automation
 *    x Region level automation
 *    - Session timecode offset (so the very begining of the timeline starts at eg. 01:00:00:00)
 *    x Markers
 *    x Multichannel audio file import (AAFOperationDef_AudioChannelCombiner)
 *    - Multichannel region from multiple source audio files (1 file per channel) ?
 *    - Mono region from a specific channel of a multichannel file ?
 *    x Muted region
 *    - Video file import
 */

static void
usage ();
static void
list_templates ();
static std::string
template_path_from_name (std::string const& name);
static Session*
create_new_session (string const& dir, string const& state, float samplerate, ARDOUR::SampleFormat bitdepth, int master_bus_chn, string const& template_path);
// static void set_session_video_from_aaf( Session *s, AAF_Iface *aafi );
static std::shared_ptr<AudioTrack>
get_nth_audio_track (uint32_t nth, std::shared_ptr<RouteList const> routes);
static bool
import_sndfile_as_region (Session* s, struct aafiAudioEssence* audioEssence, SrcQuality quality, timepos_t& pos, SourceList& sources, ImportStatus& status, vector<std::shared_ptr<Region>>* regions /* boost::shared_ptr<Region> r*/);
static void
set_session_range (Session* s, AAF_Iface* aafi);
static std::shared_ptr<Region>
create_region (vector<std::shared_ptr<Region>> source_regions, aafiAudioClip* aafAudioClip, SourceList& oneClipSources, aafPosition_t clipOffset, aafRational_t samplerate_r);
static void
set_region_gain (aafiAudioClip* aafAudioClip, std::shared_ptr<Region> region, Session* s);
static std::shared_ptr<AudioTrack>
prepare_audio_track (aafiAudioTrack* aafTrack, Session* s);
static void
set_region_fade (aafiAudioClip* aafAudioClip, std::shared_ptr<Region> region, aafRational_t* samplerate);
static void
set_session_timecode (Session* s, AAF_Iface* aafi);

static void
usage ()
{
	// help2man compatible format (standard GNU help-text)
	printf (UTILNAME " - create a new session based on an AAF file from the commandline.\n\n");
	printf ("Usage: " UTILNAME " [ OPTIONS ] -p <session-path> --aaf <file.aaf>\n\n");
	printf ("Options:\n\n\
  -h, --help                        Display this help and exit.\n\
  -L, --list-templates              List available Ardour templates and exit.\n\
\n\
  -m, --master-channels      <chn>  Master-bus channel count (default 2).\n\
  -r, --sample-rate         <rate>  Sample rate of the new Ardour session (default is AAF).\n\
  -s, --sample-size     <16|24|32>  Audio bit depth of the new Ardour session (default is AAF).\n\
\n\
  -t, --template        <template>  Use given template for new session.\n\
  -p, --session-path        <path>  Where to store the new session folder.\n\
  -n, --session-name        <name>  The new session name. A new folder will be created into session path with that name.\n\
                                    Default is the AAF composition name or file name as a fallback.\n\
                                    Set <name> to AAFFILE to force the use of AAF file name as session name.\n\
\n\
  -l, --media-location      <path>  Path to AAF media files (when not embedded)\n\
  -c, --media-cache         <path>  Path where AAF embedded media files will be extracted, prior to Ardour import. Default is TEMP.\n\
  -k, --keep-cache                  Do not clear cache. Useful for analysis of extracted audio files.\n\
\n\
  -a, --aaf             <aaf file>  AAF file to load.\n\
\n\
Vendor Options:\n\
\n\
  Davinci Resolve\n\
\n\
  --import-disabled-clips           Import disabled clips (skipped by default)\n\
\n\
  Pro Tools\n\
\n\
  --remove-sample-accurate-edit     Remove clips added by PT to pad to frame boundary.\n\
  --convert-fade-clips              Remove clip fades and replace by real fades.\n\
\n\
\n");

	printf ("\n\
Examples:\n\
" UTILNAME " --session-path /path/to/sessions/ --aaf /path/to/file.aaf\n\
\n");

	printf ("Report bugs to <http://tracker.ardour.org/>\n"
	        "Website: <http://ardour.org/>\n");
	::exit (EXIT_SUCCESS);
}

static void
list_templates ()
{
	vector<TemplateInfo> templates;
	find_session_templates (templates, false);

	for (vector<TemplateInfo>::iterator x = templates.begin (); x != templates.end (); ++x) {
		printf ("%s\n", (*x).name.c_str ());
	}
}

static std::string
template_path_from_name (std::string const& name)
{
	vector<TemplateInfo> templates;
	find_session_templates (templates, false);

	for (vector<TemplateInfo>::iterator x = templates.begin (); x != templates.end (); ++x) {
		if ((*x).name == name)
			return (*x).path;
	}

	return "";
}

static Session*
create_new_session (string const& dir, string const& state, float samplerate, ARDOUR::SampleFormat bitdepth, int master_bus_chn, string const& template_path)
{
	AudioEngine* engine = AudioEngine::create ();

	if (!engine->set_backend ("None (Dummy)", "Unit-Test", "")) {
		PBD::error << "Cannot create Audio/MIDI engine." << endmsg;
		return NULL;
	}

	// engine->set_input_channels( 32 );
	// engine->set_output_channels( 32 );

	if (engine->set_sample_rate (samplerate)) {
		PBD::error << string_compose ("Cannot set session's samplerate to %1.", samplerate) << endmsg;
		return NULL;
	}

	if (engine->start () != 0) {
		PBD::error << "Cannot start Audio/MIDI engine." << endmsg;
		return NULL;
	}

	string s = Glib::build_filename (dir, state + statefile_suffix);

	if (Glib::file_test (dir, Glib::FILE_TEST_EXISTS)) {
		PBD::error << string_compose ("Session folder already exists '%1", dir) << endmsg;
		return NULL;
	}

	if (Glib::file_test (s, Glib::FILE_TEST_EXISTS)) {
		PBD::error << string_compose ("Session file exists '%1'", s) << endmsg;
		return NULL;
	}

	BusProfile  bus_profile;
	BusProfile* bus_profile_ptr = NULL;

	if (master_bus_chn > 0) {
		bus_profile_ptr                 = &bus_profile;
		bus_profile.master_out_channels = master_bus_chn;
	}

	if (!template_path.empty ()) {
		bus_profile_ptr = NULL;
	}

	Session* session = new Session (*engine, dir, state, bus_profile_ptr, template_path);

	engine->set_session (session);

	session->config.set_native_file_data_format (bitdepth);

	return session;
}

/*
 * libs/ardour/import.cc
 * - Reimplement since function was removed in 4620d13 : https://github.com/Ardour/ardour/commit/4620d138eefad57bc55e1901d8410c36803ce0d6 -
 */

static std::shared_ptr<AudioTrack>
get_nth_audio_track (uint32_t nth, std::shared_ptr<RouteList const> routes)
{
	RouteList rl = *(routes);
	rl.sort (Stripable::Sorter ());

	for (auto const& r : rl) {
		std::shared_ptr<AudioTrack> at = std::dynamic_pointer_cast<AudioTrack> (r);
		if (!at) {
			continue;
		}
		if (nth-- == 0) {
			return at;
		}
	}
	return std::shared_ptr<AudioTrack> ();
}

/*
 * https://github.com/Ardour/ardour/blob/365f6d633731229e7bc5d37a5fe2c9107b527e28/libs/ardour/import_pt.cc#L82
 */

static bool
import_sndfile_as_region (Session* s, struct aafiAudioEssence* audioEssence, SrcQuality quality, timepos_t& pos, SourceList& sources, ImportStatus& status, vector<std::shared_ptr<Region>>* regions /* boost::shared_ptr<Region> r*/)
{
	wstring ws (audioEssence->usable_file_path);
	string  usable_file_path (ws.begin (), ws.end ());

	/* Import the source */
	status.paths.clear ();
	status.paths.push_back (usable_file_path);
	status.current                 = 1;
	status.total                   = 1;
	status.freeze                  = false;
	status.quality                 = quality;
	status.replace_existing_source = false;
	status.split_midi_channels     = false;
	status.import_markers          = false;
	status.done                    = false;
	status.cancel                  = false;

	s->import_files (status);

	status.progress = 1.0;
	sources.clear ();

	/* FIXME: There is no way to tell if cancel button was pressed
	 * or if the file failed to import, just that one of these occurred.
	 * We want status.cancel to reflect the user's choice only
	 */
	if (status.cancel && status.current > 1) {
		/* Succeeded to import file, assume user hit cancel */
		return false;
	} else if (status.cancel && status.current == 1) {
		/* Failed to import file, assume user did not hit cancel */
		status.cancel = false;
		return false;
	}

	for (int i = 0; i < audioEssence->channels; i++) {
		sources.push_back (status.sources.at (i));
	}

	/* build peakfiles */
	for (SourceList::iterator x = sources.begin (); x != sources.end (); ++x) {
		SourceFactory::setup_peakfile (*x, true);
	}

	/* Put the source on a region */
	std::shared_ptr<Region> region;
	string                  region_name;

	/* take all the sources we have and package them up as a region */

	region_name = region_name_from_path (status.paths.front (), (sources.size () > 1), false);

	/* we checked in import_sndfiles() that there were not too many */

	while (RegionFactory::region_by_name (region_name)) {
		region_name = bump_name_once (region_name, '.');
	}

	ws = audioEssence->unique_file_name;
	string unique_file_name (ws.begin (), ws.end ());

	PropertyList proplist;

	proplist.add (ARDOUR::Properties::start, 0);
	proplist.add (ARDOUR::Properties::length, timecnt_t (sources[0]->length (), pos));
	proplist.add (ARDOUR::Properties::name, unique_file_name);
	proplist.add (ARDOUR::Properties::layer, 0);
	proplist.add (ARDOUR::Properties::whole_file, true);
	proplist.add (ARDOUR::Properties::external, true);

	region = RegionFactory::create (sources, proplist);
	(*regions).push_back (region);

	/* NOTE: Don't know what that's for */

	// bool use_timestamp;
	// use_timestamp = (pos == -1);
	// if (use_timestamp && boost::dynamic_pointer_cast<AudioRegion>(r)) {
	// 	boost::dynamic_pointer_cast<AudioRegion>(r)->special_set_position(sources[0]->natural_position());
	// }
	//
	//
	// /* if we're creating a new track, name it after the cleaned-up
	//  * and "merged" region name.
	//  */
	//
	// regions.push_back (r);
	// int n = 0;
	//
	// for (vector<boost::shared_ptr<Region> >::iterator r = regions.begin(); r != regions.end(); ++r, ++n) {
	// 	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (*r);
	//
	// 	if (use_timestamp) {
	// 		if (ar) {
	//
	// 			/* get timestamp for this region */
	//
	// 			const boost::shared_ptr<Source> s (ar->sources().front());
	// 			const boost::shared_ptr<AudioSource> as = boost::dynamic_pointer_cast<AudioSource> (s);
	//
	// 			assert (as);
	//
	// 			if (as->natural_position() != 0) {
	// 				pos = as->natural_position();
	// 			} else {
	// 				pos = 0;
	// 			}
	// 		} else {
	// 			/* should really get first position in MIDI file, but for now, use 0 */
	// 			pos = 0;
	// 		}
	// 	}
	// }

	return true;
}

static void
set_session_range (Session* s, AAF_Iface* aafi)
{
	samplepos_t start = samplepos_t (eu2sample (s->sample_rate (), &aafi->compositionStart_editRate, aafi->compositionStart));
	samplepos_t end   = samplepos_t (eu2sample (s->sample_rate (), &aafi->compositionLength_editRate, aafi->compositionLength)) + start;

	/* https://github.com/agfline/LibAAF/issues/5#issuecomment-1632522578 */
	// s->set_session_extents( timepos_t(start), timepos_t(end) );
	s->maybe_update_session_range (timepos_t (start), timepos_t (end));
}

static std::shared_ptr<Region>
create_region (vector<std::shared_ptr<Region>> source_regions, aafiAudioClip* aafAudioClip, SourceList& oneClipSources, aafPosition_t clipOffset, aafRational_t samplerate_r)
{
	wstring ws = aafAudioClip->Essence->unique_file_name;
	string  unique_file_name (ws.begin (), ws.end ());

	aafPosition_t clipPos       = convertEditUnit (aafAudioClip->pos, *aafAudioClip->track->edit_rate, samplerate_r);
	aafPosition_t clipLen       = convertEditUnit (aafAudioClip->len, *aafAudioClip->track->edit_rate, samplerate_r);
	aafPosition_t essenceOffset = convertEditUnit (aafAudioClip->essence_offset, *aafAudioClip->track->edit_rate, samplerate_r);

	PropertyList proplist;

	proplist.add (ARDOUR::Properties::start, essenceOffset);
	proplist.add (ARDOUR::Properties::length, clipLen);
	proplist.add (ARDOUR::Properties::name, unique_file_name);
	proplist.add (ARDOUR::Properties::layer, 0);
	proplist.add (ARDOUR::Properties::whole_file, false);
	proplist.add (ARDOUR::Properties::external, true);

	/* NOTE: region position is set when calling add_region() */

	std::shared_ptr<Region> region = RegionFactory::create (oneClipSources, proplist);

	for (SourceList::iterator source = oneClipSources.begin (); source != oneClipSources.end (); ++source) {
		/* position displayed in Ardour source list */
		(*source)->set_natural_position (timepos_t (clipPos + clipOffset));

		for (vector<std::shared_ptr<Region>>::iterator region = source_regions.begin (); region != source_regions.end (); ++region) {
			if ((*region)->source (0) == *source) {
				/* Enable "Move to Original Position" */
				(*region)->set_position (timepos_t (clipPos + clipOffset - essenceOffset));
			}
		}
	}

	return region;
}

static void
set_region_gain (aafiAudioClip* aafAudioClip, std::shared_ptr<Region> region, Session* s)
{
	if (aafAudioClip->gain && aafAudioClip->gain->flags & AAFI_AUDIO_GAIN_CONSTANT) {
		std::dynamic_pointer_cast<AudioRegion> (region)->set_scale_amplitude (aafRationalToFloat (aafAudioClip->gain->value[0]));
	}

	if (aafAudioClip->automation) {
		aafiAudioGain*                  level = aafAudioClip->automation;
		std::shared_ptr<AudioRegion>    ar    = std::dynamic_pointer_cast<AudioRegion> (region);
		std::shared_ptr<AutomationList> al    = ar->envelope ();

		for (int i = 0; i < level->pts_cnt; i++) {
			al->fast_simple_add (timepos_t (aafRationalToFloat (level->time[i]) * region->length ().samples ()), aafRationalToFloat (level->value[i]));
		}
	}
}

static std::shared_ptr<AudioTrack>
prepare_audio_track (aafiAudioTrack* aafTrack, Session* s)
{
	/*
	 * https://github.com/Ardour/ardour/blob/365f6d633731229e7bc5d37a5fe2c9107b527e28/libs/ardour/import_pt.cc#L327
	 */

	/* Use existing track if possible */
	std::shared_ptr<AudioTrack> track = get_nth_audio_track ((aafTrack->number - 1), s->get_routes ());

	/* Or create a new track if needed */
	if (!track) {
		wstring ws_track_name = std::wstring (aafTrack->name);
		string  track_name    = string (ws_track_name.begin (), ws_track_name.end ());

		PBD::info << string_compose ("Track number %1 (%2) does not exist. Adding new track.", aafTrack->number, track_name) << endmsg;

		// TODO: second argument is "output_channels". How should it be set ?
		list<std::shared_ptr<AudioTrack>> at (s->new_audio_track (aafTrack->format, 2, 0, 1, track_name, PresentationInfo::max_order, Normal));

		if (at.empty ()) {
			PBD::error << "Could not create new audio track." << endmsg;
			::exit (EXIT_FAILURE);
		}

		track = at.back ();
	}

	return track;
}

static FadeShape
aaf_fade_interpol_to_ardour_fade_shape (aafiInterpolation_e interpol)
{
	/*
	 * https://github.com/Ardour/ardour/blob/b84c99639f0dd28e210ed9c064429c17014093a7/libs/ardour/ardour/types.h#L705
	 *
	 * enum FadeShape {
	 *   FadeLinear,
	 *   FadeFast,
	 *   FadeSlow,
	 *   FadeConstantPower,
	 *   FadeSymmetric,
	 * };
	 *
	 * https://github.com/Ardour/ardour/blob/365f6d633731229e7bc5d37a5fe2c9107b527e28/libs/ardour/ardour/audioregion.h#L143
	 * https://github.com/Ardour/ardour/blob/365f6d633731229e7bc5d37a5fe2c9107b527e28/libs/temporal/temporal/types.h#L39
	 */

	switch (interpol & AAFI_INTERPOL_MASK) {
		case AAFI_INTERPOL_NONE:
			PBD::warning << "Fade type is set to AAFI_INTERPOL_NONE : Falling back to FadeConstantPower." << endmsg;
			return FadeConstantPower;

		case AAFI_INTERPOL_LINEAR:
			return FadeLinear;

		case AAFI_INTERPOL_LOG:
			PBD::warning << "Fade type is set to AAFI_INTERPOL_LOG : Falling back to FadeConstantPower." << endmsg;
			return FadeConstantPower;

		case AAFI_INTERPOL_CONSTANT:
			PBD::warning << "Fade type is set to AAFI_INTERPOL_CONSTANT : Falling back to FadeConstantPower." << endmsg;
			return FadeConstantPower;

		case AAFI_INTERPOL_POWER:
			return FadeConstantPower;

		case AAFI_INTERPOL_BSPLINE:
			PBD::warning << "Fade type is set to AAFI_INTERPOL_BSPLINE : Falling back to FadeConstantPower." << endmsg;
			return FadeConstantPower;

		default:
			PBD::warning << "Unknown fade type : Falling back to FadeConstantPower." << endmsg;
			return FadeConstantPower;
	}

	PBD::warning << "Unknown fade type : Falling back to FadeConstantPower." << endmsg;
	return FadeConstantPower;
}

static void
set_region_fade (aafiAudioClip* aafAudioClip, std::shared_ptr<Region> region, aafRational_t* samplerate)
{
	if (aafAudioClip == NULL) {
		return;
	}

	aafiTransition* fadein  = aafi_get_fadein (aafAudioClip->Item);
	aafiTransition* fadeout = aafi_get_fadeout (aafAudioClip->Item);
	aafiTransition* xfade   = aafi_get_xfade (aafAudioClip->Item);

	if (xfade) {
		if (fadein == NULL) {
			fadein = xfade;
		} else {
			PBD::warning << "Clip has both fadein and crossfade : crossfade will be ignored." << endmsg;
		}
	}

	FadeShape   fade_shape;
	samplecnt_t fade_len;

	if (fadein != NULL) {
		fade_shape = aaf_fade_interpol_to_ardour_fade_shape ((aafiInterpolation_e)(fadein->flags & AAFI_INTERPOL_MASK));
		fade_len   = convertEditUnit (fadein->len, *aafAudioClip->track->edit_rate, *samplerate);

		std::dynamic_pointer_cast<AudioRegion> (region)->set_fade_in (fade_shape, fade_len);
	}

	if (fadeout != NULL) {
		fade_shape = aaf_fade_interpol_to_ardour_fade_shape ((aafiInterpolation_e)(fadeout->flags & AAFI_INTERPOL_MASK));
		fade_len   = convertEditUnit (fadeout->len, *aafAudioClip->track->edit_rate, *samplerate);

		std::dynamic_pointer_cast<AudioRegion> (region)->set_fade_out (fade_shape, fade_len);
	}
}

static void
set_session_timecode (Session* s, AAF_Iface* aafi)
{
	uint16_t       aafFPS = aafi->Timecode->fps;
	TimecodeFormat ardourtc;

	/*
	 *  Fractional timecodes are never explicitly set into tc->fps, so we deduce
	 *  them based on edit_rate value.
	 */

	switch (aafFPS) {
		case 24:
			if (aafi->Timecode->edit_rate->numerator == 24000 &&
			    aafi->Timecode->edit_rate->denominator == 1001) {
				ardourtc = timecode_23976;
			} else {
				ardourtc = timecode_24;
			}
			break;

		case 25:
			if (aafi->Timecode->edit_rate->numerator == 25000 &&
			    aafi->Timecode->edit_rate->denominator == 1001) {
				ardourtc = timecode_24976;
			} else {
				ardourtc = timecode_25;
			}
			break;

		case 30:
			if (aafi->Timecode->edit_rate->numerator == 30000 &&
			    aafi->Timecode->edit_rate->denominator == 1001) {
				if (aafi->Timecode->drop) {
					ardourtc = timecode_2997drop;
				} else {
					ardourtc = timecode_2997;
				}
			} else {
				if (aafi->Timecode->drop) {
					ardourtc = timecode_30drop;
				} else {
					ardourtc = timecode_30;
				}
			}
			break;

		case 60:
			if (aafi->Timecode->edit_rate->numerator == 60000 &&
			    aafi->Timecode->edit_rate->denominator == 1001) {
				ardourtc = timecode_5994;
			} else {
				ardourtc = timecode_60;
			}
			break;

		default:
			PBD::error << string_compose ("Unknown AAF timecode fps : %1.", aafFPS) << endmsg;
			return;
	}

	s->config.set_timecode_format (ardourtc);
}

int
prepare_cache (AAF_Iface* aafi, string* media_cache_path)
{
	if (!(*media_cache_path).empty ()) {
		/* if media cache is not empty, user forced cache path with --media-cache */
		return 0;
	}

	const char* tmppath = g_get_tmp_dir ();

	if (!aafi->compositionName || aafi->compositionName[0] == 0x00) {
		*media_cache_path = g_build_path (G_DIR_SEPARATOR_S, tmppath, Glib::path_get_basename (aafi->aafd->cfbd->file).c_str(), NULL); //+ string(DIR_SEP_STR);
	} else {
		int   compoNameLen = wcslen (aafi->compositionName) + 1;
		char* compoName    = (char*)malloc (compoNameLen);

		wcstombs (compoName, aafi->compositionName, compoNameLen);

		*media_cache_path = g_build_path (G_DIR_SEPARATOR_S, tmppath, laaf_util_clean_filename (compoName), NULL);

		g_free (compoName);
	}

	int    i       = 0;
	string testdir = *media_cache_path;

	while (g_file_test (testdir.c_str (), G_FILE_TEST_EXISTS))
		testdir = *media_cache_path + "_" + to_string (i++);

	*media_cache_path = testdir;

	if (g_mkdir_with_parents ((*media_cache_path).c_str (), 0760) < 0) {
		PBD::error << string_compose ("Could not create cache directory at '%1' : %2", (*media_cache_path), strerror (errno)) << endmsg;
		return -1;
	}

	return 0;
}

void
clear_cache (AAF_Iface* aafi, string media_cache_path)
{
	aafiAudioEssence* audioEssence = NULL;

	foreachEssence (audioEssence, aafi->Audio->Essences)
	{
		if (!audioEssence->is_embedded) {
			continue;
		}

		char* filepath = (char*)malloc (wcslen (audioEssence->usable_file_path) + 1);
		snprintf (filepath, wcslen (audioEssence->usable_file_path) + 1, "%ls", audioEssence->usable_file_path);

		if (g_file_test (filepath, G_FILE_TEST_EXISTS)) {
			if (remove (filepath) < 0) {
				PBD::error << string_compose ("Failed to remove a file from cache (%1) : %2", filepath, strerror (errno)) << endmsg;
			}
		} else {
			PBD::error << string_compose ("Missing a file from cache (%1) : %2", filepath, strerror (errno)) << endmsg;
		}

		free (filepath);
	}

	if (rmdir (media_cache_path.c_str ()) < 0) {
		PBD::error << string_compose ("Failed to remove cache directory (%1) : %2", media_cache_path, strerror (errno)) << endmsg;
	}
}

int
main (int argc, char* argv[])
{
	setlocale (LC_ALL, "");
	SessionUtils::init ();

	ARDOUR::SampleFormat bitdepth       = ARDOUR::FormatInt24;
	int                  samplesize     = 0;
	int                  samplerate     = 0;
	int                  master_bus_chn = 2;
	string               template_path;
	string               output_folder;
	string               session_name;
	string               media_location_path;
	string               media_cache_path;
	int                  keep_cache = 0;
	string               aaf_file;
	uint32_t             aaf_resolve_options  = 0;
	uint32_t             aaf_protools_options = 0;
	// bool replace_session_if_exists = false;

	printf ("using libaaf %s\n", LIBAAF_VERSION);

	const char* optstring = "hLm:r:s:t:p:n:l:c:a:";

	const struct option longopts[] = {
		{ "help", no_argument, 0, 'h' },

		{ "list-templates", no_argument, 0, 'L' },

		{ "master-channels", required_argument, 0, 'm' },

		{ "sample-rate", required_argument, 0, 'r' },
		{ "sample-size", required_argument, 0, 's' },

		{ "template", required_argument, 0, 't' },
		{ "session-path", required_argument, 0, 'p' },
		{ "session-name", required_argument, 0, 'n' },

		{ "media-location", required_argument, 0, 'l' },
		{ "media-cache", required_argument, 0, 'c' },
		{ "keep-cache", required_argument, 0, 'k' },

		{ "aaf", required_argument, 0, 'a' },

		{ "import-disabled-clips", no_argument, 0, 0x01 },
		{ "remove-sample-accurate-edit", no_argument, 0, 0x02 },
		{ "convert-fade-clips", no_argument, 0, 0x03 },

		// { "replace-session-if-exists", no_argument, 0, 0x01 }
	};

	int c = 0;

	while (EOF != (c = getopt_long (argc, argv, optstring, longopts, (int*)0))) {
		switch (c) {
			case 'h':
				usage ();
				break;

			case 'L':
				list_templates ();
				exit (EXIT_SUCCESS);
				break;

			case 'm':
				master_bus_chn = atoi (optarg);
				/* TODO check min / max */
				break;

			case 'r':
				samplerate = atoi (optarg);

				if (samplerate < 44100 || samplerate > 192000) {
					PBD::error << string_compose ("Invalid sample rate (%1). Sample rate must be between 44100 and 192000.", optarg) << endmsg;
					::exit (EXIT_FAILURE);
				}
				break;

			case 's':
				samplesize = atoi (optarg);

				if (samplesize != 16 && samplesize != 24 && samplesize != 32) {
					PBD::error << string_compose ("Invalid sample size (%1). Sample size must be either 16, 24 or 32.", optarg) << endmsg;
					::exit (EXIT_FAILURE);
				}
				break;

			case 't':
				template_path = template_path_from_name (optarg);
				if (template_path.empty ()) {
					cerr << "Invalid (non-existent) template:" << optarg << "\n";
					::exit (EXIT_FAILURE);
				}
				break;

			case 'p':
				output_folder = string (optarg);
				break;

			case 'n':
				session_name = string (optarg);
				break;

			case 'l':
				media_location_path = string (optarg);
				break;

			case 'c':
				media_cache_path = string (optarg);
				break;

			case 'k':
				keep_cache = 1;
				break;

			case 'a':
				aaf_file = string (optarg);
				break;

			case 0x01:
				aaf_resolve_options |= RESOLVE_INCLUDE_DISABLED_CLIPS;
				break;

			case 0x02:
				aaf_protools_options |= PROTOOLS_REMOVE_SAMPLE_ACCURATE_EDIT;
				break;

			case 0x03:
				aaf_protools_options |= PROTOOLS_REPLACE_CLIP_FADES;
				break;

			default:
				cerr << "Error: unrecognized option. See --help for usage information.\n";
				::exit (EXIT_FAILURE);
				break;
		}
	}

	int missing_param = 0;

	// if ( template_path.empty() )
	// {
	// 	PBD::error << "Missing template. Use --template parameter." << endmsg;
	//   missing_param = 1;
	// }

	if (output_folder.empty ()) {
		PBD::error << "Missing session path. Use --session-path parameter." << endmsg;
		missing_param = 1;
	}

	if (aaf_file.empty ()) {
		PBD::error << "Missing AAF file. Use --aaf parameter." << endmsg;
		missing_param = 1;
	}

	if (missing_param) {
		::exit (EXIT_FAILURE);
	}

	AAF_Iface* aafi = aafi_alloc (NULL);

	aafi_set_option_int (aafi, "trace", 1);
	aafi_set_option_int (aafi, "protools", aaf_protools_options);
	aafi_set_option_int (aafi, "resolve", aaf_resolve_options);

	/*
	 * The following "forbid_nonlatin_filenames" option is there until we find a
	 * solution to avoid issue with e.g korean filenames (e.g: pt-ko.aaf) :
	 *
	 * [e] ../session_utils/new_aaf_session.cc : main() on line 1279 : Could not import '샘플 단위 정밀 편집_2' to session.
	 * : [ERROR]: FFMPEGFileImportableSource: Failed to read file metadata
	 * : [ERROR]: Import: cannot open input sound file "/tmp/pt-ko/Ø
	 *
	 * NOTE: "Could not import '샘플 단위 정밀 편집_2' to session." prints korean chars well in console.
	 */
	aafi->ctx.options.forbid_nonlatin_filenames = 1;

	/*
	 * prepare libAAF log file.
	 * using a file pointer avoids the need to reimplement debug_callback()
	 */

	string logfile = g_build_path (G_DIR_SEPARATOR_S, output_folder.c_str (), string (Glib::path_get_basename (aaf_file.c_str ()) + ".log").c_str (), NULL);

	PBD::info << string_compose ("Writting AAF log to : %1", logfile) << endmsg;

	FILE* logfilefp = fopen (logfile.c_str (), "w");

	if (logfilefp == NULL) {
		PBD::error << string_compose ("Could not open log file '%1'", logfile) << endmsg;
		::exit (EXIT_FAILURE);
	}

	aafi_set_debug (aafi, VERB_DEBUG, 0, logfilefp, NULL, NULL);

	aafi_set_option_str (aafi, "media_location", media_location_path.c_str ());

	if (aafi_load_file (aafi, aaf_file.c_str ())) {
		PBD::error << "Could not load AAF file." << endmsg;
		::exit (EXIT_FAILURE);
	}

	if (prepare_cache (aafi, &media_cache_path)) {
		PBD::error << "Could not prepare media cache path." << endmsg;
		::exit (EXIT_FAILURE);
	}

	printf ("Media Cache : %s\n\n", media_cache_path.c_str ());

	/*
	 * At this stage, AFF was loaded and parsed,
	 * so we can print a few things first.
	 */

	aaf_dump_Header (aafi->aafd);

	aaf_dump_Identification (aafi->aafd);

	printf (" Composition Name       : %ls\n", aafi->compositionName);
	printf (" Composition Start      : %lu\n", eu2sample (aafi->Audio->samplerate, &aafi->compositionStart_editRate, aafi->compositionStart));
	printf (" Composition End        : %lu\n", eu2sample (aafi->Audio->samplerate, &aafi->compositionLength_editRate, aafi->compositionLength) + eu2sample (aafi->Audio->samplerate, &aafi->compositionStart_editRate, aafi->compositionStart));
	printf (" Composition SampleRate : %li Hz\n", aafi->Audio->samplerate);
	printf (" Composition SampleSize : %i bits\n", aafi->Audio->samplesize);
	printf ("\n");

	if (!samplerate) {
		PBD::info << string_compose ("Using AAF file sample rate : %1 Hz", aafi->Audio->samplerate) << endmsg;
		samplerate = aafi->Audio->samplerate;
	} else {
		PBD::info << string_compose ("Ignoring AAF file sample rate (%li Hz), using user defined : %1 Hz", aafi->Audio->samplerate, samplerate) << endmsg;
	}

	aafRational_t samplerate_r;

	samplerate_r.numerator   = samplerate;
	samplerate_r.denominator = 1;

	if (!samplesize) {
		PBD::info << string_compose ("Using AAF file bit depth : %1 bits", aafi->Audio->samplesize) << endmsg;
		samplesize = aafi->Audio->samplesize;
	} else {
		PBD::info << string_compose ("Ignoring AAF file bit depth (%1 bits), using user defined : %2 bits", aafi->Audio->samplesize, samplesize) << endmsg;
	}

	switch (samplesize) {
		case 16:
			bitdepth = ARDOUR::FormatInt16;
			break;
		case 24:
			bitdepth = ARDOUR::FormatInt24;
			break;
		case 32:
			bitdepth = ARDOUR::FormatFloat;
			break;
		default:
			PBD::error << string_compose ("Invalid sample size (%1). Sample size must be either 16, 24 or 32.", samplesize) << endmsg;
			::exit (EXIT_FAILURE);
	}

	if (session_name.empty () && aafi->compositionName && aafi->compositionName[0] != 0x00) {
		wstring ws_session_name = std::wstring (aafi->compositionName);
		session_name            = string (ws_session_name.begin (), ws_session_name.end ());
		PBD::info << string_compose ("Using AAF composition name for Ardour session name : %1", aafi->compositionName) << endmsg;
	} else if (session_name.empty () || session_name == "AAFFILE") {
		/*
		 * Code from gtk2_ardour/utils_videotl.cc
		 * VideoUtils::strip_file_extension()
		 */
		std::string infile = Glib::path_get_basename (string (aafi->aafd->cfbd->file));
		char *      ext, *bn = strdup (infile.c_str ());
		if ((ext = strrchr (bn, '.'))) {
			if (!strchr (ext, G_DIR_SEPARATOR)) {
				*ext = 0;
			}
		}
		session_name = std::string (bn);
		free (bn);

		if (session_name.empty ()) {
			PBD::info << string_compose ("AAF has no composition name, using AAF file name for Ardour session name : %1", session_name) << endmsg;
		} else {
			PBD::info << string_compose ("Force using AAF file name for Ardour session name : %1", session_name) << endmsg;
		}
	}

	laaf_util_clean_filename (&session_name[0]);

	if (Glib::file_test (string (output_folder + G_DIR_SEPARATOR + session_name), Glib::FILE_TEST_IS_DIR)) {
		PBD::error << string_compose ("Session folder already exists '%1'", string (output_folder + G_DIR_SEPARATOR + session_name)) << endmsg;
		::exit (EXIT_FAILURE);
	}

	Session* s = NULL;

	try {
		s = create_new_session (output_folder + G_DIR_SEPARATOR + session_name /*session_file*/, session_name, samplerate, bitdepth, master_bus_chn, template_path);
	} catch (ARDOUR::SessionException& e) {
		// cerr << "Error: " << e.what () << "\n";
		PBD::error << string_compose ("Could not create ardour session : %1", e.what ()) << endmsg;
		SessionUtils::unload_session (s);
		SessionUtils::cleanup ();
		aafi_release (&aafi);
		::exit (EXIT_FAILURE);
	} catch (...) {
		// cerr << "Error: unknown exception.\n";
		PBD::error << "Could not create ardour session." << endmsg;
		SessionUtils::unload_session (s);
		SessionUtils::cleanup ();
		aafi_release (&aafi);
		::exit (EXIT_FAILURE);
	}

	/*
	 *
	 *  Extract audio files and import as sources
	 *  libs/ardour/import_pt.cc#L188
	 *
	 */

	SourceList                      oneClipSources;
	ARDOUR::ImportStatus            import_status;
	vector<std::shared_ptr<Region>> source_regions;
	timepos_t                       pos = timepos_t::max (Temporal::AudioTime);

	aafiAudioEssence* audioEssence = NULL;

	foreachEssence (audioEssence, aafi->Audio->Essences)
	{
		/*
		 *  If we extract embedded essences to `s->session_directory().sound_path()` then we end up with a duplicate on import.
		 *  So we extract essence to a cache folder
		 */

		if (audioEssence->is_embedded) {
			if (media_cache_path.empty ()) {
				PBD::error << "Could not extract audio file from AAF : media cache was not set." << endmsg;
				continue;
			}
			if (aafi_extract_audio_essence (aafi, audioEssence, media_cache_path.c_str (), NULL) < 0) {
				PBD::error << string_compose ("Could not extract audio file '%1' from AAF.", audioEssence->unique_file_name) << endmsg;
				continue; // TODO or fail ?
			}
		} else {
			if (!audioEssence->usable_file_path) {
				PBD::error << string_compose ("Could not locate external audio file: '%1'", audioEssence->original_file_path) << endmsg;
				continue; // TODO or fail ?
			}
		}

		if (!import_sndfile_as_region (s, audioEssence, SrcBest, pos, oneClipSources, import_status, &source_regions)) {
			PBD::error << string_compose ("Could not import '%1' to session.", audioEssence->unique_file_name) << endmsg;
			continue; // TODO or fail ?
		}

		audioEssence->user = new SourceList (oneClipSources);

		PBD::info << string_compose ("Source file '%1' successfully imported to session.", audioEssence->unique_file_name) << endmsg;
	}

	oneClipSources.clear ();

	/*
	 *  Get timeline offset as sample value
	 */
	aafPosition_t sessionStart = convertEditUnit (aafi->compositionStart, aafi->compositionStart_editRate, samplerate_r);

	/*
	 *
	 *  Create all audio clips
	 *
	 */

	aafiAudioTrack*   aafAudioTrack = NULL;
	aafiTimelineItem* aafAudioItem  = NULL;
	aafiAudioClip*    aafAudioClip  = NULL;

	foreach_audioTrack (aafAudioTrack, aafi)
	{
		std::shared_ptr<AudioTrack> track = prepare_audio_track (aafAudioTrack, s);

		foreach_Item (aafAudioItem, aafAudioTrack)
		{
			if (aafAudioItem->type != AAFI_AUDIO_CLIP) {
				continue;
			}

			aafAudioClip = (aafiAudioClip*)aafAudioItem->data;

			if (aafAudioClip->Essence == NULL) {
				PBD::error << "AAF clip has no essence" << endmsg;
				continue;
			}

			/* converts whatever edit_rate clip is in, to samples */
			aafPosition_t clipPos = convertEditUnit (aafAudioClip->pos, *aafAudioClip->track->edit_rate, samplerate_r);

			PBD::info << string_compose ("Importing new clip %1 [%2 dB] on track %3 @%4",
			             aafAudioClip->Essence->unique_file_name,
			             ((aafAudioClip->gain && aafAudioClip->gain->flags & AAFI_AUDIO_GAIN_CONSTANT) ? 20 * log10 (aafRationalToFloat (aafAudioClip->gain->value[0])) : 0),
			             aafAudioClip->track->number,
			             timecode_format_sampletime ((clipPos + sessionStart), samplerate, aafi->Timecode->fps, false))
			          << endmsg;

			aafiAudioEssence* audioEssence = aafAudioClip->Essence;

			if (!audioEssence || !audioEssence->user) {
				PBD::error << string_compose ("Could not create new region for clip %1 : Missing audio essence", aafAudioClip->Essence->unique_file_name) << endmsg;
				continue;
			}

			SourceList* oneClipSources = static_cast<SourceList*> (audioEssence->user);

			if (oneClipSources->size () == 0) {
				PBD::error << string_compose ("Could not create new region for clip %1: Region has no source", aafAudioClip->Essence->unique_file_name) << endmsg;
				continue;
			}

			std::shared_ptr<Region> region = create_region (source_regions, aafAudioClip, *oneClipSources, sessionStart, samplerate_r);

			if (!region) {
				PBD::error << string_compose ("Could not create new region for clip %2", aafAudioClip->Essence->unique_file_name) << endmsg;
				::exit (EXIT_FAILURE);
			}

			/* Put region on track */

			std::shared_ptr<Playlist> playlist = track->playlist ();

			playlist->add_region (region, timepos_t (clipPos + sessionStart));

			set_region_gain (aafAudioClip, region, s);

			set_region_fade (aafAudioClip, region, &samplerate_r);

			if (aafAudioClip->mute) {
				region->set_muted (true);
			}
		}
	}

	aafiMarker* marker = NULL;

	foreachMarker (marker, aafi)
	{
		aafPosition_t markerStart = sessionStart + convertEditUnit (marker->start, *marker->edit_rate, samplerate_r);
		aafPosition_t markerEnd   = sessionStart + convertEditUnit ((marker->start + marker->length), *marker->edit_rate, samplerate_r);

		wstring markerName (marker->name);

		Location* location;

		if (marker->length == 0) {
			location = new Location (*s, timepos_t (markerStart), timepos_t (markerStart), string (markerName.begin (), markerName.end ()), Location::Flags (Location::IsMark));
		} else {
			location = new Location (*s, timepos_t (markerStart), timepos_t (markerEnd), string (markerName.begin (), markerName.end ()), Location::Flags (Location::IsRangeMarker));
		}

		s->locations ()->add (location, true);
	}

	set_session_range (s, aafi);

	/* Import Video from AAF: SegFault ! */
	// set_session_video_from_aaf( s, aafi );

	set_session_timecode (s, aafi);

	import_status.progress = 1.0;
	import_status.done     = true;
	s->save_state ("");
	import_status.sources.clear ();
	import_status.all_done = true;

	/* clear */

	foreachEssence (audioEssence, aafi->Audio->Essences)
	{
		if (audioEssence && audioEssence->user) {
			static_cast<SourceList*> (audioEssence->user)->clear ();
		}
	}

	source_regions.clear ();

	if (!keep_cache) {
		clear_cache (aafi, media_cache_path);
	}

	SessionUtils::unload_session (s);
	SessionUtils::cleanup ();

	aafi_release (&aafi);

	fclose (logfilefp);

	return 0;
}

// static void set_session_video_from_aaf( Session *s, AAF_Iface *aafi )
// {
//   if ( aafi->Video->Tracks && aafi->Video->Tracks->Items ) {
//
// 		aafiVideoClip *videoClip = (aafiVideoClip*)&aafi->Video->Tracks->Items->data;
//
// 		// printf( "\n\n\nGot video Track and Item : %ls\n\n\n", videoClip->Essence->original_file_path/*->Essence->original_file_path*/ );
//     // char origf[PATH_MAX+1];
//     // snprintf(origf, PATH_MAX, "%ls", videoClip->Essence->original_file_path ); // TODOPATH
//     // printf("Looking for : %s\n", strrchr(origf, '/') + 1 );
//
// 		char *file = locate_external_essence_file( aafi, videoClip->Essence->original_file_path, NULL );
//
// 		if ( file != NULL ) {
//       PBD::info << string_compose ("Importing video : %1", Glib::path_get_basename(string(file))/*fop_get_filename(file)*/) << endmsg;
//
// 			/* get absolute video file path */
// 			std::string absFile (PBD::canonical_path (file));
//
// 			// /* get original mxf video filename */
// 			// char *file_name = remove_file_ext( basename(file), '.', '/' );
// 			//
// 			// /* creates project video folder */
// 			// mkdir( s->session_directory().video_path().c_str(), 0755 );
// 			//
// 			// /* extract mpeg video from original mxf */
// 			// char cmdstr[PATH_MAX*6];
// 			// snprintf( cmdstr, sizeof(cmdstr), "ffmpeg -y -i \"%s\" -c copy -f mpeg2video \"%s/%s.mpg\"", absFile, s->session_directory().video_path().c_str(), file_name );
// 			// //snprintf( cmdstr, sizeof(cmdstr), "ffmpeg -y -i \"%s\" -c copy -map_metadata 0 \"%s/%s.mkv\"", absFile, s->session_directory().video_path().c_str(), file_name );
// 			//
// 			// system(cmdstr);
//
//
// 			/* Add video to Ardour
// 			 * ===================
// 			 * https://github.com/Ardour/ardour/blob/6987196ea18cbf171e22ed62760962576ccb54da/gtk2_ardour/ardour_ui_video.cc#L317
// 			 *
// 			 *	<Videotimeline Filename="/home/agfline/Developpement/ardio/watchfolder/3572607_RUGBY_F2_S65CFA3D0V.mxf" AutoFPS="1" LocalFile="1" OriginalVideoFile="/home/agfline/Developpement/ardio/watchfolder/3572607_RUGBY_F2_S65CFA3D0V.mxf"/>
//             <RulerVisibility timecode="1" bbt="1" samples="0" minsec="0" tempo="1" meter="1" marker="1" rangemarker="1" transportmarker="1" cdmarker="1" videotl="1"/>
// 			 */
//
// 			XMLNode* videoTLnode = new XMLNode( "Videotimeline" );
// 			videoTLnode->set_property( "Filename", absFile/*string(file_name) + ".mpg"*/ );
// 			videoTLnode->set_property( "AutoFPS", true );
// 			videoTLnode->set_property( "LocalFile", true );
// 			videoTLnode->set_property( "OriginalVideoFile", string(absFile) );
// 			videoTLnode->set_property( "id", 51 );
// 			videoTLnode->set_property( "Height", 3 );
// 			videoTLnode->set_property( "VideoOffsetLock", true );
// 			videoTLnode->set_property( "VideoOffset", eu2sample( s->sample_rate(), videoClip->track->Video->tc->edit_rate, (videoClip->pos + videoClip->track->Video->tc->start)) );
//
//       // printf("\n\n\n%li  |  %li\n\n\n", videoClip->pos, videoClip->track->Video->tc->start );
//
// 			XMLNode* videoMONnode = new XMLNode( "Videomonitor" );
// 			videoMONnode->set_property( "active", true );
//
//
//
// 			XMLNode* xjnode = new XMLNode( "XJSettings" );
//
//       XMLNode* xjsetting;
//       xjsetting = xjnode->add_child( "XJSetting" );
//       xjsetting->set_property( "k", "set offset" );
//       xjsetting->set_property( "v", "-90000" ); //videoClip->pos * videoClip->track->Video->tc->edit_rate );
//
//       xjsetting = xjnode->add_child( "XJSetting" );
//       xjsetting->set_property( "k", "osd smpte" );
//       xjsetting->set_property( "v", "95" );
//
//       /* video_monitor.cc
//       <XJSettings>
//         <XJSetting k="window fullscreen" v="on"/>
//         <XJSetting k="set offset" v="-90000"/>
//         <XJSetting k="osd smpte" v="95"/>
//       </XJSettings>
//       */
//
// 			s->add_extra_xml(*xjnode);
// 			s->add_extra_xml(*videoTLnode);
// 			s->add_extra_xml(*videoMONnode);
//
// 			// s->set_dirty();
// 		}
//     else {
//       PBD::error << string_compose ("Could not locate video file: %1", videoClip->Essence->original_file_path) << endmsg;
//     }
// 	}
//   else {
//     PBD::error << "Could not retrieve video from AAF." << endmsg;
//   }
// }
