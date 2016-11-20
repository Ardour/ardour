/*
    Copyright (C) 2000 Paul Davis

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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <climits>
#include <cerrno>
#include <cmath>
#include <string>
#include <vector>

#include "pbd/stl_delete.h"
#include "pbd/unwind.h"
#include "pbd/xml++.h"
#include "pbd/failed_constructor.h"

#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/barcontroller.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/slider_controller.h>

#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"
#include "ardour/value_as_string.h"

#include "prompter.h"
#include "plugin_ui.h"
#include "gui_thread.h"
#include "automation_controller.h"
#include "ardour_knob.h"
#include "gain_meter.h"
#include "timers.h"
#include "tooltips.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace ARDOUR_UI_UTILS;

GenericPluginUI::GenericPluginUI (boost::shared_ptr<PluginInsert> pi, bool scrollable)
	: PlugUIBase (pi)
	, automation_menu (0)
	, is_scrollable(scrollable)
{
	set_name ("PluginEditor");
	set_border_width (10);
	//set_homogeneous (false);

	pack_start (main_contents, true, true);
	settings_box.set_homogeneous (false);

	HBox* constraint_hbox = manage (new HBox);
	HBox* smaller_hbox = manage (new HBox);
	HBox* automation_hbox = manage (new HBox);
	smaller_hbox->set_spacing (4);
	automation_hbox->set_spacing (6);
	Label* combo_label = manage (new Label (_("<span size=\"large\">Presets</span>")));
	combo_label->set_use_markup (true);

	latency_button.signal_clicked.connect (sigc::mem_fun (*this, &PlugUIBase::latency_button_clicked));
	set_latency_label ();

	smaller_hbox->pack_start (latency_button, false, false, 4);
	smaller_hbox->pack_start (pin_management_button, false, false, 4);
	smaller_hbox->pack_start (_preset_combo, false, false);
	smaller_hbox->pack_start (_preset_modified, false, false);
	smaller_hbox->pack_start (add_button, false, false);
	smaller_hbox->pack_start (save_button, false, false);
	smaller_hbox->pack_start (delete_button, false, false);
	smaller_hbox->pack_start (reset_button, false, false, 4);
	smaller_hbox->pack_start (bypass_button, false, true, 4);

	automation_manual_all_button.set_text(_("Manual"));
	automation_manual_all_button.set_name (X_("generic button"));
	automation_play_all_button.set_text(_("Play"));
	automation_play_all_button.set_name (X_("generic button"));
	automation_write_all_button.set_text(_("Write"));
	automation_write_all_button.set_name (X_("generic button"));
	automation_touch_all_button.set_text(_("Touch"));
	automation_touch_all_button.set_name (X_("generic button"));

	Label* l = manage (new Label (_("All Automation")));
	l->set_alignment (1.0, 0.5);
	automation_hbox->pack_start (*l, true, true);
	automation_hbox->pack_start (automation_manual_all_button, false, false);
	automation_hbox->pack_start (automation_play_all_button, false, false);
	automation_hbox->pack_start (automation_write_all_button, false, false);
	automation_hbox->pack_start (automation_touch_all_button, false, false);

	constraint_hbox->set_spacing (5);
	constraint_hbox->set_homogeneous (false);

	VBox* v1_box = manage (new VBox);
	VBox* v2_box = manage (new VBox);
	pack_end (plugin_analysis_expander, false, false);
	if (!plugin->get_docs().empty()) {
		pack_end (description_expander, false, false);
	}

	v1_box->set_spacing (6);
	v1_box->pack_start (*smaller_hbox, false, true);
	v1_box->pack_start (*automation_hbox, false, true);
	v2_box->pack_start (focus_button, false, true);

	main_contents.pack_start (settings_box, false, false);

	constraint_hbox->pack_end (*v2_box, false, false);
	constraint_hbox->pack_end (*v1_box, false, false);

	main_contents.pack_start (*constraint_hbox, false, false);

	if (is_scrollable) {
		Gtk::ScrolledWindow *scroller = manage (new Gtk::ScrolledWindow());
		scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
		scroller->set_name ("PluginEditor");
		scroller->add (hpacker);
		main_contents.pack_start (*scroller, true, true);
	} else {
		main_contents.pack_start (hpacker, false, false);
	}

	pi->ActiveChanged.connect (active_connection, invalidator (*this), boost::bind (&GenericPluginUI::processor_active_changed, this, boost::weak_ptr<Processor>(pi)), gui_context());

	bypass_button.set_active (!pi->enabled());

	prefheight = 0;
	build ();

	/* Listen for property changes that are not notified normally because
	 * AutomationControl has only support for numeric values currently.
	 * The only case is Variant::PATH for now */
	plugin->PropertyChanged.connect(*this, invalidator(*this),
	                                boost::bind(&GenericPluginUI::path_property_changed, this, _1, _2),
	                                gui_context());

	main_contents.show ();
}

