/*
    Copyright (C) 1999 Paul Davis

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

#include <cstdio> // for sprintf
#include <cmath>

#include "pbd/convert.h"
#include "pbd/enumwriter.h"

#include <gtkmm/style.h>
#include <gtkmm2ext/utils.h>

#include "ardour/ardour.h"
#include "ardour/session.h"
#include "ardour/tempo.h"
#include "ardour/profile.h"
#include <sigc++/bind.h>

#include "ardour_ui.h"
#include "audio_clock.h"
#include "utils.h"
#include "keyboard.h"
#include "gui_thread.h"
#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace std;

using Gtkmm2ext::Keyboard;

using PBD::atoi;
using PBD::atof;

sigc::signal<void> AudioClock::ModeChanged;
vector<AudioClock*> AudioClock::clocks;

const uint32_t AudioClock::field_length[(int) AudioClock::AudioFrames+1] = {
	2,   /* Timecode_Hours */
	2,   /* Timecode_Minutes */
	2,   /* Timecode_Seconds */
	2,   /* Timecode_Frames */
	2,   /* MS_Hours */
	2,   /* MS_Minutes */
	2,   /* MS_Seconds */
	3,   /* MS_Milliseconds */
	3,   /* Bars */
	2,   /* Beats */
	4,   /* Tick */
	10   /* Audio Frame */
};

AudioClock::AudioClock (const string& clock_name, bool transient, const string& widget_name, 
			bool allow_edit, bool follows_playhead, bool duration, bool with_info)
	: _name (clock_name),
	  is_transient (transient),
	  is_duration (duration),
	  editable (allow_edit),
	  _follows_playhead (follows_playhead),
	  colon1 (":"),
	  colon2 (":"),
	  colon3 (":"),
	  colon4 (":"),
	  colon5 (":"),
	  period1 ("."),
	  b1 ("|"),
	  b2 ("|"),
	  last_when(0),
	  _canonical_time_is_displayed (true),
	  _canonical_time (0)
{
	/* XXX: these are leaked, but I don't suppose it's the end of the world */
	
	_eboxes[Timecode_Hours] = new EventBox;
	_eboxes[Timecode_Minutes] = new EventBox;
	_eboxes[Timecode_Seconds] = new EventBox;
	_eboxes[Timecode_Frames] = new EventBox;
	_eboxes[MS_Hours] = new EventBox;
	_eboxes[MS_Minutes] = new EventBox;
	_eboxes[MS_Seconds] = new EventBox;
	_eboxes[MS_Milliseconds] = new EventBox;
	_eboxes[Bars] = new EventBox;
	_eboxes[Beats] = new EventBox;
	_eboxes[Ticks] = new EventBox;
	_eboxes[AudioFrames] = new EventBox;

	_labels[Timecode_Hours] = new Label;
	_labels[Timecode_Minutes] = new Label;
	_labels[Timecode_Seconds] = new Label;
	_labels[Timecode_Frames] = new Label;
	_labels[MS_Hours] = new Label;
	_labels[MS_Minutes] = new Label;
	_labels[MS_Seconds] = new Label;
	_labels[MS_Milliseconds] = new Label;
	_labels[Bars] = new Label;
	_labels[Beats] = new Label;
	_labels[Ticks] = new Label;
	_labels[AudioFrames] = new Label;
	
	last_when = 0;
	last_pdelta = 0;
	last_sdelta = 0;
	key_entry_state = 0;
	ops_menu = 0;
	dragging = false;
	bbt_reference_time = -1;

	if (with_info) {
		frames_upper_info_label = manage (new Label);
		frames_lower_info_label = manage (new Label);
		timecode_upper_info_label = manage (new Label);
		timecode_lower_info_label = manage (new Label);
		bbt_upper_info_label = manage (new Label);
		bbt_lower_info_label = manage (new Label);

		frames_upper_info_label->set_name ("AudioClockFramesUpperInfo");
		frames_lower_info_label->set_name ("AudioClockFramesLowerInfo");
		timecode_upper_info_label->set_name ("AudioClockTimecodeUpperInfo");
		timecode_lower_info_label->set_name ("AudioClockTimecodeLowerInfo");
		bbt_upper_info_label->set_name ("AudioClockBBTUpperInfo");
		bbt_lower_info_label->set_name ("AudioClockBBTLowerInfo");

		Gtkmm2ext::set_size_request_to_display_given_text(*timecode_upper_info_label, "23.98",0,0);
		Gtkmm2ext::set_size_request_to_display_given_text(*timecode_lower_info_label, "NDF",0,0);

		Gtkmm2ext::set_size_request_to_display_given_text(*bbt_upper_info_label, "88|88",0,0);
		Gtkmm2ext::set_size_request_to_display_given_text(*bbt_lower_info_label, "888.88",0,0);

		frames_info_box.pack_start (*frames_upper_info_label, true, true);
		frames_info_box.pack_start (*frames_lower_info_label, true, true);
		timecode_info_box.pack_start (*timecode_upper_info_label, true, true);
		timecode_info_box.pack_start (*timecode_lower_info_label, true, true);
		bbt_info_box.pack_start (*bbt_upper_info_label, true, true);
		bbt_info_box.pack_start (*bbt_lower_info_label, true, true);

	} else {
		frames_upper_info_label = 0;
		frames_lower_info_label = 0;
		timecode_upper_info_label = 0;
		timecode_lower_info_label = 0;
		bbt_upper_info_label = 0;
		bbt_lower_info_label = 0;
	}

	frames_packer.set_homogeneous (false);
	frames_packer.set_border_width (2);
	frames_packer.pack_start (*_eboxes[AudioFrames], false, false);

	if (with_info) {
		frames_packer.pack_start (frames_info_box, false, false, 5);
	}

	frames_packer_hbox.pack_start (frames_packer, true, false);

	for (std::map<Field, EventBox*>::iterator i = _eboxes.begin(); i != _eboxes.end(); ++i) {
		i->second->add (*_labels[i->first]);
	}

	timecode_packer.set_homogeneous (false);
	timecode_packer.set_border_width (2);
	timecode_packer.pack_start (*_eboxes[Timecode_Hours], false, false);
	timecode_packer.pack_start (colon1, false, false);
	timecode_packer.pack_start (*_eboxes[Timecode_Minutes], false, false);
	timecode_packer.pack_start (colon2, false, false);
	timecode_packer.pack_start (*_eboxes[Timecode_Seconds], false, false);
	timecode_packer.pack_start (colon3, false, false);
	timecode_packer.pack_start (*_eboxes[Timecode_Frames], false, false);

	if (with_info) {
		timecode_packer.pack_start (timecode_info_box, false, false, 5);
	}

	timecode_packer_hbox.pack_start (timecode_packer, true, false);

	bbt_packer.set_homogeneous (false);
	bbt_packer.set_border_width (2);
	bbt_packer.pack_start (*_eboxes[Bars], false, false);
	bbt_packer.pack_start (b1, false, false);
	bbt_packer.pack_start (*_eboxes[Beats], false, false);
	bbt_packer.pack_start (b2, false, false);
	bbt_packer.pack_start (*_eboxes[Ticks], false, false);

	if (with_info) {
		bbt_packer.pack_start (bbt_info_box, false, false, 5);
	}

	bbt_packer_hbox.pack_start (bbt_packer, true, false);

	minsec_packer.set_homogeneous (false);
	minsec_packer.set_border_width (2);
	minsec_packer.pack_start (*_eboxes[MS_Hours], false, false);
	minsec_packer.pack_start (colon4, false, false);
	minsec_packer.pack_start (*_eboxes[MS_Minutes], false, false);
	minsec_packer.pack_start (colon5, false, false);
	minsec_packer.pack_start (*_eboxes[MS_Seconds], false, false);
	minsec_packer.pack_start (period1, false, false);
	minsec_packer.pack_start (*_eboxes[MS_Milliseconds], false, false);

	minsec_packer_hbox.pack_start (minsec_packer, true, false);

	clock_frame.set_shadow_type (SHADOW_IN);
	clock_frame.set_name ("BaseFrame");

	clock_frame.add (clock_base);

	set_widget_name (widget_name);

	_mode = BBT; /* lie to force mode switch */
	set_mode (Timecode);

	pack_start (clock_frame, true, true);

	/* the clock base handles button releases for menu popup regardless of
	   editable status. if the clock is editable, the clock base is where
	   we pass focus to after leaving the last editable "field", which
	   will then shutdown editing till the user starts it up again.

	   it does this because the focus out event on the field disables
	   keyboard event handling, and we don't connect anything up to
	   notice focus in on the clock base. hence, keyboard event handling
	   stays disabled.
	*/

	clock_base.add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::SCROLL_MASK);
	clock_base.signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &AudioClock::field_button_release_event), Timecode_Hours));

	if (editable) {
		setup_events ();
	}

	set (last_when, true);

	if (!is_transient) {
		clocks.push_back (this);
	}
}

