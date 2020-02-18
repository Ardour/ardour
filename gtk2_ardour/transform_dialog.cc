/*
 * Copyright (C) 2014-2019 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>

#include "transform_dialog.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

TransformDialog::Model::Model()
	: source_list(Gtk::ListStore::create(source_cols))
	, property_list(Gtk::ListStore::create(property_cols))
	, operator_list(Gtk::ListStore::create(operator_cols))
{
	static const char* source_labels[] = {
		/* no NOTHING */
		_("this note's"),
		_("the previous note's"),
		_("this note's index"),
		_("the number of notes"),
		_("exactly"),
		_("a random number from"),
		NULL
	};
	for (int s = 0; source_labels[s]; ++s) {
		Gtk::TreeModel::Row row = *(source_list->append());
		row[source_cols.source] = (Source)(s + 1);  // Skip NOTHING
		row[source_cols.label]  = source_labels[s];
	}
	// Special row for ramp, which doesn't correspond to a source
	Gtk::TreeModel::Row row = *(source_list->append());
	row[source_cols.source] = Value::NOWHERE;
	row[source_cols.label]  = _("equal steps from");

	static const char* property_labels[] = {
		_("note number"),
		_("velocity"),
		_("start time"),
		_("length"),
		_("channel"),
		NULL
	};
	for (int p = 0; property_labels[p]; ++p) {
		Gtk::TreeModel::Row row = *(property_list->append());
		row[property_cols.property] = (Property)p;
		row[property_cols.label]    = property_labels[p];
	}

	static const char* operator_labels[] = {
		/* no PUSH */ "+", "-", "*", "/", "mod", NULL
	};
	for (int o = 0; operator_labels[o]; ++o) {
		Gtk::TreeModel::Row row = *(operator_list->append());
		row[operator_cols.op]    = (Operator)(o + 1);  // Skip PUSH
		row[operator_cols.label] = operator_labels[o];
	}
}

TransformDialog::TransformDialog()
	: ArdourDialog(_("Transform"), false, false)
{
	_property_combo.set_model(_model.property_list);
	_property_combo.pack_start(_model.property_cols.label);
	_property_combo.set_active(1);
	_property_combo.signal_changed().connect(
		sigc::mem_fun(this, &TransformDialog::property_changed));

	Gtk::HBox* property_hbox = Gtk::manage(new Gtk::HBox);
	property_hbox->pack_start(*Gtk::manage(new Gtk::Label(_("Set "))), false, false);
	property_hbox->pack_start(_property_combo, false, false);
	property_hbox->pack_start(*Gtk::manage(new Gtk::Label(_(" to "))), false, false);

	_seed_chooser = Gtk::manage(new ValueChooser(_model));
	_seed_chooser->set_target_property(MidiModel::NoteDiffCommand::Velocity);
	_seed_chooser->source_combo.set_active(0);
	property_hbox->pack_start(*_seed_chooser, false, false);

	Gtk::HBox* add_hbox = Gtk::manage(new Gtk::HBox);
	_add_button.add(
		*Gtk::manage(new Gtk::Image(Gtk::Stock::ADD, Gtk::ICON_SIZE_BUTTON)));
	add_hbox->pack_start(_add_button, false, false);
	_add_button.signal_clicked().connect(
		sigc::mem_fun(*this, &TransformDialog::add_clicked));

	get_vbox()->set_spacing(6);
	get_vbox()->pack_start(*property_hbox, false, false);
	get_vbox()->pack_start(_operations_box, false, false);
	get_vbox()->pack_start(*add_hbox, false, false);

	add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button(_("Transform"), Gtk::RESPONSE_OK);

	show_all();
	_seed_chooser->value_spinner.hide();
	_seed_chooser->source_changed();
}

TransformDialog::ValueChooser::ValueChooser(const Model& model)
	: model(model)
	, target_property((Property)1)
	, to_label(" to ")
{
	source_combo.set_model(model.source_list);
	source_combo.pack_start(model.source_cols.label);
	source_combo.signal_changed().connect(
		sigc::mem_fun(this, &TransformDialog::ValueChooser::source_changed));

	property_combo.set_model(model.property_list);
	property_combo.pack_start(model.property_cols.label);

	set_spacing(4);
	pack_start(source_combo, false, false);
	pack_start(property_combo, false, false);
	pack_start(value_spinner, false, false);
	pack_start(to_label, false, false);
	pack_start(max_spinner, false, false);
	show_all();

	source_combo.set_active(4);
	property_combo.set_active(1);
	set_target_property(MidiModel::NoteDiffCommand::Velocity);
	max_spinner.set_value(127);
	source_changed();
}