GenericPluginUI::~GenericPluginUI ()
{
	if (output_controls.size() > 0) {
		screen_update_connection.disconnect();
	}
}

// Some functions for calculating the 'similarity' of two plugin
// control labels.

static int get_number(string label) {
static const char *digits = "0123456789";
int value = -1;

	std::size_t first_digit_pos = label.find_first_of(digits);
	if (first_digit_pos != string::npos) {
		// found some digits: there's a number in there somewhere
		stringstream s;
		s << label.substr(first_digit_pos);
		s >> value;
	}
	return value;
}

static int match_or_digit(char c1, char c2) {
	return c1 == c2 || (isdigit(c1) && isdigit(c2));
}

static std::size_t matching_chars_at_head(const string s1, const string s2) {
std::size_t length, n = 0;

	length = min(s1.length(), s2.length());
	while (n < length) {
		if (!match_or_digit(s1[n], s2[n]))
			break;
		n++;
	}
	return n;
}

static std::size_t matching_chars_at_tail(const string s1, const string s2) {
std::size_t s1pos, s2pos, n = 0;

	s1pos = s1.length();
	s2pos = s2.length();
	while (s1pos-- > 0 && s2pos-- > 0) {
		if (!match_or_digit(s1[s1pos], s2[s2pos])	)
			break;
		n++;
	}
	return n;
}

static const guint32 min_controls_per_column = 17, max_controls_per_column = 24;
static const float default_similarity_threshold = 0.3;

void
GenericPluginUI::build ()
{
	std::vector<ControlUI *> control_uis;
	bool grid = plugin->parameter_count() > 0;

	// Build a ControlUI for each control port
	for (size_t i = 0; i < plugin->parameter_count(); ++i) {

		if (plugin->parameter_is_control (i)) {

			/* Don't show latency control ports */

			const Evoral::Parameter param(PluginAutomation, 0, i);
			if (plugin->describe_parameter (param) == X_("latency")) {
				continue;
			}

			if (plugin->describe_parameter (param) == X_("hidden")) {
				continue;
			}

			const float value = plugin->get_parameter(i);

			ControlUI* cui;
			Plugin::UILayoutHint hint;

			if (!plugin->get_layout(i, hint)) {
				grid = false;
			}

			boost::shared_ptr<ARDOUR::AutomationControl> c
				= boost::dynamic_pointer_cast<ARDOUR::AutomationControl>(
					insert->control(param));

			ParameterDescriptor desc;
			plugin->get_parameter_descriptor(i, desc);
			if ((cui = build_control_ui (param, desc, c, value, plugin->parameter_is_input(i), hint.knob)) == 0) {
				error << string_compose(_("Plugin Editor: could not build control element for port %1"), i) << endmsg;
				continue;
			}

			if (grid) {
				cui->x0 = hint.x0;
				cui->x1 = hint.x1;
				cui->y0 = hint.y0;
				cui->y1 = hint.y1;
			}

			const std::string param_docs = plugin->get_parameter_docs(i);
			if (!param_docs.empty()) {
				set_tooltip(cui, param_docs.c_str());
			}

			control_uis.push_back(cui);
		}
	}

	// Build a ControlUI for each property
	const Plugin::PropertyDescriptors& descs = plugin->get_supported_properties();
	for (Plugin::PropertyDescriptors::const_iterator d = descs.begin(); d != descs.end(); ++d) {
		const ParameterDescriptor& desc = d->second;
		const Evoral::Parameter    param(PluginPropertyAutomation, 0, desc.key);

		boost::shared_ptr<ARDOUR::AutomationControl> c
			= boost::dynamic_pointer_cast<ARDOUR::AutomationControl>(
				insert->control(param));

		if (!c) {
			error << string_compose(_("Plugin Editor: no control for property %1"), desc.key) << endmsg;
			continue;
		}

		ControlUI* cui = build_control_ui(param, desc, c, c->get_value(), true);
		if (!cui) {
			error << string_compose(_("Plugin Editor: could not build control element for property %1"),
			                        desc.key) << endmsg;
			continue;
		}

		control_uis.push_back(cui);
	}
	if (!descs.empty()) {
		plugin->announce_property_values();
	}

	if (grid) {
		custom_layout (control_uis);
	} else {
		automatic_layout (control_uis);
	}

	output_update ();

	automation_manual_all_button.signal_clicked.connect(sigc::bind (sigc::mem_fun (*this, &GenericPluginUI::set_all_automation), ARDOUR::Off));
	automation_play_all_button.signal_clicked.connect(sigc::bind (sigc::mem_fun (*this, &GenericPluginUI::set_all_automation), ARDOUR::Play));
	automation_write_all_button.signal_clicked.connect(sigc::bind (sigc::mem_fun (*this, &GenericPluginUI::set_all_automation), ARDOUR::Write));
	automation_touch_all_button.signal_clicked.connect(sigc::bind (sigc::mem_fun (*this, &GenericPluginUI::set_all_automation), ARDOUR::Touch));

	/* XXX This is a workaround for AutomationControl not knowing about preset loads */
	plugin->PresetLoaded.connect (*this, invalidator (*this), boost::bind (&GenericPluginUI::update_input_displays, this), gui_context ());
}


