/*
 * Copyright (C) 2006-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2006-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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

#include <cmath>

#include <gtkmm2ext/utils.h>

#include "pbd/memento_command.h"
#include "pbd/pthread_utils.h"
#include "pbd/stateful_diff_command.h"

#include "temporal/tempo.h"

#include "ardour/audioregion.h"
#include "ardour/dB.h"
#include "ardour/region_fx_plugin.h"
#include "ardour/session_event.h"

#include "audio_region_editor.h"
#include "audio_region_view.h"
#include "gui_thread.h"
#include "public_editor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;
using namespace Gtkmm2ext;

static void*
_peak_amplitude_thread (void* arg)
{
	static_cast<AudioRegionEditor*> (arg)->peak_amplitude_thread ();
	return 0;
}

AudioRegionEditor::AudioRegionEditor (Session* s, AudioRegionView* arv)
	: RegionEditor (s, arv->region())
	, _arv (arv)
	, _audio_region (arv->audio_region ())
	, _gain_adjustment (accurate_coefficient_to_dB (fabsf (_audio_region->scale_amplitude ())), -40.0, +40.0, 0.1, 1.0, 0)
	, _polarity_toggle (_("Polarity Invert"))
	, _fade_before_fx_toggle (_("Fade before Fx"))
	, _show_on_touch (_("Show on Touch"))
	, _peak_channel (false)
{
	_gain_label.set_name ("AudioRegionEditorLabel");
	_gain_label.set_text (_("Region gain"));
	_gain_label.set_alignment (0, 0.5);

	_peak_amplitude_label.set_name ("AudioRegionEditorLabel");
	_peak_amplitude_label.set_text (_("Peak Amplitude"));
	_peak_amplitude_label.set_alignment (1, 0.5);

	_region_line_label.set_name ("AudioRegionEditorLabel");
	_region_line_label.set_text (_("Region Line:"));
	_region_line_label.set_alignment (1, 0.5);

	_region_line.set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	_region_line.set_layout_ellipsize_width (PANGO_SCALE * 240);

	Gtk::Table* gain_table = manage (new Gtk::Table ());
	gain_table->set_homogeneous ();
	gain_table->set_col_spacings (12);
	gain_table->set_row_spacings (6);
	gain_table->set_border_width (0);

	_gain_entry.configure (_gain_adjustment, 0, 1);

	int row = 0;
	gain_table->attach (_gain_label, 0, 2, row, row + 1, Gtk::FILL, Gtk::FILL);
	gain_table->attach (_peak_amplitude_label, 2, 4, row, row + 1, Gtk::FILL, Gtk::FILL);
	++row;

	Gtk::HBox* b = Gtk::manage (new Gtk::HBox);
	b->set_spacing (6);
	b->pack_start (_gain_entry);
	b->pack_start (*Gtk::manage (new Gtk::Label (_("dB"))), false, false);
	gain_table->attach (*b, 0, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);

	b = Gtk::manage (new Gtk::HBox);
	b->set_spacing (6);
	b->pack_start (_peak_amplitude);
	b->pack_start (*Gtk::manage (new Gtk::Label (_("dBFS"))), false, false);
	gain_table->attach (*b, 2, 4, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::FILL);
	++row;

	gain_table->attach (_polarity_toggle, 0, 2, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	gain_table->attach (_fade_before_fx_toggle, 2, 4, row, row + 1, Gtk::FILL, Gtk::SHRINK);
	++row;

	/* Add to main layout */

	_table_main.attach (*gain_table,        1, 2, 3, 4, Gtk::FILL, Gtk::SHRINK);
	_table_main.attach (_region_line_label, 0, 1, 5, 6, Gtk::FILL, Gtk::SHRINK);
	_table_main.attach (_region_line,       1, 3, 5, 6, Gtk::EXPAND | Gtk::FILL, Gtk::SHRINK);
	_table_main.attach (_show_on_touch,     3, 4, 5, 6, Gtk::FILL, Gtk::SHRINK);

	UI::instance()->set_tip (_polarity_toggle, _("Invert the signal polarity (180deg phase shift)"));
	UI::instance()->set_tip (_fade_before_fx_toggle, _("Apply region effects after the region fade.\nThis is useful if the effect(s) have tail, which would otherwise be faded out by the region fade (e.g. reverb, delay)"));
	UI::instance()->set_tip (_show_on_touch, _("When touching a control in a region effect plugin UI, the corresponding region-automation line is shown the editor, and edit mode is set to 'draw'."));

	gain_changed ();
	fade_before_fx_changed ();
	refill_region_line ();

	_gain_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &AudioRegionEditor::gain_adjustment_changed));
	_polarity_toggle.signal_toggled ().connect (sigc::mem_fun (*this, &AudioRegionEditor::gain_adjustment_changed));
	_fade_before_fx_toggle.signal_toggled ().connect (sigc::mem_fun (*this, &AudioRegionEditor::fade_before_fx_toggle_changed));
	_show_on_touch.signal_toggled ().connect (sigc::mem_fun (*this, &AudioRegionEditor::show_on_touch_changed));

	_arv->region_line_changed.connect ((sigc::mem_fun (*this, &AudioRegionEditor::refill_region_line)));

	_peak_amplitude.property_editable () = false;
	_peak_amplitude.set_width_chars (9);
	_peak_amplitude.set_text (_("Analyzing"));

	PeakAmplitudeFound.connect (_peak_amplitude_connection, invalidator (*this), std::bind (&AudioRegionEditor::peak_amplitude_found, this, _1), gui_context ());

	char name[64];
	snprintf (name, 64, "peak amplitude-%p", this);
	pthread_create_and_store (name, &_peak_amplitude_thread_handle, _peak_amplitude_thread, this);
	signal_peak_thread ();
}

