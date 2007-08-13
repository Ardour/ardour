#ifndef __gtk2_ardour_engine_dialog_h__
#define __gtk2_ardour_engine_dialog_h__

#include <map>
#include <vector>
#include <string>

#include <gtkmm/checkbutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/notebook.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/table.h>
#include <gtkmm/expander.h>
#include <gtkmm/box.h>

#include "ardour_dialog.h"

class EngineDialog : public ArdourDialog {
  public:
	EngineDialog ();
	~EngineDialog ();

	static bool engine_running ();

  private:
	Gtk::Adjustment periods_adjustment;
	Gtk::SpinButton periods_spinner;
	Gtk::Adjustment priority_adjustment;
	Gtk::SpinButton priority_spinner;
	Gtk::Adjustment ports_adjustment;
	Gtk::SpinButton ports_spinner;
	Gtk::SpinButton input_channels;
	Gtk::SpinButton output_channels;
	Gtk::SpinButton input_latency;
	Gtk::SpinButton output_latency;

	Gtk::CheckButton realtime_button;
	Gtk::CheckButton no_memory_lock_button;
	Gtk::CheckButton unlock_memory_button;
	Gtk::CheckButton soft_mode_button;
	Gtk::CheckButton monitor_button;
	Gtk::CheckButton force16bit_button;
	Gtk::CheckButton hw_monitor_button;
	Gtk::CheckButton hw_meter_button;
	Gtk::CheckButton verbose_output_button;

	Gtk::ComboBoxText sample_rate_combo;
	Gtk::ComboBoxText period_size_combo;

	Gtk::ComboBoxText preset_combo;
	Gtk::ComboBoxText serverpath_combo;
	Gtk::ComboBoxText driver_combo;
	Gtk::ComboBoxText interface_combo;
	Gtk::ComboBoxText timeout_combo;
	Gtk::ComboBoxText dither_mode_combo;
	Gtk::ComboBoxText audio_mode_combo;
	Gtk::ComboBoxText input_device_combo;
	Gtk::ComboBoxText output_device_combo;

	Gtk::Table basic_packer;
	Gtk::Table options_packer;
	Gtk::Table device_packer;

	Gtk::Notebook notebook;

	Gtk::Button* start_button;
	Gtk::Button* stop_button;

	void realtime_changed ();
	void driver_changed ();

	void build_command_line (std::vector<std::string>&);
	void start_engine ();
	void stop_engine ();
	Glib::Pid engine_pid;
	int engine_stdin;
	int engine_stdout;
	int engine_stderr;

	std::map<std::string,std::vector<std::string> > devices;
	void enumerate_devices ();
#ifdef __APPLE
	std::vector<std::string> enumerate_coreaudio_devices ();
#else
	std::vector<std::string> enumerate_alsa_devices ();
	std::vector<std::string> enumerate_oss_devices ();
	std::vector<std::string> enumerate_netjack_devices ();
	std::vector<std::string> enumerate_ffado_devices ();
	std::vector<std::string> enumerate_dummy_devices ();
#endif	
};

#endif /* __gtk2_ardour_engine_dialog_h__ */
