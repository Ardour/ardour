/*
    Copyright (C) 2007 Paul Davis

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

#include <gtkmm/stock.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/entry.h>
#include <gtkmm/table.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/alignment.h>

#include "ardour/session.h"
#include "ardour/user_bundle.h"
#include "bundle_manager.h"
#include "gui_thread.h"
#include "i18n.h"
#include "utils.h"

using namespace std;
using namespace ARDOUR;

BundleEditorMatrix::BundleEditorMatrix (Gtk::Window* parent, Session* session, boost::shared_ptr<Bundle> bundle)
	: PortMatrix (parent, session, DataType::NIL)
	, _bundle (bundle)
{
	_port_group = boost::shared_ptr<PortGroup> (new PortGroup (""));
	_port_group->add_bundle (_bundle);

	setup_all_ports ();
	init ();
}

void
BundleEditorMatrix::setup_ports (int dim)
{
	if (dim == OURS) {
		_ports[OURS].clear ();
		_ports[OURS].add_group (_port_group);
	} else {
		_ports[OTHER].suspend_signals ();

		/* when we gather, allow the matrix to contain bundles with duplicate port sets,
		   otherwise in some cases the basic system IO ports may be hidden, making
		   the bundle editor useless */

		_ports[OTHER].gather (_session, DataType::NIL, _bundle->ports_are_inputs(), true, show_only_bundles ());
		_ports[OTHER].remove_bundle (_bundle);
		_ports[OTHER].resume_signals ();
	}
}

void
BundleEditorMatrix::set_state (BundleChannel c[2], bool s)
{
	Bundle::PortList const& pl = c[OTHER].bundle->channel_ports (c[OTHER].channel);
	for (Bundle::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
		if (s) {
			c[OURS].bundle->add_port_to_channel (c[OURS].channel, *i);
		} else {
			c[OURS].bundle->remove_port_from_channel (c[OURS].channel, *i);
		}
	}
}

PortMatrixNode::State
BundleEditorMatrix::get_state (BundleChannel c[2]) const
{
	if (c[0].bundle->nchannels() == ChanCount::ZERO || c[1].bundle->nchannels() == ChanCount::ZERO) {
		return PortMatrixNode::NOT_ASSOCIATED;
	}

	Bundle::PortList const& pl = c[OTHER].bundle->channel_ports (c[OTHER].channel);
	if (pl.empty ()) {
		return PortMatrixNode::NOT_ASSOCIATED;
	}

	for (Bundle::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
		if (!c[OURS].bundle->port_attached_to_channel (c[OURS].channel, *i)) {
			return PortMatrixNode::NOT_ASSOCIATED;
		}
	}

	return PortMatrixNode::ASSOCIATED;
}

bool
BundleEditorMatrix::can_add_channels (boost::shared_ptr<Bundle> b) const
{
	if (b == _bundle) {
		return true;
	}

	return PortMatrix::can_add_channels (b);
}

void
BundleEditorMatrix::add_channel (boost::shared_ptr<Bundle> b, DataType t)
{
	if (b == _bundle) {

		NameChannelDialog d;

		if (d.run () != Gtk::RESPONSE_ACCEPT) {
			return;
		}

		_bundle->add_channel (d.get_name(), t);
		setup_ports (OURS);

	} else {

		PortMatrix::add_channel (b, t);

	}
}

bool
BundleEditorMatrix::can_remove_channels (boost::shared_ptr<Bundle> b) const
{
	if (b == _bundle) {
		return true;
	}

	return PortMatrix::can_remove_channels (b);
}

void
BundleEditorMatrix::remove_channel (BundleChannel bc)
{
	bc.bundle->remove_channel (bc.channel);
	setup_ports (OURS);
}

bool
BundleEditorMatrix::can_rename_channels (boost::shared_ptr<Bundle> b) const
{
	if (b == _bundle) {
		return true;
	}

	return PortMatrix::can_rename_channels (b);
}

void
BundleEditorMatrix::rename_channel (BundleChannel bc)
{
	NameChannelDialog d (bc.bundle, bc.channel);

	if (d.run () != Gtk::RESPONSE_ACCEPT) {
		return;
	}

	bc.bundle->set_channel_name (bc.channel, d.get_name ());
}

bool
BundleEditorMatrix::list_is_global (int dim) const
{
	return (dim == OTHER);
}

string
BundleEditorMatrix::disassociation_verb () const
{
	return _("Disassociate");
}

