#ifndef __bb_gui_h__
#define __bb_gui_h__

#include <gtkmm.h>
#include <jack/jack.h>

class BeatBox;

class BBGUI {
  public:
	BBGUI (int*, char** [], jack_client_t* jack, BeatBox* bb);
	~BBGUI ();

	void run ();

  private:
	jack_client_t* jack;
	BeatBox* bbox;
	Gtk::Main main;
	Gtk::Window window;

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

	Gtk::VBox global_vbox;
	Gtk::VBox quantize_button_box;
	Gtk::HBox misc_button_box;


	void set_quantize (int divisor);
	void toggle_play ();
	void clear ();
	void tempo_changed ();
};

#endif /* __bb_gui_h__ */
