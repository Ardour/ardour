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
	using namespace Gtk::Menu_Helpers;

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
	pack ();
}

ScaleDialog::~ScaleDialog ()
{
}

void
ScaleDialog ::set (ARDOUR::MusicalKey const & key)
{
	_key = key;

	pack ();
}

ARDOUR::MusicalKey
ScaleDialog::get() const
{
	return ARDOUR::MusicalKey (1.0, ARDOUR::MusicalMode (ARDOUR::MusicalMode::Dorian));
}

void
ScaleDialog::pack ()
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
