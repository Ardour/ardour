/*
 * Copyright (C) 2022 Robin Gareus <robin@gareus.org>
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

#include <boost/shared_ptr.hpp>

#include <gtkmm/frame.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour/audioengine.h"
#include "ardour/io_plug.h"
#include "ardour/session.h"
#include "ardour/types.h"
#include "ardour/user_bundle.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/menu_elems.h"
#include "gtkmm2ext/utils.h"

#include "widgets/tooltips.h"

#include "context_menu_helper.h"
#include "gui_thread.h"
#include "io_plugin_window.h"
#include "io_button.h"
#include "io_selector.h"
#include "mixer_ui.h"
#include "plugin_selector.h"
#include "plugin_ui.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

IOPluginWindow::IOPluginWindow ()
	: ArdourWindow (_("I/O Plugins"))
	, _box_pre (true)
	, _box_post (false)
{
	Gtk::VBox*           vbox = manage (new Gtk::VBox);
	Gtk::Label*          label;
	Gtk::ScrolledWindow* scroller;

	label = manage (new Label (_("Pre-Process")));
	vbox->pack_start (*label, false, false);

	_box_pre.set_name ("ProcessorList");
	_box_post.set_name ("ProcessorList");

	scroller = manage (new ScrolledWindow);
	scroller->set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);
	scroller->set_shadow_type (Gtk::SHADOW_NONE);
	scroller->set_border_width (0);
	scroller->add (_box_pre);
	vbox->pack_start (*scroller);

	label = manage (new Label (_("Post-Process")));
	vbox->pack_start (*label, false, false);

	scroller = manage (new ScrolledWindow);
	scroller->set_policy (Gtk::POLICY_AUTOMATIC, Gtk::POLICY_NEVER);
	scroller->set_shadow_type (Gtk::SHADOW_NONE);
	scroller->set_border_width (0);
	scroller->add (_box_post);
	vbox->pack_start (*scroller);

	add (*vbox);
	vbox->show_all ();

	set_size_request (PX_SCALE (400), -1);
}

void
IOPluginWindow::set_session (Session* s)
{
	ArdourWindow::set_session (s);
	_box_pre.set_session (s);
	_box_post.set_session (s);

	if (!_session) {
		return;
	}
	refill ();
	_session->IOPluginsChanged.connect (_session_connections, invalidator (*this), boost::bind (&IOPluginWindow::refill, this), gui_context ());
}

void
IOPluginWindow::on_show ()
{
	ArdourWindow::on_show ();
	refill ();
}

void
IOPluginWindow::on_hide ()
{
	ArdourWindow::on_hide ();
}

void
IOPluginWindow::refill ()
{
	_box_pre.clear ();
	_box_post.clear ();
	if (!_session) {
		return;
	}
	boost::shared_ptr<IOPlugList> iop (_session->io_plugs ());
	for (auto& i : *iop) {
		IOPlugUI* iopup = manage (new IOPlugUI (i));
		if (i->is_pre ()) {
			_box_pre.add_child (*iopup);
		} else {
			_box_post.add_child (*iopup);
		}
		iopup->show ();
	}
}

/* ****************************************************************************/

IOPluginWindow::PluginBox::PluginBox (bool is_pre)
	: _is_pre (is_pre)
{
	add_events (Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK);
	signal_button_press_event ().connect (sigc::mem_fun (*this, &IOPluginWindow::PluginBox::button_press_event));
	_base.signal_expose_event ().connect (sigc::bind (sigc::ptr_fun (&ArdourWidgets::ArdourIcon::expose_with_text), &_base, ArdourWidgets::ArdourIcon::ShadedPlusSign, _("Right-click or Double-click here\nto add I/O Plugins")));

	std::vector<Gtk::TargetEntry> target_table;
	target_table.push_back (Gtk::TargetEntry ("x-ardour/plugin.favorite", Gtk::TARGET_SAME_APP)); // from sidebar
	target_table.push_back (Gtk::TargetEntry ("x-ardour/plugin.info", Gtk::TARGET_SAME_APP));     // from plugin-manager
	//target_table.push_back (Gtk::TargetEntry ("x-ardour/plugin.preset", Gtk::TARGET_SAME_APP)); // from processor-box
	_base.drag_dest_set (target_table, DEST_DEFAULT_ALL, Gdk::ACTION_COPY);
	_base.signal_drag_data_received ().connect (sigc::mem_fun (*this, &PluginBox::drag_data_received));

	_hbox.set_spacing (4);
	_top.pack_start (_hbox, false, false);
	_top.pack_end (_base, true, true);
	add (_top);
	set_size_request (-1, PX_SCALE (40));
	show_all ();
}

