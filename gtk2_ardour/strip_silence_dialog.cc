/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#include <iostream>

#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/stock.h>

#include "pbd/pthread_utils.h"

#include "ardour/audioregion.h"
#include "ardour/dB.h"
#include "ardour/logmeter.h"
#include "ardour_ui.h"

#include "audio_clock.h"
#include "gui_thread.h"
#include "strip_silence_dialog.h"
#include "region_view.h"
#include "rgb_macros.h"
#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace ArdourCanvas;

/** Construct Strip silence dialog box */
StripSilenceDialog::StripSilenceDialog (Session* s, list<RegionView*> const & v)
	: ArdourDialog (_("Strip Silence"))
	, ProgressReporter ()
	, _minimum_length (new AudioClock (X_("silence duration"), true, "", true, false, true, false))
	, _fade_length (new AudioClock (X_("silence duration"), true, "", true, false, true, false))
	, _destroying (false)
	, analysis_progress_cur (0)
	, analysis_progress_max (0)
	, _threshold_value (-60)
	, _minimum_length_value (1000)
	, _fade_length_value (64)
{
	set_session (s);

	for (list<RegionView*>::const_iterator r = v.begin(); r != v.end(); ++r) {
		views.push_back (ViewInterval (*r));
	}

	_minimum_length->set_is_duration (true, views.front().view->region()->nt_position());
	_fade_length->set_is_duration (true, views.front().view->region()->nt_position());

	Gtk::HBox* hbox = Gtk::manage (new Gtk::HBox);

	Gtk::Table* table = Gtk::manage (new Gtk::Table (3, 3));
	table->set_spacings (6);

	//get the last used settings for this
	XMLNode* node = _session->extra_xml(X_("StripSilence"));
	if (node) {
		node->get_property(X_("threshold"), _threshold_value);
		node->get_property(X_("min-length"), _minimum_length_value);
		node->get_property(X_("fade-length"), _fade_length_value);
	}

	int n = 0;

	table->attach (*Gtk::manage (new Gtk::Label (_("Threshold"), 1, 0.5)), 0, 1, n, n + 1, Gtk::FILL);
	table->attach (_threshold, 1, 2, n, n + 1, Gtk::FILL);
	table->attach (*Gtk::manage (new Gtk::Label (_("dBFS"))), 2, 3, n, n + 1, Gtk::FILL);
	++n;

	_threshold.set_digits (1);
	_threshold.set_increments (1, 10);
	_threshold.set_range (-120, 0);
	_threshold.set_value (_threshold_value);
	_threshold.set_activates_default ();

	table->attach (*Gtk::manage (new Gtk::Label (_("Minimum length"), 1, 0.5)), 0, 1, n, n + 1, Gtk::FILL);
	table->attach (*_minimum_length, 1, 2, n, n + 1, Gtk::FILL);
	++n;

	_minimum_length->set_session (s);
	_minimum_length->set_mode (AudioClock::Samples);
	_minimum_length->set_duration (timecnt_t (_minimum_length_value), true);

	table->attach (*Gtk::manage (new Gtk::Label (_("Fade length"), 1, 0.5)), 0, 1, n, n + 1, Gtk::FILL);
	table->attach (*_fade_length, 1, 2, n, n + 1, Gtk::FILL);
	++n;

	_fade_length->set_session (s);
	_fade_length->set_mode (AudioClock::Samples);
	_fade_length->set_is_duration (true, timepos_t());
	_fade_length->set_duration (timecnt_t (_fade_length_value), true);

	hbox->pack_start (*table);

	get_vbox()->pack_start (*hbox, false, false);

	cancel_button = add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	apply_button = add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_OK);
	set_default_response (Gtk::RESPONSE_OK);

	get_vbox()->pack_start (_progress_bar, true, true, 12);

	show_all ();

	_threshold.get_adjustment()->signal_value_changed().connect (sigc::mem_fun (*this, &StripSilenceDialog::threshold_changed));
	_minimum_length->ValueChanged.connect (sigc::mem_fun (*this, &StripSilenceDialog::restart_thread));
	_fade_length->ValueChanged.connect (sigc::mem_fun (*this, &StripSilenceDialog::restart_thread));

	update_silence_rects ();
	update_threshold_line ();

	_progress_bar.set_text (_("Analyzing"));
	update_progress_gui (0);
	apply_button->set_sensitive (false);
	progress_idle_connection = Glib::signal_idle().connect (sigc::mem_fun (*this, &StripSilenceDialog::idle_update_progress));

	/* Create a thread which runs while the dialogue is open to compute the silence regions */
	Completed.connect (_completed_connection, invalidator(*this), boost::bind (&StripSilenceDialog::update, this), gui_context ());
	_thread_should_finish = false;
	pthread_create (&_thread, 0, StripSilenceDialog::_detection_thread_work, this);

	signal_response().connect(sigc::mem_fun (*this, &StripSilenceDialog::finished));
}


StripSilenceDialog::~StripSilenceDialog ()
{
	_destroying = true;
	progress_idle_connection.disconnect();

	/* Terminate our thread */
	_interthread_info.cancel = true;
	_lock.lock ();
	_thread_should_finish = true;
	_lock.unlock ();

	_run_cond.signal ();
	pthread_join (_thread, 0);

	delete _minimum_length;
	delete _fade_length;
}

bool
StripSilenceDialog::idle_update_progress()
{
	if (analysis_progress_max > 0) {
		// AudioRegion::find_silence() has
		// itt.progress = (end - pos) / length
		// not sure if that's intentional, but let's use (1. - val)
		float rp = std::min(1.f, std::max (0.f, (1.f - _interthread_info.progress)));
		float p = analysis_progress_cur / (float) analysis_progress_max
		        + rp / (float) analysis_progress_max;
		update_progress_gui (p);
	}
	return !_destroying;
}

