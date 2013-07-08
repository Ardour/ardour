/*
    Copyright (C) 2000-2010 Paul Davis

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

#include "ardour/session.h"

#include "gui_thread.h"
#include "session_option_editor.h"
#include "search_path_option.h"
#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace Timecode;

SessionOptionEditor::SessionOptionEditor (Session* s)
	: OptionEditor (&(s->config), _("Session Properties"))
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

	add_option (_("Sync"), new BoolOption (
			    "use-video-file-fps",
			    _("Use Video File's FPS Instead of Timecode Value for Timeline and Video Monitor."),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_use_video_file_fps),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_use_video_file_fps)
			    ));

	add_option (_("Sync"), new BoolOption (
			    "videotimeline-pullup",
			    _("Apply Pull-Up/Down to Video Timeline and Video Monitor (Unless in JACK-sync)."),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_videotimeline_pullup),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_videotimeline_pullup)
			    ));

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

	/* FADES */

	ComboOption<CrossfadeChoice>* cfc = new ComboOption<CrossfadeChoice> (
		"xfade-choice",
		_("Default crossfade type"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_xfade_choice),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_xfade_choice)
		);

	cfc->add (ConstantPowerMinus3dB, _("Constant power (-3dB) crossfade"));
	cfc->add (ConstantPowerMinus6dB, _("Linear (-6dB) crossfade"));

	add_option (_("Fades"), cfc);

	add_option (_("Fades"), new SpinOption<float> (
		_("destructive-xfade-seconds"),
		_("Destructive crossfade length"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_destructive_xfade_msecs),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_destructive_xfade_msecs),
		0, 1000, 1, 10,
		_("ms")
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

	add_option (_("Media"), new OptionEditorHeading (_("Audio file format")));

	ComboOption<SampleFormat>* sf = new ComboOption<SampleFormat> (
		"native-file-data-format",
		_("Sample format"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_native_file_data_format),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_native_file_data_format)
		);

	sf->add (FormatFloat, _("32-bit floating point"));
	sf->add (FormatInt24, _("24-bit integer"));
	sf->add (FormatInt16, _("16-bit integer"));

	add_option (_("Media"), sf);

	ComboOption<HeaderFormat>* hf = new ComboOption<HeaderFormat> (
		"native-file-header-format",
		_("File type"),
		sigc::mem_fun (*_session_config, &SessionConfiguration::get_native_file_header_format),
		sigc::mem_fun (*_session_config, &SessionConfiguration::set_native_file_header_format)
		);

	hf->add (BWF, _("Broadcast WAVE"));
	hf->add (WAVE, _("WAVE"));
	hf->add (WAVE64, _("WAVE-64"));
	hf->add (CAF, _("CAF"));

	add_option (_("Media"), hf);

	add_option (_("Media"), new OptionEditorHeading (_("File locations")));

        SearchPathOption* spo = new SearchPathOption ("audio-search-path", _("Search for audio files in:"),
						      _session->path(),
                                                      sigc::mem_fun (*_session_config, &SessionConfiguration::get_audio_search_path),
                                                      sigc::mem_fun (*_session_config, &SessionConfiguration::set_audio_search_path));
        add_option (_("Media"), spo);

        spo = new SearchPathOption ("midi-search-path", _("Search for MIDI files in:"),
				    _session->path(),
                                    sigc::mem_fun (*_session_config, &SessionConfiguration::get_midi_search_path),
                                    sigc::mem_fun (*_session_config, &SessionConfiguration::set_midi_search_path));

        add_option (_("Media"), spo);

	/* Monitoring */

        add_option (_("Monitoring"), new BoolOption (
			    "auto-input",
			    _("Track Input Monitoring automatically follows transport state (\"auto-input\")"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_auto_input),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_auto_input)
			    ));

        add_option (_("Monitoring"), new BoolOption (
			    "have-monitor-section",
			    _("Use monitor section in this session"),
			    sigc::mem_fun (*this, &SessionOptionEditor::get_use_monitor_section),
			    sigc::mem_fun (*this, &SessionOptionEditor::set_use_monitor_section)
			    ));

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

	add_option (_("Misc"), new OptionEditorHeading (_("Glue to bars and beats")));

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

	add_option (_("Misc"), new OptionEditorHeading (_("Meterbridge Options")));

	add_option (_("Misc"), new BoolOption (
			    "show-midi-on-meterbridge",
			    _("Include Midi Tracks on Meterbridge"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_midi_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_midi_on_meterbridge)
			    ));

	add_option (_("Misc"), new BoolOption (
			    "show-busses-on-meterbridge",
			    _("Include Busses on Meterbridge"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_busses_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_busses_on_meterbridge)
			    ));

	add_option (_("Misc"), new BoolOption (
			    "show-master-on-meterbridge",
			    _("Include Master Bus on Meterbridge"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_master_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_master_on_meterbridge)
			    ));

	add_option (_("Misc"), new BoolOption (
			    "show-rec-on-meterbridge",
			    _("Show Rec-enable Button on Meterbridge"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_rec_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_rec_on_meterbridge)
			    ));

	add_option (_("Misc"), new BoolOption (
			    "show-mute-on-meterbridge",
			    _("Include Mute Button on Meterbridge"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_mute_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_mute_on_meterbridge)
			    ));

	add_option (_("Misc"), new BoolOption (
			    "show-solo-on-meterbridge",
			    _("Show Solo Button on Meterbridge"),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::get_show_solo_on_meterbridge),
			    sigc::mem_fun (*_session_config, &SessionConfiguration::set_show_solo_on_meterbridge)
			    ));

}

void
SessionOptionEditor::parameter_changed (std::string const & p)
{
	OptionEditor::parameter_changed (p);
	if (p == "external-sync") {
		if (Config->get_sync_source() == JACK) {
			_vpu->set_sensitive(!_session_config->get_external_sync());
		} else {
			_vpu->set_sensitive(true);
		}
	}
	if (p == "timecode-format") {
		/* update offset clocks */
		parameter_changed("timecode-generator-offset");
		parameter_changed("slave-timecode-offset");
	}
}

/* the presence of absence of a monitor section is not really a regular session
 * property so we provide these two functions to act as setter/getter slots
 */

bool
SessionOptionEditor::set_use_monitor_section (bool yn)
{
	bool had_monitor_section = _session->monitor_out();

	if (yn) {
		_session->add_monitor_section ();
	} else {
		_session->remove_monitor_section ();
	}

	/* store this choice for any new sessions */
	
	Config->set_use_monitor_bus (yn);

	return had_monitor_section != yn;
}

bool
SessionOptionEditor::get_use_monitor_section ()
{
	return _session->monitor_out() != 0;
}