void
AudioClock::set_widget_name (string name)
{
	Widget::set_name (name);

	clock_base.set_name (name);

	for (std::map<Field, EventBox*>::iterator i = _eboxes.begin(); i != _eboxes.end(); ++i) {
		i->second->set_name (name);
	}

	for (std::map<Field, Label*>::iterator i = _labels.begin(); i != _labels.end(); ++i) {
		i->second->set_name (name);
	}

	colon1.set_name (name);
	colon2.set_name (name);
	colon3.set_name (name);
	colon4.set_name (name);
	colon5.set_name (name);
	b1.set_name (name);
	b2.set_name (name);
	period1.set_name (name);

	queue_draw ();
}

void
AudioClock::setup_events ()
{
	clock_base.set_flags (CAN_FOCUS);
	
	for (std::map<Field, EventBox*>::iterator i = _eboxes.begin(); i != _eboxes.end(); ++i) {
		i->second->add_events (
			Gdk::BUTTON_PRESS_MASK |
			Gdk::BUTTON_RELEASE_MASK |
			Gdk::KEY_PRESS_MASK |
			Gdk::KEY_RELEASE_MASK |
			Gdk::FOCUS_CHANGE_MASK |
			Gdk::POINTER_MOTION_MASK |
			Gdk::SCROLL_MASK);
		
		i->second->set_flags (CAN_FOCUS);
		i->second->signal_motion_notify_event().connect (sigc::bind (sigc::mem_fun (*this, &AudioClock::field_motion_notify_event), i->first));
		i->second->signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &AudioClock::field_button_press_event), i->first));
		i->second->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &AudioClock::field_button_release_event), i->first));
		i->second->signal_scroll_event().connect (sigc::bind (sigc::mem_fun (*this, &AudioClock::field_button_scroll_event), i->first));
		i->second->signal_key_press_event().connect (sigc::bind (sigc::mem_fun (*this, &AudioClock::field_key_press_event), i->first));
		i->second->signal_key_release_event().connect (sigc::bind (sigc::mem_fun (*this, &AudioClock::field_key_release_event), i->first));
		i->second->signal_focus_in_event().connect (sigc::bind (sigc::mem_fun (*this, &AudioClock::field_focus_in_event), i->first));
		i->second->signal_focus_out_event().connect (sigc::bind (sigc::mem_fun (*this, &AudioClock::field_focus_out_event), i->first));
	}

	clock_base.signal_focus_in_event().connect (sigc::mem_fun (*this, &AudioClock::drop_focus_handler));
}

bool
AudioClock::drop_focus_handler (GdkEventFocus*)
{
	Keyboard::magic_widget_drop_focus ();
	return false;
}

void
AudioClock::on_realize ()
{
	HBox::on_realize ();

	/* styles are not available until the widgets are bound to a window */

	set_size_requests ();
}

void
AudioClock::set (framepos_t when, bool force, framecnt_t offset, char which)
{
 	if ((!force && !is_visible()) || _session == 0) {
		return;
	}

	bool const pdelta = Config->get_primary_clock_delta_edit_cursor ();
	bool const sdelta = Config->get_secondary_clock_delta_edit_cursor ();

	if (offset && which == 'p' && pdelta) {
		when = (when > offset) ? when - offset : offset - when;
	} else if (offset && which == 's' && sdelta) {
		when = (when > offset) ? when - offset : offset - when;
	}

	if (when == last_when && !force) {
		return;
	}

	if (which == 'p' && pdelta && !last_pdelta) {
		set_widget_name("TransportClockDisplayDelta");
		last_pdelta = true;
	} else if (which == 'p' && !pdelta && last_pdelta) {
		set_widget_name("TransportClockDisplay");
		last_pdelta = false;
	} else if (which == 's' && sdelta && !last_sdelta) {
		set_widget_name("SecondaryClockDisplayDelta");
		last_sdelta = true;
	} else if (which == 's' && !sdelta && last_sdelta) {
		set_widget_name("SecondaryClockDisplay");
		last_sdelta = false;
	}

	switch (_mode) {
	case Timecode:
		set_timecode (when, force);
		break;

	case BBT:
		set_bbt (when, force);
		break;

	case MinSec:
		set_minsec (when, force);
		break;

	case Frames:
		set_frames (when, force);
		break;

	case Off:
		break;
	}

	last_when = when;

	/* we're setting the time from a frames value, so keep it as the canonical value */
	_canonical_time = when;
	_canonical_time_is_displayed = false;
}