void
IOPluginWindow::PluginBox::clear ()
{
	container_clear (_hbox, true);
}

void
IOPluginWindow::PluginBox::add_child (Gtk::Widget& w)
{
	_hbox.pack_start (w, false, false);
	queue_resize ();
}

bool
IOPluginWindow::PluginBox::use_plugins (SelectedPlugins const& plugins)
{
	for (SelectedPlugins::const_iterator p = plugins.begin (); p != plugins.end (); ++p) {
		_session->load_io_plugin (boost::shared_ptr<IOPlug> (new IOPlug (*_session, *p, _is_pre)));
	}
	return false;
}

void
IOPluginWindow::PluginBox::load_plugin (PluginPresetPtr const& ppp)
{
	PluginInfoPtr pip = ppp->_pip;
	PluginPtr     p   = pip->load (*_session);
	if (!p) {
		return;
	}
	if (ppp->_preset.valid) {
		p->load_preset (ppp->_preset);
	}
	_session->load_io_plugin (boost::shared_ptr<IOPlug> (new IOPlug (*_session, p, _is_pre)));
}

bool
IOPluginWindow::PluginBox::button_press_event (GdkEventButton* ev)
{
	if (!_session || _session->actively_recording ()) {
		/* swallow event, do nothing */
		return true;
	}

	if (Keyboard::is_context_menu_event (ev)) {
		PluginSelector* ps = Mixer_UI::instance ()->plugin_selector ();
		ps->set_interested_object (*this);
		ps->plugin_menu ()->popup (ev->button, ev->time);
		return true;
	} else if (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS) {
		PluginSelector* ps = Mixer_UI::instance ()->plugin_selector ();
		ps->set_interested_object (*this);
		ps->show_manager ();
		return true;
	}

	return false;
}

void
IOPluginWindow::PluginBox::drag_data_received (Glib::RefPtr<Gdk::DragContext> const& context, int, int, Gtk::SelectionData const& data, guint, guint time)
{
	if (!_session) {
		context->drag_finish (false, false, time);
	} else if (data.get_target () == "x-ardour/plugin.info") {
		auto tv = reinterpret_cast<const Gtkmm2ext::DnDTreeView<ARDOUR::PluginInfoPtr>*> (data.get_data ());

		PluginInfoList nfos;
		TreeView*      source;
		tv->get_object_drag_data (nfos, &source);

		for (auto const& i : nfos) {
			PluginPtr p = i->load (*_session);
			if (p) {
				_session->load_io_plugin (boost::shared_ptr<IOPlug> (new IOPlug (*_session, p, _is_pre)));
			}
		}

	} else if (data.get_target () == "x-ardour/plugin.favorite") {
		auto tv = reinterpret_cast<const Gtkmm2ext::DnDTreeView<ARDOUR::PluginPresetPtr>*> (data.get_data ());

		PluginPresetList nfos;
		TreeView*        source;
		tv->get_object_drag_data (nfos, &source);

		for (auto const& i : nfos) {
			load_plugin (i);
		}

	} else if (data.get_target () == "x-ardour/plugin.preset") {
		const void* d = data.get_data ();
		load_plugin (*(static_cast<const PluginPresetPtr*> (d)));
	} else {
		context->drag_finish (false, false, time);
	}
}

/* ****************************************************************************/

