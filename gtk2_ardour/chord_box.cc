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

#include "widgets/tooltips.h"

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/utils.h"

#include "editing_context.h"
#include "chord_box.h"
#include "chord_dialog.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

struct DoubleButton : public Gtk::HBox
{
	DoubleButton (ArdourWidgets::ArdourButton & left, ArdourWidgets::ArdourButton & right);
};

DoubleButton::DoubleButton (ArdourWidgets::ArdourButton & left, ArdourWidgets::ArdourButton & right)
{
	using namespace ArdourWidgets;

	left.set_corner_mask (ArdourButton::LEFT);
	left.set_border_mask (ArdourButton::HIDE_RIGHT);

	right.set_corner_mask (ArdourButton::RIGHT);
	right.set_border_mask (ArdourButton::HIDE_LEFT);

	pack_start (left, true, true);
	pack_start (right, false, false);
	left.show ();
	right.show ();
}


ChordBox::ChordBox (EditingContext& ec)
	: editing_context (ec)
	, arpeggiate_button (_("Arpeggiate"))
	, triad_label (_("3-Note Chords (Triads)"))
	, tetrad_label (_("4-Note Chords (Tetrads)"))
	, ext_label (_("Extended Chords"))
	, inversion_label (_("Inversions"))
	, drop_label (_("Drop Notes"))
	, _root (0)
	, _tuning (TwelveTone)
{
	using namespace Gtk;
	using namespace Menu_Helpers;
	using namespace ArdourWidgets;

	if (chord_info.empty()) {
		load_12tet_chords ();
	}

	/* these must match the enum decl order */
	tuning_button.add_menu_elem (MenuElem (_("Twelve Tone"), [this]() { set_tuning (TwelveTone); }));

	pack_start (tuning_button, false, false);
	/* Until this does something, don't show it */
	// tuning_button.show ();
	tuning_button.hide ();
	tuning_button.set_no_show_all ();
	tuning_button.set_active (0);

	set_border_width (12);
	set_spacing (6);

	EditingContext::ChordsChanged.connect (chord_connection, invalidator (*this), [&]() { refill_tables(); }, gui_context());
}

ChordBox::~ChordBox ()
{
}

void
ChordBox::set_tuning (TuningSystem tuning)
{
	if (tuning_button.get_active_row_number() != (int) tuning) {
		tuning_button.set_active ((int) tuning);
	}

	_tuning = tuning;

	switch (_tuning) {
	case TwelveTone:
		if (twelvetone_vbox.children().empty()) {
			build_twelvetone ();
		}
		pack (twelvetone_vbox);
		break;
	}
}


void
ChordBox::pack (Gtk::Widget& widget)
{
	if (twelvetone_vbox.get_parent()) {
		remove (twelvetone_vbox);
	}
	/* Other tuning boxes go here */

	pack_start (widget, false, false);
}

bool
ChordBox::radio_ardour_button_hack (GdkEventButton* ev, ArdourWidgets::ArdourButton* button)
{
	Glib::RefPtr<Gtk::Action> act = button->get_related_action ();
	if (act) {
		Glib::RefPtr<Gtk::RadioAction> ract = Glib::RefPtr<Gtk::RadioAction>::cast_dynamic (act);
		if (ract) {
			if (ract->get_active ()) {
				editing_context.no_chord_action()->activate ();
				return true;
			}
		}
	}
	return false;
}

void
ChordBox::refill_tables ()
{
	Gtkmm2ext::container_clear (triad_table);
	Gtkmm2ext::container_clear (tetrad_table);
	Gtkmm2ext::container_clear (ext_table);

	int tetrads = 0;
	int triads = 0;
	int exts = 0;

	for (auto & s : editing_context.chord_name_list()) {
		ChordInfo const * ci = ChordProvider::by_short_name (s);

		if (!ci) {
			continue;
		}

		if (ci->intervals.size() == 3) {
			triads++;
		} else if (ci->intervals.size() == 4) {
			tetrads++;
		} else {
			exts++;
		}
	}

	triad_table.resize ((triads + 1) / 2, 2);
	tetrad_table.resize ((tetrads + 1) / 2, 2);
	ext_table.resize ((exts + 1) / 2, 2);

	fill_table (triad_table, editing_context.chord_name_list(), 3);
	fill_table (tetrad_table, editing_context.chord_name_list(), 4);
	fill_table (ext_table, editing_context.chord_name_list(), -5);
}