void
AudioClock::session_configuration_changed (std::string p)
{
	if (p != "timecode-offset" && p != "timecode-offset-negative") {
		return;
	}
	
	framecnt_t current;

	switch (_mode) {
	case Timecode:
		if (is_duration) {
			current = current_duration ();
		} else {
			current = current_time ();
		}
		set (current, true);
		break;
	default:
		break;
	}
}

void
AudioClock::set_frames (framepos_t when, bool /*force*/)
{
	char buf[32];
	snprintf (buf, sizeof (buf), "%" PRId64, when);
	_labels[AudioFrames]->set_text (buf);

	if (frames_upper_info_label) {
		framecnt_t rate = _session->frame_rate();

		if (fmod (rate, 1000.0) == 0.000) {
			sprintf (buf, "%" PRId64 "K", rate/1000);
		} else {
			sprintf (buf, "%.3fK", rate/1000.0f);
		}

		if (frames_upper_info_label->get_text() != buf) {
			frames_upper_info_label->set_text (buf);
		}

		float vid_pullup = _session->config.get_video_pullup();

		if (vid_pullup == 0.0) {
			if (frames_lower_info_label->get_text () != _("none")) {
				frames_lower_info_label->set_text(_("none"));
			}
		} else {
			sprintf (buf, "%-6.4f", vid_pullup);
			if (frames_lower_info_label->get_text() != buf) {
				frames_lower_info_label->set_text (buf);
			}
		}
	}
}

void
AudioClock::set_minsec (framepos_t when, bool force)
{
	char buf[32];
	framecnt_t left;
	int hrs;
	int mins;
	int secs;
	int millisecs;

	left = when;
	hrs = (int) floor (left / (_session->frame_rate() * 60.0f * 60.0f));
	left -= (framecnt_t) floor (hrs * _session->frame_rate() * 60.0f * 60.0f);
	mins = (int) floor (left / (_session->frame_rate() * 60.0f));
	left -= (framecnt_t) floor (mins * _session->frame_rate() * 60.0f);
	secs = (int) floor (left / (float) _session->frame_rate());
	left -= (framecnt_t) floor (secs * _session->frame_rate());
	millisecs = floor (left * 1000.0 / (float) _session->frame_rate());

	if (force || hrs != ms_last_hrs) {
		sprintf (buf, "%02d", hrs);
		_labels[MS_Hours]->set_text (buf);
		ms_last_hrs = hrs;
	}

	if (force || mins != ms_last_mins) {
		sprintf (buf, "%02d", mins);
		_labels[MS_Minutes]->set_text (buf);
		ms_last_mins = mins;
	}

	if (force || secs != ms_last_secs) {
		sprintf (buf, "%02d", secs);
		_labels[MS_Seconds]->set_text (buf);
		ms_last_secs = secs;
	}

	if (force || millisecs != ms_last_millisecs) {
		sprintf (buf, "%03d", millisecs);
		_labels[MS_Milliseconds]->set_text (buf);
		ms_last_millisecs = millisecs;
	}
}

void
AudioClock::set_timecode (framepos_t when, bool force)
{
	char buf[32];
	Timecode::Time timecode;

	if (is_duration) {
		_session->timecode_duration (when, timecode);
	} else {
		_session->timecode_time (when, timecode);
	}

	if (force || timecode.hours != last_hrs || timecode.negative != last_negative) {
		if (timecode.negative) {
			sprintf (buf, "-%02" PRIu32, timecode.hours);
		} else {
			sprintf (buf, " %02" PRIu32, timecode.hours);
		}
		_labels[Timecode_Hours]->set_text (buf);
		last_hrs = timecode.hours;
		last_negative = timecode.negative;
	}

	if (force || timecode.minutes != last_mins) {
		sprintf (buf, "%02" PRIu32, timecode.minutes);
		_labels[Timecode_Minutes]->set_text (buf);
		last_mins = timecode.minutes;
	}

	if (force || timecode.seconds != last_secs) {
		sprintf (buf, "%02" PRIu32, timecode.seconds);
		_labels[Timecode_Seconds]->set_text (buf);
		last_secs = timecode.seconds;
	}

	if (force || timecode.frames != last_frames) {
		sprintf (buf, "%02" PRIu32, timecode.frames);
		_labels[Timecode_Frames]->set_text (buf);
		last_frames = timecode.frames;
	}

	if (timecode_upper_info_label) {
		double timecode_frames = _session->timecode_frames_per_second();

		if (fmod(timecode_frames, 1.0) == 0.0) {
			sprintf (buf, "%u", int (timecode_frames));
		} else {
			sprintf (buf, "%.2f", timecode_frames);
		}

		if (timecode_upper_info_label->get_text() != buf) {
			timecode_upper_info_label->set_text (buf);
		}

		if ((fabs(timecode_frames - 29.97) < 0.0001) || timecode_frames == 30) {
			if (_session->timecode_drop_frames()) {
				sprintf (buf, "DF");
			} else {
				sprintf (buf, "NDF");
			}
		} else {
			// there is no drop frame alternative
			buf[0] = '\0';
		}

		if (timecode_lower_info_label->get_text() != buf) {
			timecode_lower_info_label->set_text (buf);
		}
	}
}