void
GenericPluginUI::automatic_layout (const std::vector<ControlUI*>& control_uis)
{
	guint32 x = 0;

	static const int32_t initial_button_rows = 12;
	static const int32_t initial_button_cols = 1;
	static const int32_t initial_output_rows = 1;
	static const int32_t initial_output_cols = 4;

	Gtk::Table* button_table = manage (new Gtk::Table (initial_button_rows, initial_button_cols));
	Gtk::Table* output_table = manage (new Gtk::Table (initial_output_rows, initial_output_cols));

	Frame* frame;
	Frame* bt_frame;
	VBox* box;
	int output_row, output_col;
	int button_row, button_col;
	int output_rows, output_cols;
	int button_rows, button_cols;

	hpacker.set_spacing (10);
	hpacker.set_border_width (10);

	output_rows = initial_output_rows;
	output_cols = initial_output_cols;
	button_rows = initial_button_rows;
	button_cols = initial_button_cols;
	output_row = 0;
	button_row = 0;
	output_col = 0;
	button_col = 0;

	button_table->set_homogeneous (false);
	button_table->set_row_spacings (2);
	button_table->set_col_spacings (2);
	output_table->set_homogeneous (true);
	output_table->set_row_spacings (2);
	output_table->set_col_spacings (2);
	button_table->set_border_width (5);
	output_table->set_border_width (5);


	bt_frame = manage (new Frame);
	bt_frame->set_name ("BaseFrame");
	bt_frame->set_label (_("Switches"));
	bt_frame->add (*button_table);
	hpacker.pack_start(*bt_frame, true, true);

	box = manage (new VBox);
	box->set_border_width (5);
	box->set_spacing (1);

	frame = manage (new Frame);
	frame->set_name ("BaseFrame");
	frame->set_label (_("Controls"));
	frame->add (*box);
	hpacker.pack_start(*frame, true, true);

	// Add special controls to UI, and build list of normal controls to be layed out later
	std::vector<ControlUI *> cui_controls_list;
	for (size_t i = 0; i < control_uis.size(); ++i) {
		ControlUI* cui = control_uis[i];

		if (cui->button || cui->file_button) {

			if (!is_scrollable && button_row == button_rows) {
				button_row = 0;
				if (++button_col == button_cols) {
					button_cols += 2;
					button_table->resize (button_rows, button_cols);
				}
			}

			button_table->attach (*cui, button_col, button_col + 1, button_row, button_row+1,
			                     FILL|EXPAND, FILL);
			button_row++;

		} else if (cui->controller || cui->clickbox || cui->combo) {
			// Get all of the controls into a list, so that
			// we can lay them out a bit more nicely later.
			cui_controls_list.push_back(cui);

		} else if (cui->display) {

			output_table->attach (*cui, output_col, output_col + 1, output_row, output_row+1,
			                     FILL|EXPAND, FILL);

			// TODO: The meters should be divided into multiple rows

			if (++output_col == output_cols) {
				output_cols ++;
				output_table->resize (output_rows, output_cols);
			}
		}
	}

	// Iterate over the list of controls to find which adjacent controls
	// are similar enough to be grouped together.

	string label, previous_label = "";
	std::vector<int> numbers_in_labels(cui_controls_list.size());

	std::vector<float> similarity_scores(cui_controls_list.size());
	float most_similar = 0.0, least_similar = 1.0;

	size_t i = 0;
	for (vector<ControlUI*>::iterator cuip = cui_controls_list.begin(); cuip != cui_controls_list.end(); ++cuip, ++i) {
		label = (*cuip)->label.get_text();
		numbers_in_labels[i] = get_number(label);

		if (i > 0) {
			// A hand-wavy calculation of how similar this control's
			// label is to the previous.
			similarity_scores[i] =
				(float) (
					( matching_chars_at_head(label, previous_label) +
					  matching_chars_at_tail(label, previous_label) +
					  1
					)
				) / (label.length() + previous_label.length());
			if (numbers_in_labels[i] >= 0) {
				similarity_scores[i] += (numbers_in_labels[i] == numbers_in_labels[i-1]);
			}
			least_similar = min(least_similar, similarity_scores[i]);
			most_similar  = max(most_similar, similarity_scores[i]);
		} else {
			similarity_scores[0] = 1.0;
		}

		// cerr << "label: " << label << " sim: " << fixed << setprecision(3) << similarity_scores[i] << " num: " << numbers_in_labels[i] << endl;
		previous_label = label;
	}


	// cerr << "most similar: " << most_similar << ", least similar: " << least_similar << endl;
	float similarity_threshold;

	if (most_similar > 1.0) {
		similarity_threshold = default_similarity_threshold;
	} else {
		similarity_threshold = most_similar - (1 - default_similarity_threshold);
	}

	// Now iterate over the list of controls to display them, placing an
	// HSeparator between controls of less than a certain similarity, and
	// starting a new column when necessary.

	i = 0;
	for (vector<ControlUI*>::iterator cuip = cui_controls_list.begin(); cuip != cui_controls_list.end(); ++cuip, ++i) {

		ControlUI* cui = *cuip;

		if (!is_scrollable) {
			x++;
		}

		if (x > max_controls_per_column || similarity_scores[i] <= similarity_threshold) {
			if (x > min_controls_per_column) {
				frame = manage (new Frame);
				frame->set_name ("BaseFrame");
				frame->set_label (_("Controls"));
				box = manage (new VBox);
				box->set_border_width (5);
				box->set_spacing (1);
				frame->add (*box);
				hpacker.pack_start(*frame, true, true);
				x = 0;
			} else {
				HSeparator *split = new HSeparator();
				split->set_size_request(-1, 5);
				box->pack_start(*split, false, false, 0);
			}

		}
		box->pack_start (*cui, false, false);
	}

	if (is_scrollable) {
		prefheight = 30 * i;
	}

	if (box->children().empty()) {
		hpacker.remove (*frame);
	}

	if (button_table->children().empty()) {
		hpacker.remove (*bt_frame);
		delete button_table;
	} else {
		button_table->show_all ();
	}

	if (!output_table->children().empty()) {
		frame = manage (new Frame);
		frame->set_name ("BaseFrame");
		frame->set_label(_("Meters"));
		frame->add (*output_table);
		hpacker.pack_end (*frame, true, true);
		output_table->show_all ();
	} else {
		delete output_table;
	}
	show_all();

}