BundleEditor::BundleEditor (Session* session, boost::shared_ptr<UserBundle> bundle)
	: ArdourDialog (_("Edit Bundle")), _matrix (this, session, bundle), _bundle (bundle)
{
	Gtk::Table* t = new Gtk::Table (3, 2);
	t->set_spacings (4);

	/* Bundle name */
	Gtk::Alignment* a = new Gtk::Alignment (1, 0.5, 0, 1);
	a->add (*Gtk::manage (new Gtk::Label (_("Name:"))));
	t->attach (*Gtk::manage (a), 0, 1, 0, 1, Gtk::FILL, Gtk::FILL);
	t->attach (_name, 1, 2, 0, 1);
	_name.set_text (_bundle->name ());
	_name.signal_changed().connect (sigc::mem_fun (*this, &BundleEditor::name_changed));

	/* Direction (input or output) */
	a = new Gtk::Alignment (1, 0.5, 0, 1);
	a->add (*Gtk::manage (new Gtk::Label (_("Direction:"))));
	t->attach (*Gtk::manage (a), 0, 1, 1, 2, Gtk::FILL, Gtk::FILL);
	a = new Gtk::Alignment (0, 0.5, 0, 1);
	a->add (_input_or_output);
	t->attach (*Gtk::manage (a), 1, 2, 1, 2);
	_input_or_output.append_text (_("Input"));
	_input_or_output.append_text (_("Output"));

	if (bundle->ports_are_inputs()) {
		_input_or_output.set_active_text (_("Input"));
	} else {
		_input_or_output.set_active_text (_("Output"));
	}

	_input_or_output.signal_changed().connect (sigc::mem_fun (*this, &BundleEditor::input_or_output_changed));

	get_vbox()->pack_start (*Gtk::manage (t), false, false);
	get_vbox()->pack_start (_matrix);
	get_vbox()->set_spacing (4);

	add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_ACCEPT);
	show_all ();

	signal_key_press_event().connect (sigc::mem_fun (_matrix, &BundleEditorMatrix::key_press));
}

void
BundleEditor::on_show ()
{
	Gtk::Window::on_show ();
	pair<uint32_t, uint32_t> const pm_max = _matrix.max_size ();
	resize_window_to_proportion_of_monitor (this, pm_max.first, pm_max.second);
}

void
BundleEditor::name_changed ()
{
	_bundle->set_name (_name.get_text ());
}

void
BundleEditor::input_or_output_changed ()
{
	_bundle->remove_ports_from_channels ();

	if (_input_or_output.get_active_text() == _("Output")) {
		_bundle->set_ports_are_outputs ();
	} else {
		_bundle->set_ports_are_inputs ();
	}

	_matrix.setup_all_ports ();
}

void
BundleEditor::on_map ()
{
	_matrix.setup_all_ports ();
	Window::on_map ();
}


BundleManager::BundleManager (Session* session)
	: ArdourDialog (_("Bundle Manager"))
	, edit_button (_("Edit"))
	, delete_button (_("Delete"))
{
	set_session (session);

	_list_model = Gtk::ListStore::create (_list_model_columns);
	_tree_view.set_model (_list_model);
	_tree_view.append_column (_("Name"), _list_model_columns.name);
	_tree_view.set_headers_visible (false);

	boost::shared_ptr<BundleList> bundles = _session->bundles ();
	for (BundleList::iterator i = bundles->begin(); i != bundles->end(); ++i) {
		add_bundle (*i);
	}

	/* New / Edit / Delete buttons */
	Gtk::VBox* buttons = new Gtk::VBox;
	buttons->set_spacing (8);
	Gtk::Button* b = new Gtk::Button (_("New"));
	b->set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::NEW, Gtk::ICON_SIZE_BUTTON)));
	b->signal_clicked().connect (sigc::mem_fun (*this, &BundleManager::new_clicked));
	buttons->pack_start (*Gtk::manage (b), false, false);
	edit_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::EDIT, Gtk::ICON_SIZE_BUTTON)));
	edit_button.signal_clicked().connect (sigc::mem_fun (*this, &BundleManager::edit_clicked));
	buttons->pack_start (edit_button, false, false);
	delete_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::StockID(GTK_STOCK_DELETE), Gtk::ICON_SIZE_BUTTON)));
	delete_button.signal_clicked().connect (sigc::mem_fun (*this, &BundleManager::delete_clicked));
	buttons->pack_start (delete_button, false, false);

	Gtk::HBox* h = new Gtk::HBox;
	h->set_spacing (8);
	h->set_border_width (8);
	h->pack_start (_tree_view);
	h->pack_start (*Gtk::manage (buttons), false, false);

	get_vbox()->set_spacing (8);
	get_vbox()->pack_start (*Gtk::manage (h));

	set_default_size (480, 240);

	_tree_view.get_selection()->signal_changed().connect (
		sigc::mem_fun (*this, &BundleManager::set_button_sensitivity)
		);

	_tree_view.signal_row_activated().connect (
		sigc::mem_fun (*this, &BundleManager::row_activated)
		);

	Gtk::Button* close_but = add_button (Gtk::Stock::CLOSE, Gtk::RESPONSE_ACCEPT);
	close_but->signal_clicked ().connect (sigc::mem_fun (*this, &Gtk::Window::hide));

	set_button_sensitivity ();

	show_all ();
}

