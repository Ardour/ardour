/*
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2018 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2018 Robin Gareus <robin@gareus.org>
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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <gtkmm/stock.h>

#include "ardour/lv2_plugin.h"
#include "ardour/session.h"
#include "pbd/error.h"

#include "gui_thread.h"
#include "lv2_plugin_ui.h"
#include "timers.h"

#include "gtkmm2ext/utils.h"

#include "lv2/lv2plug.in/ns/extensions/ui/ui.h"

#include <lilv/lilv.h>
#include <suil/suil.h>

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace PBD;

#define NS_UI "http://lv2plug.in/ns/extensions/ui#"

static SuilHost* ui_host = NULL;

void
LV2PluginUI::write_from_ui(void*       controller,
                           uint32_t    port_index,
                           uint32_t    buffer_size,
                           uint32_t    format,
                           const void* buffer)
{
	LV2PluginUI* me = (LV2PluginUI*)controller;
	if (format == 0) {
		if (port_index >= me->_controllables.size()) {
			return;
		}

		boost::shared_ptr<AutomationControl> ac = me->_controllables[port_index];

		me->_updates.insert (port_index);

		if (ac) {
			ac->set_value(*(const float*)buffer, Controllable::NoGroup);
		}
	} else if (format == URIMap::instance().urids.atom_eventTransfer) {

		const int cnt = me->_pi->get_count();
		for (int i=0; i < cnt; i++ ) {
			boost::shared_ptr<LV2Plugin> lv2i = boost::dynamic_pointer_cast<LV2Plugin> (me->_pi->plugin(i));
			lv2i->write_from_ui(port_index, format, buffer_size, (const uint8_t*)buffer);
		}
	}
}

void
LV2PluginUI::write_to_ui(void*       controller,
                         uint32_t    port_index,
                         uint32_t    buffer_size,
                         uint32_t    format,
                         const void* buffer)
{
	LV2PluginUI* me = (LV2PluginUI*)controller;
	if (me->_inst) {
		suil_instance_port_event((SuilInstance*)me->_inst,
					 port_index, buffer_size, format, buffer);
	}
}

uint32_t
LV2PluginUI::port_index(void* controller, const char* symbol)
{
	return ((LV2PluginUI*)controller)->_lv2->port_index(symbol);
}

void
LV2PluginUI::touch(void*    controller,
                   uint32_t port_index,
                   bool     grabbed)
{
	LV2PluginUI* me = (LV2PluginUI*)controller;
	if (port_index >= me->_controllables.size()) {
		return;
	}
	if (!me->_lv2->parameter_is_control(port_index) || !me->_lv2->parameter_is_input(port_index)) {
		return;
	}

	ControllableRef control = me->_controllables[port_index];
	if (grabbed) {
		control->start_touch (timepos_t (control->session().transport_sample()));
	} else {
		control->stop_touch (timepos_t (control->session().transport_sample()));
	}
}

void
LV2PluginUI::set_path_property (int response,
                                const ParameterDescriptor& desc,
                                Gtk::FileChooserDialog*    widget)
{
	if (response == Gtk::RESPONSE_ACCEPT) {
		plugin->set_property (desc.key, Variant (Variant::PATH, widget->get_filename()));
	}
#if 0
	widget->hide ();
	delete_when_idle (widget);
#else
	delete widget;
#endif
	active_parameter_requests.erase (desc.key);
}

#ifdef HAVE_LV2_1_17_2
LV2UI_Request_Value_Status
LV2PluginUI::request_value(void*                     handle,
                           LV2_URID                  key,
                           LV2_URID                  type,
                           const LV2_Feature* const* features)
{
	LV2PluginUI* me = (LV2PluginUI*)handle;

	const ParameterDescriptor& desc (me->_lv2->get_property_descriptor(key));
	if (desc.key == (uint32_t)-1) {
		return LV2UI_REQUEST_VALUE_ERR_UNKNOWN;
	} else if (desc.datatype != Variant::PATH) {
		return LV2UI_REQUEST_VALUE_ERR_UNSUPPORTED;
	} else if (me->active_parameter_requests.count (key)) {
		return LV2UI_REQUEST_VALUE_BUSY;
	}
	me->active_parameter_requests.insert (key);

	Gtk::FileChooserDialog* lv2ui_file_dialog = new Gtk::FileChooserDialog(desc.label, FILE_CHOOSER_ACTION_OPEN);
	Gtkmm2ext::add_volume_shortcuts (*lv2ui_file_dialog);
	lv2ui_file_dialog->add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	lv2ui_file_dialog->add_button (Gtk::Stock::OPEN, Gtk::RESPONSE_ACCEPT);
	lv2ui_file_dialog->set_default_response(Gtk::RESPONSE_ACCEPT);

	/* this assumes  announce_property_values() was called, or
	 * the plugin has previously sent a patch:Set */
	const Variant& value = me->_lv2->get_property_value (desc.key);
	if (value.type() == Variant::PATH) {
		lv2ui_file_dialog->set_filename (value.get_path());
	}