void
GenericPluginUI::custom_layout (const std::vector<ControlUI*>& control_uis)
{
	Gtk::Table* layout = manage (new Gtk::Table ());

	for (vector<ControlUI*>::const_iterator i = control_uis.begin(); i != control_uis.end(); ++i) {
		ControlUI* cui = *i;
		if (cui->x0 < 0 || cui->y0 < 0) {
			continue;
		}
		layout->attach (*cui, cui->x0, cui->x1, cui->y0, cui->y1, FILL, SHRINK, 2, 2);
	}
	hpacker.pack_start (*layout, true, true);
}

GenericPluginUI::ControlUI::ControlUI (const Evoral::Parameter& p)
	: param(p)
	, automate_button (X_("")) // force creation of a label
	, combo (0)
	, clickbox (0)
	, file_button (0)
	, spin_box (0)
	, display (0)
	, hbox (0)
	, vbox (0)
	, meterinfo (0)
	, knobtable (0)
{
	automate_button.set_name ("plugin automation state button");
	set_tooltip (automate_button, _("Automation control"));

	/* XXX translators: use a string here that will be at least as long
	   as the longest automation label (see ::automation_state_changed()
	   below). be sure to include a descender.
	*/

	automate_button.set_sizing_text(_("Mgnual"));

	ignore_change = false;
	update_pending = false;
	button = false;

	x0 = x1 = y0 = y1 = -1;
}

GenericPluginUI::ControlUI::~ControlUI()
{
	if (meterinfo) {
		delete meterinfo->meter;
		delete meterinfo;
	}
}

void
GenericPluginUI::automation_state_changed (ControlUI* cui)
{
	/* update button label */

	// don't lock to avoid deadlock because we're triggered by
	// AutomationControl::Changed() while the automation lock is taken

	AutoState state = insert->get_parameter_automation_state (cui->parameter());

	cui->automate_button.set_active((state != ARDOUR::Off));

	if (cui->knobtable) {
		cui->automate_button.set_text (
				GainMeterBase::astate_string (state));
		return;
	}

	switch (state & (ARDOUR::Off|Play|Touch|Write)) {
	case ARDOUR::Off:
		cui->automate_button.set_text (S_("Automation|Manual"));
		break;
	case Play:
		cui->automate_button.set_text (_("Play"));
		break;
	case Write:
		cui->automate_button.set_text (_("Write"));
		break;
	case Touch:
		cui->automate_button.set_text (_("Touch"));
		break;
	default:
		cui->automate_button.set_text (_("???"));
		break;
	}
}