IOPluginWindow::IOPlugUI::IOPlugUI (boost::shared_ptr<ARDOUR::IOPlug> iop)
	: Alignment (0, 0.5, 0, 0)
	, _btn_input (iop->input (), iop->is_pre ())
	, _btn_output (iop->output (), iop->is_pre ())
	, _iop (iop)
{
	_btn_ioplug.set_text (iop->name ());
	if (_iop->is_pre ()) {
		_btn_ioplug.set_name ("processor prefader");
	} else {
		_btn_ioplug.set_name ("processor postfader");
	}

	_btn_ioplug.set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	_btn_ioplug.signal_size_allocate ().connect (sigc::mem_fun (*this, &IOPlugUI::button_resized));

	if (_iop->plugin ()->has_editor ()) {
		set_tooltip (_btn_ioplug, string_compose (_("<b>%1</b>\nDouble-click to show GUI.\n%2+double-click to show generic GUI."), iop->name (), Keyboard::secondary_modifier_name ()));
	} else {
		set_tooltip (_btn_ioplug, string_compose (_("<b>%1</b>\nDouble-click to show generic GUI.%2"), iop->name ()));
	}

	_box.pack_start (_btn_input, true, true);
	_box.pack_start (_btn_ioplug, true, true);
	_box.pack_start (_btn_output, true, true);
	_box.set_border_width (1);

	Gdk::Color bg;
	ARDOUR_UI_UTILS::set_color_from_rgba (bg, UIConfiguration::instance ().color (X_("theme:bg1")));
	_frame.modify_bg (STATE_NORMAL, bg);

	_frame.add (_box);
	_frame.set_size_request (PX_SCALE (100), -1);
	add (_frame);

	if (iop->window_proxy ()) {
		_window_proxy = dynamic_cast<PluginWindowProxy*> (iop->window_proxy ());
		assert (_window_proxy);
	} else {
		_window_proxy = new PluginWindowProxy (string_compose ("IOP-%1", _iop->id ()), _iop);

		const XMLNode* ui_xml = _iop->session ().extra_xml (X_("UI"));
		if (ui_xml) {
			_window_proxy->set_state (*ui_xml, 0);
		}

		iop->set_window_proxy (_window_proxy);
		WM::Manager::instance ().register_window (_window_proxy);
	}

	_btn_ioplug.signal_button_press_event ().connect (sigc::mem_fun (*this, &IOPluginWindow::IOPlugUI::button_press_event), false);
	_iop->DropReferences.connect (_going_away_connection, invalidator (*this), boost::bind (&IOPluginWindow::IOPlugUI::self_delete, this), gui_context ());
	show_all ();
}

void
IOPluginWindow::IOPlugUI::self_delete ()
{
	_iop.reset ();
	_going_away_connection.disconnect ();
	delete this;
}

void
IOPluginWindow::IOPlugUI::self_remove ()
{
	_iop->session ().unload_io_plugin (_iop); // this calls self_delete()
}

void
IOPluginWindow::IOPlugUI::edit_plugin (bool custom_ui)
{
	_window_proxy->set_custom_ui_mode (custom_ui);
	_window_proxy->show_the_right_window ();
	Gtk::Window* tlw = dynamic_cast<Gtk::Window*> (get_toplevel ());
	_window_proxy->get ()->set_transient_for (*tlw);
}

bool
IOPluginWindow::IOPlugUI::button_press_event (GdkEventButton* ev)
{
	if (Keyboard::is_delete_event (ev)) {
		self_remove ();
		return true;
	} else if (Keyboard::is_context_menu_event (ev)) {
		using namespace Gtk::Menu_Helpers;

		Gtk::Menu* m = ARDOUR_UI_UTILS::shared_popup_menu ();
		MenuList& items = m->items ();

		items.push_back (MenuElem (_("Edit.."), sigc::bind (sigc::mem_fun (*this, &IOPluginWindow::IOPlugUI::edit_plugin), true)));
		items.back().set_sensitive (_iop->plugin ()->has_editor ());
		items.push_back (MenuElem (_("Edit with generic controls..."), sigc::bind (sigc::mem_fun (*this, &IOPluginWindow::IOPlugUI::edit_plugin), false)));
		items.push_back (SeparatorElem());
		items.push_back (MenuElem (_("Delete"), sigc::mem_fun (*this, &IOPluginWindow::IOPlugUI::self_remove)));

		m->popup (ev->button, ev->time);
		return true;
	} else if (Keyboard::is_edit_event (ev) || (ev->button == 1 && ev->type == GDK_2BUTTON_PRESS)) {
		edit_plugin (!Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier));
		return true;
	}
	return false;
}

void
IOPluginWindow::IOPlugUI::button_resized (Gtk::Allocation& alloc)
{
	_btn_ioplug.set_layout_ellipsize_width (alloc.get_width () * PANGO_SCALE);
}

/* ****************************************************************************/

IOPluginWindow::PluginWindowProxy::PluginWindowProxy (std::string const& name, boost::weak_ptr<PlugInsertBase> plugin)
	: WM::ProxyBase (name, std::string ())
	, _pib (plugin)
	, _is_custom (true)
	, _want_custom (true)
{
	boost::shared_ptr<PlugInsertBase> p = _pib.lock ();
	if (!p) {
		return;
	}
	p->DropReferences.connect (_going_away_connection, MISSING_INVALIDATOR, boost::bind (&IOPluginWindow::PluginWindowProxy::plugin_going_away, this), gui_context ());
}

IOPluginWindow::PluginWindowProxy::~PluginWindowProxy ()
{
	_window = 0;
}