void
AudioClock::set_bbt (framepos_t when, bool force)
{
	char buf[16];
	Timecode::BBT_Time bbt;

	/* handle a common case */
	if (is_duration) {
		if (when == 0) {
			bbt.bars = 0;
			bbt.beats = 0;
			bbt.ticks = 0;
		} else {
			_session->tempo_map().bbt_time (when, bbt);
			bbt.bars--;
			bbt.beats--;
		}
	} else {
		_session->tempo_map().bbt_time (when, bbt);
	}

	sprintf (buf, "%03" PRIu32, bbt.bars);
	if (force || _labels[Bars]->get_text () != buf) {
		_labels[Bars]->set_text (buf);
	}
	sprintf (buf, "%02" PRIu32, bbt.beats);
	if (force || _labels[Beats]->get_text () != buf) {
		_labels[Beats]->set_text (buf);
	}
	sprintf (buf, "%04" PRIu32, bbt.ticks);
	if (force || _labels[Ticks]->get_text () != buf) {
		_labels[Ticks]->set_text (buf);
	}

	if (bbt_upper_info_label) {
		framepos_t pos;

		if (bbt_reference_time < 0) {
			pos = when;
		} else {
			pos = bbt_reference_time;
		}

		TempoMetric m (_session->tempo_map().metric_at (pos));

		sprintf (buf, "%-5.2f", m.tempo().beats_per_minute());
		if (bbt_lower_info_label->get_text() != buf) {
			bbt_lower_info_label->set_text (buf);
		}
		sprintf (buf, "%g|%g", m.meter().beats_per_bar(), m.meter().note_divisor());
		if (bbt_upper_info_label->get_text() != buf) {
			bbt_upper_info_label->set_text (buf);
		}
	}
}

void
AudioClock::set_session (Session *s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {

		_session->config.ParameterChanged.connect (_session_connections, invalidator (*this), boost::bind (&AudioClock::session_configuration_changed, this, _1), gui_context());

		XMLProperty* prop;
		XMLNode* node = _session->extra_xml (X_("ClockModes"));
		AudioClock::Mode amode;

		if (node) {
			if ((prop = node->property (_name)) != 0) {
				amode = AudioClock::Mode (string_2_enum (prop->value(), amode));
				set_mode (amode);
			}
		}

		set (last_when, true);
	}
}

void
AudioClock::focus ()
{
	switch (_mode) {
	case Timecode:
		_eboxes[Timecode_Hours]->grab_focus ();
		break;

	case BBT:
		_eboxes[Bars]->grab_focus ();
		break;

	case MinSec:
		_eboxes[MS_Hours]->grab_focus ();
		break;

	case Frames:
		_eboxes[AudioFrames]->grab_focus ();
		break;

	case Off:
		break;
	}
}


bool
AudioClock::field_key_press_event (GdkEventKey */*ev*/, Field /*field*/)
{
	/* all key activity is handled on key release */
	return true;
}

bool
AudioClock::field_key_release_event (GdkEventKey *ev, Field field)
{
	Label *label = _labels[field];
	string new_text;
	char new_char = 0;
	bool move_on = false;

	switch (ev->keyval) {
	case GDK_0:
	case GDK_KP_0:
		new_char = '0';
		break;
	case GDK_1:
	case GDK_KP_1:
		new_char = '1';
		break;
	case GDK_2:
	case GDK_KP_2:
		new_char = '2';
		break;
	case GDK_3:
	case GDK_KP_3:
		new_char = '3';
		break;
	case GDK_4:
	case GDK_KP_4:
		new_char = '4';
		break;
	case GDK_5:
	case GDK_KP_5:
		new_char = '5';
		break;
	case GDK_6:
	case GDK_KP_6:
		new_char = '6';
		break;
	case GDK_7:
	case GDK_KP_7:
		new_char = '7';
		break;
	case GDK_8:
	case GDK_KP_8:
		new_char = '8';
		break;
	case GDK_9:
	case GDK_KP_9:
		new_char = '9';
		break;

	case GDK_period:
	case GDK_KP_Decimal:
		if (_mode == MinSec && field == MS_Seconds) {
			new_char = '.';
		} else {
			return false;
		}
		break;

	case GDK_Tab:
	case GDK_Return:
	case GDK_KP_Enter:
		move_on = true;
		break;

	case GDK_Escape:
		key_entry_state = 0;
		clock_base.grab_focus ();
		ChangeAborted();  /*  EMIT SIGNAL  */
		return true;

	default:
		return false;
	}

	if (!move_on) {

		if (key_entry_state == 0) {

			/* initialize with a fresh new string */

			if (field != AudioFrames) {
				for (uint32_t xn = 0; xn < field_length[field] - 1; ++xn) {
					new_text += '0';
				}
			} else {
				new_text = "";
			}

		} else {

			string existing = label->get_text();
			if (existing.length() >= field_length[field]) {
				new_text = existing.substr (1, field_length[field] - 1);
			} else {
				new_text = existing.substr (0, field_length[field] - 1);
			}
		}

		new_text += new_char;
		label->set_text (new_text);
		_canonical_time_is_displayed = true;
		key_entry_state++;
	}

	if (key_entry_state == field_length[field]) {
		move_on = true;
	}

	if (move_on) {

		if (key_entry_state) {

			switch (field) {
			case Timecode_Hours:
			case Timecode_Minutes:
			case Timecode_Seconds:
			case Timecode_Frames:
				// Check Timecode fields for sanity (may also adjust fields)
				timecode_sanitize_display();
				break;
			case Bars:
			case Beats:
			case Ticks:
				// Bars should never be, unless this clock is for a duration
				if (atoi (_labels[Bars]->get_text()) == 0 && !is_duration) {
					_labels[Bars]->set_text("001");
					_canonical_time_is_displayed = true;
				}
				//  beats should never be 0, unless this clock is for a duration
				if (atoi (_labels[Beats]->get_text()) == 0 && !is_duration) {
					_labels[Beats]->set_text("01");
					_canonical_time_is_displayed = true;
				}
				break;
			default:
				break;
			}

			ValueChanged(); /* EMIT_SIGNAL */
		}

		/* move on to the next field.
		 */

		switch (field) {

			/* Timecode */

		case Timecode_Hours:
			_eboxes[Timecode_Minutes]->grab_focus ();
			break;
		case Timecode_Minutes:
			_eboxes[Timecode_Seconds]->grab_focus ();
			break;
		case Timecode_Seconds:
			_eboxes[Timecode_Frames]->grab_focus ();
			break;
		case Timecode_Frames:
			clock_base.grab_focus ();
			break;

		/* audio frames */
		case AudioFrames:
			clock_base.grab_focus ();
			break;

		/* Min:Sec */

		case MS_Hours:
			_eboxes[MS_Minutes]->grab_focus ();
			break;
		case MS_Minutes:
			_eboxes[MS_Seconds]->grab_focus ();
			break;
		case MS_Seconds:
			_eboxes[MS_Milliseconds]->grab_focus ();
			break;
		case MS_Milliseconds:
			clock_base.grab_focus ();
			break;

		/* BBT */

		case Bars:
			_eboxes[Beats]->grab_focus ();
			break;
		case Beats:
			_eboxes[Ticks]->grab_focus ();
			break;
		case Ticks:
			clock_base.grab_focus ();
			break;

		default:
			break;
		}

	}

	//if user hit Enter, lose focus
	switch (ev->keyval) {
	case GDK_Return:
	case GDK_KP_Enter:
		clock_base.grab_focus ();
	}

	return true;
}

