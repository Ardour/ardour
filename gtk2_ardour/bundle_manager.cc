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
#include "ardour/audioengine.h"
#include "bundle_manager.h"
#include "i18n.h"

BundleEditorMatrix::BundleEditorMatrix (
	ARDOUR::Session& session, boost::shared_ptr<ARDOUR::Bundle> bundle
	)
	: PortMatrix (session, bundle->type()),
	  _bundle (bundle)
{
	_port_group = boost::shared_ptr<PortGroup> (new PortGroup (""));
	_port_group->add_bundle (bundle);
	_ports[OURS].add_group (_port_group);
}

void
BundleEditorMatrix::setup ()
{
	_ports[OTHER].gather (_session, _bundle->ports_are_inputs());
	PortMatrix::setup ();
}

void
BundleEditorMatrix::set_state (ARDOUR::BundleChannel c[2], bool s)
{
	ARDOUR::Bundle::PortList const& pl = c[OTHER].bundle->channel_ports (c[OTHER].channel);
	for (ARDOUR::Bundle::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
		if (s) {
			c[OURS].bundle->add_port_to_channel (c[OURS].channel, *i);
		} else {
			c[OURS].bundle->remove_port_from_channel (c[OURS].channel, *i);
		}
	}
}

PortMatrix::State
BundleEditorMatrix::get_state (ARDOUR::BundleChannel c[2]) const
{
	ARDOUR::Bundle::PortList const& pl = c[OTHER].bundle->channel_ports (c[OTHER].channel);
	for (ARDOUR::Bundle::PortList::const_iterator i = pl.begin(); i != pl.end(); ++i) {
		if (!c[OURS].bundle->port_attached_to_channel (c[OURS].channel, *i)) {
			return NOT_ASSOCIATED;
		}
	}

	return ASSOCIATED;
}

void
BundleEditorMatrix::add_channel (boost::shared_ptr<ARDOUR::Bundle> b)
{
	NameChannelDialog d;
	d.set_position (Gtk::WIN_POS_MOUSE);

	if (d.run () != Gtk::RESPONSE_ACCEPT) {
		return;
	}

	_bundle->add_channel (d.get_name());
	setup ();
}

void
BundleEditorMatrix::remove_channel (ARDOUR::BundleChannel bc)
{
	bc.bundle->remove_channel (bc.channel);
	setup ();
}

void
BundleEditorMatrix::rename_channel (ARDOUR::BundleChannel bc)
{
	NameChannelDialog d (bc.bundle, bc.channel);
	d.set_position (Gtk::WIN_POS_MOUSE);

	if (d.run () != Gtk::RESPONSE_ACCEPT) {
		return;
	}

	bc.bundle->set_channel_name (bc.channel, d.get_name ());
}

BundleEditor::BundleEditor (ARDOUR::Session& session, boost::shared_ptr<ARDOUR::UserBundle> bundle, bool add)
	: ArdourDialog (_("Edit Bundle")), _matrix (session, bundle), _bundle (bundle)
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
		_input_or_output.set_active_text (_("Output"));
	} else {
		_input_or_output.set_active_text (_("Input"));
	}

	_input_or_output.signal_changed().connect (sigc::mem_fun (*this, &BundleEditor::input_or_output_changed));

	/* Type (audio or MIDI) */
	a = new Gtk::Alignment (1, 0.5, 0, 1);
	a->add (*Gtk::manage (new Gtk::Label (_("Type:"))));
	t->attach (*Gtk::manage (a), 0, 1, 2, 3, Gtk::FILL, Gtk::FILL);
	a = new Gtk::Alignment (0, 0.5, 0, 1);
	a->add (_type);
	t->attach (*Gtk::manage (a), 1, 2, 2, 3);
	
	_type.append_text (_("Audio"));
	_type.append_text (_("MIDI"));
	
	switch (bundle->type ()) {
	case ARDOUR::DataType::AUDIO:
		_type.set_active_text (_("Audio"));
		break;
	case ARDOUR::DataType::MIDI:
		_type.set_active_text (_("MIDI"));
		break;
	}

	_type.signal_changed().connect (sigc::mem_fun (*this, &BundleEditor::type_changed));
					
	get_vbox()->pack_start (*Gtk::manage (t), false, false);
	get_vbox()->pack_start (_matrix);
	get_vbox()->set_spacing (4);

	/* Add Channel button */
	Gtk::Button* add_channel_button = Gtk::manage (new Gtk::Button (_("Add Channel")));
	add_channel_button->set_name ("IOSelectorButton");
	add_channel_button->set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::ADD, Gtk::ICON_SIZE_BUTTON)));
	get_action_area()->pack_start (*add_channel_button, false, false);
	add_channel_button->signal_clicked().connect (sigc::bind (sigc::mem_fun (_matrix, &BundleEditorMatrix::add_channel), boost::shared_ptr<ARDOUR::Bundle> ()));

	if (add) {
		add_button (Gtk::Stock::CANCEL, 1);
		add_button (Gtk::Stock::ADD, 0);
	} else {
		add_button (Gtk::Stock::CLOSE, 0);
	}

	show_all ();
}