#if 0 // TODO mime-type, file-extension filter, get from LV2 Parameter Property
	FileFilter file_ext_filter;
	file_ext_filter.add_pattern ("*.foo");
	file_ext_filter.set_name ("Foo File");
	lv2ui_file_dialog.add_filter (file_ext_filter);
#endif

	lv2ui_file_dialog->signal_response().connect (sigc::bind (sigc::mem_fun (*me, &LV2PluginUI::set_path_property), desc, lv2ui_file_dialog));
	lv2ui_file_dialog->present();
	return LV2UI_REQUEST_VALUE_SUCCESS;
}
#endif

void
LV2PluginUI::update_timeout()
{
	_lv2->emit_to_ui(this, &LV2PluginUI::write_to_ui);
}

void
LV2PluginUI::on_external_ui_closed(void* controller)
{
	//printf("LV2PluginUI::on_external_ui_closed\n");
	LV2PluginUI* me = (LV2PluginUI*)controller;
	me->_screen_update_connection.disconnect();
	me->_external_ui_ptr = NULL;
}

void
LV2PluginUI::control_changed (uint32_t port_index)
{
	/* Must run in GUI thread because we modify _updates with no lock */
	if (_lv2->get_parameter (port_index) != _values_last_sent_to_ui[port_index]) {
		/* current plugin parameter does not match last value received
		   from GUI, so queue an update to push it to the GUI during
		   our regular timeout.
		*/
		_updates.insert (port_index);
	}
}

bool
LV2PluginUI::start_updating(GdkEventAny*)
{
	_screen_update_connection.disconnect();
	_screen_update_connection = Timers::super_rapid_connect
		(sigc::mem_fun(*this, &LV2PluginUI::output_update));
	return false;
}

bool
LV2PluginUI::stop_updating(GdkEventAny*)
{
	//cout << "stop_updating" << endl;
	_screen_update_connection.disconnect();
	return false;
}

void
LV2PluginUI::queue_port_update()
{
	const uint32_t num_ports = _lv2->num_ports();
	for (uint32_t i = 0; i < num_ports; ++i) {
		bool     ok;
		uint32_t port = _lv2->nth_parameter(i, ok);
		if (ok) {
			_updates.insert (port);
		}
	}
}

void
LV2PluginUI::output_update()
{
	//cout << "output_update" << endl;
	if (_external_ui_ptr) {
		LV2_EXTERNAL_UI_RUN(_external_ui_ptr);
		if (_lv2->is_external_kx() && !_external_ui_ptr) {
			// clean up external UI if it closes itself via
			// on_external_ui_closed() during run()
			//printf("LV2PluginUI::output_update -- UI was closed\n");
			//_screen_update_connection.disconnect();
			_message_update_connection.disconnect();
			if (_inst) {
				suil_instance_free((SuilInstance*)_inst);
			}
			_inst = NULL;
			_external_ui_ptr = NULL;
			return;
		}
	}

	if (!_inst) {
		return;
	}

	/* output ports (values set by DSP) need propagating to GUI */

	uint32_t nports = _output_ports.size();
	for (uint32_t i = 0; i < nports; ++i) {
		uint32_t index = _output_ports[i];
		float val = _lv2->get_parameter (index);

		if (val != _values_last_sent_to_ui[index]) {
			/* Send to GUI */
			suil_instance_port_event ((SuilInstance*)_inst, index, 4, 0, &val);
			/* Cache current value */
			_values_last_sent_to_ui[index] = val;
		}
	}

	/* Input ports marked for update because the control value changed
	   since the last redisplay.
	*/

	for (Updates::iterator i = _updates.begin(); i != _updates.end(); ++i) {
		float val = _lv2->get_parameter (*i);
		/* push current value to the GUI */
		suil_instance_port_event ((SuilInstance*)_inst, (*i), 4, 0, &val);
		_values_last_sent_to_ui[(*i)] = val;
	}

	_updates.clear ();
}

