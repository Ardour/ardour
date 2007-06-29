#include <vector>

#include <gtkmm/stock.h>
#include <gtkmm2ext/utils.h>

#include "engine_dialog.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

EngineDialog::EngineDialog ()
	: ArdourDialog (_("Audio Engine"), false, true),
	  realtime_button (_("Realtime")),
	  no_memory_lock_button (_("Do not lock memory")),
	  unlock_memory_button (_("Unlock memory")),
	  soft_mode_button (_("No zombies")),
	  monitor_button (_("Monitor ports")),
	  force16bit_button (_("Force 16 bit")),
	  hw_monitor_button (_("H/W monitoring")),
	  hw_meter_button (_("H/W metering")),
	  verbose_output_button (_("Verbose output")),
	  basic_packer (2, 2),
	  options_packer (11, 2),
	  device_packer (10, 2)
{
	using namespace Notebook_Helpers;
	Label* label;

	vector<string> strings;

	strings.push_back (_("8000Hz"));
	strings.push_back (_("22050Hz"));
	strings.push_back (_("44100Hz"));
	strings.push_back (_("48000Hz"));
	strings.push_back (_("88200Hz"));
	strings.push_back (_("96000Hz"));
	strings.push_back (_("192000Hz"));
	set_popdown_strings (sample_rate_combo, strings);

	strings.clear ();
	strings.push_back ("32");
	strings.push_back ("64");
	strings.push_back ("128");
	strings.push_back ("256");
	strings.push_back ("512");
	strings.push_back ("1024");
	strings.push_back ("2048");
	strings.push_back ("4096");
	strings.push_back ("8192");
	set_popdown_strings (period_size_combo, strings);

	/* parameters */

	basic_packer.set_spacings (6);

	label = manage (new Label (_("Sample Rate")));
	basic_packer.attach (*label, 0, 1, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (sample_rate_combo, 1, 2, 0, 1, FILL|EXPAND, (AttachOptions) 0);

	label = manage (new Label (_("Period Size")));
	basic_packer.attach (*label, 0, 1, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	basic_packer.attach (period_size_combo, 1, 2, 1, 2, FILL|EXPAND, (AttachOptions) 0);


	/* options */

	options_packer.attach (realtime_button, 0, 1, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (no_memory_lock_button, 0, 1, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (unlock_memory_button, 0, 1, 2, 3, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (monitor_button, 0, 1, 3, 4, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (soft_mode_button, 0, 1, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (force16bit_button, 0, 1, 5, 6, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (hw_monitor_button, 0, 1, 6, 7, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (hw_meter_button, 0, 1, 7, 8, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (verbose_output_button, 0, 1, 8, 9, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (priority_spinner, 0, 1, 9, 10, FILL|EXPAND, (AttachOptions) 0);
	options_packer.attach (periods_spinner, 0, 1, 10, 11, FILL|EXPAND, (AttachOptions) 0);

	/* device */

	device_packer.set_spacings (6);

	strings.clear ();
#ifndef __APPLE
	strings.push_back (X_("ALSA"));
	strings.push_back (X_("OSS"));
	strings.push_back (X_("FFADO"));
#else
	strings.push_back (X_("CoreAudio"));
#endif
	strings.push_back (X_("NetJACK"));
	strings.push_back (X_("Dummy"));
	set_popdown_strings (driver_combo, strings);
	driver_combo.set_active_text (strings.front());

	strings.clear ();
	strings.push_back (_("Duplex"));
	strings.push_back (_("Playback only"));
	strings.push_back (_("Capture only"));
	set_popdown_strings (audio_mode_combo, strings);
	audio_mode_combo.set_active_text (strings.front());

	label = manage (new Label (_("Driver")));
	device_packer.attach (*label, 0, 1, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (driver_combo, 1, 2, 0, 1, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Interface")));
	device_packer.attach (*label, 0, 1, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (interface_combo, 1, 2, 1, 2, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Audio Mode")));
	device_packer.attach (*label, 0, 1, 2, 3, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (audio_mode_combo, 1, 2, 2, 3, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Input device")));
	device_packer.attach (*label, 0, 1, 3, 4, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_device_combo, 1, 2, 3, 4, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Output device")));
	device_packer.attach (*label, 0, 1, 4, 5, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_device_combo, 1, 2, 4, 5, FILL|EXPAND, (AttachOptions) 0);	
	label = manage (new Label (_("Input channels")));
	device_packer.attach (*label, 0, 1, 5, 6, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_channels, 1, 2, 5, 6, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Output channels")));
	device_packer.attach (*label, 0, 1, 6, 7, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_channels, 1, 2, 6, 7, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Input latency")));
	device_packer.attach (*label, 0, 1, 7, 8, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (input_latency, 1, 2, 7, 8, FILL|EXPAND, (AttachOptions) 0);
	label = manage (new Label (_("Output latency")));
	device_packer.attach (*label, 0, 1, 8, 9, FILL|EXPAND, (AttachOptions) 0);
	device_packer.attach (output_latency, 1, 2, 8, 9, FILL|EXPAND, (AttachOptions) 0);

	notebook.pages().push_back (TabElem (basic_packer, _("Parameters")));
	notebook.pages().push_back (TabElem (options_packer, _("Options")));
	notebook.pages().push_back (TabElem (device_packer, _("Device")));

	get_vbox()->set_border_width (12);
	get_vbox()->pack_start (notebook);

	add_button (Stock::OK, RESPONSE_ACCEPT);
}

EngineDialog::~EngineDialog ()
{

}
