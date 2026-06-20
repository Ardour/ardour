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

#include "gtkmm2ext/utils.h"

#include "scale_dialog.h"

#include "pbd/i18n.h"

std::map<ARDOUR::MusicalModeType,std::string> ScaleDialog::type_string_map;
std::map<std::string,ARDOUR::MusicalModeType> ScaleDialog::string_type_map;


void
ScaleDialog::fill_maps ()
{
	struct stpair {
		stpair (char const * const s, ARDOUR::MusicalModeType t) : str (s), type (t) {}
		char const * const str;
		ARDOUR::MusicalModeType type;
	};

	std::vector<stpair> pairs = {
		{ _("Absolute Pitch (Hz)"), ARDOUR::AbsolutePitch },
		{ _("Semitone Steps") ,ARDOUR::SemitoneSteps },
		{ _("Whole Tone Steps"), ARDOUR::WholeToneSteps },
		{ _("Ratio Steps"), ARDOUR::RatioSteps },
		{ _("Ratios from root"), ARDOUR::RatioFromRoot },
		{ _("MIDI Note Numbers"), ARDOUR::MidiNote },
		};

	for (auto const & p : pairs) {
		type_string_map[p.type] = p.str;
		string_type_map[p.str] = p.type;
	}
}

ScaleDialog::ScaleDialog ()
	: ArdourDialog (_("Scale Editor"))
	, _tuning (ARDOUR::TwelveTone)
	, _key (nullptr)
	, name_label (_("Name"))
	, type_label (_("Type"))
	, step_adjustment (7, 1, 56, 1, 8)
	, steps_label (_("Pitches"))
	, step_spinner (step_adjustment)
	, scala_label (_("Load a Scala file"))
	, clear_button (_("Remove scale"))
{
	if (type_string_map.empty()) {
		fill_maps ();
	}

	using namespace Gtk;
	using namespace Gtk::Menu_Helpers;

	tuning_dropdown.add_menu_elem (MenuElem (_("Twelve Tone"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::fill_dropdowns), ARDOUR::TwelveTone)));
	tuning_dropdown.set_active (0);

	root_mode_box.pack_start (root_dropdown, true, false);
	root_mode_box.pack_start (mode_dropdown, true, false);

	named_scale_box.pack_start (tuning_dropdown, false, false);
	named_scale_box.pack_start (root_mode_box, false, false);

	type_dropdown.add_menu_elem (MenuElem (_("Absolute Pitch (Hz)"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), ARDOUR::AbsolutePitch)));
	type_dropdown.add_menu_elem (MenuElem (_("Semitone Steps"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), ARDOUR::SemitoneSteps)));
	type_dropdown.add_menu_elem (MenuElem (_("Whole Tone Steps"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), ARDOUR::WholeToneSteps)));
	type_dropdown.add_menu_elem (MenuElem (_("Ratio steps"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), ARDOUR::RatioSteps)));
	type_dropdown.add_menu_elem (MenuElem (_("Ratios from root"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), ARDOUR::RatioFromRoot)));
	type_dropdown.add_menu_elem (MenuElem (_("MIDI Note Numbers"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::set_type), ARDOUR::MidiNote)));

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
	vbox->pack_start (name_packer, false, false);
	vbox->pack_start (named_scale_box, false, false);
	vbox->pack_start (scala_box, false, false);
	vbox->pack_start (type_box, false, false);
	vbox->pack_start (steps_box, false, false);
	vbox->pack_start (step_packer, false, false);

	Gtk::HBox* clear_box (manage (new Gtk::HBox));
	clear_box->pack_start (clear_button, true, false);
	vbox->pack_start (*clear_box, false, false);

	vbox->set_border_width (6);
	vbox->set_spacing (12);
	vbox->show_all ();

	add_button (_("Cancel"), Gtk::RESPONSE_CANCEL);
	add_button (_("OK"), Gtk::RESPONSE_OK);

	step_packer.set_spacing (12);
	pack_steps ();

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);

}

ScaleDialog::~ScaleDialog ()
{
}

void
ScaleDialog::set_tuning (ARDOUR::TuningSystem c)
{
	_tuning = c;
	tuning_dropdown.set_active ((int) c);
}

void
ScaleDialog::set (ARDOUR::MusicalKey const * key)
{
	using namespace ARDOUR;

	switch (_tuning) {
	case TwelveTone:
		we12tet_set (key);
		break;
	}
}

ARDOUR::MusicalKey*
ScaleDialog::get() const
{
	using namespace ARDOUR;

	switch (_tuning) {
	case TwelveTone:
		return we12tet_get ();
	}

	return nullptr;
}

void
ScaleDialog::we12tet_set (ARDOUR::MusicalKey const * key)
{
	std::cerr << "we12tet set\n";

	if (!key) {
		mode_dropdown.set_active (0);
		std::cerr << "a\n";
	} else {
		mode_dropdown.set_active (key->type() + 1);
		root_dropdown.set_active (key->root() + 4);
		std::cerr << "b, mode should show " << key->type() + 1 << " actual " << mode_dropdown.get_active_row_number()
		          << " root " << key->root() + 4 << " actual " << root_dropdown.get_active_row_number()
		          << std::endl;
	}

	_key = key;
	pack_steps ();
}

ARDOUR::MusicalKey*
ScaleDialog::we12tet_get() const
{
	std::string mode = mode_dropdown.get_active ();

	if (mode.empty()) {
		return nullptr;
	}

	int root_index = root_dropdown.get_active_row_number ();
	int root_midi_note = root_index - 4; /* A is first in list, but 0 is C */

	/* XXX this leaks. Probably need a "None" value for a MusicalKey */
	return new ARDOUR::MusicalKey (root_midi_note, mode);
}

void
ScaleDialog::fill_dropdowns (ARDOUR::TuningSystem tuning)
{
	using namespace Gtk::Menu_Helpers;
	using namespace ARDOUR;

	root_dropdown.clear_items ();
	mode_dropdown.clear_items ();

	switch (tuning) {
	case TwelveTone:
		std::cerr << "Filling 12TET dropdowns\n";
		root_dropdown.add_menu_elem (MenuElem (_("A")));
		root_dropdown.add_menu_elem (MenuElem (_("A#")));
		root_dropdown.add_menu_elem (MenuElem (_("B")));
		root_dropdown.add_menu_elem (MenuElem (_("C")));
		root_dropdown.add_menu_elem (MenuElem (_("C#")));
		root_dropdown.add_menu_elem (MenuElem (_("D")));
		root_dropdown.add_menu_elem (MenuElem (_("D#")));
		root_dropdown.add_menu_elem (MenuElem (_("E")));
		root_dropdown.add_menu_elem (MenuElem (_("F")));
		root_dropdown.add_menu_elem (MenuElem (_("F#")));
		root_dropdown.add_menu_elem (MenuElem (_("G")));
		root_dropdown.add_menu_elem (MenuElem (_("G#")));
		break;
	}

	for (auto const & [tune,scale]: MusicalMode::scales_by_tuning) {
		if (tune != tuning) {
			continue;
		}
		mode_dropdown.add_menu_elem (MenuElem (scale.name()));
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
ScaleDialog::set_type (ARDOUR::MusicalModeType t)
{
}