Gtk::Window*
IOPluginWindow::PluginWindowProxy::get (bool create)
{
	boost::shared_ptr<PlugInsertBase> p = _pib.lock ();
	if (!p) {
		return 0;
	}

	if (_window && (_is_custom != _want_custom)) {
		set_state_mask (WindowProxy::StateMask (state_mask () & ~WindowProxy::Size));
		drop_window ();
	}

	if (!_window) {
		if (!create) {
			return 0;
		}

		_is_custom = _want_custom;
		_window    = new PluginUIWindow (p, false, _is_custom);

		if (_window) {
			boost::shared_ptr<ARDOUR::IOPlug> iop = boost::dynamic_pointer_cast<ARDOUR::IOPlug> (p);
			assert (iop);
			_window->set_title (iop->name ());
			setup ();
			_window->show_all ();
		}
	}
	return _window;
}

void
IOPluginWindow::PluginWindowProxy::show_the_right_window ()
{
	if (_window && (_is_custom != _want_custom)) {
		set_state_mask (WindowProxy::StateMask (state_mask () & ~WindowProxy::Size));
		drop_window ();
	}

	if (_window) {
		_window->unset_transient_for ();
	}
	toggle ();
}

int
IOPluginWindow::PluginWindowProxy::set_state (const XMLNode& node, int)
{
	XMLNodeList                 children = node.children ();
	XMLNodeList::const_iterator i        = children.begin ();
	while (i != children.end ()) {
		std::string name;
		if ((*i)->name () == X_("Window") && (*i)->get_property (X_("name"), name) && name == _name) {
			break;
		}
		++i;
	}

	if (i != children.end ()) {
		(*i)->get_property (X_("custom-ui"), _want_custom);
	}

	return ProxyBase::set_state (node, 0);
}

XMLNode&
IOPluginWindow::PluginWindowProxy::get_state () const
{
	XMLNode* node;
	node = &ProxyBase::get_state ();
	node->set_property (X_("custom-ui"), _is_custom);
	return *node;
}

void
IOPluginWindow::PluginWindowProxy::plugin_going_away ()
{
	delete _window;
	_window = 0;
	WM::Manager::instance ().remove (this);
	_going_away_connection.disconnect ();
	delete this;
}

/* ****************************************************************************/

IOPluginWindow::IOButton::IOButton (boost::shared_ptr<ARDOUR::IO> io, bool pre)
	: _io (io)
	, _pre (pre)
	, _io_selector (0)
{
	set_text (_io->direction () == IO::Input ? _("Input") : _("Output"));
	set_name ("mixer strip button");
	set_text_ellipsize (Pango::ELLIPSIZE_MIDDLE);
	signal_size_allocate ().connect (sigc::mem_fun (*this, &IOButton::button_resized));

	if (io->n_ports ().n_total () == 0) {
		set_sensitive (false);
		return;
	}

	update ();

	signal_button_press_event ().connect (sigc::mem_fun (*this, &IOButton::button_press), false);
	signal_button_release_event ().connect (sigc::mem_fun (*this, &IOButton::button_release), false);

	AudioEngine::instance ()->PortConnectedOrDisconnected.connect (_connections, invalidator (*this), boost::bind (&IOButton::port_connected_or_disconnected, this, _1, _3), gui_context ());
	AudioEngine::instance ()->PortPrettyNameChanged.connect (_connections, invalidator (*this), boost::bind (&IOButton::port_pretty_name_changed, this, _1), gui_context ());

	_io->changed.connect (_connections, invalidator (*this), boost::bind (&IOButton::update, this), gui_context ());
	_io->session ().BundleAddedOrRemoved.connect (_connections, invalidator (*this), boost::bind (&IOButton::update, this), gui_context ());
}

IOPluginWindow::IOButton::~IOButton ()
{
	delete _io_selector;
}

void
IOPluginWindow::IOButton::button_resized (Gtk::Allocation& alloc)
{
	set_layout_ellipsize_width (alloc.get_width () * PANGO_SCALE);
}

void
IOPluginWindow::IOButton::port_pretty_name_changed (std::string pn)
{
	if (_io->connected_to (pn)) {
		update ();
	}
}

void
IOPluginWindow::IOButton::port_connected_or_disconnected (boost::weak_ptr<Port> wa, boost::weak_ptr<Port> wb)
{
	boost::shared_ptr<Port> a = wa.lock ();
	boost::shared_ptr<Port> b = wb.lock ();

	if ((a && _io->has_port (a)) || (b && _io->has_port (b))) {
		update ();
	}
}

void
IOPluginWindow::IOButton::disconnect ()
{
	_io->disconnect (this);
}

