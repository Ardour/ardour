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

#include "pbd/stl_delete.h"
#include "pbd/xml++.h"
#include "pbd/failed_constructor.h"

#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/barcontroller.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/doi.h>
#include <gtkmm2ext/slider_controller.h>

#include "midi++/manager.h"

#include "ardour/plugin.h"
#include "ardour/plugin_insert.h"
#include "ardour/session.h"

#include <lrdf.h>

#include "ardour_ui.h"
#include "prompter.h"
#include "plugin_ui.h"
#include "utils.h"
#include "gui_thread.h"
#include "automation_controller.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;

GenericPluginUI::GenericPluginUI (boost::shared_ptr<PluginInsert> pi, bool scrollable)
	: PlugUIBase (pi),
	  button_table (initial_button_rows, initial_button_cols),
	  output_table (initial_output_rows, initial_output_cols),
	  hAdjustment(0.0, 0.0, 0.0),
	  vAdjustment(0.0, 0.0, 0.0),
	  scroller_view(hAdjustment, vAdjustment),
	  automation_menu (0),
	  is_scrollable(scrollable)
{
	set_name ("PluginEditor");
	set_border_width (10);
	//set_homogeneous (false);

	pack_start (main_contents, true, true);
	settings_box.set_homogeneous (false);

	HBox* constraint_hbox = manage (new HBox);
	HBox* smaller_hbox = manage (new HBox);
	smaller_hbox->set_spacing (4);
	Label* combo_label = manage (new Label (_("<span size=\"large\">Presets</span>")));
	combo_label->set_use_markup (true);

	latency_button.add (latency_label);
	latency_button.signal_clicked().connect (sigc::mem_fun (*this, &PlugUIBase::latency_button_clicked));
	set_latency_label ();

	smaller_hbox->pack_start (latency_button, false, false, 10);
	smaller_hbox->pack_start (_preset_combo, false, false);
	smaller_hbox->pack_start (_preset_modified, false, false);
	smaller_hbox->pack_start (add_button, false, false);
	smaller_hbox->pack_start (save_button, false, false);
	smaller_hbox->pack_start (delete_button, false, false);
	smaller_hbox->pack_start (bypass_button, false, true);

	constraint_hbox->set_spacing (5);
	constraint_hbox->set_homogeneous (false);

	VBox* v1_box = manage (new VBox);
	VBox* v2_box = manage (new VBox);
	pack_end (plugin_analysis_expander, false, false);
	if (!plugin->get_docs().empty()) {
		pack_end (description_expander, false, false);
	}

	v1_box->pack_start (*smaller_hbox, false, true);
	v2_box->pack_start (focus_button, false, true);

	main_contents.pack_start (settings_box, false, false);

	constraint_hbox->pack_end (*v2_box, false, false);
	constraint_hbox->pack_end (*v1_box, false, false);

	main_contents.pack_start (*constraint_hbox, false, false);

	if (is_scrollable ) {
		scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
		scroller.set_name ("PluginEditor");
		scroller_view.set_name("PluginEditor");
		scroller_view.add (hpacker);
		scroller.add (scroller_view);

		main_contents.pack_start (scroller, true, true);

	} else {
		main_contents.pack_start (hpacker, false, false);
	}

	pi->ActiveChanged.connect (active_connection, invalidator (*this), boost::bind (&GenericPluginUI::processor_active_changed, this, boost::weak_ptr<Processor>(pi)), gui_context());

	bypass_button.set_active (!pi->active());

	prefheight = 0;
	build ();
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
	guint32 i = 0;
	guint32 x = 0;
	Frame* frame;
	Frame* bt_frame;
	VBox* box;
	int output_row, output_col;
	int button_row, button_col;
	int output_rows, output_cols;
	int button_rows, button_cols;

	hpacker.set_spacing (10);

	output_rows = initial_output_rows;
	output_cols = initial_output_cols;
	button_rows = initial_button_rows;
	button_cols = initial_button_cols;
	output_row = 0;
	button_row = 0;
	output_col = 0;
	button_col = 0;

	button_table.set_homogeneous (false);
	button_table.set_row_spacings (2);
	button_table.set_col_spacings (2);
	output_table.set_homogeneous (true);
	output_table.set_row_spacings (2);
	output_table.set_col_spacings (2);
	button_table.set_border_width (5);
	output_table.set_border_width (5);

	hpacker.set_border_width (10);

	bt_frame = manage (new Frame);
	bt_frame->set_name ("BaseFrame");
	bt_frame->set_label (_("Switches"));
	bt_frame->add (button_table);
	hpacker.pack_start(*bt_frame, true, true);

	box = manage (new VBox);
	box->set_border_width (5);
	box->set_spacing (1);

	frame = manage (new Frame);
	frame->set_name ("BaseFrame");
	frame->set_label (_("Controls"));
	frame->add (*box);
	hpacker.pack_start(*frame, true, true);

	/* find all ports. build control elements for all appropriate control ports */
	std::vector<ControlUI *> cui_controls_list;

	for (i = 0; i < plugin->parameter_count(); ++i) {

		if (plugin->parameter_is_control (i)) {

			/* Don't show latency control ports */

			if (plugin->describe_parameter (Evoral::Parameter(PluginAutomation, 0, i)) == X_("latency")) {
				continue;
			}

			if (plugin->describe_parameter (Evoral::Parameter(PluginAutomation, 0, i)) == X_("hidden")) {
				continue;
			}

			ControlUI* cui;

			boost::shared_ptr<ARDOUR::AutomationControl> c
				= boost::dynamic_pointer_cast<ARDOUR::AutomationControl>(
					insert->control(Evoral::Parameter(PluginAutomation, 0, i)));

			if ((cui = build_control_ui (i, c)) == 0) {
				error << string_compose(_("Plugin Editor: could not build control element for port %1"), i) << endmsg;
				continue;
			}

			const std::string param_docs = plugin->get_parameter_docs(i);
			if (!param_docs.empty()) {
				ARDOUR_UI::instance()->set_tip(cui, param_docs.c_str());
			}

			if (cui->controller || cui->clickbox || cui->combo) {
				// Get all of the controls into a list, so that
				// we can lay them out a bit more nicely later.
				cui_controls_list.push_back(cui);
			} else if (cui->button) {

				if (!is_scrollable && button_row == button_rows) {
					button_row = 0;
					if (++button_col == button_cols) {
						button_cols += 2;
						button_table.resize (button_rows, button_cols);
					}
				}

				button_table.attach (*cui, button_col, button_col + 1, button_row, button_row+1,
						     FILL|EXPAND, FILL);
				button_row++;

			} else if (cui->display) {

				output_table.attach (*cui, output_col, output_col + 1, output_row, output_row+1,
						     FILL|EXPAND, FILL);

 				// TODO: The meters should be divided into multiple rows

				if (++output_col == output_cols) {
					output_cols ++;
					output_table.resize (output_rows, output_cols);
				}
			}
		} 
	}

	// Iterate over the list of controls to find which adjacent controls
	// are similar enough to be grouped together.
	
	string label, previous_label = "";
	int numbers_in_labels[cui_controls_list.size()];
	
	float similarity_scores[cui_controls_list.size()];
	float most_similar = 0.0, least_similar = 1.0;
	
	i = 0;
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

	if (button_table.children().empty()) {
		hpacker.remove (*bt_frame);
	}

	if (!output_table.children().empty()) {
		frame = manage (new Frame);
		frame->set_name ("BaseFrame");
		frame->set_label(_("Meters"));
		frame->add (output_table);
		hpacker.pack_end (*frame, true, true);
	}

	output_update ();

	output_table.show_all ();
	button_table.show_all ();
}

