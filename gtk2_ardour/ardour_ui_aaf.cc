/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2023 Adrien Gesta-Fline <dev.agfline@posteo.net>
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

#include "pbd/basename.h"
#include "pbd/convert.h"
#include "pbd/file_utils.h"

#include "ardour/audio_track.h"
#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/filename_extensions.h"
#include "ardour/import_status.h"
#include "ardour/playlist.h"
#include "ardour/plugin_manager.h"
#include "ardour/region_factory.h"
#include "ardour/source_factory.h"
#include "ardour/utils.h"

#include "aaf/libaaf.h"
#include "aaf/utils.h"

#include "ardour_ui.h"
#include "public_editor.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;
using namespace ARDOUR;

static void
aaf_debug_callback (struct dbg* dbg, void* ctxdata, int lib, int type, const char* srcfile, const char* srcfunc, int lineno, const char* msg, void* user)
{
}

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

static std::shared_ptr<AudioTrack>
prepare_audio_track (aafiAudioTrack* aafTrack, Session* s)
{
	/* Use existing track */
	std::shared_ptr<AudioTrack> track = get_nth_audio_track ((aafTrack->number - 1), s->get_routes ());

	if (track) {
		return track;
	}

	/* ..or create a new track */
	wstring ws_track_name = std::wstring (aafTrack->name);
	string  track_name    = string (ws_track_name.begin (), ws_track_name.end ());

	uint32_t outputs = 2;
	if (s->master_out ()) {
		outputs = max (outputs, s->master_out ()->n_inputs ().n_audio ());
	}

	list<std::shared_ptr<AudioTrack>> at (s->new_audio_track (aafTrack->format, outputs, NULL, 1, track_name, PresentationInfo::max_order));

	if (at.empty ()) {
		PBD::fatal << "AAF: Could not create new audio track." << endmsg;
		abort (); /*NOTREACHED*/
	}

	return at.back ();
}

static bool
import_sndfile_as_region (Session* s, struct aafiAudioEssence* audioEssence, SrcQuality quality, timepos_t& pos, SourceList& sources, ImportStatus& status, vector<std::shared_ptr<Region>>& regions)
{
	wstring ws (audioEssence->usable_file_path);
	string  usable_file_path (ws.begin (), ws.end ());

	/* Import the source */
	status.clear ();

	status.current                 = 1;
	status.total                   = 1;
	status.freeze                  = false;
	status.quality                 = quality;
	status.replace_existing_source = false;
	status.split_midi_channels     = false;
	status.import_markers          = false;
	status.done                    = false;
	status.cancel                  = false;

	status.paths.push_back (usable_file_path);

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
	regions.push_back (region);
	return true;
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

		for (int i = 0; i < level->pts_cnt; ++i) {
			al->fast_simple_add (timepos_t (aafRationalToFloat (level->time[i]) * region->length ().samples ()), aafRationalToFloat (level->value[i]));
		}
	}
}

static FadeShape
aaf_fade_interpol_to_ardour_fade_shape (aafiInterpolation_e interpol)
{
	switch (interpol & AAFI_INTERPOL_MASK) {
		case AAFI_INTERPOL_NONE:
			return FadeConstantPower;
		case AAFI_INTERPOL_LINEAR:
			return FadeLinear;
		case AAFI_INTERPOL_LOG:
			return FadeConstantPower;
		case AAFI_INTERPOL_CONSTANT:
			return FadeConstantPower;
		case AAFI_INTERPOL_POWER:
			return FadeConstantPower;
		case AAFI_INTERPOL_BSPLINE:
			return FadeConstantPower;
		default:
			return FadeConstantPower;
	}
}

static void
set_region_fade (aafiAudioClip* aafAudioClip, std::shared_ptr<Region> region, aafRational_t const& samplerate)
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
		fade_shape = aaf_fade_interpol_to_ardour_fade_shape ((aafiInterpolation_e) (fadein->flags & AAFI_INTERPOL_MASK));
		fade_len   = convertEditUnit (fadein->len, *aafAudioClip->track->edit_rate, samplerate);

		std::dynamic_pointer_cast<AudioRegion> (region)->set_fade_in (fade_shape, fade_len);
	}

	if (fadeout != NULL) {
		fade_shape = aaf_fade_interpol_to_ardour_fade_shape ((aafiInterpolation_e) (fadeout->flags & AAFI_INTERPOL_MASK));
		fade_len   = convertEditUnit (fadeout->len, *aafAudioClip->track->edit_rate, samplerate);

		std::dynamic_pointer_cast<AudioRegion> (region)->set_fade_out (fade_shape, fade_len);
	}
}