bool
AudioClock::field_focus_in_event (GdkEventFocus */*ev*/, Field field)
{
	key_entry_state = 0;

	Keyboard::magic_widget_grab_focus ();

	_eboxes[field]->set_flags (HAS_FOCUS);
	_eboxes[field]->set_state (STATE_ACTIVE);

	return false;
}

bool
AudioClock::field_focus_out_event (GdkEventFocus */*ev*/, Field field)
{
	_eboxes[field]->unset_flags (HAS_FOCUS);
	_eboxes[field]->set_state (STATE_NORMAL);

	Keyboard::magic_widget_drop_focus ();

	return false;
}

bool
AudioClock::field_button_release_event (GdkEventButton *ev, Field field)
{
	if (dragging) {
		gdk_pointer_ungrab (GDK_CURRENT_TIME);
		dragging = false;
		if (ev->y > drag_start_y+1 || ev->y < drag_start_y-1 || Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)){
			// we actually dragged so return without setting editing focus, or we shift clicked
			return true;
		}
	}

	if (!editable) {
		if (ops_menu == 0) {
			build_ops_menu ();
		}
		ops_menu->popup (1, ev->time);
		return true;
	}

	if (Keyboard::is_context_menu_event (ev)) {
		if (ops_menu == 0) {
			build_ops_menu ();
		}
		ops_menu->popup (1, ev->time);
		return true;
	}

	switch (ev->button) {
	case 1:
		_eboxes[field]->grab_focus ();
		break;

	default:
		break;
	}

	return true;
}

bool
AudioClock::field_button_press_event (GdkEventButton *ev, Field /*field*/)
{
	if (_session == 0) {
		return false;
	}

	framepos_t frames = 0;

	switch (ev->button) {
	case 1:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
			set (frames, true);
			ValueChanged (); /* EMIT_SIGNAL */
					}

		/* make absolutely sure that the pointer is grabbed */
		gdk_pointer_grab(ev->window,false ,
				 GdkEventMask( Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK |Gdk::BUTTON_RELEASE_MASK),
				 NULL,NULL,ev->time);
		dragging = true;
		drag_accum = 0;
		drag_start_y = ev->y;
		drag_y = ev->y;
		break;

	case 2:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
			set (frames, true);
			ValueChanged (); /* EMIT_SIGNAL */
		}
		break;

	case 3:
		/* used for context sensitive menu */
		return false;
		break;

	default:
		return false;
		break;
	}

	return true;
}

bool
AudioClock::field_button_scroll_event (GdkEventScroll *ev, Field field)
{
	if (_session == 0) {
		return false;
	}

	framepos_t frames = 0;

	switch (ev->direction) {

	case GDK_SCROLL_UP:
	       frames = get_frames (field);
	       if (frames != 0) {
		      if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			     frames *= 10;
		      }
		      set (current_time() + frames, true);
		      ValueChanged (); /* EMIT_SIGNAL */
	       }
	       break;

	case GDK_SCROLL_DOWN:
	       frames = get_frames (field);
	       if (frames != 0) {
		      if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			     frames *= 10;
		      }

		      if ((double)current_time() - (double)frames < 0.0) {
			     set (0, true);
		      } else {
			     set (current_time() - frames, true);
		      }

		      ValueChanged (); /* EMIT_SIGNAL */
	       }
	       break;

	default:
		return false;
		break;
	}

	return true;
}

bool
AudioClock::field_motion_notify_event (GdkEventMotion *ev, Field field)
{
	if (_session == 0 || !dragging) {
		return false;
	}

	float pixel_frame_scale_factor = 0.2f;

/*
	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier))  {
		pixel_frame_scale_factor = 0.1f;
	}


	if (Keyboard::modifier_state_contains (ev->state,
					       Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)) {

		pixel_frame_scale_factor = 0.025f;
	}
*/
	double y_delta = ev->y - drag_y;

	drag_accum +=  y_delta*pixel_frame_scale_factor;

	drag_y = ev->y;

	if (trunc(drag_accum) != 0) {

		framepos_t frames;
		framepos_t pos;
		int dir;
		dir = (drag_accum < 0 ? 1:-1);
		pos = current_time();
		frames = get_frames (field,pos,dir);

		if (frames  != 0 &&  frames * drag_accum < current_time()) {

			set ((framepos_t) floor (pos - drag_accum * frames), false); // minus because up is negative in computer-land

		} else {
			set (0 , false);

 		}

	       	drag_accum= 0;
		ValueChanged();	 /* EMIT_SIGNAL */


	}

	return true;
}

framepos_t
AudioClock::get_frames (Field field, framepos_t pos, int dir)
{
	framecnt_t frames = 0;
	Timecode::BBT_Time bbt;
	switch (field) {
	case Timecode_Hours:
		frames = (framecnt_t) floor (3600.0 * _session->frame_rate());
		break;
	case Timecode_Minutes:
		frames = (framecnt_t) floor (60.0 * _session->frame_rate());
		break;
	case Timecode_Seconds:
		frames = _session->frame_rate();
		break;
	case Timecode_Frames:
		frames = (framecnt_t) floor (_session->frame_rate() / _session->timecode_frames_per_second());
		break;

	case AudioFrames:
		frames = 1;
		break;

	case MS_Hours:
		frames = (framecnt_t) floor (3600.0 * _session->frame_rate());
		break;
	case MS_Minutes:
		frames = (framecnt_t) floor (60.0 * _session->frame_rate());
		break;
	case MS_Seconds:
		frames = (framecnt_t) _session->frame_rate();
		break;
	case MS_Milliseconds:
		frames = (framecnt_t) floor (_session->frame_rate() / 1000.0);
		break;

	case Bars:
		bbt.bars = 1;
		bbt.beats = 0;
		bbt.ticks = 0;
		frames = _session->tempo_map().bbt_duration_at(pos,bbt,dir);
		break;
	case Beats:
		bbt.bars = 0;
		bbt.beats = 1;
		bbt.ticks = 0;
		frames = _session->tempo_map().bbt_duration_at(pos,bbt,dir);
		break;
	case Ticks:
		bbt.bars = 0;
		bbt.beats = 0;
		bbt.ticks = 1;
		frames = _session->tempo_map().bbt_duration_at(pos,bbt,dir);
		break;
	}

	return frames;
}

