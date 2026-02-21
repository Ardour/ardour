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

#include "scale_dialog.h"

#include "gtkmm2ext/utils.h"

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
		{ _("Ratio steps"), ARDOUR::RatioSteps },
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
	, _key (440.0, ARDOUR::MusicalMode (ARDOUR::MusicalMode::IonianMajor))
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

	using namespace Gtk::Menu_Helpers;

	culture_dropdown.add_menu_elem (MenuElem (_("Western Europe (12TET)"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::fill_dropdowns), ARDOUR::WesternEurope12TET)));
	culture_dropdown.add_menu_elem (MenuElem (_("Byzantine"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::fill_dropdowns), ARDOUR::Byzantine)));
	culture_dropdown.add_menu_elem (MenuElem (_("Maqams"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::fill_dropdowns), ARDOUR::Maqams)));
	culture_dropdown.add_menu_elem (MenuElem (_("Hindustani"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::fill_dropdowns), ARDOUR::Hindustani)));
	culture_dropdown.add_menu_elem (MenuElem (_("Carnatic"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::fill_dropdowns), ARDOUR::Carnatic)));
	culture_dropdown.add_menu_elem (MenuElem (_("SE Asian Archipelago"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::fill_dropdowns), ARDOUR::SEAsia)));
	culture_dropdown.add_menu_elem (MenuElem (_("China"), sigc::bind (sigc::mem_fun (*this, &ScaleDialog::fill_dropdowns), ARDOUR::China)));
	culture_dropdown.set_active (0);
	fill_dropdowns (ARDOUR::WesternEurope12TET);

	root_mode_box.pack_start (root_dropdown, true, false);
	root_mode_box.pack_start (mode_dropdown, true, false);

	named_scale_box.pack_start (culture_dropdown, false, false);
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

	step_packer.set_spacing (12);
	pack_steps ();
}

ScaleDialog::~ScaleDialog ()
{
}

void
ScaleDialog ::set (ARDOUR::MusicalKey const & key)
{
	_key = key;

	pack_steps ();
}

ARDOUR::MusicalKey
ScaleDialog::get() const
{
	return ARDOUR::MusicalKey (1.0, ARDOUR::MusicalMode (ARDOUR::MusicalMode::Dorian));
}

void
ScaleDialog::fill_dropdowns (ARDOUR::MusicalModeCulture culture)
{
	using namespace Gtk::Menu_Helpers;
	using namespace ARDOUR;

	root_dropdown.clear_items ();
	mode_dropdown.clear_items ();

	culture_dropdown.set_active ((int) culture);

	switch (culture) {
	case WesternEurope12TET:
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

		/* Must match enum order */

		mode_dropdown.add_menu_elem (MenuElem (_("Major (Ionian)")));
		mode_dropdown.add_menu_elem (MenuElem (_("Minor (Aeolian)")));
		mode_dropdown.add_menu_elem (MenuElem (_("Dorian")));
		mode_dropdown.add_menu_elem (MenuElem (_("Harmonic Minor")));
		mode_dropdown.add_menu_elem (MenuElem (_("Melodic Minor Ascending")));
		mode_dropdown.add_menu_elem (MenuElem (_("Melodic Minor Descending")));
		mode_dropdown.add_menu_elem (MenuElem (_("Phrygian")));
		mode_dropdown.add_menu_elem (MenuElem (_("Lydian")));
		mode_dropdown.add_menu_elem (MenuElem (_("Mixolydian")));
		mode_dropdown.add_menu_elem (MenuElem (_("Locrian")));
		mode_dropdown.add_menu_elem (MenuElem (_("Pentatonic Major")));
		mode_dropdown.add_menu_elem (MenuElem (_("Pentatonic Minor")));
		mode_dropdown.add_menu_elem (MenuElem (_("Chromatic")));
		mode_dropdown.add_menu_elem (MenuElem (_("Blues")));
		mode_dropdown.add_menu_elem (MenuElem (_("Neapolitan Minor")));
		mode_dropdown.add_menu_elem (MenuElem (_("Neapolitan Major")));
		mode_dropdown.add_menu_elem (MenuElem (_("Oriental")));
		mode_dropdown.add_menu_elem (MenuElem (_("Double Harmonic")));
		mode_dropdown.add_menu_elem (MenuElem (_("Enigmatic")));
		mode_dropdown.add_menu_elem (MenuElem (_("Hungarian Minor")));
		mode_dropdown.add_menu_elem (MenuElem (_("Hungarian Major")));
		mode_dropdown.add_menu_elem (MenuElem (_("Spanish 8 Tone")));
		mode_dropdown.add_menu_elem (MenuElem (_("Hungarian Gypsy")));
		mode_dropdown.add_menu_elem (MenuElem (_("Overtone")));
		mode_dropdown.add_menu_elem (MenuElem (_("Leading Whole Tone")));

		root_dropdown.set_active (0);
		mode_dropdown.set_active (0);
		break;
	case Byzantine:
		break;
	case Maqams:
		break;
	case Hindustani:
		break;
	case Carnatic:
		break;
	case SEAsia:
		break;
	case China:
		break;
	}
}

void
ScaleDialog::pack_steps ()
{
	Gtkmm2ext::container_clear (step_packer);

	for (int n = 0; n < _key.size(); ++n) {
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
