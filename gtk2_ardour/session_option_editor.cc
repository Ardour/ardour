/*
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2012 Ben Loftis <ben@harrisonconsoles.com>
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

#include "ardour/session.h"
#include "ardour/transport_master_manager.h"

#include "actions.h"
#include "gui_thread.h"
#include "session_option_editor.h"
#include "search_path_option.h"
#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace Timecode;

SessionOptionEditor::SessionOptionEditor (Session* s)
	: OptionEditorWindow (&(s->config), _("Session Properties"))
	, _session_config (&(s->config))
{
	set_session (s);

	set_name ("SessionProperties");

	/* TIMECODE*/

	add_option (_("Timecode"), new OptionEditorHeading (_("Timecode Settings")));

	ComboOption<TimecodeFormat>* smf = new ComboOption<TimecodeFormat> (
		"timecode-format",
		_("Timecode frames-per-second"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_timecode_format),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_timecode_format)
		);

	smf->add (timecode_23976, _("23.976"));
	smf->add (timecode_24, _("24"));
	smf->add (timecode_24976, _("24.975"));
	smf->add (timecode_25, _("25"));
	smf->add (timecode_2997, _("29.97"));
	smf->add (timecode_2997drop, _("29.97 drop"));
	smf->add (timecode_30, _("30"));
	smf->add (timecode_30drop, _("30 drop"));
	smf->add (timecode_5994, _("59.94"));
	smf->add (timecode_60, _("60"));

	add_option (_("Timecode"), smf);

	_vpu = new ComboOption<float> (
		"video-pullup",
		_("Pull-up / pull-down"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_video_pullup),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_video_pullup)
		);

	_vpu->add (4.1667 + 0.1, _("4.1667 + 0.1%"));
	_vpu->add (4.1667, _("4.1667"));
	_vpu->add (4.1667 - 0.1, _("4.1667 - 0.1%"));
	_vpu->add (0.1, _("0.1"));
	_vpu->add (0, _("none"));
	_vpu->add (-0.1, _("-0.1"));
	_vpu->add (-4.1667 + 0.1, _("-4.1667 + 0.1%"));
	_vpu->add (-4.1667, _("-4.1667"));
	_vpu->add (-4.1667 - 0.1, _("-4.1667 - 0.1%"));

	add_option (_("Timecode"), _vpu);
	add_option (_("Timecode"), new OptionEditorHeading (_("Ext Timecode Offsets")));

	ClockOption* sco = new ClockOption (
		"slave-timecode-offset",
		_("Slave Timecode offset"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_slave_timecode_offset),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_slave_timecode_offset)
		);

	sco->set_session (_session);
	sco->clock().set_negative_allowed (true);
	Gtkmm2ext::UI::instance()->set_tip (sco->tip_widget(), _("The specified offset is added to the received timecode (MTC or LTC)."));

	add_option (_("Timecode"), sco);

	ClockOption* gco = new ClockOption (
		"timecode-generator-offset",
		_("Timecode Generator offset"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_timecode_generator_offset),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_timecode_generator_offset)
		);

	gco->set_session (_session);
	gco->clock().set_negative_allowed (true);
	Gtkmm2ext::UI::instance()->set_tip (gco->tip_widget(), _("Specify an offset which is added to the generated timecode (so far only LTC)."));

	add_option (_("Timecode"), gco);

	add_option (_("Timecode"), new OptionEditorHeading (_("JACK Transport/Time Settings")));

	add_option (_("Timecode"), new BoolOption (
			    "jack-time-master",
			    string_compose (_("%1 is JACK Time Master (provides Bar|Beat|Tick and other information to JACK)"), PROGRAM_NAME),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_jack_time_master),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_jack_time_master)
			    ));

	/* Sync */

	add_option (_("Sync"), new OptionEditorHeading (_("A/V Synchronization")));
	add_option (_("Sync"), new BoolOption (
			    "use-video-file-fps",
			    _("Use Video File's FPS Instead of Timecode Value for Timeline and Video Monitor."),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_use_video_file_fps),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_use_video_file_fps)
			    ));

	add_option (_("Sync"), new BoolOption (
			    "videotimeline-pullup",
			    _("Apply Pull-Up/Down to Video Timeline and Video Monitor (Unless using JACK-sync)."),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_videotimeline_pullup),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_videotimeline_pullup)
			    ));

	add_option (_("Sync"), new OptionEditorBlank ());

	/* FADES */

	add_option (_("Fades"), new OptionEditorHeading (_("Audio Fades")));

	add_option (_("Fades"), new BoolOption (
			    "use-transport-fades",
			    _("Declick when transport starts and stops"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_use_transport_fades),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_use_transport_fades)
			    ));

	add_option (_("Fades"), new BoolOption (
			    "use-monitor-fades",
			    _("Declick when monitor state changes"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_use_monitor_fades),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_use_monitor_fades)
			    ));

	add_option (_("Fades"), new BoolOption (
			    "use-region-fades",
			    _("Region fades active"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_use_region_fades),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_use_region_fades)
			    ));

	add_option (_("Fades"), new BoolOption (
			    "show-region-fades",
			    _("Region fades visible"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_region_fades),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_region_fades)
			    ));

	/* Media */

	add_option (_("Media"), new OptionEditorHeading (_("Audio File Format")));

	_sf = new ComboOption<SampleFormat> (
		"native-file-data-format",
		_("Sample format"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_native_file_data_format),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_native_file_data_format)
		);
	add_option (_("Media"), _sf);
	/* refill available sample-formats, depending on file-format */
	parameter_changed ("native-file-header-format");

	ComboOption<HeaderFormat>* hf = new ComboOption<HeaderFormat> (
		"native-file-header-format",
		_("File type"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_native_file_header_format),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_native_file_header_format)
		);

	hf->add (BWF, _("Broadcast WAVE (4GB size limit)"));