AudioRegionEditor::~AudioRegionEditor ()
{
	void* v;
	_peak_channel.deliver ('t');
	pthread_join (_peak_amplitude_thread_handle, &v);
}

void
AudioRegionEditor::region_changed (const PBD::PropertyChange& what_changed)
{
	RegionEditor::region_changed (what_changed);

	if (what_changed.contains (ARDOUR::Properties::scale_amplitude)) {
		gain_changed ();
	}

	if (what_changed.contains (ARDOUR::Properties::fade_before_fx)) {
		fade_before_fx_changed ();
	}

	if (what_changed.contains (ARDOUR::Properties::start) || what_changed.contains (ARDOUR::Properties::length)) {
		/* ask the peak thread to run again */
		signal_peak_thread ();
	}
}

void
AudioRegionEditor::region_fx_changed ()
{
	RegionEditor::region_fx_changed ();
	refill_region_line ();
}

void
AudioRegionEditor::gain_changed ()
{
	const gain_t scale_amplitude = _audio_region->scale_amplitude ();

	float const region_gain_dB = accurate_coefficient_to_dB (fabsf (scale_amplitude));
	if (region_gain_dB != _gain_adjustment.get_value ()) {
		_gain_adjustment.set_value (region_gain_dB);
	}
	_polarity_toggle.set_active (scale_amplitude < 0);
}

void
AudioRegionEditor::gain_adjustment_changed ()
{
	float gain = dB_to_coefficient (_gain_adjustment.get_value ());
	if (_polarity_toggle.get_active ()) {
		gain *= -1;
	}
	if (_audio_region->scale_amplitude () != gain) {
		_audio_region->set_scale_amplitude (gain);
	}
}

void
AudioRegionEditor::fade_before_fx_changed ()
{
	_fade_before_fx_toggle.set_active (_audio_region->fade_before_fx ());
}

void
AudioRegionEditor::fade_before_fx_toggle_changed ()
{
	_audio_region->set_fade_before_fx (_fade_before_fx_toggle.get_active ());
}

void
AudioRegionEditor::signal_peak_thread ()
{
	_peak_channel.deliver ('c');
}

void
AudioRegionEditor::peak_amplitude_thread ()
{
	while (1) {
		char msg;
		/* await instructions to run */
		_peak_channel.receive (msg);

		if (msg == 't') {
			break;
		}

		/* update thread-local tempo map */

		Temporal::TempoMap::fetch ();

		/* compute peak amplitude and signal the fact */
		PeakAmplitudeFound (accurate_coefficient_to_dB (_audio_region->maximum_amplitude ())); /* EMIT SIGNAL */
	}
}