void
BundleManager::set_button_sensitivity ()
{
	bool const sel = (_tree_view.get_selection()->get_selected() != 0);
	edit_button.set_sensitive (sel);
	delete_button.set_sensitive (sel);
}


void
BundleManager::new_clicked ()
{
	boost::shared_ptr<UserBundle> b (new UserBundle (_("Bundle")));

	/* Start off with a single channel */
	/* XXX: allow user to specify type */
	b->add_channel ("1", DataType::AUDIO);

	_session->add_bundle (b);
	add_bundle (b);

	BundleEditor e (_session, b);
	e.run ();
}

void
BundleManager::edit_clicked ()
{
	Gtk::TreeModel::iterator i = _tree_view.get_selection()->get_selected();
	if (i) {
		boost::shared_ptr<UserBundle> b = (*i)[_list_model_columns.bundle];
		BundleEditor e (_session, b);
		e.run ();
	}
}

void
BundleManager::delete_clicked ()
{
	Gtk::TreeModel::iterator i = _tree_view.get_selection()->get_selected();
	if (i) {
		boost::shared_ptr<UserBundle> b = (*i)[_list_model_columns.bundle];
		_session->remove_bundle (b);
		_list_model->erase (i);
	}
}

void
BundleManager::add_bundle (boost::shared_ptr<Bundle> b)
{
	boost::shared_ptr<UserBundle> u = boost::dynamic_pointer_cast<UserBundle> (b);
	if (u == 0) {
		return;
	}

	Gtk::TreeModel::iterator i = _list_model->append ();
	(*i)[_list_model_columns.name] = u->name ();
	(*i)[_list_model_columns.bundle] = u;

	u->Changed.connect (bundle_connections, invalidator (*this), boost::bind (&BundleManager::bundle_changed, this, _1, u), gui_context());
}

void
BundleManager::bundle_changed (Bundle::Change c, boost::shared_ptr<UserBundle> b)
{
	if ((c & Bundle::NameChanged) == 0) {
		return;
	}

	Gtk::TreeModel::iterator i = _list_model->children().begin ();
	while (i != _list_model->children().end()) {
		boost::shared_ptr<UserBundle> t = (*i)[_list_model_columns.bundle];
		if (t == b) {
			break;
		}
		++i;
	}

	if (i != _list_model->children().end()) {
		(*i)[_list_model_columns.name] = b->name ();
	}
}

void
BundleManager::row_activated (Gtk::TreeModel::Path const & p, Gtk::TreeViewColumn*)
{
	Gtk::TreeModel::iterator i = _list_model->get_iter (p);
	if (!i) {
		return;
	}

	boost::shared_ptr<UserBundle> b = (*i)[_list_model_columns.bundle];
	BundleEditor e (_session, b);
	e.run ();
}

NameChannelDialog::NameChannelDialog ()
	: ArdourDialog (_("Add Channel")),
	  _adding (true)
{
	setup ();
}

NameChannelDialog::NameChannelDialog (boost::shared_ptr<Bundle> b, uint32_t c)
	: ArdourDialog (_("Rename Channel")),
	  _bundle (b),
	  _channel (c),
	  _adding (false)
{
	_name.set_text (b->channel_name (c));

	setup ();
}

void
NameChannelDialog::setup ()
{
	Gtk::HBox* box = Gtk::manage (new Gtk::HBox ());

	box->pack_start (*Gtk::manage (new Gtk::Label (_("Name"))));
	box->pack_start (_name);
	_name.set_activates_default (true);

	get_vbox ()->pack_end (*box);
	box->show_all ();

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	if (_adding) {
		add_button (Gtk::Stock::ADD, Gtk::RESPONSE_ACCEPT);
	} else {
		add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_ACCEPT);
	}
	set_default_response (Gtk::RESPONSE_ACCEPT);
}

string
NameChannelDialog::get_name () const
{
	return _name.get_text ();
}