LV2PluginUI::LV2PluginUI(boost::shared_ptr<PluginInsert> pi,
                         boost::shared_ptr<LV2Plugin>    lv2p)
	: PlugUIBase(pi)
	, _pi(pi)
	, _lv2(lv2p)
	, _gui_widget(NULL)
	, _values_last_sent_to_ui(NULL)
	, _external_ui_ptr(NULL)
	, _inst(NULL)
{
	_ardour_buttons_box.set_spacing (6);
	_ardour_buttons_box.set_border_width (6);
	add_common_widgets (&_ardour_buttons_box);

	plugin->PresetLoaded.connect (*this, invalidator (*this), boost::bind (&LV2PluginUI::queue_port_update, this), gui_context ());
}

void
LV2PluginUI::lv2ui_instantiate(const std::string& title)
{
	bool          is_external_ui = _lv2->is_external_ui();
	LV2_Feature** features_src   = const_cast<LV2_Feature**>(_lv2->features());
	LV2_Feature** features       = const_cast<LV2_Feature**>(_lv2->features());
	size_t        features_count = 0;
	while (*features++) {
		++features_count;
	}

	if (is_external_ui) {
		features = (LV2_Feature**)malloc(sizeof(LV2_Feature*) * (features_count + 4));
	} else {
		features = (LV2_Feature**)malloc(sizeof(LV2_Feature*) * (features_count + 3));
	}

	size_t fi = 0;
	for (; fi < features_count; ++fi) {
		features[fi] = features_src[fi];
	}

#ifdef HAVE_LV2_1_17_2
	_lv2ui_request_value.handle  = this;
	_lv2ui_request_value.request = LV2PluginUI::request_value;
	_lv2ui_request_feature.URI   = LV2_UI__requestValue;
	_lv2ui_request_feature.data  = &_lv2ui_request_value;

	features[fi++] = &_lv2ui_request_feature;
#endif

	Gtk::Alignment* container = NULL;
	if (is_external_ui) {
		_external_ui_host.ui_closed       = LV2PluginUI::on_external_ui_closed;
		_external_ui_host.plugin_human_id = strdup(title.c_str());

		_external_ui_feature.URI  = LV2_EXTERNAL_UI_URI;
		_external_ui_feature.data = &_external_ui_host;

		_external_kxui_feature.URI  = LV2_EXTERNAL_UI_KX__Host;
		_external_kxui_feature.data = &_external_ui_host;

		features[fi++] = &_external_kxui_feature;
		features[fi++] = &_external_ui_feature;
	} else {
		if (_ardour_buttons_box.get_parent()) {
			_ardour_buttons_box.get_parent()->remove(_ardour_buttons_box);
		}
		pack_start(_ardour_buttons_box, false, false);
		_ardour_buttons_box.show_all();

		_gui_widget = Gtk::manage((container = new Gtk::Alignment()));
		pack_start(*_gui_widget, true, true);
		_gui_widget->show();

		_parent_feature.URI  = LV2_UI__parent;
		_parent_feature.data = _gui_widget->gobj();

		features[fi++] = &_parent_feature;
	}

	features[fi] = NULL;
#ifdef HAVE_LV2_1_17_2
	assert (fi == features_count + (is_external_ui ? 3 : 2));
#else
	assert (fi == features_count + (is_external_ui ? 2 : 1));
#endif

	if (!ui_host) {
		ui_host = suil_host_new(LV2PluginUI::write_from_ui,
		                        LV2PluginUI::port_index,
		                        NULL, NULL);
		suil_host_set_touch_func(ui_host, LV2PluginUI::touch);
	}
	const char* container_type = (is_external_ui)
		? NS_UI "external"
		: NS_UI "GtkUI";

	if (_lv2->has_message_output()) {
		_lv2->enable_ui_emission();
	}

	const LilvUI*   ui     = (const LilvUI*)_lv2->c_ui();
	const LilvNode* bundle = lilv_ui_get_bundle_uri(ui);
	const LilvNode* binary = lilv_ui_get_binary_uri(ui);
	char* ui_bundle_path = lilv_file_uri_parse(lilv_node_as_uri(bundle), NULL);
	char* ui_binary_path = lilv_file_uri_parse(lilv_node_as_uri(binary), NULL);
	if (!ui_bundle_path || !ui_binary_path) {
		error << _("failed to get path for UI bindle or binary") << endmsg;
		free(ui_bundle_path);
		free(ui_binary_path);
		free(features);
		return;
	}

	_inst = suil_instance_new(
		ui_host,
		this,
		container_type,
		_lv2->uri(),
		lilv_node_as_uri(lilv_ui_get_uri(ui)),
		lilv_node_as_uri((const LilvNode*)_lv2->c_ui_type()),
		ui_bundle_path,
		ui_binary_path,
		features);

	free(ui_bundle_path);
	free(ui_binary_path);
	free(features);

	if (!_inst) {
		error << _("failed to instantiate LV2 GUI") << endmsg;
		return;
	}

#define GET_WIDGET(inst) suil_instance_get_widget((SuilInstance*)inst);

	const uint32_t num_ports = _lv2->num_ports();
	for (uint32_t i = 0; i < num_ports; ++i) {
		if (_lv2->parameter_is_output(i)
		    && _lv2->parameter_is_control(i)
		    && is_update_wanted(i)) {
			_output_ports.push_back(i);
		}
	}

	_external_ui_ptr = NULL;
	if (!is_external_ui) {
		GtkWidget* c_widget = (GtkWidget*)GET_WIDGET(_inst);
		if (!c_widget) {
			error << _("failed to get LV2 UI widget") << endmsg;
			suil_instance_free((SuilInstance*)_inst);
			_inst = NULL;
			return;
		}
		if (!container->get_child()) {
			// Suil didn't add the UI to the container for us, so do it now
			container->add(*Gtk::manage(Glib::wrap(c_widget)));
		}
		container->show_all();
		gtk_widget_set_can_focus(c_widget, true);
		gtk_widget_grab_focus(c_widget);
	} else {
		_external_ui_ptr = (struct lv2_external_ui*)GET_WIDGET(_inst);
	}

	_values_last_sent_to_ui = new float[num_ports];
	_controllables.resize(num_ports);

	for (uint32_t i = 0; i < num_ports; ++i) {
		bool     ok;
		uint32_t port = _lv2->nth_parameter(i, ok);
		if (ok) {
			/* Cache initial value of the parameter, regardless of
			   whether it is input or output
			*/

			_values_last_sent_to_ui[port]        = _lv2->get_parameter(port);
			_controllables[port] = boost::dynamic_pointer_cast<ARDOUR::AutomationControl> (
				insert->control(Evoral::Parameter(PluginAutomation, 0, port)));

			if (_lv2->parameter_is_control(port) && _lv2->parameter_is_input(port)) {
				if (_controllables[port]) {
					_controllables[port]->Changed.connect (control_connections, invalidator (*this), boost::bind (&LV2PluginUI::control_changed, this, port), gui_context());
				}
			}

			/* queue for first update ("push") to GUI */
			_updates.insert (port);
		}
	}

	if (_lv2->has_message_output()) {
		_message_update_connection = Timers::super_rapid_connect (
			sigc::mem_fun(*this, &LV2PluginUI::update_timeout));
	}
}