bool
GenericPluginUI::integer_printer (char buf[32], Adjustment &adj, ControlUI* cui)
{
	float const        v   = cui->control->interface_to_internal(adj.get_value ());
	const std::string& str = ARDOUR::value_as_string(cui->control->desc(), Variant(v));
	const size_t       len = str.copy(buf, 31);
	buf[len] = '\0';
	return true;
}

bool
GenericPluginUI::midinote_printer (char buf[32], Adjustment &adj, ControlUI* cui)
{
	float const        v   = cui->control->interface_to_internal(adj.get_value ());
	const std::string& str = ARDOUR::value_as_string(cui->control->desc(), Variant(v));
	const size_t       len = str.copy(buf, 31);
	buf[len] = '\0';
	return true;
}

void
GenericPluginUI::print_parameter (char *buf, uint32_t len, uint32_t param)
{
	plugin->print_parameter (param, buf, len);
}

/** Build a ControlUI for a parameter/property.
 * Note that mcontrol may be NULL for outputs.
 */
GenericPluginUI::ControlUI*
GenericPluginUI::build_control_ui (const Evoral::Parameter&             param,
                                   const ParameterDescriptor&           desc,
                                   boost::shared_ptr<AutomationControl> mcontrol,
                                   float                                value,
                                   bool                                 is_input,
                                   bool                                 use_knob)
{
	ControlUI* control_ui = 0;

	control_ui = manage (new ControlUI (param));
	control_ui->combo = 0;
	control_ui->control = mcontrol;
	control_ui->label.set_text (desc.label);
	control_ui->label.set_alignment (0.0, 0.5);
	control_ui->label.set_name ("PluginParameterLabel");
	control_ui->set_spacing (5);

	if (is_input) {

		if (desc.datatype == Variant::PATH) {

			/* We shouldn't get that type for input ports */
			assert(param.type() == PluginPropertyAutomation);

			/* Build a file selector button */

			// Create/add controller
			control_ui->file_button = manage(new Gtk::FileChooserButton(Gtk::FILE_CHOOSER_ACTION_OPEN));
			control_ui->file_button->set_title(desc.label);

			if (use_knob) {
				control_ui->knobtable = manage (new Table());
				control_ui->pack_start(*control_ui->knobtable, true, false);
				control_ui->knobtable->attach (control_ui->label, 0, 1, 0, 1);
				control_ui->knobtable->attach (*control_ui->file_button, 0, 1, 1, 2);
			} else {
				control_ui->pack_start (control_ui->label, false, true);
				control_ui->pack_start (*control_ui->file_button, true, true);
			}

			// Monitor changes from the user.
			control_ui->file_button->signal_file_set().connect(
				sigc::bind(sigc::mem_fun(*this, &GenericPluginUI::set_path_property),
				           desc, control_ui->file_button));

			/* Add the filebutton control to a map so that we can update it when
			 * the corresponding property changes. This doesn't go through the usual
			 * AutomationControls, because they don't support non-numeric values. */
			_filepath_controls.insert(std::make_pair(desc.key, control_ui->file_button));

			return control_ui;
		}

		assert(mcontrol);

		/* See if there any named values for our input value */
		control_ui->scale_points = desc.scale_points;

		/* If this parameter is an integer, work out the number of distinct values
		   it can take on (assuming that lower and upper values are allowed).
		*/
		int const steps = desc.integer_step ? (desc.upper - desc.lower + 1) / desc.step : 0;

		if (control_ui->scale_points && ((steps && int (control_ui->scale_points->size()) == steps) || desc.enumeration)) {

			/* Either:
			 *   a) There is a label for each possible value of this input, or
			 *   b) This port is marked as being an enumeration.
			 */

			control_ui->combo = new ArdourDropdown();
			for (ARDOUR::ScalePoints::const_iterator i = control_ui->scale_points->begin();
			     i != control_ui->scale_points->end();
			     ++i) {
				control_ui->combo->AddMenuElem(Menu_Helpers::MenuElem(
						i->first,
						sigc::bind(sigc::mem_fun(*this, &GenericPluginUI::control_combo_changed),
						           control_ui,
						           i->second)));
			}

			update_control_display(control_ui);

		} else {

			/* create the controller */

			/* XXX memory leak: SliderController not destroyed by ControlUI
			 * destructor, and manage() reports object hierarchy
			 * ambiguity.
			 */
			control_ui->controller = AutomationController::create(insert, mcontrol->parameter(), desc, mcontrol, use_knob);

			/* Control UI's don't need the rapid timer workaround */
			control_ui->controller->stop_updating ();

			/* XXX this code is not right yet, because it doesn't handle
			   the absence of bounds in any sensible fashion.
			*/

			Adjustment* adj = control_ui->controller->adjustment();

			if (desc.integer_step && !desc.toggled) {
				control_ui->clickbox = new ClickBox (adj, "PluginUIClickBox", true);
				Gtkmm2ext::set_size_request_to_display_given_text (*control_ui->clickbox, "g9999999", 2, 2);
				if (desc.unit == ParameterDescriptor::MIDI_NOTE) {
					control_ui->clickbox->set_printer (sigc::bind (sigc::mem_fun (*this, &GenericPluginUI::midinote_printer), control_ui));
				} else {
					control_ui->clickbox->set_printer (sigc::bind (sigc::mem_fun (*this, &GenericPluginUI::integer_printer), control_ui));
				}
			} else if (desc.toggled) {
				ArdourButton* but = dynamic_cast<ArdourButton*> (control_ui->controller->widget());
				assert(but);
				but->set_tweaks(ArdourButton::Square);
			} else if (use_knob) {
				/* Delay size request so that styles are gotten right */
				control_ui->controller->widget()->signal_size_request().connect(
						sigc::bind (sigc::mem_fun (*this, &GenericPluginUI::knob_size_request), control_ui));
			} else {
				control_ui->controller->set_size_request (200, -1);
				control_ui->controller->set_name (X_("ProcessorControlSlider"));
			}

			if (!desc.integer_step && !desc.toggled && use_knob) {
				control_ui->spin_box = manage (new ArdourSpinner (mcontrol, adj, insert));
			}

			adj->set_value (mcontrol->internal_to_interface(value));

		}

		if (use_knob) {
			control_ui->automate_button.set_sizing_text("M");

			control_ui->label.set_alignment (0.5, 0.5);
			control_ui->knobtable = manage (new Table());
			control_ui->pack_start(*control_ui->knobtable, true, true);

			if (control_ui->combo) {
				control_ui->knobtable->attach (control_ui->label, 0, 1, 0, 1);
				control_ui->knobtable->attach (*control_ui->combo, 0, 1, 1, 2);
			} else if (control_ui->clickbox) {
				control_ui->knobtable->attach (*control_ui->clickbox, 0, 2, 0, 1);
				control_ui->knobtable->attach (control_ui->label, 0, 1, 1, 2, FILL, SHRINK);
				control_ui->knobtable->attach (control_ui->automate_button, 1, 2, 1, 2, SHRINK, SHRINK, 2, 0);
			} else if (control_ui->spin_box) {
				ArdourKnob* knob = dynamic_cast<ArdourKnob*>(control_ui->controller->widget ());
				knob->set_tooltip_prefix (desc.label + ": ");
				knob->set_printer (insert);
				Alignment *align = manage (new Alignment (.5, .5, 0, 0));
				align->add (*control_ui->controller);
				control_ui->knobtable->attach (*align, 0, 1, 0, 1, EXPAND, SHRINK, 1, 2);
				control_ui->knobtable->attach (*control_ui->spin_box, 0, 2, 1, 2);
				control_ui->knobtable->attach (control_ui->automate_button, 1, 2, 0, 1, SHRINK, SHRINK, 2, 0);
			} else if (desc.toggled) {
				Alignment *align = manage (new Alignment (.5, .5, 0, 0));
				align->add (*control_ui->controller);
				control_ui->knobtable->attach (*align, 0, 2, 0, 1, EXPAND, SHRINK, 2, 2);
				control_ui->knobtable->attach (control_ui->label, 0, 1, 1, 2, FILL, SHRINK);
				control_ui->knobtable->attach (control_ui->automate_button, 1, 2, 1, 2, SHRINK, SHRINK, 2, 0);
			} else {
				control_ui->knobtable->attach (*control_ui->controller, 0, 2, 0, 1);
				control_ui->knobtable->attach (control_ui->label, 0, 1, 1, 2, FILL, SHRINK);
				control_ui->knobtable->attach (control_ui->automate_button, 1, 2, 1, 2, SHRINK, SHRINK, 2, 0);
			}

		} else {

			control_ui->pack_start (control_ui->label, true, true);
			if (control_ui->combo) {
				control_ui->pack_start(*control_ui->combo, false, true);
			} else if (control_ui->clickbox) {
				control_ui->pack_start (*control_ui->clickbox, false, false);
			} else if (control_ui->spin_box) {
				control_ui->pack_start (*control_ui->spin_box, false, false);
				control_ui->pack_start (*control_ui->controller, false, false);
			} else {
				control_ui->pack_start (*control_ui->controller, false, false);
			}
			control_ui->pack_start (control_ui->automate_button, false, false);
		}


		if (mcontrol->flags () & Controllable::NotAutomatable) {
			control_ui->automate_button.set_sensitive (false);
			set_tooltip(control_ui->automate_button, _("This control cannot be automated"));
		} else {
			control_ui->automate_button.signal_button_press_event().connect (
					sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::astate_button_event),
					            control_ui),
					false);
			mcontrol->alist()->automation_state_changed.connect (
					control_connections,
					invalidator (*this),
					boost::bind (&GenericPluginUI::automation_state_changed, this, control_ui),
					gui_context());
			input_controls_with_automation.push_back (control_ui);
		}

		if (desc.toggled) {
			control_ui->button = true;
			ArdourButton* but = dynamic_cast<ArdourButton*>(control_ui->controller->widget ());
			assert (but);
			but->set_name ("pluginui toggle");
			update_control_display(control_ui);
		}

		automation_state_changed (control_ui);

		/* Add to the list of CUIs that need manual update to workaround
		 * AutomationControl not knowing about preset loads */
		input_controls.push_back (control_ui);

	} else {

		control_ui->display = manage (new EventBox);
		control_ui->display->set_name ("ParameterValueDisplay");

		control_ui->display_label = manage (new Label);

		control_ui->display_label->set_name ("ParameterValueDisplay");

		control_ui->display->add (*control_ui->display_label);
		Gtkmm2ext::set_size_request_to_display_given_text (*control_ui->display, "-888.8g", 2, 6);

		control_ui->display->show_all ();

		control_ui->vbox = manage (new VBox);
		control_ui->vbox->set_spacing(3);

		if (desc.integer_step || desc.enumeration) {
			control_ui->vbox->pack_end (*control_ui->display, false, false);
			control_ui->vbox->pack_end (control_ui->label, false, false);
		} else {
			/* set up a meter for float ports */

			MeterInfo * info = new MeterInfo();
			control_ui->meterinfo = info;

			info->meter = new FastMeter (
					5, 5, FastMeter::Vertical, 0,
					0x0000aaff,
					0x008800ff, 0x008800ff,
					0x00ff00ff, 0x00ff00ff,
					0xcccc00ff, 0xcccc00ff,
					0xffaa00ff, 0xffaa00ff,
					0xff0000ff,
					UIConfiguration::instance().color ("meter background bottom"),
					UIConfiguration::instance().color ("meter background top")
					);

			info->min_unbound = desc.min_unbound;
			info->max_unbound = desc.max_unbound;

			info->min = desc.lower;
			info->max = desc.upper;

			control_ui->label.set_angle(90);

			HBox* center =  manage (new HBox);
			center->set_spacing(1);
			center->pack_start (control_ui->label, false, false);
			center->pack_start (*info->meter, false, false);

			control_ui->hbox = manage (new HBox);
			control_ui->hbox->pack_start (*center, true, false);

			// horizontally center this hbox in the vbox
			control_ui->vbox->pack_start (*control_ui->hbox, false, false);

			control_ui->meterinfo->meter->show_all();
			control_ui->meterinfo->packed = true;
			control_ui->vbox->pack_start (*control_ui->display, false, false);
		}

		control_ui->pack_start (*control_ui->vbox);

		output_controls.push_back (control_ui);
	}

	if (mcontrol) {
		mcontrol->Changed.connect(control_connections, invalidator(*this),
		                          boost::bind(&GenericPluginUI::ui_parameter_changed,
		                                      this, control_ui),
		                          gui_context());
	}

	return control_ui;
}