void
BundleEditor::name_changed ()
{
	_bundle->set_name (_name.get_text ());
}

void
BundleEditor::input_or_output_changed ()
{
	if (_input_or_output.get_active_text() == _("Output")) {
		_bundle->set_ports_are_inputs ();
	} else {
		_bundle->set_ports_are_outputs ();
	}

	_matrix.setup ();
}

void
BundleEditor::type_changed ()
{
	ARDOUR::DataType const t = _type.get_active_text() == _("Audio") ?
		ARDOUR::DataType::AUDIO : ARDOUR::DataType::MIDI;

	_bundle->set_type (t);
	_matrix.set_type (t);
}

void
BundleEditor::on_map ()
{
	_matrix.setup ();
	Window::on_map ();
}


BundleManager::BundleManager (ARDOUR::Session& session)
	: ArdourDialog (_("Bundle manager")), _session (session), edit_button (_("Edit")), delete_button (_("Delete"))
{
	_list_model = Gtk::ListStore::create (_list_model_columns);
	_tree_view.set_model (_list_model);
	_tree_view.append_column (_("Name"), _list_model_columns.name);
	_tree_view.set_headers_visible (false);

	boost::shared_ptr<ARDOUR::BundleList> bundles = _session.bundles ();
	for (ARDOUR::BundleList::iterator i = bundles->begin(); i != bundles->end(); ++i) {
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
	delete_button.set_image (*Gtk::manage (new Gtk::Image (Gtk::Stock::DELETE, Gtk::ICON_SIZE_BUTTON)));
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
	boost::shared_ptr<ARDOUR::UserBundle> b (new ARDOUR::UserBundle (""));

	/* Start off with a single channel */
	b->add_channel ("");

	BundleEditor e (_session, b, true);
	if (e.run () == 0) {
		_session.add_bundle (b);
		add_bundle (b);
	}
}

void
BundleManager::edit_clicked ()
{
	Gtk::TreeModel::iterator i = _tree_view.get_selection()->get_selected();
	if (i) {
		boost::shared_ptr<ARDOUR::UserBundle> b = (*i)[_list_model_columns.bundle];
		BundleEditor e (_session, b, false);
		e.run ();
	}
	
}

void
BundleManager::delete_clicked ()
{
	Gtk::TreeModel::iterator i = _tree_view.get_selection()->get_selected();
	if (i) {
		boost::shared_ptr<ARDOUR::UserBundle> b = (*i)[_list_model_columns.bundle];
		_session.remove_bundle (b);
		_list_model->erase (i);
	}
}

void
BundleManager::add_bundle (boost::shared_ptr<ARDOUR::Bundle> b)
{
	boost::shared_ptr<ARDOUR::UserBundle> u = boost::dynamic_pointer_cast<ARDOUR::UserBundle> (b);
	if (u == 0) {
		return;
	}

	Gtk::TreeModel::iterator i = _list_model->append ();
	(*i)[_list_model_columns.name] = u->name ();
	(*i)[_list_model_columns.bundle] = u;

	u->NameChanged.connect (sigc::bind (sigc::mem_fun (*this, &BundleManager::bundle_name_changed), u));
}

void
BundleManager::bundle_name_changed (boost::shared_ptr<ARDOUR::UserBundle> b)
{
	Gtk::TreeModel::iterator i = _list_model->children().begin ();
	while (i != _list_model->children().end()) {
		boost::shared_ptr<ARDOUR::UserBundle> t = (*i)[_list_model_columns.bundle];
		if (t == b) {
			break;
		}
		++i;
	}

	if (i != _list_model->children().end()) {
		(*i)[_list_model_columns.name] = b->name ();
	}
}


NameChannelDialog::NameChannelDialog ()
	: ArdourDialog (_("Add channel")),
	  _adding (true)
{
	setup ();
}

NameChannelDialog::NameChannelDialog (boost::shared_ptr<ARDOUR::Bundle> b, uint32_t c)
	: ArdourDialog (_("Rename channel")),
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

std::string
NameChannelDialog::get_name () const
{
	return _name.get_text ();
}
