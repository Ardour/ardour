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

#include "gtkmm2ext/cairocell.h"
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

std::map<AudioClock::Field,uint32_t> AudioClock::field_length;

void
AudioClock::fill_field_lengths()
{
	field_length[Timecode_Hours] = 2;
	field_length[Timecode_Minutes] = 2;
	field_length[Timecode_Seconds] = 2;
	field_length[Timecode_Frames] = 2;
	field_length[MS_Hours] = 2;
	field_length[MS_Minutes] = 2;
	field_length[MS_Seconds] = 2;
	field_length[MS_Milliseconds] = 3;
	field_length[Bars] = 4;
	field_length[Beats] = 2;
	field_length[Ticks] = 4;
	field_length[AudioFrames] = 10;
};

AudioClock::AudioClock (const string& clock_name, bool transient, const string& widget_name,
			bool allow_edit, bool follows_playhead, bool duration, bool with_info)
	: _name (clock_name),
	  is_transient (transient),
	  is_duration (duration),
	  editable (allow_edit),
	  _follows_playhead (follows_playhead),
	  last_when(0),
	  _canonical_time_is_displayed (true),
	  _canonical_time (0)
{
	CairoTextCell* tc;
	CairoBarCell* bc;
	CairoColonCell* cc;

	if (field_length.empty()) {
		fill_field_lengths ();
	}

	timecode = new CairoEditableText ();
	minsec = new CairoEditableText ();
	bbt = new CairoEditableText ();
	frames = new CairoEditableText ();

	// add an extra 0.6 "average char width" for the negative sign
	tc = new CairoTextCell (field_length[Timecode_Hours] + 0.6); 
	_text_cells[Timecode_Hours] = tc;
	timecode->add_cell (Timecode_Hours, tc);

	cc = new CairoColonCell ();
	_fixed_cells[Timecode_Colon1] = cc;
	timecode->add_cell (Timecode_Colon1, cc);

	tc = new CairoTextCell (field_length[Timecode_Minutes]);
	_text_cells[Timecode_Minutes] = tc;
	timecode->add_cell (Timecode_Minutes, tc);
	
	cc = new CairoColonCell ();
	_fixed_cells[Timecode_Colon2] = cc;
	timecode->add_cell (Timecode_Colon2, cc);

	tc = new CairoTextCell (field_length[Timecode_Seconds]);
	_text_cells[Timecode_Seconds] = tc;
	timecode->add_cell (Timecode_Seconds, tc);

	cc = new CairoColonCell ();
	_fixed_cells[Timecode_Colon3] = cc;
	timecode->add_cell (Timecode_Colon3, cc);

	tc = new CairoTextCell (field_length[Timecode_Frames]);
	_text_cells[Timecode_Frames] = tc;
	timecode->add_cell (Timecode_Frames, tc);

	/* Minutes/Seconds */
	
	tc = new CairoTextCell (field_length[MS_Hours]);
	_text_cells[MS_Hours] = tc;
	minsec->add_cell (MS_Hours, tc);
	
	cc = new CairoColonCell ();
	_fixed_cells[MS_Colon1] = cc;
	minsec->add_cell (MS_Colon1, cc);

	tc = new CairoTextCell (field_length[MS_Minutes]);
	_text_cells[MS_Minutes] = tc;
	minsec->add_cell (MS_Minutes, tc);
	
	cc = new CairoColonCell ();
	_fixed_cells[MS_Colon2] = cc;
	minsec->add_cell (MS_Colon2, cc);

	tc = new CairoTextCell (field_length[MS_Seconds]);
	_text_cells[MS_Seconds] = tc;
	minsec->add_cell (MS_Seconds, tc);

	cc = new CairoColonCell ();
	_fixed_cells[MS_Colon3] = cc;
	minsec->add_cell (MS_Colon3, cc);
	
	tc = new CairoTextCell (field_length[MS_Milliseconds]);
	_text_cells[MS_Milliseconds] = tc;
	minsec->add_cell (MS_Milliseconds, tc);

	/* Beats/Bars/Ticks */
	
	tc = new CairoTextCell (field_length[Bars]);
	_text_cells[Bars] = tc;
	bbt->add_cell (Bars, tc);

	bc = new CairoBarCell ();
	_fixed_cells[BBT_Bar1] = bc;
	bbt->add_cell (BBT_Bar1, bc);
	
	tc = new CairoTextCell (field_length[Beats]);
	_text_cells[Beats] = tc;
	bbt->add_cell (Beats, tc);
	
	bc = new CairoBarCell ();
	_fixed_cells[BBT_Bar2] = bc;
	bbt->add_cell (BBT_Bar2, bc);

	tc = new CairoTextCell (field_length[Ticks]);
	_text_cells[Ticks] = tc;
	bbt->add_cell (Ticks, tc);

	/* Audio Frames */
	
	tc = new CairoTextCell (field_length[AudioFrames]);
	_text_cells[AudioFrames] = tc;
	frames->add_cell (AudioFrames, tc);

	last_when = 0;
	
	last_hrs = 9999;
	last_mins = 9999;
	last_secs = 9999;
	last_frames = 99999;

	ms_last_hrs = 9999;
	ms_last_mins = 9999;
	ms_last_secs = 9999;
	ms_last_millisecs = 99999;

	last_negative = false;
	
	last_pdelta = 0;
	last_sdelta = 0;
	key_entry_state = 0;
	ops_menu = 0;
	dragging = false;
	bbt_reference_time = -1;
	editing_field = (Field) 0;
	current_cet = 0;

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
	frames_packer.pack_start (*frames);

#ifdef CLOCKFIX
	/* XXX THE with_info CLAUSES NEED HANDLING BECAUSE WE'RE NOT PACKING
	   INTO BOXES
	*/

	if (with_info) {
		frames_packer.pack_start (frames_info_box, false, false, 5);
	}
#endif

	timecode_packer.set_homogeneous (false);
	timecode_packer.set_border_width (2);
	timecode_packer.pack_start (*timecode);
	
#ifdef CLOCKFIX
	if (with_info) {
		timecode_packer.pack_start (timecode_info_box, false, false, 5);
	}
#endif

	bbt_packer.set_homogeneous (false);
	bbt_packer.set_border_width (2);
	bbt_packer.pack_start (*bbt);
	
#ifdef CLOCKFIX
	if (with_info) {
		bbt_packer.pack_start (bbt_info_box, false, false, 5);
	}
#endif
	
	minsec_packer.set_homogeneous (false);
	minsec_packer.set_border_width (2);
	minsec_packer.pack_start (*minsec);

	set_widget_name (widget_name);

	_mode = BBT; /* lie to force mode switch */
	set_mode (Timecode);
	set (last_when, true);

	if (!is_transient) {
		clocks.push_back (this);
	}
}
	
