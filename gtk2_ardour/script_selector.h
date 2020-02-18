/*
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/button.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>

#include "ardour/luascripting.h"

#include "ardour_dialog.h"

class ScriptSelector : public ArdourDialog
{
public:
	ScriptSelector (std::string title, ARDOUR::LuaScriptInfo::ScriptType t);
	ARDOUR::LuaScriptInfoPtr script() const { return _script; }

private:
	void setup_list ();
	void refresh ();
	void script_combo_changed ();
	bool script_separator (const Glib::RefPtr<Gtk::TreeModel> &, const Gtk::TreeModel::iterator &i);
	
	Gtk::Button* _add;
	Gtk::ComboBoxText _script_combo;

	Gtk::Label  _type_label;
	Gtk::Label  _type;
	Gtk::Label  _author_label;
	Gtk::Label  _author;
	Gtk::Label  _description;

	ARDOUR::LuaScriptList _scripts;
	ARDOUR::LuaScriptInfoPtr _script;
	ARDOUR::LuaScriptInfo::ScriptType _script_type;
	sigc::connection _combocon;
};

class SessionScriptManager : public ArdourDialog
{
public:
	SessionScriptManager (std::string title, const std::vector<std::string>&);
	std::string name () { return _names_combo.get_active_text (); }

private:
	Gtk::ComboBoxText _names_combo;
};

class ScriptParameterDialog : public ArdourDialog
{
public:
	ScriptParameterDialog (std::string title, const ARDOUR::LuaScriptInfoPtr&, const std::vector<std::string>&, ARDOUR::LuaScriptParamList&);
	std::string name () { return _name_entry.get_text (); }
	bool need_interation () const;

private:
	void update_sensitivity ();
	bool parameters_ok () const;
	void active_changed (int, Gtk::CheckButton*, Gtk::Entry*);
	void value_changed (int, Gtk::Entry*);

	Gtk::Entry _name_entry;
	Gtk::Button* _add;
	const std::vector<std::string> &_existing_names;
	ARDOUR::LuaScriptParamList& _lsp;
};