GenericPluginUI::ControlUI::ControlUI ()
	: automate_button (X_("")) // force creation of a label
{
	automate_button.set_name ("PluginAutomateButton");
	ARDOUR_UI::instance()->set_tip (automate_button, _("Automation control"));

	/* XXX translators: use a string here that will be at least as long
	   as the longest automation label (see ::automation_state_changed()
	   below). be sure to include a descender.
	*/

	set_size_request_to_display_given_text (automate_button, _("Mgnual"), 15, 10);

	ignore_change = 0;
	display = 0;
	button = 0;
	clickbox = 0;
	meterinfo = 0;
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
	switch (insert->get_parameter_automation_state (cui->parameter()) & (ARDOUR::Off|Play|Touch|Write)) {
	case ARDOUR::Off:
		cui->automate_button.set_label (S_("Automation|Manual"));
		break;
	case Play:
		cui->automate_button.set_label (_("Play"));
		break;
	case Write:
		cui->automate_button.set_label (_("Write"));
		break;
	case Touch:
		cui->automate_button.set_label (_("Touch"));
		break;
	default:
		cui->automate_button.set_label (_("???"));
		break;
	}
}


bool
GenericPluginUI::integer_printer (char buf[32], Adjustment &adj, ControlUI* cui)
{
	float const v = adj.get_value ();
	
	if (cui->scale_points) {
		Plugin::ScalePoints::const_iterator i = cui->scale_points->begin ();
		while (i != cui->scale_points->end() && i->second != v) {
			++i;
		}

		if (i != cui->scale_points->end ()) {
			snprintf (buf, 32, "%s", i->first.c_str());
			return true;
		}
	}
		
	snprintf (buf, 32, "%.0f", v);
	return true;
}