void
GenericPluginUI::knob_size_request(Gtk::Requisition* req, ControlUI* cui) {
	Gtk::Requisition astate_req (cui->automate_button.size_request());
	const int size = (int) (astate_req.height * 1.5);
	req->width = max(req->width, size);
	req->height = max(req->height, size);
}


bool
GenericPluginUI::astate_button_event (GdkEventButton* ev, ControlUI* cui)
{
	if (ev->button != 1) {
		return true;
	}

	using namespace Menu_Helpers;

	if (automation_menu == 0) {
		automation_menu = manage (new Menu);
		automation_menu->set_name ("ArdourContextMenu");
		automation_menu->set_reserve_toggle_size(false);
	}

	MenuList& items (automation_menu->items());

	items.clear ();
	items.push_back (MenuElem (S_("Automation|Manual"),
				   sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::set_automation_state), (AutoState) ARDOUR::Off, cui)));
	items.push_back (MenuElem (_("Play"),
				   sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::set_automation_state), (AutoState) Play, cui)));
	items.push_back (MenuElem (_("Write"),
				   sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::set_automation_state), (AutoState) Write, cui)));
	items.push_back (MenuElem (_("Touch"),
				   sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::set_automation_state), (AutoState) Touch, cui)));

	anchored_menu_popup(automation_menu, &cui->automate_button, cui->automate_button.get_text(),
	                    1, ev->time);

	return true;
}