void
LV2PluginUI::grab_focus()
{
	if (_inst && !_lv2->is_external_ui()) {
		GtkWidget* c_widget = (GtkWidget*)GET_WIDGET(_inst);
		gtk_widget_grab_focus(c_widget);
	}
}

void
LV2PluginUI::lv2ui_free()
{
	stop_updating (0);

	if (_gui_widget) {
		remove (*_gui_widget);
		_gui_widget = NULL;
	}

	if (_inst) {
		suil_instance_free((SuilInstance*)_inst);
		_inst = NULL;
	}
}

LV2PluginUI::~LV2PluginUI ()
{
	delete [] _values_last_sent_to_ui;

	_message_update_connection.disconnect();
	_screen_update_connection.disconnect();

	if (_external_ui_ptr && _lv2->is_external_kx()) {
		LV2_EXTERNAL_UI_HIDE(_external_ui_ptr);
	}
	lv2ui_free();
	_external_ui_ptr = NULL;
}

int
LV2PluginUI::get_preferred_height()
{
	Gtk::Requisition r = size_request();
	return r.height;
}

int
LV2PluginUI::get_preferred_width()
{
	Gtk::Requisition r = size_request();
	return r.width;
}

bool
LV2PluginUI::resizable()
{
	return _lv2->ui_is_resizable();
}