void
GenericPluginUI::print_parameter (char *buf, uint32_t len, uint32_t param)
{
	plugin->print_parameter (param, buf, len);
}

GenericPluginUI::ControlUI*
GenericPluginUI::build_control_ui (guint32 port_index, boost::shared_ptr<AutomationControl> mcontrol)
{
	ControlUI* control_ui = 0;

	Plugin::ParameterDescriptor desc;

	plugin->get_parameter_descriptor (port_index, desc);

	control_ui = manage (new ControlUI ());
	control_ui->combo = 0;
	control_ui->control = mcontrol;
	control_ui->update_pending = false;
	control_ui->label.set_text (desc.label);
	control_ui->label.set_alignment (0.0, 0.5);
	control_ui->label.set_name ("PluginParameterLabel");
	control_ui->port_index = port_index;

	control_ui->set_spacing (5);

	Gtk::Requisition req (control_ui->automate_button.size_request());

	if (plugin->parameter_is_input(port_index)) {

		/* See if there any named values for our input value */
		control_ui->scale_points = plugin->get_scale_points (port_index);

		/* If this parameter is an integer, work out the number of distinct values
		   it can take on (assuming that lower and upper values are allowed).
		*/
		int const steps = desc.integer_step ? (desc.upper - desc.lower + 1) / desc.step : 0;

		if (control_ui->scale_points && ((steps && int (control_ui->scale_points->size()) == steps) || desc.enumeration)) {
			
			/* Either:
			 *   a) There is a label for each possible value of this input, or
			 *   b) This port is marked as being an enumeration.
			 */

			std::vector<std::string> labels;
			for (
				ARDOUR::Plugin::ScalePoints::const_iterator i = control_ui->scale_points->begin();
				i != control_ui->scale_points->end();
				++i) {
				
				labels.push_back(i->first);
			}

			control_ui->combo = new Gtk::ComboBoxText();
			set_popdown_strings(*control_ui->combo, labels);
			control_ui->combo->signal_changed().connect(
				sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::control_combo_changed),
				            control_ui));
			mcontrol->Changed.connect(control_connections, invalidator(*this),
			                          boost::bind(&GenericPluginUI::ui_parameter_changed,
			                                      this, control_ui),
			                          gui_context());
			control_ui->pack_start(control_ui->label, true, true);
			control_ui->pack_start(*control_ui->combo, false, true);

			update_control_display(control_ui);

			return control_ui;
		}

		if (desc.toggled) {

			/* Build a button */

			control_ui->button = manage (new ToggleButton ());
			control_ui->button->set_name ("PluginEditorButton");
			control_ui->button->set_size_request (20, 20);

			control_ui->pack_start (control_ui->label, true, true);
			control_ui->pack_start (*control_ui->button, false, true);
			control_ui->pack_start (control_ui->automate_button, false, false);

			control_ui->button->signal_clicked().connect (sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::control_port_toggled), control_ui));
			control_ui->automate_button.signal_clicked().connect (bind (mem_fun(*this, &GenericPluginUI::astate_clicked), control_ui, (uint32_t) port_index));

			mcontrol->Changed.connect (control_connections, invalidator (*this), boost::bind (&GenericPluginUI::toggle_parameter_changed, this, control_ui), gui_context());
			mcontrol->alist()->automation_state_changed.connect (control_connections, invalidator (*this), boost::bind (&GenericPluginUI::automation_state_changed, this, control_ui), gui_context());

			if (plugin->get_parameter (port_index) > 0.5){
				control_ui->button->set_active(true);
			}

			automation_state_changed (control_ui);

			return control_ui;
		}

		/* create the controller */

		if (mcontrol) {
			control_ui->controller = AutomationController::create(insert, mcontrol->parameter(), mcontrol);
		}

		/* XXX this code is not right yet, because it doesn't handle
		   the absence of bounds in any sensible fashion.
		*/

		Adjustment* adj = control_ui->controller->adjustment();
		boost::shared_ptr<PluginInsert::PluginControl> pc = boost::dynamic_pointer_cast<PluginInsert::PluginControl> (control_ui->control);

		adj->set_lower (pc->internal_to_interface (desc.lower));
		adj->set_upper (pc->internal_to_interface (desc.upper));

		adj->set_step_increment (desc.step);
		adj->set_page_increment (desc.largestep);

		if (desc.integer_step) {
			control_ui->clickbox = new ClickBox (adj, "PluginUIClickBox");
			Gtkmm2ext::set_size_request_to_display_given_text (*control_ui->clickbox, "g9999999", 2, 2);
			control_ui->clickbox->set_printer (sigc::bind (sigc::mem_fun (*this, &GenericPluginUI::integer_printer), control_ui));
		} else {
			//sigc::slot<void,char*,uint32_t> pslot = sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::print_parameter), (uint32_t) port_index);

			control_ui->controller->set_size_request (200, req.height);
			control_ui->controller->set_name (X_("PluginSlider"));
			control_ui->controller->set_style (BarController::LeftToRight);
			control_ui->controller->set_use_parent (true);
			control_ui->controller->set_logarithmic (desc.logarithmic);

			control_ui->controller->StartGesture.connect (sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::start_touch), control_ui));
			control_ui->controller->StopGesture.connect (sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::stop_touch), control_ui));

		}

		adj->set_value (pc->internal_to_interface (plugin->get_parameter (port_index)));

		/* XXX memory leak: SliderController not destroyed by ControlUI
		   destructor, and manage() reports object hierarchy
		   ambiguity.
		*/

		control_ui->pack_start (control_ui->label, true, true);
		if (desc.integer_step) {
			control_ui->pack_start (*control_ui->clickbox, false, false);
		} else {
			control_ui->pack_start (*control_ui->controller, false, false);
		}

		control_ui->pack_start (control_ui->automate_button, false, false);
		control_ui->automate_button.signal_clicked().connect (sigc::bind (sigc::mem_fun(*this, &GenericPluginUI::astate_clicked), control_ui, (uint32_t) port_index));

		automation_state_changed (control_ui);

		mcontrol->Changed.connect (control_connections, invalidator (*this), boost::bind (&GenericPluginUI::ui_parameter_changed, this, control_ui), gui_context());
		mcontrol->alist()->automation_state_changed.connect (control_connections, invalidator (*this), boost::bind (&GenericPluginUI::automation_state_changed, this, control_ui), gui_context());

		input_controls.push_back (control_ui);

	} else if (plugin->parameter_is_output (port_index)) {

		control_ui->display = manage (new EventBox);
		control_ui->display->set_name ("ParameterValueDisplay");

		control_ui->display_label = manage (new Label);

		control_ui->display_label->set_name ("ParameterValueDisplay");

		control_ui->display->add (*control_ui->display_label);
		Gtkmm2ext::set_size_request_to_display_given_text (*control_ui->display, "-99,99", 2, 2);

		control_ui->display->show_all ();

		/* set up a meter */
		/* TODO: only make a meter if the port is Hinted for it */

		MeterInfo * info = new MeterInfo(port_index);
 		control_ui->meterinfo = info;

		info->meter = new FastMeter (
				5, 5, FastMeter::Vertical, 0,
				0x0000aaff,
				0x008800ff, 0x008800ff,
				0x00ff00ff, 0x00ff00ff,
				0xcccc00ff, 0xcccc00ff,
				0xffaa00ff, 0xffaa00ff,
				0xff0000ff,
				ARDOUR_UI::config()->canvasvar_MeterBackgroundBot.get(),
				ARDOUR_UI::config()->canvasvar_MeterBackgroundTop.get()
				);

		info->min_unbound = desc.min_unbound;
		info->max_unbound = desc.max_unbound;

		info->min = desc.lower;
		info->max = desc.upper;

		control_ui->vbox = manage (new VBox);
		control_ui->hbox = manage (new HBox);

		control_ui->label.set_angle(90);
		control_ui->hbox->pack_start (control_ui->label, false, false);
 		control_ui->hbox->pack_start (*info->meter, false, false);

 		control_ui->vbox->pack_start (*control_ui->hbox, false, false);

 		control_ui->vbox->pack_start (*control_ui->display, false, false);

		control_ui->pack_start (*control_ui->vbox);

		control_ui->meterinfo->meter->show_all();
		control_ui->meterinfo->packed = true;

		output_controls.push_back (control_ui);
	}

	if (mcontrol) {
		mcontrol->Changed.connect (control_connections, invalidator (*this), boost::bind (&GenericPluginUI::ui_parameter_changed, this, control_ui), gui_context());
	}

	return control_ui;
}

