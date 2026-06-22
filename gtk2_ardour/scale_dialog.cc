/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ytkmm/stock.h"

#include "pbd/unwind.h"

#include "gtkmm2ext/utils.h"

#include "ardour/parameter_descriptor.h"

#include "scale_dialog.h"

#include "pbd/i18n.h"

std::map<ARDOUR::MusicalModeType,std::string> ScaleDialog::type_string_map;
std::map<std::string,ARDOUR::MusicalModeType> ScaleDialog::string_type_map;

using namespace ARDOUR;

void
ScaleDialog::fill_maps ()
{
	struct stpair {
		stpair (char const * const s, MusicalModeType t) : str (s), type (t) {}
		char const * const str;
		MusicalModeType type;
	};

	std::vector<stpair> pairs = {
		{ _("Absolute Pitch (Hz)"), AbsolutePitch },
		{ _("Semitone Steps") ,SemitoneSteps },
		{ _("Whole Tone Steps"), WholeToneSteps },
		{ _("Ratio Steps"), RatioSteps },
		{ _("Ratios from root"), RatioFromRoot },
		{ _("MIDI Note Numbers"), MidiNote },
		};

	for (auto const & p : pairs) {
		type_string_map[p.type] = p.str;
		string_type_map[p.str] = p.type;
	}
}

ScaleDialog::ScaleDialog ()
	: ArdourDialog (_("Scale Editor"))
	, _tuning (TwelveTone)
	, _key (nullptr)
	, name_label (_("Name"))
	, type_label (_("Type"))
	, tuning_label (_("Tuning System"))
	, step_adjustment (7, 1, 56, 1, 8)
	, steps_label (_("Pitches"))
	, step_spinner (step_adjustment)
	, scala_label (_("Load a Scala file"))
	, clear_button (_("Remove scale"))
	, ignore_set (false)
{
	if (type_string_map.empty()) {
		fill_maps ();
	}

	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;

	Gtk::HBox* inner_tuning_box (manage (new Gtk::HBox));
	inner_tuning_box->set_spacing (12);
	inner_tuning_box->set_border_width (12);

	tuning_dropdown.add_menu_elem (MenuElem (_("Twelve Tone"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::fill_dropdowns), TwelveTone)));
	tuning_dropdown.set_active (0);

	inner_tuning_box->pack_start (tuning_label, false, false);
	inner_tuning_box->pack_start (tuning_dropdown, true, true);

	root_mode_box.pack_start (root_dropdown, false, false);
	root_mode_box.pack_start (mode_dropdown, true, true);

	named_scale_box.pack_start (*inner_tuning_box, false, false);
	named_scale_box.pack_start (root_mode_box, false, false);

	type_dropdown.add_menu_elem (MenuElem (_("Absolute Pitch (Hz)"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), AbsolutePitch)));
	type_dropdown.add_menu_elem (MenuElem (_("Semitone Steps"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), SemitoneSteps)));
	type_dropdown.add_menu_elem (MenuElem (_("Whole Tone Steps"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), WholeToneSteps)));
	type_dropdown.add_menu_elem (MenuElem (_("Ratio steps"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), RatioSteps)));
	type_dropdown.add_menu_elem (MenuElem (_("Ratios from root"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), RatioFromRoot)));
	type_dropdown.add_menu_elem (MenuElem (_("MIDI Note Numbers"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), MidiNote)));

	type_dropdown.set_active (_("Semitone Steps"));

	Gtk::HBox* inner_type_box (manage (new Gtk::HBox));
	inner_type_box->pack_start (type_label, false, false);
	inner_type_box->pack_start (type_dropdown, false, false);
	inner_type_box->set_spacing (12);
	type_box.pack_start (*inner_type_box, true, false);

	Gtk::HBox* inner_name_box (manage (new Gtk::HBox));

	inner_name_box->pack_start (name_label, false, false);
	inner_name_box->pack_start (name_entry, false, false);
	inner_name_box->set_spacing (12);

	name_packer.pack_start (*inner_name_box, true, false);

	Gtk::HBox* inner_step_box (manage (new Gtk::HBox));
	inner_step_box->pack_start (steps_label, false, false);
	inner_step_box->pack_start (step_spinner, false, false);
	inner_step_box->set_spacing (12);
	steps_box.pack_start (*inner_step_box, true, false);

	Gtk::HBox* inner_scala_box (manage (new Gtk::HBox));
	inner_scala_box->pack_start (scala_label, false, false);
	inner_scala_box->pack_start (scala_file_button, true, true);
	inner_scala_box->set_spacing (12);
	scala_box.pack_start (*inner_scala_box);

	scala_file_button.set_current_folder (Glib::get_home_dir());

	Gtk::VBox* vbox (get_vbox());
	vbox->pack_start (named_scale_box, false, false);
	vbox->pack_start (scala_box, false, false);
	vbox->pack_start (type_box, false, false);
	vbox->pack_start (name_packer, false, false);

	vbox->pack_start (steps_box, false, false);
	vbox->pack_start (step_packer, false, false);

	Gtk::HBox* clear_box (manage (new Gtk::HBox));
	clear_box->pack_start (clear_button, true, false);
	vbox->pack_start (*clear_box, false, false);

	vbox->set_border_width (6);
	vbox->set_spacing (12);
	vbox->show_all ();

	step_packer.set_spacing (12);
	pack_steps ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);

}