int
LV2PluginUI::package(Gtk::Window& win)
{
	if (!_external_ui_ptr) {
		/* forward configure events to plugin window */
		win.signal_configure_event().connect(
			sigc::mem_fun(*this, &LV2PluginUI::configure_handler));
		win.signal_map_event().connect(
			sigc::mem_fun(*this, &LV2PluginUI::start_updating));
		win.signal_unmap_event().connect(
			sigc::mem_fun(*this, &LV2PluginUI::stop_updating));
	}
	return 0;
}

bool
LV2PluginUI::configure_handler(GdkEventConfigure*)
{
	std::cout << "CONFIGURE" << std::endl;
	return false;
}

bool
LV2PluginUI::is_update_wanted(uint32_t /*index*/)
{
	/* FIXME: use port notification properties
	   and/or new UI extension subscription methods
	*/
	return true;
}

bool
LV2PluginUI::on_window_show(const std::string& title)
{
	//cout << "on_window_show - " << title << endl; flush(cout);

	if (_lv2->is_external_ui()) {
		if (_external_ui_ptr) {
			_screen_update_connection.disconnect();
			_message_update_connection.disconnect();
			LV2_EXTERNAL_UI_SHOW(_external_ui_ptr);
			_screen_update_connection = Timers::super_rapid_connect
		        (sigc::mem_fun(*this, &LV2PluginUI::output_update));
			if (_lv2->has_message_output()) {
				_message_update_connection = Timers::super_rapid_connect (
					sigc::mem_fun(*this, &LV2PluginUI::update_timeout));
			}
			return false;
		}
		lv2ui_instantiate(title);
		if (!_external_ui_ptr) {
			return false;
		}

		_screen_update_connection.disconnect();
		_message_update_connection.disconnect();
		LV2_EXTERNAL_UI_SHOW(_external_ui_ptr);
		_screen_update_connection = Timers::super_rapid_connect
			(sigc::mem_fun(*this, &LV2PluginUI::output_update));
		if (_lv2->has_message_output()) {
			_message_update_connection = Timers::super_rapid_connect (
				sigc::mem_fun(*this, &LV2PluginUI::update_timeout));
		}
		return false;
	} else {
		lv2ui_instantiate("gtk2gui");
	}

	return _inst ? true : false;
}

void
LV2PluginUI::on_window_hide()
{
	//printf("LV2PluginUI::on_window_hide\n");

	if (_lv2->is_external_ui()) {
		if (!_external_ui_ptr) { return; }
		LV2_EXTERNAL_UI_HIDE(_external_ui_ptr);
		if (!_lv2->is_external_kx()) { return ; }
		_message_update_connection.disconnect();
		_screen_update_connection.disconnect();
		_external_ui_ptr = NULL;
		suil_instance_free((SuilInstance*)_inst);
		_inst = NULL;
	} else {
		lv2ui_free();
	}
}
