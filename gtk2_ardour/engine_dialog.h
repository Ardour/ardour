#ifndef __gtk2_ardour_engine_dialog_h__
#define __gtk2_ardour_engine_dialog_h__

#include <gtkmm/checkbutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/notebook.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/table.h>

#include "ardour_dialog.h"

class EngineDialog : public ArdourDialog {
  public:
	EngineDialog ();
	~EngineDialog ();

  private:
	Gtk::CheckButton realtime_button;
	Gtk::CheckButton no_memory_lock_button;
	Gtk::CheckButton unlock_memory_button;
	Gtk::CheckButton soft_mode_button;
	Gtk::CheckButton monitor_button;
	Gtk::CheckButton force16bit_button;
	Gtk::CheckButton hw_monitor_button;
	Gtk::CheckButton hw_meter_button;
	Gtk::CheckButton verbose_output_button;
	
	Gtk::SpinButton priority_spinner;
	Gtk::SpinButton periods_spinner;
	Gtk::SpinButton input_channels;
	Gtk::SpinButton output_channels;
	Gtk::SpinButton input_latency;
	Gtk::SpinButton output_latency;

	Gtk::ComboBoxText sample_rate_combo;
	Gtk::ComboBoxText period_size_combo;

	Gtk::ComboBoxText preset_combo;
	Gtk::ComboBoxText serverpath_combo;
	Gtk::ComboBoxText driver_combo;
	Gtk::ComboBoxText interface_combo;
	Gtk::ComboBoxText port_maximum_combo;
	Gtk::ComboBoxText timeout_combo;
	Gtk::ComboBoxText dither_mode_combo;
	Gtk::ComboBoxText audio_mode_combo;
	Gtk::ComboBoxText input_device_combo;
	Gtk::ComboBoxText output_device_combo;

	Gtk::Table basic_packer;
	Gtk::Table options_packer;
	Gtk::Table device_packer;

	Gtk::Notebook notebook;
};

#endif /* __gtk2_ardour_engine_dialog_h__ */