void
AudioRegionEditor::peak_amplitude_found (double p)
{
	stringstream s;
	s.precision (2);
	s.setf (ios::fixed, ios::floatfield);
	s << p;
	_peak_amplitude.set_text (s.str ());
}

void
AudioRegionEditor::show_touched_automation (std::weak_ptr<PBD::Controllable> wac)
{
	if (!_arv->set_region_fx_line (wac)) {
		return;
	}

	switch (PublicEditor::instance ().current_mouse_mode ()) {
		case Editing::MouseObject:
		case Editing::MouseTimeFX:
		case Editing::MouseGrid:
		case Editing::MouseCut:
			PublicEditor::instance ().set_mouse_mode (Editing::MouseDraw, false);
			break;
		default:
			break;
	}
}

void
AudioRegionEditor::show_on_touch_changed ()
{
	if (!_show_on_touch.get_active ()) {
		_ctrl_touched_connection.disconnect ();
		return;
	}
	Controllable::ControlTouched.connect (_ctrl_touched_connection, invalidator (*this), std::bind (&AudioRegionEditor::show_touched_automation, this, _1), gui_context ());
}

void
AudioRegionEditor::refill_region_line ()
{
	using namespace Gtk::Menu_Helpers;

	_region_line.clear_items ();

	MenuList& rm_items (_region_line.items ());

	int      nth = 0;
	PBD::ID  rfx_id (0);
	uint32_t param_id    = 0;
	string   active_text = _("Gain Envelope");

	_arv->get_region_fx_line (rfx_id, param_id);
	_arv->set_ignore_line_change (true);

	Gtk::RadioMenuItem::Group grp;
	AudioRegionView*          arv = _arv;

	rm_items.push_back (RadioMenuElem (grp, _("Gain Envelope")));
	Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*> (&rm_items.back ());
	cmi->set_active (rfx_id == 0 || param_id == UINT32_MAX);
	cmi->signal_activate ().connect ([cmi, arv] () { if (cmi->get_active ()) {arv->set_region_gain_line (); } });

	_audio_region->foreach_plugin ([&rm_items, arv, &nth, &grp, &active_text, rfx_id, param_id] (std::weak_ptr<RegionFxPlugin> wfx) {
		std::shared_ptr<RegionFxPlugin> fx (wfx.lock ());
		if (!fx) {
			return;
		}
		std::shared_ptr<Plugin> plugin = fx->plugin ();

		if (!plugin) {
			return;
		}

		Gtk::Menu* acm = manage (new Gtk::Menu);
		MenuList&  acm_items (acm->items ());

		for (size_t i = 0; i < plugin->parameter_count (); ++i) {
			if (!plugin->parameter_is_control (i) || !plugin->parameter_is_input (i)) {
				continue;
			}
			const Evoral::Parameter param (PluginAutomation, 0, i);
			std::string             label = plugin->describe_parameter (param);
			if (label == X_("latency") || label == X_("hidden")) {
				continue;
			}
			std::shared_ptr<ARDOUR::AutomationControl> c (std::dynamic_pointer_cast<ARDOUR::AutomationControl> (fx->control (param)));
			if (c && c->flags () & (Controllable::HiddenControl | Controllable::NotAutomatable)) {
				continue;
			}
			bool active = fx->id () == rfx_id && param_id == i;

			acm_items.push_back (RadioMenuElem (grp, label));
			Gtk::CheckMenuItem* cmi = static_cast<Gtk::CheckMenuItem*> (&acm_items.back ());
			cmi->set_active (active);
			cmi->signal_activate ().connect ([cmi, arv, nth, i] () { if (cmi->get_active ()) {arv->set_region_fx_line (nth, i); } });
			if (active) {
				active_text = fx->name () + ": " + label;
			}
		}

		if (!acm_items.empty ()) {
			rm_items.push_back (MenuElem (fx->name (), *acm));
		} else {
			delete acm;
		}
		++nth;
	});

	if (rm_items.size () > 1) {
		_show_on_touch.set_sensitive (true);
	} else {
		_show_on_touch.set_active (false);
		_show_on_touch.set_sensitive (false);
	}

	_region_line.set_text (active_text);
	_arv->set_ignore_line_change (false);
}

void
AudioRegionEditor::on_unmap ()
{
	_show_on_touch.set_active (false);
}