#ifdef HAVE_RF64_RIFF
	hf->add (MBWF, _("Broadcast RF64"));
#endif
	hf->add (WAVE, _("WAVE (4GB size limit)"));
	hf->add (WAVE64, _("WAVE-64"));
	hf->add (CAF, _("CAF"));
	hf->add (RF64, _("RF64"));
#ifdef HAVE_RF64_RIFF
	hf->add (RF64_WAV, _("RF64 (WAV compatible)"));
#endif
	hf->add (FLAC, _("FLAC"));

	add_option (_("Media"), hf);

	add_option (S_("Files|Locations"), new OptionEditorHeading (_("File Locations")));

	SearchPathOption* spo = new SearchPathOption ("audio-search-path", _("Search for audio files in:"),
			_session->path(),
			sigc::mem_fun (*_session_config, &SessionConfiguration::get_audio_search_path),
			sigc::mem_fun (*_session_config, &SessionConfiguration::set_audio_search_path));
	add_option (S_("Files|Locations"), spo);

	spo = new SearchPathOption ("midi-search-path", _("Search for MIDI files in:"),
			_session->path(),
			sigc::mem_fun (*_session_config, &SessionConfiguration::get_midi_search_path),
			sigc::mem_fun (*_session_config, &SessionConfiguration::set_midi_search_path));

	add_option (S_("Files|Locations"), spo);

	/* File Naming  */

	add_option (_("Filenames"), new OptionEditorHeading (_("File Naming")));

	BoolOption *bo;

	bo = new RouteDisplayBoolOption (
			"track-name-number",
			_("Prefix Track number"),
			sigc::mem_fun (*_session_config, &SessionConfiguration::get_track_name_number),
			sigc::mem_fun (*_session_config, &SessionConfiguration::set_track_name_number)
			);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("Adds the current track number to the beginning of the recorded file name."));
	add_option (_("Filenames"), bo);

	bo = new BoolOption (
			"track-name-take",
			_("Prefix Take Name"),
			sigc::mem_fun (*_session_config, &SessionConfiguration::get_track_name_take),
			sigc::mem_fun (*_session_config, &SessionConfiguration::set_track_name_take)
			);
	Gtkmm2ext::UI::instance()->set_tip (bo->tip_widget(),
			_("Adds the Take Name to the beginning of the recorded file name."));
	add_option (_("Filenames"), bo);

	_take_name = new EntryOption (
		"take-name",
		_("Take Name"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_take_name),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_take_name)
		);
	_take_name->set_invalid_chars(".");
	_take_name->set_sensitive(_session_config->get_track_name_take());

	add_option (_("Filenames"), _take_name);

	/* Monitoring */

	add_option (_("Monitoring"), new OptionEditorHeading (_("Monitoring")));
	add_option (_("Monitoring"), new BoolOption (
				"auto-input",
				_("Track Input Monitoring automatically follows transport state (\"auto-input\")"),
				sigc::mem_fun (*_session_config, &SessionConfiguration::get_auto_input),
				sigc::mem_fun (*_session_config, &SessionConfiguration::set_auto_input)
				));

	add_option (_("Monitoring"), new BoolOption (
				"triggerbox-overrides-disk-monitoring",
				_("Cues containing clips disables implicit (auto) disk monitoring for the track"),
				sigc::mem_fun (*_session_config, &SessionConfiguration::get_triggerbox_overrides_disk_monitoring),
				sigc::mem_fun (*_session_config, &SessionConfiguration::set_triggerbox_overrides_disk_monitoring)
				));

	add_option (_("Monitoring"), new CheckOption (
				"unused",
				_("Use monitor section in this session"),
				ActionManager::get_action(X_("Monitor"), "UseMonitorSection")
				));

	add_option (_("Monitoring"), new OptionEditorBlank ());

	/* Meterbridge */
	add_option (_("Meterbridge"), new OptionEditorHeading (_("Display Options")));

	add_option (_("Meterbridge"), new BoolOption (
			    "show-midi-on-meterbridge",
			    _("Show Midi Tracks"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_midi_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_midi_on_meterbridge)
			    ));

	add_option (_("Meterbridge"), new BoolOption (
			    "show-busses-on-meterbridge",
			    _("Show Busses"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_busses_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_busses_on_meterbridge)
			    ));

	add_option (_("Meterbridge"), new BoolOption (
			    "show-master-on-meterbridge",
			    _("Include Master Bus"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_master_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_master_on_meterbridge)
			    ));

	add_option (_("Meterbridge"), new OptionEditorHeading (_("Button Area")));

	add_option (_("Meterbridge"), new BoolOption (
			    "show-rec-on-meterbridge",
			    _("Rec-enable Button"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_rec_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_rec_on_meterbridge)
			    ));

	add_option (_("Meterbridge"), new BoolOption (
			    "show-mute-on-meterbridge",
			    _("Mute Button"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_mute_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_mute_on_meterbridge)
			    ));

	add_option (_("Meterbridge"), new BoolOption (
			    "show-solo-on-meterbridge",
			    _("Solo Button"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_solo_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_solo_on_meterbridge)
			    ));

	add_option (_("Meterbridge"), new BoolOption (
			    "show-monitor-on-meterbridge",
			    _("Monitor Buttons"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_monitor_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_monitor_on_meterbridge)
			    ));

	add_option (_("Meterbridge"), new BoolOption (
			    "show-fader-on-meterbridge",
			    _("Fader as Gain Knob"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_fader_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_fader_on_meterbridge)
			    ));

	add_option (_("Meterbridge"), new OptionEditorHeading (_("Name Labels")));

	add_option (_("Meterbridge"), new BoolOption (
			    "show-name-on-meterbridge",
			    _("Track Name"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_name_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_name_on_meterbridge)
			    ));

	add_option (_("Meterbridge"), new OptionEditorBlank ());

	/* Misc */

	add_option (_("Misc"), new OptionEditorHeading (_("MIDI Options")));

	add_option (_("Misc"), new BoolOption (
				"midi-copy-is-fork",
				_("MIDI region copies are independent"),
				sigc::mem_fun (*_session_config, &SessionConfiguration::get_midi_copy_is_fork),
				sigc::mem_fun (*_session_config, &SessionConfiguration::set_midi_copy_is_fork)
				));

	ComboOption<InsertMergePolicy>* li = new ComboOption<InsertMergePolicy> (
			"insert-merge-policy",
			_("Policy for handling overlapping notes\n on the same MIDI channel"),
			sigc::mem_fun (*_session_config, &SessionConfiguration::get_insert_merge_policy),
			sigc::mem_fun (*_session_config, &SessionConfiguration::set_insert_merge_policy)
			);

	li->add (InsertMergeReject, _("never allow them"));
	li->add (InsertMergeRelax, _("don't do anything in particular"));
	li->add (InsertMergeReplace, _("replace any overlapped existing note"));
	li->add (InsertMergeTruncateExisting, _("shorten the overlapped existing note"));
	li->add (InsertMergeTruncateAddition, _("shorten the overlapping new note"));
	li->add (InsertMergeExtend, _("replace both overlapping notes with a single note"));

	add_option (_("Misc"), li);

	add_option (_("Misc"), new OptionEditorHeading (_("Glue to Bars and Beats")));

	add_option (_("Misc"), new BoolOption (
				"glue-new-markers-to-bars-and-beats",
				_("Glue new markers to bars and beats"),
				sigc::mem_fun (*_session_config, &SessionConfiguration::get_glue_new_markers_to_bars_and_beats),
				sigc::mem_fun (*_session_config, &SessionConfiguration::set_glue_new_markers_to_bars_and_beats)
				));

	add_option (_("Misc"), new BoolOption (
				"glue-new-regions-to-bars-and-beats",
				_("Glue new regions to bars and beats"),
				sigc::mem_fun (*_session_config, &SessionConfiguration::get_glue_new_regions_to_bars_and_beats),
				sigc::mem_fun (*_session_config, &SessionConfiguration::set_glue_new_regions_to_bars_and_beats)
				));

	add_option (_("Misc"), new OptionEditorHeading (_("Metronome")));

	add_option (_("Misc"), new BoolOption (
				"count-in",
				_("Always count-in when recording"),
				sigc::mem_fun (*_session_config, &SessionConfiguration::get_count_in),
				sigc::mem_fun (*_session_config, &SessionConfiguration::set_count_in)
				));

	add_option (_("Misc"), new OptionEditorHeading (_("Defaults")));

	Gtk::Button* btn = Gtk::manage (new Gtk::Button (_("Use these settings as defaults")));
	btn->signal_clicked().connect (sigc::mem_fun (*this, &SessionOptionEditor::save_defaults));
	add_option (_("Misc"), new FooOption (btn));

	set_current_page (_("Timecode"));
}