AudioClock::~AudioClock ()
{
	delete timecode;
	delete minsec;
	delete bbt;
	delete frames;
}

void
AudioClock::set_widget_name (string name)
{
	Widget::set_name (name);
	set_theme ();
}

void
AudioClock::set_theme ()
{
	Glib::RefPtr<Gtk::Style> style = get_style ();
	double r, g, b, a;

	if (!style) {
		return;
	}

	Pango::FontDescription font; 

	if (!is_realized()) {
		font = get_font_for_style (get_name());
	} else {
		font = style->get_font();
	}

	timecode->set_font (font);
	minsec->set_font (font);
	bbt->set_font (font);
	frames->set_font (font);

	Gdk::Color bg = style->get_base (Gtk::STATE_NORMAL);
	Gdk::Color fg = style->get_text (Gtk::STATE_NORMAL);
	Gdk::Color eg = style->get_text (Gtk::STATE_ACTIVE);

	r = bg.get_red_p ();
	g = bg.get_green_p ();
	b = bg.get_blue_p ();
	a = 1.0;

	timecode->set_bg (r, g, b, a);
	minsec->set_bg (r, g, b, a);
	bbt->set_bg (r, g, b, a);
	frames->set_bg (r, g, b, a);

	r = fg.get_red_p ();
	g = fg.get_green_p ();
	b = fg.get_blue_p ();
	a = 1.0;

	timecode->set_colors (r, g, b, a);
	minsec->set_colors (r, g, b, a);
	bbt->set_colors (r, g, b, a);
	frames->set_colors (r, g, b, a);

	r = eg.get_red_p ();
	g = eg.get_green_p ();
	b = eg.get_blue_p ();
	a = 1.0;

	timecode->set_edit_colors (r, g, b, a);
	minsec->set_edit_colors (r, g, b, a);
	bbt->set_edit_colors (r, g, b, a);
	frames->set_edit_colors (r, g, b, a);

	queue_draw ();
}