void
GenericPluginUI::set_all_automation (AutoState as)
{
	for (vector<ControlUI*>::iterator i = input_controls_with_automation.begin(); i != input_controls_with_automation.end(); ++i) {
		set_automation_state (as, (*i));
	}
}

void
GenericPluginUI::set_automation_state (AutoState state, ControlUI* cui)
{
	insert->set_parameter_automation_state (cui->parameter(), state);
}

void
GenericPluginUI::ui_parameter_changed (ControlUI* cui)
{
	if (!cui->update_pending) {
		cui->update_pending = true;
		Gtkmm2ext::UI::instance()->call_slot (MISSING_INVALIDATOR, boost::bind (&GenericPluginUI::update_control_display, this, cui));
	}
}

void
GenericPluginUI::update_control_display (ControlUI* cui)
{
	/* XXX how do we handle logarithmic stuff here ? */

	cui->update_pending = false;

	float val = cui->control->get_value();

	PBD::Unwinder<bool> (cui->ignore_change, true);

	if (cui->combo && cui->scale_points) {
		for (ARDOUR::ScalePoints::iterator it = cui->scale_points->begin(); it != cui->scale_points->end(); ++it) {
			if (it->second == val) {
				cui->combo->set_text(it->first);
				break;
			}
		}
	} else if (cui->button) {
		// AutomationController handles this
	}

	if( cui->controller ) {
	    cui->controller->display_effective_value();
	}


	/*} else {
		if (cui->logarithmic) {
			val = log(val);
		}
		if (val != cui->adjustment->get_value()) {
			cui->adjustment->set_value (val);
		}
	}*/
}