void
ChordBox::fill_table (Gtk::Table& table, std::vector<std::string> const & names, int chord_size)
{
	using namespace Gtk;
	using namespace Gtkmm2ext;
	using namespace Menu_Helpers;
	using namespace ArdourWidgets;

	ArdourButton* butl;
	ArdourButton* butr;
	DoubleButton* dbut;
	int row = 0;
	int col = 0;
	std::vector<std::string>::size_type n = 0;

	for (auto & s : names) {

		ChordInfo const * ci = ChordProvider::by_short_name (s);

		if (!ci) {
			++n;
			continue;
		}

		if (chord_size < 1) {
			if ((int) ci->intervals.size() < -chord_size) {
				++n;
				continue;
			}
		} else if ((int) ci->intervals.size() != chord_size) {
			++n;
			continue;
		}

		butl = manage (new ArdourButton (s));
		butl->signal_button_press_event().connect ([this,n,chord_size](GdkEventButton* ev) { return tet12_edit_chord (ev, n, chord_size); }, false);
		butl->signal_clicked.connect ([this,s]() { tet12_replace_chord (s); });

		butr = manage (new ArdourButton);
		butr->set_icon (ArdourIcon::ToolDraw);
		butr->set_elements (ArdourButton::Element (ArdourButton::Body|ArdourButton::Edge|ArdourButton::VectorIcon));
		butr->set_active_color (UIConfiguration::instance().color ("alert:yellow"));
		/* Hack code to catch a click on an already active draw chord
		   button, and deactivate it instead, using the highly-specific
		   EditingContext::no_chord_action() as the new active action.
		*/
		butr->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &ChordBox::radio_ardour_button_hack), butr), false);

		if (n < names.size()) {

			Glib::RefPtr<RadioAction> ract;
			ract = editing_context.draw_chord_action (n);

			if (ract) {
				butr->set_related_action (ract);
			}
		}

		dbut = manage (new DoubleButton (*butl, *butr));
		dbut->show ();
		table.attach (*dbut, col, col+1, row, row+1);

		++n;
		++col;
		if (col % 2 == 0) {
			col = 0;
			++row;
		}
	}

	table.set_homogeneous (true);
	table.set_col_spacings (6);
}