ScaleDialog::~ScaleDialog ()
{
}

void
ScaleDialog::set_tuning (TuningSystem c)
{
	_tuning = c;
	tuning_dropdown.set_active ((int) c);
}

void
ScaleDialog::set (MusicalKey const * key)
{
	using namespace ARDOUR;

	if (ignore_set) {
		return;
	}

	PBD::Unwinder<bool> uw (ignore_set, true);

	if (!key) {
		_key = nullptr;
		mode_dropdown.set_active (0);
		return;
	}

	switch (_tuning) {
	case TwelveTone:
		twelvetone_set (*key);
		break;
	}
}

void
ScaleDialog::twelvetone_set (MusicalKey const & key)
{
	if (!_key) {
		_key.reset (new MusicalKey (key));
	} else {
		*_key = key;
	}

	mode_dropdown.set_active (key.mode_name());
	std::vector<int> notes_in_alpha_order ({ 9, 10, 11, 0, 1, 2, 3, 4, 5, 6, 7, 8});
	int i = 0;

	for (auto n : notes_in_alpha_order) {
		if (n == key.root()) {
			root_dropdown.set_active (i);
			break;
		}
		++i;
	}
}

MusicalKey*
ScaleDialog::get() const
{
	using namespace ARDOUR;

	switch (_tuning) {
	case TwelveTone:
		return twelvetone_get ();
	}

	return nullptr;
}

MusicalKey*
ScaleDialog::twelvetone_get() const
{
	std::string mode = mode_dropdown.get_active ();

	if (mode.empty()) {
		return nullptr;
	}

	int root_index = root_dropdown.get_active_row_number ();
	std::vector<int> notes_in_alpha_order ({ 9, 10, 11, 0, 1, 2, 3, 4, 5, 6, 7, 8});
	int root_midi_note = notes_in_alpha_order[root_index];

	/* XXX this leaks. Probably need a "None" value for a MusicalKey */
	return new MusicalKey (root_midi_note, mode);
}

void
ScaleDialog::fill_dropdowns (TuningSystem tuning)
{
	using namespace Gtk::Menu_Helpers;
	using namespace ARDOUR;

	root_dropdown.clear_items ();
	mode_dropdown.clear_items ();

	std::vector<int> notes_in_alpha_order ({ 9, 10, 11, 0, 1, 2, 3, 4, 5, 6, 7, 8});

	switch (tuning) {
	case TwelveTone:
		for (auto n : notes_in_alpha_order) {
			root_dropdown.add_menu_elem (MenuElem (ParameterDescriptor::midi_note_name (n, true, false, true), [this,n]() { MusicalMode mode (_key ? *_key : MusicalMode (_("Major")));  MusicalKey k (n, mode); set (&k); }));
		}
		break;
	}

	for (auto const & [tune,mode]: MusicalMode::scales_by_tuning) {
		if (tune != tuning) {
			continue;
		}
		mode_dropdown.add_menu_elem (MenuElem (mode.name(), [this,mode]() { float root = _key ? _key->root() : 64; MusicalKey k (root, mode); set (&k); }));
	}

	root_dropdown.set_active (0);
	mode_dropdown.set_active (0);
}

void
ScaleDialog::pack_steps ()
{
	Gtkmm2ext::container_clear (step_packer, true);

	if (!_key) {
		return;
	}

	for (int n = 0; n < _key->size(); ++n) {
		Gtk::HBox* hb (manage (new Gtk::HBox));
		Gtk::HBox* ihb (manage (new Gtk::HBox));
		Gtk::Label* label (manage (new Gtk::Label));
		char buf[64];
		snprintf (buf, sizeof (buf), "%d", n);
		label->set_text (buf);

		StepEntry* se = manage (new StepEntry (n));
		Gtkmm2ext::set_size_request_to_display_given_text (*se, "abcdef", 2, 6);

		ihb->pack_start (*label, false, false);
		ihb->pack_start (*se, false, false);
		ihb->set_spacing (6);
		hb->pack_start (*ihb, true, false);

		step_packer.pack_start (*hb, false, false);
		hb->show_all ();
	}
}

void
ScaleDialog::set_type (MusicalModeType t)
{
}