static void
set_session_timecode (AAF_Iface* aafi, Session* s)
{
	using namespace Timecode;

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

/* Create and open Sesssion from AAF
 * return > 0 if file is not a [valid] AAF
 * return < 0 if session creation failed.
 * return 0 on success. path and snapshot are set.
 */
int
ARDOUR_UI::new_session_from_aaf (string const& aaf, string const& target_dir, string& path, string& snapshot)
{
	if (PBD::downcase (aaf).find (advanced_authoring_format_suffix) == string::npos) {
		return 1;
	}

	if (_session) {
		if (unload_session (false)) {
			/* unload cancelled by user */
			return 1;
		}
	}

	AAF_Iface* aafi = aafi_alloc (NULL);

	uint32_t aaf_resolve_options  = 0;
	uint32_t aaf_protools_options = 0;

	aafi_set_option_int (aafi, "trace", 1);
	aafi_set_option_int (aafi, "protools", aaf_protools_options);
	aafi_set_option_int (aafi, "resolve", aaf_resolve_options);

	// XXX use Glib::convert_with_fallback
	aafi->ctx.options.forbid_nonlatin_filenames = 1;

	aafi_set_debug (aafi, VERB_DEBUG, 0, 0, aaf_debug_callback, this);

	//aafi_set_option_str (aafi, "media_location", media_location_path.c_str ());

	if (aafi_load_file (aafi, aaf.c_str ())) {
		error << "AAF: Could not load AAF file." << endmsg;
		aafi_release (&aafi);
		return -1;
	}

	/* extract or set session name */
	if (aafi->compositionName && aafi->compositionName[0] != 0x00) {
		wstring ws_session_name = std::wstring (aafi->compositionName);
		snapshot                = string (ws_session_name.begin (), ws_session_name.end ());
	} else {
		snapshot = basename_nosuffix (aaf);
	}

	snapshot = legalize_for_universal_path (snapshot);
	path     = Glib::build_filename (target_dir, snapshot);

	if (Glib::file_test (path, Glib::FILE_TEST_EXISTS)) {
		error << string_compose (_("AAF: Destination '%1' already exists."), path) << endmsg;
		snapshot = ""; // XXX?
		path     = "";
		aafi_release (&aafi);
		return -1;
	}

	/* Create media cache */
	GError* err = NULL;
	char*   td  = g_dir_make_tmp ("aaf-cache-XXXXXX", &err);

	if (!td) {
		error << string_compose (_("AAF: Could not prepare media cache: %1"), err->message) << endmsg;
		aafi_release (&aafi);
		return -1;
	}

	const string media_cache_path = PBD::canonical_path (td);
	g_free (td);
	g_clear_error (&err);

	/* all systems go. create sessions */
	BusProfile bus_profile;
	bus_profile.master_out_channels = 2;

	aafRational_t samplerate_r;

	samplerate_r.numerator   = aafi->Audio->samplerate;
	samplerate_r.denominator = 1;

	std::string restore_backend;
	if (!AudioEngine::instance()->running ()) {
		AudioEngine* e = AudioEngine::instance();
		restore_backend = e->current_backend_name ();
		e->set_backend ("None (Dummy)", "", "");
		e->start ();
		PluginManager::instance ().refresh (true);
		attach_to_engine ();
	}
	if (!AudioEngine::instance()->running ()) {
		error << _("Could not start [dummy] engine for AAF import .") << endmsg;
		return -1;
	}

	build_session_stage_two (path, snapshot, "", bus_profile, false, Temporal::AudioTime, aafi->Audio->samplerate);

	if (!_session) {
		aafi_release (&aafi);
		PBD::remove_directory (media_cache_path);
		if (!restore_backend.empty ()) {
			AudioEngine::instance()->stop ();
			AudioEngine::instance()->set_backend (restore_backend, "", "");
		}
		error << _("Could not create new session for AAF import .") << endmsg;
		return -1;
	}

	switch (aafi->Audio->samplesize) {
		case 16:
			_session->config.set_native_file_data_format (ARDOUR::FormatInt16);
			break;
		case 24:
			_session->config.set_native_file_data_format (ARDOUR::FormatInt24);
			break;
		case 32:
			_session->config.set_native_file_data_format (ARDOUR::FormatFloat);
			break;
		default:
			break;
	}

	/* Import Sources */

	SourceList                      oneClipSources;
	ARDOUR::ImportStatus            import_status;
	vector<std::shared_ptr<Region>> source_regions;
	timepos_t                       pos = timepos_t::max (Temporal::AudioTime);

	aafiAudioEssence* audioEssence = NULL;

	for (aafiAudioEssence* audioEssence = aafi->Audio->Essences; audioEssence != NULL; audioEssence = audioEssence->next) {
		/*  If we extract embedded essences to `s->session_directory().sound_path()` then we end up with a duplicate on import.
		 *  So we extract essence to a cache folder
		 */

		if (audioEssence->is_embedded) {
			if (media_cache_path.empty ()) {
				error << _("Could not extract audio file from AAF: media cache was not set.") << endmsg;
				continue;
			}
			if (aafi_extract_audio_essence (aafi, audioEssence, media_cache_path.c_str (), NULL) < 0) {
				error << string_compose (_("AAF: Could not extract audio file '%1' from AAF."), audioEssence->unique_file_name) << endmsg;
				continue;
			}
		} else {
			if (!audioEssence->usable_file_path) {
				error << string_compose (_("AAF: Could not locate external audio file: '%1'"), audioEssence->original_file_path) << endmsg;
				continue;
			}
		}

		if (!import_sndfile_as_region (_session, audioEssence, SrcBest, pos, oneClipSources, import_status, source_regions)) {
			error << string_compose (_("AAF: Could not import '%1' to session."), audioEssence->unique_file_name) << endmsg;
			continue;
		}

		audioEssence->user = new SourceList (oneClipSources);

		info << string_compose ("Source file '%1' successfully imported to session.", audioEssence->unique_file_name) << endmsg;
	}

	oneClipSources.clear ();

	aafPosition_t sessionStart = convertEditUnit (aafi->compositionStart, aafi->compositionStart_editRate, samplerate_r);

	aafiAudioTrack*   aafAudioTrack = NULL;
	aafiTimelineItem* aafAudioItem  = NULL;
	aafiAudioClip*    aafAudioClip  = NULL;

	foreach_audioTrack (aafAudioTrack, aafi)
	{
		std::shared_ptr<AudioTrack> track = prepare_audio_track (aafAudioTrack, _session);

		foreach_Item (aafAudioItem, aafAudioTrack)
		{
			if (aafAudioItem->type != AAFI_AUDIO_CLIP) {
				continue;
			}

			aafAudioClip = (aafiAudioClip*)aafAudioItem->data;

			if (aafAudioClip->Essence == NULL) {
				error << _("AAF: Clip has no essence.") << endmsg;
				continue;
			}

			/* converts whatever edit_rate clip is in, to samples */
			aafPosition_t clipPos = convertEditUnit (aafAudioClip->pos, *aafAudioClip->track->edit_rate, samplerate_r);

			aafiAudioEssence* audioEssence = aafAudioClip->Essence;

			if (!audioEssence || !audioEssence->user) {
				error << string_compose (_("AAF: Could not create new region for clip '%1': Missing audio essence"), aafAudioClip->Essence->unique_file_name) << endmsg;
				continue;
			}

			SourceList* oneClipSources = static_cast<SourceList*> (audioEssence->user);

			if (oneClipSources->size () == 0) {
				error << string_compose (_("AAF: Could not create new region for clip '%1': Region has no source"), aafAudioClip->Essence->unique_file_name) << endmsg;
				continue;
			}

			std::shared_ptr<Region> region = create_region (source_regions, aafAudioClip, *oneClipSources, sessionStart, samplerate_r);

			if (!region) {
				error << string_compose (_("AAF: Could not create new region for clip '%2'"), aafAudioClip->Essence->unique_file_name) << endmsg;
				continue;
			}

			track->playlist ()->add_region (region, timepos_t (clipPos + sessionStart));
			set_region_gain (aafAudioClip, region, _session);
			set_region_fade (aafAudioClip, region, samplerate_r);
			if (aafAudioClip->mute) {
				region->set_muted (true);
			}
		}
	}

	for (aafiMarker* marker = aafi->Markers; marker != NULL; marker = marker->next) {
		aafPosition_t markerStart = sessionStart + convertEditUnit (marker->start, *marker->edit_rate, samplerate_r);
		aafPosition_t markerEnd   = sessionStart + convertEditUnit ((marker->start + marker->length), *marker->edit_rate, samplerate_r);

		wstring markerName (marker->name);

		Location* location;

		if (marker->length == 0) {
			location = new Location (*_session, timepos_t (markerStart), timepos_t (markerStart), string (markerName.begin (), markerName.end ()), Location::Flags (Location::IsMark));
		} else {
			location = new Location (*_session, timepos_t (markerStart), timepos_t (markerEnd), string (markerName.begin (), markerName.end ()), Location::Flags (Location::IsRangeMarker));
		}

		_session->locations ()->add (location, true);
	}

	/* set session range */
	samplepos_t start = samplepos_t (eu2sample (_session->nominal_sample_rate (), &aafi->compositionStart_editRate, aafi->compositionStart));
	samplepos_t end   = samplepos_t (eu2sample (_session->nominal_sample_rate (), &aafi->compositionLength_editRate, aafi->compositionLength)) + start;
	_session->maybe_update_session_range (timepos_t (start), timepos_t (end));

	/* set timecode */
	set_session_timecode (aafi, _session);

	the_editor ().access_action ("Editor", "zoom-to-session");

	/* Cleanup */
	import_status.progress = 1.0;
	import_status.done     = true;
	import_status.sources.clear ();
	import_status.all_done = true;

	_session->save_state ("");

	/* clear */

	foreachEssence (audioEssence, aafi->Audio->Essences)
	{
		if (audioEssence && audioEssence->user) {
			static_cast<SourceList*> (audioEssence->user)->clear ();
		}
	}

	source_regions.clear ();

	PBD::remove_directory (media_cache_path);

	aafi_release (&aafi);

	if (!restore_backend.empty ()) {
		AudioEngine::instance()->stop ();
		AudioEngine::instance()->set_backend (restore_backend, "", "");
	}
	return 0;
}