void
AudioClock::focus ()
{
}

void
AudioClock::end_edit ()
{
	current_cet->stop_editing ();
	editing_field = (Field) 0;
	key_entry_state = 0;

	/* move focus back to the default widget in the top level window */

	Keyboard::magic_widget_drop_focus ();

	Widget* top = get_toplevel();

	if (top->is_toplevel ()) {
		Window* win = dynamic_cast<Window*> (top);
		win->grab_focus ();
	}
}

void
AudioClock::on_realize ()
{
	Alignment::on_realize ();

	/* styles are not available until the widgets are bound to a window */
	
	set_theme ();
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
	
	frames->set_text (AudioFrames, buf);

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
		minsec->set_text (MS_Hours, buf);
		ms_last_hrs = hrs;
	}

	if (force || mins != ms_last_mins) {
		sprintf (buf, "%02d", mins);
		minsec->set_text (MS_Minutes, buf);
		ms_last_mins = mins;
	}

	if (force || secs != ms_last_secs) {
		sprintf (buf, "%02d", secs);
		minsec->set_text (MS_Seconds, buf);
		ms_last_secs = secs;
	}

	if (force || millisecs != ms_last_millisecs) {
		sprintf (buf, "%03d", millisecs);
		minsec->set_text (MS_Milliseconds, buf);
		ms_last_millisecs = millisecs;
	}
}