void
IOPluginWindow::IOButton::update ()
{
	boost::shared_ptr<ARDOUR::Bundle> bundle;
	_bundle_connections.drop_connections ();

	::IOButton::set_label (*this, _io->session (), bundle, _io);

	if (bundle) {
		bundle->Changed.connect (_bundle_connections, invalidator (*this), boost::bind (&IOButton::update, this), gui_context ());
	}
}

struct RouteCompareByName {
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b)
	{
		return a->name ().compare (b->name ()) < 0;
	}
};

bool
IOPluginWindow::IOButton::button_press (GdkEventButton* ev)
{
	using namespace Gtk::Menu_Helpers;

	MenuList& citems = _menu.items ();
	_menu.set_name ("ArdourContextMenu");
	citems.clear ();

	switch (ev->button) {
		case 3:
			return false;
		case 1:
			break;
		default:
			return true;
	}

	if (_io->connected ()) {
		citems.push_back (MenuElem (_("Disconnect"), sigc::mem_fun (*this, &IOButton::disconnect)));
		citems.push_back (SeparatorElem ());
	}

	uint32_t const n_with_separator = citems.size ();

	boost::shared_ptr<ARDOUR::BundleList> b      = _io->session ().bundles ();
	boost::shared_ptr<ARDOUR::RouteList>  routes = _io->session ().get_routes ();
	RouteList                             copy   = *routes;
	copy.sort (RouteCompareByName ());

	if (_io->direction () == IO::Input) {
		if (_pre) {
			/* list physical sources for io-plugins running before process,
			 * user-bundles first.
			 */
			for (auto const& i : *b) {
				if (boost::dynamic_pointer_cast<UserBundle> (i)) {
					maybe_add_bundle_to_menu (i);
				}
			}
			for (auto const& i : *b) {
				if (boost::dynamic_pointer_cast<UserBundle> (i) == 0) {
					maybe_add_bundle_to_menu (i);
				}
			}
		} else {
			/* route outputs */
			for (auto const& i : copy) {
				if (i->is_foldbackbus ()) {
					continue;
				}
				maybe_add_bundle_to_menu (i->output ()->bundle ());
			}
		}
	} else {
		if (_pre) {
			/* suggest connecting output of io-plugins running before process to route inputs */
			for (auto const& i : copy) {
				if (i->is_foldbackbus () || i->is_monitor ()) {
					continue;
				}
				maybe_add_bundle_to_menu (i->input ()->bundle ());
			}
		} else {
			/* output of post-process plugins go to physical sinks */
			for (auto const& i : *b) {
				if (boost::dynamic_pointer_cast<UserBundle> (i)) {
					maybe_add_bundle_to_menu (i);
				}
			}
			for (auto const& i : *b) {
				if (boost::dynamic_pointer_cast<UserBundle> (i) == 0) {
					maybe_add_bundle_to_menu (i);
				}
			}
		}
	}

	if (n_with_separator != citems.size ()) {
		citems.push_back (SeparatorElem ());
	}

	citems.push_back (MenuElem (_("Routing Grid"), sigc::mem_fun (*this, &IOButton::edit_io_configuration)));

	anchored_menu_popup (&_menu, this, "", 1, ev->time);
	return true;
}

void
IOPluginWindow::IOButton::bundle_chosen (boost::shared_ptr<Bundle> c)
{
	_io->connect_ports_to_bundle (c, true, this);
}

void
IOPluginWindow::IOButton::maybe_add_bundle_to_menu (boost::shared_ptr<Bundle> b)
{
	using namespace Menu_Helpers;

	if (_io->direction () == IO::Input) {
		if (b->ports_are_outputs () == false || b->nchannels () != _io->n_ports ()) {
			return;
		}
	} else {
		if (b->ports_are_inputs () == false || b->nchannels () != _io->n_ports ()) {
			return;
		}
	}

	MenuList& citems = _menu.items ();
	citems.push_back (MenuElemNoMnemonic (b->name (), sigc::bind (sigc::mem_fun (*this, &IOButton::bundle_chosen), b)));
}

bool
IOPluginWindow::IOButton::button_release (GdkEventButton* ev)
{
	if (ev->button == 3) {
		edit_io_configuration ();
	}
	return false;
}

void
IOPluginWindow::IOButton::edit_io_configuration ()
{
	if (_io_selector == 0) {
		_io_selector     = new IOSelectorWindow (&_io->session (), _io);
		Gtk::Widget* top = get_toplevel ();
		if (top) {
			_io_selector->set_transient_for (*dynamic_cast<Gtk::Window*> (top));
		}
	}

	if (_io_selector->get_visible ()) {
		_io_selector->get_toplevel ()->get_window ()->raise ();
	} else {
		_io_selector->present ();
	}
}