void
StripSilenceDialog::silences (AudioIntervalMap& m)
{
	for (list<ViewInterval>::iterator v = views.begin(); v != views.end(); ++v) {
		pair<boost::shared_ptr<Region>,AudioIntervalResult> newpair (v->view->region(), v->intervals);
		m.insert (newpair);
	}
}

void
StripSilenceDialog::drop_rects ()
{
	// called by parent when starting to progess (dialog::run returned),
	// but before the dialog is destoyed.

	_interthread_info.cancel = true;

	/* Block until the thread is idle */
	_lock.lock ();
	_lock.unlock ();

	for (list<ViewInterval>::iterator v = views.begin(); v != views.end(); ++v) {
		v->view->drop_silent_frames ();
	}

	cancel_button->set_sensitive (false);
	apply_button->set_sensitive (false);
}

void
StripSilenceDialog::update_threshold_line ()
{
#if 0
	int n = 0;

	/* Don't need to lock here as we're not reading the _waves silence details */

	for (list<Wave*>::iterator i = _waves.begin(); i != _waves.end(); ++i) {
		(*i)->threshold_line->property_x1() = 0;
		(*i)->threshold_line->property_x2() = _wave_width;

		double const y = alt_log_meter (_threshold.get_value());

		(*i)->threshold_line->property_y1() = (n + 1 - y) * _wave_height;
		(*i)->threshold_line->property_y2() = (n + 1 - y) * _wave_height;
	}

	++n;
#endif
}

void
StripSilenceDialog::update ()
{
	update_threshold_line ();
	update_silence_rects ();
	_progress_bar.set_text ("");
	update_progress_gui (0);
	apply_button->set_sensitive(true);
}

void
StripSilenceDialog::update_silence_rects ()
{
	/* Lock so that we don't contend with the detection thread for access to the silence regions */
	Glib::Threads::Mutex::Lock lm (_lock);
	double const y = _threshold.get_value();

	for (list<ViewInterval>::iterator v = views.begin(); v != views.end(); ++v) {
		v->view->set_silent_frames (v->intervals, y);
	}
}

void *
StripSilenceDialog::_detection_thread_work (void* arg)
{
	StripSilenceDialog* d = reinterpret_cast<StripSilenceDialog*> (arg);
	pthread_set_name ("SilenceDetect");
	return d->detection_thread_work ();
}

/** Body of our silence detection thread */
void *
StripSilenceDialog::detection_thread_work ()
{
	/* Do not register with all UIs, but do register with the GUI,
	   because we will need to queue some GUI (only) requests
	*/
	ARDOUR_UI::instance()->register_thread (pthread_self(), "silence", 32);

	/* Hold this lock when we are doing work */
	_lock.lock ();

	while (1) {
		analysis_progress_cur = 0;
		analysis_progress_max = views.size();
		for (list<ViewInterval>::iterator i = views.begin(); i != views.end(); ++i) {
			boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> ((*i).view->region());

			if (ar) {
				i->intervals = ar->find_silence (dB_to_coefficient (threshold ()), minimum_length (), fade_length(), _interthread_info);
			}

			if (_interthread_info.cancel) {
				break;
			}
			++analysis_progress_cur;
			_interthread_info.progress = 1.0;
			ARDOUR::GUIIdle ();
		}

		analysis_progress_max = 0;

		if (!_interthread_info.cancel) {
			Completed (); /* EMIT SIGNAL */
		}

		/* Our work is done; sleep until there is more to do.
		 * The lock is released while we are waiting.
		 */
		_run_cond.wait (_lock);

		if (_thread_should_finish) {
			_lock.unlock ();
			return 0;
		}
	}

	return 0;
}

void
StripSilenceDialog::restart_thread ()
{
	if (_destroying) {
		/* I don't know how this happens, but it seems to be possible for this
		   method to be called after our destructor has finished executing.
		   If this happens, bad things follow; _lock cannot be locked and
		   Ardour hangs.  So if we are destroying, just bail early.
		   */
		return;
	}

	_progress_bar.set_text (_("Analyzing"));
	update_progress_gui (0);
	apply_button->set_sensitive (false);

	/* Cancel any current run */
	_interthread_info.cancel = true;

	/* Block until the thread waits() */
	_lock.lock ();
	/* Reset the flag */
	_interthread_info.cancel = false;
	_lock.unlock ();

	/* And re-awake the thread */
	_run_cond.signal ();
}

void
StripSilenceDialog::threshold_changed ()
{
	update_threshold_line ();
	restart_thread ();
}

samplecnt_t
StripSilenceDialog::minimum_length () const
{
	return std::max((samplecnt_t)1, _minimum_length->current_duration (views.front().view->region()->nt_position()).samples());
}

samplecnt_t
StripSilenceDialog::fade_length () const
{
	return std::max((samplecnt_t)0, _fade_length->current_duration (views.front().view->region()->nt_position()).samples());
}

void
StripSilenceDialog::update_progress_gui (float p)
{
	_progress_bar.set_fraction (p);
}

XMLNode&
StripSilenceDialog::get_state ()
{
	XMLNode* node = new XMLNode(X_("StripSilence"));
	node->set_property(X_("threshold"), threshold());
	node->set_property(X_("min-length"), minimum_length());
	node->set_property(X_("fade-length"), fade_length());
	return *node;
}

void
StripSilenceDialog::set_state (const XMLNode &)
{
}

void
StripSilenceDialog::finished(int response)
{
	if(response == Gtk::RESPONSE_OK) {
		_session->add_extra_xml(get_state());
	}
}