void
ChordBox::build_twelvetone ()
{
	using namespace Gtk;
	using namespace Menu_Helpers;
	using namespace ArdourWidgets;

	inversion_table.resize (1, 2);
	drop_table.resize (2, 2);

	int row = 0;
	int col = 0;

	refill_tables ();

	/* Inversions */

	ArdourButton* but;

	but = manage (new ArdourButton);
	but->set_elements (ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text));
	but->set_text (_("Move Up"));
	but->signal_clicked.connect ([this]() { tet12_invert_chord (true); });
	ArdourWidgets::set_tooltip (*but, _("Move the lowest pitch in the selected chord up by 1 octave"));
	inversion_table.attach (*but, col, col+1, row, row+1);
	col++;
	but = manage (new ArdourButton);
	but->set_elements (ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text));
	but->set_text (_("Move Down"));
	but->signal_clicked.connect ([this]() { tet12_invert_chord (false); });
	ArdourWidgets::set_tooltip (*but, _("Move the highest pitch in the selected chord down by 1 octave"));
	inversion_table.attach (*but, col, col+1, row, row+1);
	col = 0;
	row++;

	inversion_table.set_homogeneous (true);
	inversion_table.set_col_spacings (6);

	/* Drops */

	row = 0;
	col = 0;

	but = manage (new ArdourButton);
	but->set_elements (ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text));
	but->set_text (_("Drop 2"));
	ArdourWidgets::set_tooltip (*but, _("Move the 2nd lowest pitch in the selected chord down by 1 octave"));
	but->signal_clicked.connect ([this]() { tet12_drop_chord ({ 1 }); });
	drop_table.attach (*but, col, col+1, row, row+1);
	col++;
	but = manage (new ArdourButton);
	but->set_elements (ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text));
	but->set_text (_("Drop 3"));
	ArdourWidgets::set_tooltip (*but, _("Move the 3rd lowest pitch in the selected chord down by 1 octave"));
	but->signal_clicked.connect ([this]() { tet12_drop_chord ({ 2 }); });
	drop_table.attach (*but, col, col+1, row, row+1);
	col = 0;
	row++;
	but = manage (new ArdourButton);
	but->set_elements (ArdourButton::Element (ArdourButton::Edge|ArdourButton::Body|ArdourButton::Text));
	but->set_text (_("Drop 2 + 4"));
	ArdourWidgets::set_tooltip (*but, _("Move the 2nd &amp; 4th lowest pitches in the selected chord down by 1 octave (tetrads only)"));
	but->signal_clicked.connect ([this]() { tet12_drop_chord ({ 1, 3 }); });
	drop_table.attach (*but, col, col+2, row, row+1);
	col = 0;
	row++;

	drop_table.set_homogeneous (true);
	drop_table.set_col_spacings (6);


	triad_label.set_alignment (0.0, 0.5);
	tetrad_label.set_alignment (0.0, 0.5);
	ext_label.set_alignment (0.0, 0.5);
	inversion_label.set_alignment (0.0, 0.5);
	drop_label.set_alignment (0.0, 0.5);

	name_display.modify_font (UIConfiguration::instance().get_BigBoldFont());

	twelvetone_vbox.pack_start (name_display, false, false);
	twelvetone_vbox.pack_start (triad_label, false, false);
	twelvetone_vbox.pack_start (triad_table, false, false);
	twelvetone_vbox.pack_start (tetrad_label, false, false);
	twelvetone_vbox.pack_start (tetrad_table, false, false);
	twelvetone_vbox.pack_start (ext_label, false, false);
	twelvetone_vbox.pack_start (ext_table, false, false);
	twelvetone_vbox.pack_start (inversion_label, false, false);
	twelvetone_vbox.pack_start (inversion_table, false, false);
	twelvetone_vbox.pack_start (drop_label, false, false);
	twelvetone_vbox.pack_start (drop_table, false, false);
	twelvetone_vbox.pack_start (arpeggiate_button, false, false);

	twelvetone_vbox.show_all ();
	twelvetone_vbox.set_spacing (6);

	pack_start (twelvetone_vbox);
}

void
ChordBox::show_chord (std::string const & name)
{
	name_display.set_text (name);
}

bool
ChordBox::get_midi_chord (int root_pitch, std::vector<int>& pitches, bool& arpeggiate) const
{
	std::string const & chord_name = editing_context.draw_chord_name ();
	if (chord_name.empty()) {
		return false;
	}

	arpeggiate = arpeggiate_button.get_active ();

	for (auto const & ci : chord_info) {
		if (ci.short_name == chord_name) {
			for (auto & interval : ci.intervals) {
				pitches.push_back (root_pitch + interval);
			}
			return true;
		}
	}

	return false;
}

void
ChordBox::tet12_replace_chord (std::string const & name)
{
	for (auto const & ci : chord_info) {
		if (ci.short_name == name) {
			ReplaceChord (ci.intervals); /* EMIT SIGNAL */
			return;
		}
	}
}

bool
ChordBox::tet12_edit_chord (GdkEventButton* ev, size_t n, int chord_size)
{
	using namespace Gtk;
	using namespace Gtkmm2ext;

	if (!Keyboard::is_context_menu_event (ev) &&
	    (ev->type != GDK_2BUTTON_PRESS || ev->button != 1)) {
		return false;
	}

	ChordDialog cd (editing_context, *this, chord_size);
	cd.present ();

	switch (cd.run()) {
	case RESPONSE_OK:
		break;
	default:
		return true;
	}

	ChordInfo ci (cd.get_chord ());

	if (cd.is_protected()) {
		EditingContext::change_chord_list (n, ci.short_name);
	} else {
		ChordProvider::add_chord (ci);
	}

	return true;
}


void
ChordBox::tet12_invert_chord (bool up)
{
	InvertChord (up); /* EMIT SIGNAL */
}

void
ChordBox::tet12_drop_chord (std::vector<int> const & which_notes)
{
	DropChord (which_notes);
}