void
SessionOptionEditor::parameter_changed (std::string const & p)
{
	OptionEditor::parameter_changed (p);
	if (p == "external-sync") {
		if (TransportMasterManager::instance().current()->type() == Engine) {
			_vpu->set_sensitive(!_session_config->get_external_sync());
		} else {
			_vpu->set_sensitive(true);
		}
	}
	else if (p == "timecode-format") {
		/* update offset clocks */
		parameter_changed("timecode-generator-offset");
		parameter_changed("slave-timecode-offset");
	}
	else if (p == "track-name-take") {
		_take_name->set_sensitive(_session_config->get_track_name_take());
	}
	else if (p == "native-file-header-format") {
		bool need_refill = true;
		_sf->clear ();
		if (_session_config->get_native_file_header_format() == FLAC) {
			_sf->add (FormatInt24, _("24-bit integer"));
			_sf->add (FormatInt16, _("16-bit integer"));
			if (_session_config->get_native_file_data_format() == FormatFloat) {
				_session_config->set_native_file_data_format (FormatInt24);
				need_refill = false;
			}
		} else {
			_sf->add (FormatFloat, _("32-bit floating point"));
			_sf->add (FormatInt24, _("24-bit integer"));
			_sf->add (FormatInt16, _("16-bit integer"));
		}
		if (need_refill) {
			parameter_changed ("native-file-data-format");
		}
	}
}

void
SessionOptionEditor::save_defaults ()
{
	_session->save_default_options();
}