static void
set_spinner_for(Gtk::SpinButton&                     spinner,
                MidiModel::NoteDiffCommand::Property prop)
{
	switch (prop) {
	case MidiModel::NoteDiffCommand::NoteNumber:
	case MidiModel::NoteDiffCommand::Velocity:
		spinner.get_adjustment()->set_lower(1); // no 0, note off
		spinner.get_adjustment()->set_upper(127);
		spinner.get_adjustment()->set_step_increment(1);
		spinner.get_adjustment()->set_page_increment(10);
		spinner.set_digits(0);
		break;
	case MidiModel::NoteDiffCommand::StartTime:
		spinner.get_adjustment()->set_lower(0);
		spinner.get_adjustment()->set_upper(1024);
		spinner.get_adjustment()->set_step_increment(0.125);
		spinner.get_adjustment()->set_page_increment(1.0);
		spinner.set_digits(2);
		break;
	case MidiModel::NoteDiffCommand::Length:
		spinner.get_adjustment()->set_lower(1.0 / 64.0);
		spinner.get_adjustment()->set_upper(32);
		spinner.get_adjustment()->set_step_increment(1.0 / 64.0);
		spinner.get_adjustment()->set_page_increment(1.0);
		spinner.set_digits(2);
		break;
	case MidiModel::NoteDiffCommand::Channel:
		spinner.get_adjustment()->set_lower(1);
		spinner.get_adjustment()->set_upper(16);
		spinner.get_adjustment()->set_step_increment(1);
		spinner.get_adjustment()->set_page_increment(10);
		spinner.set_digits(0);
		break;
	}
	spinner.set_value(
		std::min(spinner.get_adjustment()->get_upper(),
		         std::max(spinner.get_adjustment()->get_lower(), spinner.get_value())));
}

void
TransformDialog::ValueChooser::set_target_property(Property prop)
{
	target_property = prop;
	set_spinner_for(value_spinner, prop);
	set_spinner_for(max_spinner, prop);
}

void
TransformDialog::ValueChooser::source_changed()
{
	Gtk::TreeModel::const_iterator s      = source_combo.get_active();
	const Source                   source = (*s)[model.source_cols.source];

	value_spinner.hide();
	to_label.hide();
	max_spinner.hide();
	if (source == Value::LITERAL) {
		value_spinner.show();
		property_combo.hide();
	} else if (source == Value::RANDOM) {
		value_spinner.show();
		to_label.show();
		max_spinner.show();
		property_combo.hide();
	} else if (source == Value::NOWHERE) {
		/* Bit of a kludge, hijack this for ramps since it's the only thing
		   that doesn't correspond to a source.  When we add more fancy
		   code-generating value chooser options, the column model will need to
		   be changed a bit to reflect this. */
		value_spinner.show();
		to_label.show();
		max_spinner.show();
		property_combo.hide();
	} else if (source == Value::INDEX || source == Value::N_NOTES) {
		value_spinner.hide();
		property_combo.hide();
	} else {
		value_spinner.hide();
		property_combo.show();
	}
}

double
TransformDialog::ValueChooser::get_value() const
{
	const bool is_channel = target_property == MidiModel::NoteDiffCommand::Channel;
	return value_spinner.get_value() + (is_channel ? -1.0 : 0.0);
}

double
TransformDialog::ValueChooser::get_max() const
{
	const bool is_channel = target_property == MidiModel::NoteDiffCommand::Channel;
	return max_spinner.get_value() + (is_channel ? -1.0 : 0.0);
}