framepos_t
AudioClock::current_time (framepos_t pos) const
{
	if (!_canonical_time_is_displayed) {
		return _canonical_time;
	}
	
	framepos_t ret = 0;

	switch (_mode) {
	case Timecode:
		ret = timecode_frame_from_display ();
		break;
	case BBT:
		ret = bbt_frame_from_display (pos);
		break;

	case MinSec:
		ret = minsec_frame_from_display ();
		break;

	case Frames:
		ret = audio_frame_from_display ();
		break;

	case Off:
		break;
	}

	return ret;
}

framepos_t
AudioClock::current_duration (framepos_t pos) const
{
	framepos_t ret = 0;

	switch (_mode) {
	case Timecode:
		ret = timecode_frame_from_display ();
		break;
	case BBT:
		ret = bbt_frame_duration_from_display (pos);
		break;

	case MinSec:
		ret = minsec_frame_from_display ();
		break;

	case Frames:
		ret = audio_frame_from_display ();
		break;

	case Off:
		break;
	}

	return ret;
}

void
AudioClock::timecode_sanitize_display()
{
	// Check Timecode fields for sanity, possibly adjusting values
	if (atoi (_labels[Timecode_Minutes]->get_text()) > 59) {
		_labels[Timecode_Minutes]->set_text("59");
		_canonical_time_is_displayed = true;
	}

	if (atoi (_labels[Timecode_Seconds]->get_text()) > 59) {
		_labels[Timecode_Seconds]->set_text("59");
		_canonical_time_is_displayed = true;
	}

	switch ((long)rint(_session->timecode_frames_per_second())) {
	case 24:
		if (atoi (_labels[Timecode_Frames]->get_text()) > 23) {
			_labels[Timecode_Frames]->set_text("23");
			_canonical_time_is_displayed = true;
		}
		break;
	case 25:
		if (atoi (_labels[Timecode_Frames]->get_text()) > 24) {
			_labels[Timecode_Frames]->set_text("24");
			_canonical_time_is_displayed = true;
		}
		break;
	case 30:
		if (atoi (_labels[Timecode_Frames]->get_text()) > 29) {
			_labels[Timecode_Frames]->set_text("29");
			_canonical_time_is_displayed = true;
		}
		break;
	default:
		break;
	}

	if (_session->timecode_drop_frames()) {
		if ((atoi (_labels[Timecode_Minutes]->get_text()) % 10) && (atoi (_labels[Timecode_Seconds]->get_text()) == 0) && (atoi (_labels[Timecode_Frames]->get_text()) < 2)) {
			_labels[Timecode_Frames]->set_text("02");
			_canonical_time_is_displayed = true;
		}
	}
}

/** This is necessary because operator[] isn't const with std::map.
 *  @param f Field.
 *  @return Label widget.
 */
Label const *
AudioClock::label (Field f) const
{
	std::map<Field, Label*>::const_iterator i = _labels.find (f);
	assert (i != _labels.end ());

	return i->second;
}

framepos_t
AudioClock::timecode_frame_from_display () const
{
	if (_session == 0) {
		return 0;
	}

	Timecode::Time timecode;
	framepos_t sample;

	timecode.hours = atoi (label (Timecode_Hours)->get_text());
	timecode.minutes = atoi (label (Timecode_Minutes)->get_text());
	timecode.seconds = atoi (label (Timecode_Seconds)->get_text());
	timecode.frames = atoi (label (Timecode_Frames)->get_text());
	timecode.rate = _session->timecode_frames_per_second();
	timecode.drop= _session->timecode_drop_frames();

	_session->timecode_to_sample (timecode, sample, false /* use_offset */, false /* use_subframes */ );


#if 0
#define Timecode_SAMPLE_TEST_1
#define Timecode_SAMPLE_TEST_2
#define Timecode_SAMPLE_TEST_3
#define Timecode_SAMPLE_TEST_4
#define Timecode_SAMPLE_TEST_5
#define Timecode_SAMPLE_TEST_6
#define Timecode_SAMPLE_TEST_7

	// Testcode for timecode<->sample conversions (P.S.)
	Timecode::Time timecode1;
	framepos_t sample1;
	framepos_t oldsample = 0;
	Timecode::Time timecode2;
	framecnt_t sample_increment;

	sample_increment = (framecnt_t)rint(_session->frame_rate() / _session->timecode_frames_per_second);

#ifdef Timecode_SAMPLE_TEST_1
	// Test 1: use_offset = false, use_subframes = false
	cout << "use_offset = false, use_subframes = false" << endl;
	for (int i = 0; i < 108003; i++) {
		_session->timecode_to_sample( timecode1, sample1, false /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, false /* use_offset */, false /* use_subframes */ );

		if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1)))) {
			cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
			cout << "timecode1: " << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode1: " << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif

#ifdef Timecode_SAMPLE_TEST_2
	// Test 2: use_offset = true, use_subframes = false
	cout << "use_offset = true, use_subframes = false" << endl;

	timecode1.hours = 0;
	timecode1.minutes = 0;
	timecode1.seconds = 0;
	timecode1.frames = 0;
	timecode1.subframes = 0;
	sample1 = oldsample = 0;

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 108003; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

		if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1)))) {
			cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
			cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif

#ifdef Timecode_SAMPLE_TEST_3
	// Test 3: use_offset = true, use_subframes = false, decrement
	cout << "use_offset = true, use_subframes = false, decrement" << endl;

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 108003; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

		if ((i > 0) && ( ((oldsample - sample1) != sample_increment) && ((oldsample - sample1) != (sample_increment + 1)) && ((oldsample - sample1) != (sample_increment - 1)))) {
			cout << "ERROR: sample increment not right: " << (oldsample - sample1) << " != " << sample_increment << endl;
			cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_decrement( timecode1 );
	}

	cout << "sample_decrement: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif


#ifdef Timecode_SAMPLE_TEST_4
	// Test 4: use_offset = true, use_subframes = true
	cout << "use_offset = true, use_subframes = true" << endl;

	for (long sub = 5; sub < 80; sub += 5) {
		timecode1.hours = 0;
		timecode1.minutes = 0;
		timecode1.seconds = 0;
		timecode1.frames = 0;
		timecode1.subframes = 0;
		sample1 = oldsample = (sample_increment * sub) / 80;

		_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, true /* use_subframes */ );

		cout << "starting at sample: " << sample1 << " -> ";
		cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

		for (int i = 0; i < 108003; i++) {
			_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, true /* use_subframes */ );
			_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, true /* use_subframes */ );

			if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1)))) {
				cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
				cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
				cout << "sample: " << sample1 << endl;
				cout << "sample: " << sample1 << " -> ";
				cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
				//break;
			}

			if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames || timecode2.subframes != timecode1.subframes) {
				cout << "ERROR: timecode2 not equal timecode1" << endl;
				cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
				cout << "sample: " << sample1 << endl;
				cout << "sample: " << sample1 << " -> ";
				cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
				break;
			}
			oldsample = sample1;
			_session->timecode_increment( timecode1 );
		}

		cout << "sample_increment: " << sample_increment << endl;
		cout << "sample: " << sample1 << " -> ";
		cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

		for (int i = 0; i < 108003; i++) {
			_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, true /* use_subframes */ );
			_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, true /* use_subframes */ );

			if ((i > 0) && ( ((oldsample - sample1) != sample_increment) && ((oldsample - sample1) != (sample_increment + 1)) && ((oldsample - sample1) != (sample_increment - 1)))) {
				cout << "ERROR: sample increment not right: " << (oldsample - sample1) << " != " << sample_increment << endl;
				cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
				cout << "sample: " << sample1 << endl;
				cout << "sample: " << sample1 << " -> ";
				cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
				//break;
			}

			if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames || timecode2.subframes != timecode1.subframes) {
				cout << "ERROR: timecode2 not equal timecode1" << endl;
				cout << "timecode1: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
				cout << "sample: " << sample1 << endl;
				cout << "sample: " << sample1 << " -> ";
				cout << "timecode2: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
				break;
			}
			oldsample = sample1;
			_session->timecode_decrement( timecode1 );
		}

		cout << "sample_decrement: " << sample_increment << endl;
		cout << "sample: " << sample1 << " -> ";
		cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
	}
