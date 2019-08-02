/*
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
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

#ifndef __transform_dialog_h__
#define __transform_dialog_h__

#include <list>
#include <string>

#include <gtkmm/combobox.h>
#include <gtkmm/liststore.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/spinbutton.h>

#include "ardour/midi_model.h"
#include "ardour/transform.h"
#include "ardour/types.h"

#include "ardour_dialog.h"

/** Dialog for building a MIDI note transformation.
 *
 * This can build transformations with any number of operations, but is limited
 * in power and can't build arbitrary transformations since there is no way to do
 * conceptually parenthetical things (i.e. push things to the stack).
 *
 * With this, it is possible to build transformations that process a single
 * value in a series of steps starting with a seed, like: "value = seed OP
 * value OP value ..." where OP is +, -, *, or /, left associative with no
 * precedence.  This is simple and pretty clear to the user what's going to
 * happen, though a bit limited.  It would be nice if the GUI could build
 * fancier transformations, but it's not obvious how to do this without making
 * things more confusing.
 */
class TransformDialog : public ArdourDialog
{
public:
	TransformDialog();

	ARDOUR::Transform::Program get();

private:
	typedef ARDOUR::MidiModel::NoteDiffCommand::Property Property;
	typedef ARDOUR::Transform::Value                     Value;
	typedef ARDOUR::Transform::Value::Source             Source;
	typedef ARDOUR::Transform::Operation::Operator       Operator;
	typedef ARDOUR::Transform::Operation                 Operation;

	struct SourceCols : public Gtk::TreeModelColumnRecord {
		SourceCols() { add(source); add(label); }

		Gtk::TreeModelColumn<Source>      source;
		Gtk::TreeModelColumn<std::string> label;
	};

	struct PropertyCols : public Gtk::TreeModelColumnRecord {
		PropertyCols() { add(property); add(label); }

		Gtk::TreeModelColumn<Property>    property;
		Gtk::TreeModelColumn<std::string> label;
	};

	struct OperatorCols : public Gtk::TreeModelColumnRecord {
		OperatorCols() { add(op); add(label); }

		Gtk::TreeModelColumn<Operator>    op;
		Gtk::TreeModelColumn<std::string> label;
	};

	struct Model {
		Model();

		SourceCols                   source_cols;
		Glib::RefPtr<Gtk::ListStore> source_list;
		PropertyCols                 property_cols;
		Glib::RefPtr<Gtk::ListStore> property_list;
		OperatorCols                 operator_cols;
		Glib::RefPtr<Gtk::ListStore> operator_list;
	};

	struct ValueChooser : public Gtk::HBox {
		ValueChooser(const Model& model);

		/** Append code to `ops` that pushes value to stack. */
		void get(std::list<Operation>& ops);

		void set_target_property(Property prop);
		void source_changed();

		double get_value () const;
		double get_max () const;

		const Model&    model;            ///< Models for combo boxes
		Property        target_property;  ///< Property on source
		Gtk::ComboBox   source_combo;     ///< Value source chooser
		Gtk::ComboBox   property_combo;   ///< Property chooser
		Gtk::SpinButton value_spinner;    ///< Value or minimum for RANDOM
		Gtk::Label      to_label;         ///< "to" label for RANDOM
		Gtk::SpinButton max_spinner;      ///< Maximum for RANDOM
	};

	struct OperationChooser : public Gtk::HBox {
		OperationChooser(const Model& model);

		/** Append operations to `ops`. */
		void get(std::list<Operation>& ops);

		void remove_clicked();

		const Model&  model;
		Gtk::ComboBox operator_combo;
		ValueChooser  value_chooser;
		Gtk::Button   remove_button;
	};

	void property_changed();
	void add_clicked();

	Model         _model;
	Gtk::ComboBox _property_combo;
	ValueChooser* _seed_chooser;
	Gtk::VBox     _operations_box;
	Gtk::Button   _add_button;
};

#endif /* __transform_dialog_h__ */