void
TransformDialog::ValueChooser::get(std::list<Operation>& ops)
{
	Gtk::TreeModel::const_iterator s      = source_combo.get_active();
	const Source                   source = (*s)[model.source_cols.source];

	if (source == Transform::Value::RANDOM) {
		/* Special case: a RANDOM value is always 0..1, so here we produce some
		   code to produce a random number in a range: "rand value *". */
		const double a     = get_value();
		const double b     = get_max();
		const double min   = std::min(a, b);
		const double max   = std::max(a, b);
		const double range = max - min;

		// "rand range * min +" ((rand * range) + min)
		ops.push_back(Operation(Operation::PUSH, Value(Value::RANDOM)));
		ops.push_back(Operation(Operation::PUSH, Value(range)));
		ops.push_back(Operation(Operation::MULT));
		ops.push_back(Operation(Operation::PUSH, Value(min)));
		ops.push_back(Operation(Operation::ADD));
		return;
	} else if (source == Transform::Value::NOWHERE) {
		/* Special case: hijack NOWHERE for ramps (see above).  The language
		   knows nothing of ramps, we generate code to calculate the
		   appropriate value here. */
		const double first = get_value();
		const double last  = get_max();
		const double rise  = last - first;

		// "index rise * n_notes 1 - / first +" (index * rise / (n_notes - 1) + first)
		ops.push_back(Operation(Operation::PUSH, Value(Value::INDEX)));
		ops.push_back(Operation(Operation::PUSH, Value(rise)));
		ops.push_back(Operation(Operation::MULT));
		ops.push_back(Operation(Operation::PUSH, Value(Value::N_NOTES)));
		ops.push_back(Operation(Operation::PUSH, Value(1)));
		ops.push_back(Operation(Operation::SUB));
		ops.push_back(Operation(Operation::DIV));
		ops.push_back(Operation(Operation::PUSH, Value(first)));
		ops.push_back(Operation(Operation::ADD));
		return;
	}

	// Produce a simple Value
	Value val((*s)[model.source_cols.source]);
	if (val.source == Transform::Value::THIS_NOTE ||
	    val.source == Transform::Value::PREV_NOTE) {
		Gtk::TreeModel::const_iterator p = property_combo.get_active();
		val.prop = (*p)[model.property_cols.property];
	} else if (val.source == Transform::Value::LITERAL) {
		val.value = Variant(
			MidiModel::NoteDiffCommand::value_type(target_property),
			get_value());
	}
	ops.push_back(Operation(Operation::PUSH, val));
}

TransformDialog::OperationChooser::OperationChooser(const Model& model)
	: model(model)
	, value_chooser(model)
{
	operator_combo.set_model(model.operator_list);
	operator_combo.pack_start(model.operator_cols.label);
	operator_combo.set_active(0);

	pack_start(operator_combo, false, false);
	pack_start(value_chooser, false, false);
	pack_start(*Gtk::manage(new Gtk::Label(" ")), true, true);
	pack_start(remove_button, false, false);

	remove_button.add(
		*Gtk::manage(new Gtk::Image(Gtk::Stock::REMOVE, Gtk::ICON_SIZE_BUTTON)));

	remove_button.signal_clicked().connect(
		sigc::mem_fun(*this, &TransformDialog::OperationChooser::remove_clicked));

	value_chooser.source_combo.set_active(0);

	show_all();
	value_chooser.property_combo.hide();
	value_chooser.value_spinner.set_value(1);
	value_chooser.source_changed();
}

void
TransformDialog::OperationChooser::get(std::list<Operation>& ops)
{
	Gtk::TreeModel::const_iterator o = operator_combo.get_active();

	value_chooser.get(ops);
	ops.push_back(Operation((*o)[model.operator_cols.op]));
}

void
TransformDialog::OperationChooser::remove_clicked()
{
	delete this;
}

Transform::Program
TransformDialog::get()
{
	Transform::Program prog;

	// Set target property
	prog.prop = (*_property_combo.get_active())[_model.property_cols.property];

	// Append code to push seed to stack
	_seed_chooser->get(prog.ops);

	// Append all operations' code to program
	const std::vector<Gtk::Widget*>& choosers = _operations_box.get_children();
	for (std::vector<Gtk::Widget*>::const_iterator o = choosers.begin();
	     o != choosers.end(); ++o) {
		OperationChooser* chooser = dynamic_cast<OperationChooser*>(*o);
		if (chooser) {
			chooser->get(prog.ops);
		}
	}

	return prog;
}

void
TransformDialog::property_changed()
{
	Gtk::TreeModel::const_iterator i = _property_combo.get_active();
	_seed_chooser->set_target_property((*i)[_model.property_cols.property]);
}

void
TransformDialog::add_clicked()
{
	_operations_box.pack_start(
		*Gtk::manage(new OperationChooser(_model)), false, false);
}