#endif


#ifdef Timecode_SAMPLE_TEST_5
	// Test 5: use_offset = true, use_subframes = false, increment seconds
	cout << "use_offset = true, use_subframes = false, increment seconds" << endl;

	timecode1.hours = 0;
	timecode1.minutes = 0;
	timecode1.seconds = 0;
	timecode1.frames = 0;
	timecode1.subframes = 0;
	sample1 = oldsample = 0;
	sample_increment = _session->frame_rate();

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 3600; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

//     if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1))))
//     {
//       cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
//       break;
//     }

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment_seconds( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif


#ifdef Timecode_SAMPLE_TEST_6
	// Test 6: use_offset = true, use_subframes = false, increment minutes
	cout << "use_offset = true, use_subframes = false, increment minutes" << endl;

	timecode1.hours = 0;
	timecode1.minutes = 0;
	timecode1.seconds = 0;
	timecode1.frames = 0;
	timecode1.subframes = 0;
	sample1 = oldsample = 0;
	sample_increment = _session->frame_rate() * 60;

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 60; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

//     if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1))))
//     {
//       cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
//       break;
//     }

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment_minutes( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif

#ifdef Timecode_SAMPLE_TEST_7
	// Test 7: use_offset = true, use_subframes = false, increment hours
	cout << "use_offset = true, use_subframes = false, increment hours" << endl;

	timecode1.hours = 0;
	timecode1.minutes = 0;
	timecode1.seconds = 0;
	timecode1.frames = 0;
	timecode1.subframes = 0;
	sample1 = oldsample = 0;
	sample_increment = _session->frame_rate() * 60 * 60;

	_session->sample_to_timecode( sample1, timecode1, true /* use_offset */, false /* use_subframes */ );
	cout << "Starting at sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << endl;

	for (int i = 0; i < 10; i++) {
		_session->timecode_to_sample( timecode1, sample1, true /* use_offset */, false /* use_subframes */ );
		_session->sample_to_timecode( sample1, timecode2, true /* use_offset */, false /* use_subframes */ );

//     cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
//     cout << "sample: " << sample1 << endl;
//     cout << "sample: " << sample1 << " -> ";
//     cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;

//     if ((i > 0) && ( ((sample1 - oldsample) != sample_increment) && ((sample1 - oldsample) != (sample_increment + 1)) && ((sample1 - oldsample) != (sample_increment - 1))))
//     {
//       cout << "ERROR: sample increment not right: " << (sample1 - oldsample) << " != " << sample_increment << endl;
//       break;
//     }

		if (timecode2.hours != timecode1.hours || timecode2.minutes != timecode1.minutes || timecode2.seconds != timecode2.seconds || timecode2.frames != timecode1.frames) {
			cout << "ERROR: timecode2 not equal timecode1" << endl;
			cout << "timecode: " << (timecode1.negative ? "-" : "") << timecode1.hours << ":" << timecode1.minutes << ":" << timecode1.seconds << ":" << timecode1.frames << "::" << timecode1.subframes << " -> ";
			cout << "sample: " << sample1 << endl;
			cout << "sample: " << sample1 << " -> ";
			cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
			break;
		}
		oldsample = sample1;
		_session->timecode_increment_hours( timecode1 );
	}

	cout << "sample_increment: " << sample_increment << endl;
	cout << "sample: " << sample1 << " -> ";
	cout << "timecode: " << (timecode2.negative ? "-" : "") << timecode2.hours << ":" << timecode2.minutes << ":" << timecode2.seconds << ":" << timecode2.frames << "::" << timecode2.subframes << endl;
#endif

#endif

	return sample;
}

framepos_t
AudioClock::minsec_frame_from_display () const
{
	if (_session == 0) {
		return 0;
	}

	int hrs = atoi (label (MS_Hours)->get_text());
	int mins = atoi (label (MS_Minutes)->get_text());
	int secs = atoi (label (MS_Seconds)->get_text());
	int millisecs = atoi (label (MS_Milliseconds)->get_text());

	framecnt_t sr = _session->frame_rate();

	return (framepos_t) floor ((hrs * 60.0f * 60.0f * sr) + (mins * 60.0f * sr) + (secs * sr) + (millisecs * sr / 1000.0));
}

framepos_t
AudioClock::bbt_frame_from_display (framepos_t pos) const
{
	if (_session == 0) {
		error << "AudioClock::current_time() called with BBT mode but without session!" << endmsg;
		return 0;
	}

	AnyTime any;
	any.type = AnyTime::BBT;

	any.bbt.bars = atoi (label (Bars)->get_text());
	any.bbt.beats = atoi (label (Beats)->get_text());
	any.bbt.ticks = atoi (label (Ticks)->get_text());

	if (is_duration) {
		any.bbt.bars++;
		any.bbt.beats++;
                return _session->any_duration_to_frames (pos, any);
	} else {
                return _session->convert_to_frames (any);
        }
}


framepos_t
AudioClock::bbt_frame_duration_from_display (framepos_t pos) const
{
	if (_session == 0) {
		error << "AudioClock::current_time() called with BBT mode but without session!" << endmsg;
		return 0;
	}

	Timecode::BBT_Time bbt;


	bbt.bars = atoi (label (Bars)->get_text());
	bbt.beats = atoi (label (Beats)->get_text());
	bbt.ticks = atoi (label (Ticks)->get_text());

	return _session->tempo_map().bbt_duration_at(pos,bbt,1);
}

framepos_t
AudioClock::audio_frame_from_display () const
{
	return (framepos_t) atoi (label (AudioFrames)->get_text ());
}

void
AudioClock::build_ops_menu ()
{
	using namespace Menu_Helpers;
	ops_menu = new Menu;
	MenuList& ops_items = ops_menu->items();
	ops_menu->set_name ("ArdourContextMenu");

	if (!Profile->get_sae()) {
		ops_items.push_back (MenuElem (_("Timecode"), sigc::bind (sigc::mem_fun(*this, &AudioClock::set_mode), Timecode)));
	}
	ops_items.push_back (MenuElem (_("Bars:Beats"), sigc::bind (sigc::mem_fun(*this, &AudioClock::set_mode), BBT)));
	ops_items.push_back (MenuElem (_("Minutes:Seconds"), sigc::bind (sigc::mem_fun(*this, &AudioClock::set_mode), MinSec)));
	ops_items.push_back (MenuElem (_("Samples"), sigc::bind (sigc::mem_fun(*this, &AudioClock::set_mode), Frames)));
	ops_items.push_back (MenuElem (_("Off"), sigc::bind (sigc::mem_fun(*this, &AudioClock::set_mode), Off)));

	if (editable && !is_duration && !_follows_playhead) {
		ops_items.push_back (SeparatorElem());
		ops_items.push_back (MenuElem (_("Set From Playhead"), sigc::mem_fun(*this, &AudioClock::set_from_playhead)));
		ops_items.push_back (MenuElem (_("Locate to This Time"), sigc::mem_fun(*this, &AudioClock::locate)));
	}
}

void
AudioClock::set_from_playhead ()
{
	if (!_session) {
		return;
	}

	set (_session->transport_frame());
	ValueChanged ();
}

void
AudioClock::locate ()
{
	if (!_session || is_duration) {
		return;
	}
	
	_session->request_locate (current_time(), _session->transport_rolling ());
}

void
AudioClock::set_mode (Mode m)
{
	/* slightly tricky: this is called from within the ARDOUR_UI
	   constructor by some of its clock members. at that time
	   the instance pointer is unset, so we have to be careful.
	   the main idea is to drop keyboard focus in case we had
	   started editing the clock and then we switch clock mode.
	*/

	clock_base.grab_focus ();

	if (_mode == m) {
		return;
	}

	clock_base.remove ();

	_mode = m;

	switch (_mode) {
	case Timecode:
		clock_base.add (timecode_packer_hbox);
		break;

	case BBT:
		clock_base.add (bbt_packer_hbox);
		break;

	case MinSec:
		clock_base.add (minsec_packer_hbox);
		break;

	case Frames:
		clock_base.add (frames_packer_hbox);
		break;

	case Off:
		clock_base.add (off_hbox);
		break;
	}

	set_size_requests ();

	set (last_when, true);
	clock_base.show_all ();
	key_entry_state = 0;

        if (!is_transient) {
                ModeChanged (); /* EMIT SIGNAL (the static one)*/
        }

        mode_changed (); /* EMIT SIGNAL (the member one) */
}

void
AudioClock::set_size_requests ()
{
	/* note that in some fonts, "88" is narrower than "00" */

	switch (_mode) {
	case Timecode:
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[Timecode_Hours], "-88", 5, 5);
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[Timecode_Minutes], "88", 5, 5);
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[Timecode_Seconds], "88", 5, 5);
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[Timecode_Frames], "88", 5, 5);
		break;

	case BBT:
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[Bars], "-888", 5, 5);
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[Beats], "88", 5, 5);
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[Ticks], "8888", 5, 5);
		break;

	case MinSec:
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[MS_Hours], "88", 5, 5);
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[MS_Minutes], "88", 5, 5);
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[MS_Seconds], "88", 5, 5);
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[MS_Milliseconds], "888", 5, 5);
		break;

	case Frames:
		Gtkmm2ext::set_size_request_to_display_given_text (*_labels[AudioFrames], "8888888888", 5, 5);
		break;

	case Off:
		Gtkmm2ext::set_size_request_to_display_given_text (off_hbox, "00000", 5, 5);
		break;

	}
}

