/*
    Copyright (C) 2017 Paul Davis

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

#ifndef __gtk2_ardour_beatbox_gui_h__
#define __gtk2_ardour_beatbox_gui_h__

#include <string>
#include <boost/shared_ptr.hpp>

#include <gtkmm/radiobutton.h>
#include <gtkmm/togglebutton.h>
#include <gtkmm/button.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/box.h>
#include <gtkmm/notebook.h>

#include "gtkmm2ext/colors.h"

#include "canvas/canvas.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "ardour_dialog.h"

namespace ArdourCanvas {
class Grid;
class Item;
class Rectangle;
class StepButton;
class Text;
class VBox;
class Widget;
}

namespace ARDOUR {
class BeatBox;
}

class BBGUI : public ArdourDialog {
  public:
	BBGUI (boost::shared_ptr<ARDOUR::BeatBox> bb);
	~BBGUI ();

  protected:
	void on_map ();
	void on_unmap ();

  private:
	friend class SwitchRow;
	boost::shared_ptr<ARDOUR::BeatBox> bbox;

	ArdourCanvas::GtkCanvas switch_canvas;
	ArdourCanvas::GtkCanvas pad_canvas;
	ArdourCanvas::GtkCanvas roll_canvas;

	ArdourCanvas::Grid* pad_grid;

	ArdourWidgets::ArdourButton step_sequencer_tab_button;
	ArdourWidgets::ArdourButton pad_tab_button;
	ArdourWidgets::ArdourButton roll_tab_button;

	sigc::connection timer_connection;

	struct Pad {
		Pad (ArdourCanvas::Canvas* canvas, int x, int y, int note, std::string const & txt);
		void set_color (Gtkmm2ext::Color);

		ArdourCanvas::StepButton* button;

		static int pad_width;
		static int pad_height;
		static int pad_spacing;

		int row() const  { return _row; }
		int col() const  { return _col; }
		int note() const { return _note; }
		int velocity() const;

           private:
		int _row;
		int _col;
		int _note;
		std::string _label;
	};

	typedef std::vector<Pad*> Pads;
	Pads pads;
	int pad_rows;
	int pad_cols;


	struct Switch {
		Switch (ArdourCanvas::Canvas*, int x, int y, int note, Gtkmm2ext::Color, std::string const & txt);
		void set_color (Gtkmm2ext::Color);

		bool is_on () const { return _on; }
		bool is_off () const { return !_on; }
		bool is_flashed() const { return _flashed; }

		void on ();
		void off ();
		void flash_on ();
		void flash_off ();

		ArdourCanvas::StepButton* button;

		static int switch_width;
		static int switch_height;
		static int switch_spacing;

		int row() const  { return _row; }
		int col() const  { return _col; }
		int note() const { return _note; }
           private:
		int _row;
		int _col;
		int _note;
		std::string _label;
		Gtkmm2ext::HSV hsv;
		bool _on;
		bool _flashed;
	};

	typedef std::vector<Switch*> Switches;

	struct SwitchRow {
		BBGUI& owner;
		int row;
		int note;
		ArdourCanvas::Grid* switch_grid;
		Switches switches;
		ArdourWidgets::ArdourButton* clear_row_button;
		ArdourWidgets::ArdourDropdown* row_note_button;
		ArdourCanvas::Widget* clear_row_item;
		ArdourCanvas::Widget* row_note_item;

		SwitchRow (BBGUI&, ArdourCanvas::Item*, int row, int cols);
		~SwitchRow ();

		void set_note (int note_number);
		void update (int current_row);

            private:
		void resize (int cols);
		void drop_switches ();
		bool switch_event (GdkEvent*, int col);
		std::string print_midi_note (int note);
	};

	typedef std::vector<SwitchRow*> SwitchRows;

	ArdourCanvas::VBox* switch_vbox;

	SwitchRows switch_rows;
	int switch_cols;

	ArdourCanvas::Item* add_row_button;

	void size_switches (int cols, int rows);

	Gtk::Notebook tabs;

	ArdourWidgets::ArdourButton export_as_region_button;
	void export_as_region ();

	Gtk::RadioButtonGroup quantize_group;
	Gtk::RadioButton quantize_off;
	Gtk::RadioButton quantize_32nd;
	Gtk::RadioButton quantize_16th;
	Gtk::RadioButton quantize_8th;
	Gtk::RadioButton quantize_quarter;
	Gtk::RadioButton quantize_half;
	Gtk::RadioButton quantize_whole;

	Gtk::ToggleButton play_button;
	Gtk::Button clear_button;

	Gtk::Adjustment tempo_adjustment;
	Gtk::SpinButton tempo_spinner;

	Gtk::VBox quantize_button_box;
	Gtk::HBox misc_button_box;


	void set_quantize (int divisor);
	void toggle_play ();
	void clear ();
	void tempo_changed ();

	void setup_switch_canvas ();
	void setup_pad_canvas ();
	void setup_roll_canvas ();

	void size_pads (int cols, int rows);

	void switch_tabs (Gtk::Widget*);
	void pads_off ();
	void switches_off ();
	void update ();
	void update_pads ();
	void update_steps ();
	void update_roll ();

	bool pad_event (GdkEvent*, int col, int row);

};

#endif /* __gtk2_ardour_beatbox_gui_h__ */