void
GenericPluginUI::start_touch (GenericPluginUI::ControlUI* cui)
{
	cui->control->start_touch (cui->control->session().transport_frame());
}

void
GenericPluginUI::stop_touch (GenericPluginUI::ControlUI* cui)
{
	cui->control->stop_touch (false, cui->control->session().transport_frame());
}

void
GenericPluginUI::astate_clicked (ControlUI* cui, uint32_t /*port*/)
{
	using namespace Menu_Helpers;

	if (automation_menu == 0) {
		automation_menu = manage (new Menu);
		automation_menu->set_name ("ArdourContextMenu");
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

	automation_menu->popup (1, gtk_get_current_event_time());
}

void
GenericPluginUI::set_automation_state (AutoState state, ControlUI* cui)
{
	insert->set_parameter_automation_state (cui->parameter(), state);
}

void
GenericPluginUI::toggle_parameter_changed (ControlUI* cui)
{
	float val = cui->control->get_value();

	if (!cui->ignore_change) {
		if (val > 0.5) {
			cui->button->set_active (true);
		} else {
			cui->button->set_active (false);
		}
	}
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

	cui->ignore_change++;

	if (cui->combo && cui->scale_points) {
		for (ARDOUR::Plugin::ScalePoints::iterator it = cui->scale_points->begin(); it != cui->scale_points->end(); ++it) {
			if (it->second == val) {
				cui->combo->set_active_text(it->first);
				break;
			}
		}
	} else if (cui->button) {

		if (val > 0.5) {
			cui->button->set_active (true);
		} else {
			cui->button->set_active (false);
		}
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
	cui->ignore_change--;
}

void
GenericPluginUI::control_port_toggled (ControlUI* cui)
{
	cui->ignore_change++;
	insert->automation_control (cui->parameter())->set_value (cui->button->get_active());
	cui->ignore_change--;
}

void
GenericPluginUI::control_combo_changed (ControlUI* cui)
{
	if (!cui->ignore_change && cui->scale_points) {
		string value = cui->combo->get_active_text();
		insert->automation_control (cui->parameter())->set_value ((*cui->scale_points)[value]);
	}
}

bool
GenericPluginUI::start_updating (GdkEventAny*)
{
	if (output_controls.size() > 0 ) {
		screen_update_connection.disconnect();
		screen_update_connection = ARDOUR_UI::instance()->SuperRapidScreenUpdate.connect
			(sigc::mem_fun(*this, &GenericPluginUI::output_update));
	}
	return false;
}

bool
GenericPluginUI::stop_updating (GdkEventAny*)
{
	for (vector<ControlUI*>::iterator i = input_controls.begin(); i != input_controls.end(); ++i) {
		(*i)->controller->stop_updating ();
	}

	if (output_controls.size() > 0 ) {
		screen_update_connection.disconnect();
	}

	return false;
}

void
GenericPluginUI::output_update ()
{
	for (vector<ControlUI*>::iterator i = output_controls.begin(); i != output_controls.end(); ++i) {
		float val = plugin->get_parameter ((*i)->port_index);
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