void
AudioClock::set_bbt_reference (framepos_t pos)
{
	bbt_reference_time = pos;
}

void
AudioClock::on_style_changed (const Glib::RefPtr<Style>& old_style)
{
	HBox::on_style_changed (old_style);

	/* propagate style changes to all component widgets that should inherit the main one */

	Glib::RefPtr<RcStyle> rcstyle = get_modifier_style();

	clock_base.modify_style (rcstyle);

	for (std::map<Field, Label*>::iterator i = _labels.begin(); i != _labels.end(); ++i) {
		i->second->modify_style (rcstyle);
	}

	for (std::map<Field, EventBox*>::iterator i = _eboxes.begin(); i != _eboxes.end(); ++i) {
		i->second->modify_style (rcstyle);
	}

	colon1.modify_style (rcstyle);
	colon2.modify_style (rcstyle);
	colon3.modify_style (rcstyle);
	colon4.modify_style (rcstyle);
	colon5.modify_style (rcstyle);
	b1.modify_style (rcstyle);
	b2.modify_style (rcstyle);
	period1.modify_style (rcstyle);

	set_size_requests ();
}

void
AudioClock::set_is_duration (bool yn)
{
	if (yn == is_duration) {
		return;
	}
        
	is_duration = yn;
	set (last_when, true, 0, 's');
}