void
GenericPluginUI::update_input_displays ()
{
	/* XXX This is a workaround for AutomationControl not knowing about preset loads */
	for (vector<ControlUI*>::iterator i = input_controls.begin();
	     i != input_controls.end();
	     ++i) {
		update_control_display(*i);
	}
	return;
}

void
GenericPluginUI::control_combo_changed (ControlUI* cui, float value)
{
	if (!cui->ignore_change) {
		insert->automation_control (cui->parameter())->set_value (value, Controllable::NoGroup);
	}
}

bool
GenericPluginUI::start_updating (GdkEventAny*)
{
	if (output_controls.size() > 0 ) {
		screen_update_connection.disconnect();
		screen_update_connection = Timers::super_rapid_connect (sigc::mem_fun(*this, &GenericPluginUI::output_update));
	}
	return false;
}

bool
GenericPluginUI::stop_updating (GdkEventAny*)
{
	if (output_controls.size() > 0 ) {
		screen_update_connection.disconnect();
	}
	return false;
}

void
GenericPluginUI::output_update ()
{
	for (vector<ControlUI*>::iterator i = output_controls.begin(); i != output_controls.end(); ++i) {
		float val = plugin->get_parameter ((*i)->parameter().id());
		char buf[32];
		snprintf (buf, sizeof(buf), "%.2f", val);
		(*i)->display_label->set_text (buf);

		/* autoscaling for the meter */
		if ((*i)->meterinfo && (*i)->meterinfo->packed) {

			if (val < (*i)->meterinfo->min) {
				if ((*i)->meterinfo->min_unbound)
					(*i)->meterinfo->min = val;
				else
					val = (*i)->meterinfo->min;
			}

			if (val > (*i)->meterinfo->max) {
				if ((*i)->meterinfo->max_unbound)
					(*i)->meterinfo->max = val;
				else
					val = (*i)->meterinfo->max;
			}

			if ((*i)->meterinfo->max > (*i)->meterinfo->min ) {
				float lval = (val - (*i)->meterinfo->min) / ((*i)->meterinfo->max - (*i)->meterinfo->min) ;
				(*i)->meterinfo->meter->set (lval );
			}
		}
	}
}

void
GenericPluginUI::set_path_property (const ParameterDescriptor& desc,
                                    Gtk::FileChooserButton*    widget)
{
	plugin->set_property(desc.key, Variant(Variant::PATH, widget->get_filename()));
}

void
GenericPluginUI::path_property_changed (uint32_t key, const Variant& value)
{
	FilePathControls::iterator c = _filepath_controls.find(key);
	if (c != _filepath_controls.end()) {
		c->second->set_filename(value.get_path());
	} else {
		std::cerr << "warning: property change for property with no control" << std::endl;
	}
}