void
AudioClock::set_timecode (framepos_t when, bool force)
{
	char buf[32];
	Timecode::Time TC;

	if (is_duration) {
		_session->timecode_duration (when, TC);
	} else {
		_session->timecode_time (when, TC);
	}

	if (force || TC.hours != last_hrs || TC.negative != last_negative) {
		if (TC.negative) {
			sprintf (buf, "-%0*" PRIu32, field_length[Timecode_Hours], TC.hours);
		} else {
			sprintf (buf, " %0*" PRIu32, field_length[Timecode_Hours], TC.hours);
		}
		timecode->set_text (Timecode_Hours, buf);
		last_hrs = TC.hours;
		last_negative = TC.negative;
	}

	if (force || TC.minutes != last_mins) {
		sprintf (buf, "%0*" PRIu32, field_length[Timecode_Minutes], TC.minutes);
		timecode->set_text (Timecode_Minutes, buf);
		last_mins = TC.minutes;
	}

	if (force || TC.seconds != last_secs) {
		sprintf (buf, "%0*" PRIu32, field_length[Timecode_Seconds], TC.seconds);
		timecode->set_text (Timecode_Seconds, buf);
		last_secs = TC.seconds;
	}

	if (force || TC.frames != last_frames) {
		sprintf (buf, "%0*" PRIu32, field_length[Timecode_Frames], TC.frames);
		timecode->set_text (Timecode_Frames, buf);
		last_frames = TC.frames;
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
	Timecode::BBT_Time BBT;

	/* handle a common case */
	if (is_duration) {
		if (when == 0) {
			BBT.bars = 0;
			BBT.beats = 0;
			BBT.ticks = 0;
		} else {
			_session->tempo_map().bbt_time (when, BBT);
			BBT.bars--;
			BBT.beats--;
		}
	} else {
		_session->tempo_map().bbt_time (when, BBT);
	}

	sprintf (buf, "%0*" PRIu32, field_length[Bars], BBT.bars);
	if (force || _text_cells[Bars]->get_text () != buf) {
		bbt->set_text (Bars, buf);
		_text_cells[Bars]->set_text (buf);
	}
	sprintf (buf, "%0*" PRIu32, field_length[Beats], BBT.beats);
	if (force || _text_cells[Beats]->get_text () != buf) {
		bbt->set_text (Beats, buf);
	}
	sprintf (buf, "%0*" PRIu32, field_length[Ticks], BBT.ticks);
	if (force || _text_cells[Ticks]->get_text () != buf) {
		bbt->set_text (Ticks, buf);
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
AudioClock::edit_next_field ()
{
	/* move on to the next field.
	 */
	
	switch (editing_field) {
		
		/* Timecode */
		
	case Timecode_Hours:
		editing_field = Timecode_Minutes;
		timecode->start_editing (Timecode_Minutes);
		break;
	case Timecode_Minutes:
		editing_field = Timecode_Seconds;
		timecode->start_editing (Timecode_Seconds);
		break;
	case Timecode_Seconds:
		editing_field = Timecode_Frames;
		timecode->start_editing (Timecode_Frames);
		break;
	case Timecode_Frames:
		end_edit ();
		break;
		
		/* Min:Sec */
		
	case MS_Hours:
		editing_field = MS_Minutes;
		minsec->start_editing (MS_Minutes);
		break;
	case MS_Minutes:
		editing_field = MS_Seconds;
		minsec->start_editing (MS_Seconds);
		break;
	case MS_Seconds:
		editing_field = MS_Milliseconds;
		minsec->start_editing (MS_Milliseconds);
		break;
	case MS_Milliseconds:
		end_edit ();
		break;
		
		/* BBT */
		
	case Bars:
		editing_field = Beats;
		bbt->start_editing (Beats);
		break;
	case Beats:
		editing_field = Ticks;
		bbt->start_editing (Ticks);
		break;
	case Ticks:
		end_edit ();
		break;
		
		/* audio frames */
	case AudioFrames:
		end_edit ();
		break;
		
	default:
		break;
	}

	key_entry_state = 0;
}

bool
AudioClock::on_key_press_event (GdkEventKey* ev)
{
	/* return true for keys that we MIGHT use 
	   at release
	*/
	switch (ev->keyval) {
	case GDK_0:
	case GDK_KP_0:
	case GDK_1:
	case GDK_KP_1:
	case GDK_2:
	case GDK_KP_2:
	case GDK_3:
	case GDK_KP_3:
	case GDK_4:
	case GDK_KP_4:
	case GDK_5:
	case GDK_KP_5:
	case GDK_6:
	case GDK_KP_6:
	case GDK_7:
	case GDK_KP_7:
	case GDK_8:
	case GDK_KP_8:
	case GDK_9:
	case GDK_KP_9:
	case GDK_period:
	case GDK_comma:
	case GDK_KP_Decimal:
	case GDK_Tab:
	case GDK_Return:
	case GDK_KP_Enter:
	case GDK_Escape:
		return true;
	default:
		return false;
	}
}

bool
AudioClock::on_key_release_event (GdkEventKey *ev)
{
	if (editing_field == 0) {
		return false;
	}

	CairoTextCell *cell = _text_cells[editing_field];

	if (!cell) {
		return false;
	}

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
	case GDK_comma:
	case GDK_KP_Decimal:
		if (_mode == MinSec && editing_field == MS_Seconds) {
			new_char = '.'; // XXX i18n
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
		end_edit ();
		ChangeAborted();  /*  EMIT SIGNAL  */
		return true;

	default:
		return false;
	}

	if (!move_on) {

		if (key_entry_state == 0) {

			/* initialize with a fresh new string */

			if (editing_field != AudioFrames) {
				for (uint32_t xn = 0; xn < field_length[editing_field] - 1; ++xn) {
					new_text += '0';
				}
			} else {
				new_text = "";
			}

		} else {

			string existing = cell->get_text();
			if (existing.length() >= field_length[editing_field]) {
				new_text = existing.substr (1, field_length[editing_field] - 1);
			} else {
				new_text = existing.substr (0, field_length[editing_field] - 1);
			}
		}

		new_text += new_char;

		if (current_cet) {
			current_cet->set_text (editing_field, new_text);
		}

		_canonical_time_is_displayed = true;
		key_entry_state++;
	}

	if (key_entry_state == field_length[editing_field]) {
		move_on = true;
	}

	if (move_on) {

		if (key_entry_state) {

			/* if key_entry_state != then we edited the text
			 */

			char buf[16];

			switch (editing_field) {
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
				// Bars should never be zero, unless this clock is for a duration
				if (atoi (_text_cells[Bars]->get_text()) == 0 && !is_duration) {
					snprintf (buf, sizeof (buf), "%0*" PRIu32, field_length[Bars], 1);
					_text_cells[Bars]->set_text (buf);
					_canonical_time_is_displayed = true;
				}
				//  beats should never be zero, unless this clock is for a duration
				if (atoi (_text_cells[Beats]->get_text()) == 0 && !is_duration) {
					snprintf (buf, sizeof (buf), "%0*" PRIu32, field_length[Beats], 1);
					_text_cells[Beats]->set_text (buf);
					_canonical_time_is_displayed = true;
				}
				break;
			default:
				break;
			}

			ValueChanged(); /* EMIT_SIGNAL */
		}
		
		edit_next_field ();
	}

	//if user hit Enter, lose focus
	switch (ev->keyval) {
	case GDK_Return:
	case GDK_KP_Enter:
		end_edit ();
	}

	return true;
}

bool
AudioClock::button_press (GdkEventButton *ev, uint32_t id)
{
	Field field = (Field) id;

	switch (ev->button) {
	case 1:
		editing_field = field;
		current_cet->start_editing (field);

		Keyboard::magic_widget_grab_focus ();

		/* make absolutely sure that the pointer is grabbed */
		gdk_pointer_grab(ev->window,false ,
				 GdkEventMask( Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK |Gdk::BUTTON_RELEASE_MASK),
				 NULL,NULL,ev->time);
		dragging = true;
		drag_accum = 0;
		drag_start_y = ev->y;
		drag_y = ev->y;
		break;
		
	default:
		return false;
		break;
	}

	return true;
}

bool
AudioClock::button_release (GdkEventButton *ev, uint32_t id)
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

	return true;
}

bool
AudioClock::scroll (GdkEventScroll *ev, uint32_t id)
{
	Field field = (Field) id;

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
	framecnt_t f = 0;
	Timecode::BBT_Time BBT;
	switch (field) {
	case Timecode_Hours:
		f = (framecnt_t) floor (3600.0 * _session->frame_rate());
		break;
	case Timecode_Minutes:
		f = (framecnt_t) floor (60.0 * _session->frame_rate());
		break;
	case Timecode_Seconds:
		f = _session->frame_rate();
		break;
	case Timecode_Frames:
		f = (framecnt_t) floor (_session->frame_rate() / _session->timecode_frames_per_second());
		break;

	case AudioFrames:
		f = 1;
		break;

	case MS_Hours:
		f = (framecnt_t) floor (3600.0 * _session->frame_rate());
		break;
	case MS_Minutes:
		f = (framecnt_t) floor (60.0 * _session->frame_rate());
		break;
	case MS_Seconds:
		f = (framecnt_t) _session->frame_rate();
		break;
	case MS_Milliseconds:
		f = (framecnt_t) floor (_session->frame_rate() / 1000.0);
		break;

	case Bars:
		BBT.bars = 1;
		BBT.beats = 0;
		BBT.ticks = 0;
		f = _session->tempo_map().bbt_duration_at (pos,BBT,dir);
		break;
	case Beats:
		BBT.bars = 0;
		BBT.beats = 1;
		BBT.ticks = 0;
		f = _session->tempo_map().bbt_duration_at(pos,BBT,dir);
		break;
	case Ticks:
		BBT.bars = 0;
		BBT.beats = 0;
		BBT.ticks = 1;
		f = _session->tempo_map().bbt_duration_at(pos,BBT,dir);
		break;
	default:
		error << string_compose (_("programming error: %1"), "attempt to get frames from non-text field!") << endmsg;
		f = 0;
		break;
	}

	return f;
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
	if (atoi (_text_cells[Timecode_Minutes]->get_text()) > 59) {
		_text_cells[Timecode_Minutes]->set_text("59");
		_canonical_time_is_displayed = true;
	}

	if (atoi (_text_cells[Timecode_Seconds]->get_text()) > 59) {
		_text_cells[Timecode_Seconds]->set_text("59");
		_canonical_time_is_displayed = true;
	}

	switch ((long)rint(_session->timecode_frames_per_second())) {
	case 24:
		if (atoi (_text_cells[Timecode_Frames]->get_text()) > 23) {
			_text_cells[Timecode_Frames]->set_text("23");
			_canonical_time_is_displayed = true;
		}
		break;
	case 25:
		if (atoi (_text_cells[Timecode_Frames]->get_text()) > 24) {
			_text_cells[Timecode_Frames]->set_text("24");
			_canonical_time_is_displayed = true;
		}
		break;
	case 30:
		if (atoi (_text_cells[Timecode_Frames]->get_text()) > 29) {
			_text_cells[Timecode_Frames]->set_text("29");
			_canonical_time_is_displayed = true;
		}
		break;
	default:
		break;
	}

	if (_session->timecode_drop_frames()) {
		if ((atoi (_text_cells[Timecode_Minutes]->get_text()) % 10) && (atoi (_text_cells[Timecode_Seconds]->get_text()) == 0) && (atoi (_text_cells[Timecode_Frames]->get_text()) < 2)) {
			_text_cells[Timecode_Frames]->set_text("02");
			_canonical_time_is_displayed = true;
		}
	}
}

/** This is necessary because operator[] isn't const with std::map.
 *  @param f Field.
 *  @return Label widget.
 */
CairoTextCell*
AudioClock::label (Field f) const
{
	std::map<Field,CairoTextCell*>::const_iterator i = _text_cells.find (f);
	assert (i != _text_cells.end ());

	return i->second;
}

framepos_t
AudioClock::timecode_frame_from_display () const
{
	if (_session == 0) {
		return 0;
	}

	Timecode::Time TC;
	framepos_t sample;

	TC.hours = atoi (label (Timecode_Hours)->get_text());
	TC.minutes = atoi (label (Timecode_Minutes)->get_text());
	TC.seconds = atoi (label (Timecode_Seconds)->get_text());
	TC.frames = atoi (label (Timecode_Frames)->get_text());
	TC.rate = _session->timecode_frames_per_second();
	TC.drop= _session->timecode_drop_frames();

	_session->timecode_to_sample (TC, sample, false /* use_offset */, false /* use_subframes */ );


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
AudioClock::disconnect_signals ()
{
	scroll_connection.disconnect ();
	button_press_connection.disconnect ();
	button_release_connection.disconnect ();
}

void
AudioClock::connect_signals ()
{
	disconnect_signals ();

	if (editable && current_cet) {
		scroll_connection = current_cet->scroll.connect (sigc::mem_fun (*this, &AudioClock::scroll));
		button_press_connection = current_cet->button_press.connect (sigc::mem_fun (*this, &AudioClock::button_press));
		button_release_connection = current_cet->button_release.connect (sigc::mem_fun (*this, &AudioClock::button_release));
	}	
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

	// clock_base.grab_focus ();

	if (_mode == m) {
		return;
	}

	remove ();
	_mode = m;

	switch (_mode) {
	case Timecode:
		current_cet = timecode;
		add (timecode_packer);
		break;

	case BBT:
		current_cet = bbt;
		add (bbt_packer);
		break;

	case MinSec:
		current_cet = minsec;
		add (minsec_packer);
		break;

	case Frames:
		current_cet = frames;
		add (frames_packer);
		break;

	case Off:
		current_cet = 0;
		add (off_hbox);
		break;
	}

	if (current_cet) {
		connect_signals ();
	} else {
		disconnect_signals  ();
	}

	get_child()->show_all ();

	set (last_when, true);

        if (!is_transient) {
                ModeChanged (); /* EMIT SIGNAL (the static one)*/
        }

        mode_changed (); /* EMIT SIGNAL (the member one) */
}

void
AudioClock::set_bbt_reference (framepos_t pos)
{
	bbt_reference_time = pos;
}

void
AudioClock::on_style_changed (const Glib::RefPtr<Gtk::Style>& old_style)
{
	Alignment::on_style_changed (old_style);
	set_theme ();
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
