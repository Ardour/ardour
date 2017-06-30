/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <gtkmm.h>

#include "ardour/dB.h"
#include "ardour/rc_configuration.h"

#include "gtkmm2ext/slider_controller.h"
#include "gtkmm2ext/utils.h"

#include "ardour_dialog.h"
#include "ardour_dropdown.h"
#include "luadialog.h"
#include "utils.h"

using namespace LuaDialog;

/*******************************************************************************
 * Simple Message Dialog
 */
Message::Message (std::string const& title, std::string const& msg, Message::MessageType mt, Message::ButtonType bt)
	: _message_dialog (msg, false, to_gtk_mt (mt), to_gtk_bt (bt), true)
{
	_message_dialog.set_title (title);
}

int
Message::run ()
{
	int rv = _message_dialog.run ();
	_message_dialog.hide ();
	switch (rv) {
		case Gtk::RESPONSE_OK:
			return 0;
		case Gtk::RESPONSE_CANCEL:
			return 1;
		case Gtk::RESPONSE_CLOSE:
			return 2;
		case Gtk::RESPONSE_YES:
			return 3;
		case Gtk::RESPONSE_NO:
			return 4;
		default:
			break;
	}
	return -1;
}

Gtk::ButtonsType
Message::to_gtk_bt (ButtonType bt)
{
	switch (bt) {
		case OK:
			return Gtk::BUTTONS_OK;
		case Close:
			return Gtk::BUTTONS_CLOSE;
		case Cancel:
			return Gtk::BUTTONS_CANCEL;
		case Yes_No:
			return Gtk::BUTTONS_YES_NO;
		case OK_Cancel:
			return Gtk::BUTTONS_OK_CANCEL;
	}
	assert (0);
	return Gtk::BUTTONS_OK;
}

Gtk::MessageType
Message::to_gtk_mt (MessageType mt)
{
	switch (mt) {
		case Info:
			return Gtk::MESSAGE_INFO;
		case Warning:
			return Gtk::MESSAGE_WARNING;
		case Question:
			return Gtk::MESSAGE_QUESTION;
		case Error:
			return Gtk::MESSAGE_ERROR;
	}
	assert (0);
	return Gtk::MESSAGE_INFO;
}


/* *****************************************************************************
 * Lua Dialog Widgets
 */

class LuaDialogLabel : public LuaDialogWidget
{
public:
	LuaDialogLabel (std::string const& title, Gtk::AlignmentEnum xalign)
		: LuaDialogWidget ("", "")
		, _lbl ("<b>" + title + "</b>", xalign, Gtk::ALIGN_CENTER, false)
	{
		_lbl.set_use_markup ();
	}

	Gtk::Widget* widget ()
	{
		return &_lbl;
	}

	void assign (luabridge::LuaRef* rv) const { }
protected:
	Gtk::Label _lbl;
};

class LuaDialogCheckbox : public LuaDialogWidget
{
public:
	LuaDialogCheckbox (std::string const& key, std::string const& title, bool on)
		: LuaDialogWidget (key, "")
		, _cb (title)
	{
		_cb.set_active (on);
	}

	Gtk::Widget* widget ()
	{
		return &_cb;
	}

	void assign (luabridge::LuaRef* rv) const
	{
		(*rv)[_key] = _cb.get_active ();
	}

protected:
	Gtk::CheckButton _cb;
};

class LuaDialogEntry : public LuaDialogWidget
{
public:
	LuaDialogEntry (std::string const& key, std::string const& title, std::string const& dflt)
		: LuaDialogWidget (key, title)
	{
		_entry.set_text (dflt);
	}

	Gtk::Widget* widget ()
	{
		return &_entry;
	}

	void assign (luabridge::LuaRef* rv) const
	{
		(*rv)[_key] = std::string (_entry.get_text ());
	}

protected:
	Gtk::Entry _entry;
};

class LuaDialogFader : public LuaDialogWidget
{
public:
	LuaDialogFader (std::string const& key, std::string const& title, double dflt)
		: LuaDialogWidget (key, title)
		, _db_adjustment (ARDOUR::gain_to_slider_position_with_max (1.0, ARDOUR::Config->get_max_gain ()), 0, 1, 0.01, 0.1)
	{
		_db_slider = Gtk::manage (new Gtkmm2ext::HSliderController (&_db_adjustment, boost::shared_ptr<PBD::Controllable> (), 220, 18));

		_fader_centering_box.pack_start (*_db_slider, true, false);

		_box.set_spacing (4);
		_box.set_homogeneous (false);
		_box.pack_start (_fader_centering_box, false, false);
		_box.pack_start (_db_display, false, false);
		_box.pack_start (*Gtk::manage (new Gtk::Label ("dB")), false, false);

		Gtkmm2ext::set_size_request_to_display_given_text (_db_display, "-99.00", 12, 0);

		_db_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &LuaDialogFader::db_changed));
		_db_display.signal_activate ().connect (sigc::mem_fun (*this, &LuaDialogFader::on_activate));
		_db_display.signal_key_press_event ().connect (sigc::mem_fun (*this, &LuaDialogFader::on_key_press), false);

		double coeff_val = dB_to_coefficient (dflt);
		_db_adjustment.set_value (ARDOUR::gain_to_slider_position_with_max (coeff_val, ARDOUR::Config->get_max_gain ()));
		db_changed ();
	}

	Gtk::Widget* widget ()
	{
		return &_box;
	}

	void assign (luabridge::LuaRef* rv) const
	{
		double const val = ARDOUR::slider_position_to_gain_with_max (_db_adjustment.get_value (), ARDOUR::Config->get_max_gain ());
		(*rv)[_key] = accurate_coefficient_to_dB (val);
	}

protected:
	void db_changed ()
	{
		double const val = ARDOUR::slider_position_to_gain_with_max (_db_adjustment.get_value (), ARDOUR::Config->get_max_gain ());
		char buf[16];
		if (val == 0.0) {
			snprintf (buf, sizeof (buf), "-inf");
		} else {
			snprintf (buf, sizeof (buf), "%.2f", accurate_coefficient_to_dB (val));
		}
		_db_display.set_text (buf);
	}

	void on_activate ()
	{
		float db_val = atof (_db_display.get_text ().c_str ());
		double coeff_val = dB_to_coefficient (db_val);
		_db_adjustment.set_value (ARDOUR::gain_to_slider_position_with_max (coeff_val, ARDOUR::Config->get_max_gain ()));
	}

	bool on_key_press (GdkEventKey* ev)
	{
		if (ARDOUR_UI_UTILS::key_is_legal_for_numeric_entry (ev->keyval)) {
			return false;
		}
		return true;
	}

	Gtk::Adjustment _db_adjustment;
	Gtkmm2ext::HSliderController* _db_slider;
	Gtk::Entry _db_display;
	Gtk::HBox _box;
	Gtk::VBox _fader_centering_box;
};

class LuaDialogSlider : public LuaDialogWidget
{
public:
	LuaDialogSlider (std::string const& key, std::string const& title, double lower, double upper, double dflt, int digits, luabridge::LuaRef scalepoints)
		: LuaDialogWidget (key, title)
		, _adj (dflt, lower, upper, 1, (upper - lower) / 20, 0)
		, _hscale (_adj)
	{
		_hscale.set_digits (digits);
		_hscale.set_draw_value (true);
		_hscale.set_value_pos (Gtk::POS_TOP);

		if (!scalepoints.isTable ()) {
			return;
		}

		for (luabridge::Iterator i (scalepoints); !i.isNil (); ++i) {
			if (!i.key ().isNumber ())  { continue; }
			if (!i.value ().isString ())  { continue; }
			_hscale.add_mark (i.key ().cast<double> (), Gtk::POS_BOTTOM, i.value ().cast<std::string> ());
		}
	}

	Gtk::Widget* widget ()
	{
		return &_hscale;
	}

	void assign (luabridge::LuaRef* rv) const
	{
		(*rv)[_key] = _adj.get_value ();
	}

protected:
	Gtk::Adjustment _adj;
	Gtk::HScale _hscale;
};

class LuaDialogSpinBox : public LuaDialogWidget
{
public:
	LuaDialogSpinBox (std::string const& key, std::string const& title, double lower, double upper, double dflt, double step, int digits)
		: LuaDialogWidget (key, title)
		, _adj (dflt, lower, upper, step, step, 0)
		, _spin (_adj)
	{
		_spin.set_digits (digits);
	}

	Gtk::Widget* widget ()
	{
		return &_spin;
	}

	void assign (luabridge::LuaRef* rv) const
	{
		(*rv)[_key] = _adj.get_value ();
	}

protected:
	Gtk::Adjustment _adj;
	Gtk::SpinButton _spin;
};

class LuaDialogRadio : public LuaDialogWidget
{
public:
	LuaDialogRadio (std::string const& key, std::string const& title, luabridge::LuaRef values, std::string const& dflt)
		: LuaDialogWidget (key, title)
		, _rv (0)
	{
		for (luabridge::Iterator i (values); !i.isNil (); ++i) {
			if (!i.key ().isString ())  { continue; }
			std::string key = i.key ().cast<std::string> ();
			Gtk::RadioButton* rb = Gtk::manage (new Gtk::RadioButton (_group, key));
			_hbox.pack_start (*rb);
			luabridge::LuaRef* ref = new luabridge::LuaRef (i.value ());
			_refs.push_back (ref);
			if (!_rv) { _rv = ref; }
			rb->signal_toggled ().connect (sigc::bind (
						sigc::mem_fun (*this, &LuaDialogRadio::rb_toggled), rb, ref
						) , false);

			if (key == dflt) {
				rb->set_active ();
			}
		}
	}

	~LuaDialogRadio ()
	{
		for (std::vector<luabridge::LuaRef*>::const_iterator i = _refs.begin (); i != _refs.end (); ++i) {
			delete *i;
		}
		_refs.clear ();
	}

	Gtk::Widget* widget ()
	{
		return &_hbox;
	}

	void assign (luabridge::LuaRef* rv) const
	{
		if (_rv) {
			(*rv)[_key] = *_rv;
		} else {
			(*rv)[_key] = luabridge::Nil ();
		}
	}

protected:
	LuaDialogRadio (LuaDialogRadio const&); // prevent cc
	void rb_toggled (Gtk::RadioButton* b, luabridge::LuaRef* rv) {
		if (b->get_active ()) {
			_rv = rv;
		}
	}

	Gtk::HBox _hbox;
	Gtk::RadioButtonGroup _group;
	std::vector<luabridge::LuaRef*> _refs;
	luabridge::LuaRef* _rv;
};

class LuaDialogDropDown : public LuaDialogWidget
{
public:
	LuaDialogDropDown (std::string const& key, std::string const& title, luabridge::LuaRef values, std::string const& dflt)
		: LuaDialogWidget (key, title)
		, _rv (0)
	{
		populate (_dd.items (), values, dflt);
	}

	~LuaDialogDropDown ()
	{
		for (std::vector<luabridge::LuaRef*>::const_iterator i = _refs.begin (); i != _refs.end (); ++i) {
			delete *i;
		}
		_refs.clear ();
	}

	Gtk::Widget* widget ()
	{
		return &_dd;
	}

	void assign (luabridge::LuaRef* rv) const
	{
		if (_rv) {
			(*rv)[_key] = *_rv;
		} else {
			(*rv)[_key] = luabridge::Nil ();
		}
	}

protected:
	void populate (Gtk::Menu_Helpers::MenuList& items, luabridge::LuaRef values, std::string const& dflt)
	{
		using namespace Gtk::Menu_Helpers;
		for (luabridge::Iterator i (values); !i.isNil (); ++i) {
			if (!i.key ().isString ())  { continue; }
			std::string key = i.key ().cast<std::string> ();
			if (i.value ().isTable ())  {
				Gtk::Menu* menu  = Gtk::manage (new Gtk::Menu);
				items.push_back (MenuElem (key, *menu));
				populate (menu->items (), i.value (), dflt);
				continue;
			}
			luabridge::LuaRef* ref = new luabridge::LuaRef (i.value ());
			_refs.push_back (ref);
			items.push_back (MenuElem (i.key ().cast<std::string> (),
						sigc::bind (sigc::mem_fun (*this, &LuaDialogDropDown::dd_select), key, ref)));

			if (!_rv || key == dflt) {
				_rv = ref;
				_dd.set_text (key);
			}
		}
	}

	void dd_select (std::string const& key, luabridge::LuaRef* rv) {
		_dd.set_text (key);
		_rv = rv;
	}

	ArdourDropdown _dd;
	std::vector<luabridge::LuaRef*> _refs;
	luabridge::LuaRef* _rv;
};

class LuaFileChooser : public LuaDialogWidget
{
public:
	LuaFileChooser (std::string const& key, std::string const& title, Gtk::FileChooserAction a, const std::string& path)
		: LuaDialogWidget (key, title)
		, _fc (a)
	{
		if (!path.empty ()) {
			switch (a) {
				case Gtk::FILE_CHOOSER_ACTION_OPEN:
				case Gtk::FILE_CHOOSER_ACTION_SAVE:
					if (Glib::file_test (path, Glib::FILE_TEST_IS_REGULAR|Glib::FILE_TEST_EXISTS)) {
						_fc.set_filename (path);
					}
					break;
				case Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER:
					if (Glib::file_test (path, Glib::FILE_TEST_IS_DIR|Glib::FILE_TEST_EXISTS)) {
						_fc.set_filename (path);
					}
					break;
				case Gtk::FILE_CHOOSER_ACTION_CREATE_FOLDER:
					break;
			}
		}
	}

	Gtk::Widget* widget ()
	{
		return &_fc;
	}

	void assign (luabridge::LuaRef* rv) const
	{
		(*rv)[_key] = std::string (_fc.get_filename ());
	}

protected:
	Gtk::FileChooserButton _fc;
};



/*******************************************************************************
 * Lua Parameter Dialog
 */
Dialog::Dialog (std::string const& title, luabridge::LuaRef lr)
	:_ad (title, true, false)
	, _title (title)
{
	if (!lr.isTable ()) {
		return;
	}
	for (luabridge::Iterator i (lr); !i.isNil (); ++i) {
		if (!i.key ().isNumber ())  { continue; }
		if (!i.value ().isTable ()) { continue; }
		if (!i.value ()["title"].isString ()) { continue; }
		if (!i.value ()["type"].isString ()) { continue; }

		std::string title = i.value ()["title"].cast<std::string> ();
		std::string type = i.value ()["type"].cast<std::string> ();

		if (type == "heading") {
			Gtk::AlignmentEnum xalign = Gtk::ALIGN_CENTER;
			if (i.value ()["align"].isString ()) {
				std::string align = i.value ()["align"].cast <std::string> ();
				if (align == "left") {
					xalign = Gtk::ALIGN_LEFT;
				} else if (align == "right") {
					xalign = Gtk::ALIGN_RIGHT;
				}
			}
			_widgets.push_back (new LuaDialogLabel (title, xalign));
			continue;
		}

		if (!i.value ()["key"].isString ()) { continue; }
		std::string key = i.value ()["key"].cast<std::string> ();

		if (type == "checkbox") {
			bool dflt = false;
			if (i.value ()["default"].isBoolean ()) {
				dflt = i.value ()["default"].cast<bool> ();
			}
			_widgets.push_back (new LuaDialogCheckbox (key, title, dflt));
		} else if (type == "entry") {
			std::string dflt;
			if (i.value ()["default"].isString ()) {
				dflt = i.value ()["default"].cast<std::string> ();
			}
			_widgets.push_back (new LuaDialogEntry (key, title, dflt));
		} else if (type == "radio") {
			std::string dflt;
			if (!i.value ()["values"].isTable ()) {
				continue;
			}
			if (i.value ()["default"].isString ()) {
				dflt = i.value ()["default"].cast<std::string> ();
			}
			_widgets.push_back (new LuaDialogRadio (key, title, i.value ()["values"], dflt));
		} else if (type == "fader") {
			double dflt = 0;
			if (i.value ()["default"].isNumber ()) {
				dflt = i.value ()["default"].cast<double> ();
			}
			_widgets.push_back (new LuaDialogFader (key, title, dflt));
		} else if (type == "slider") {
			double lower, upper, dflt;
			int digits = 0;
			if (!i.value ()["min"].isNumber ()) { continue; }
			if (!i.value ()["max"].isNumber ()) { continue; }
			lower = i.value ()["min"].cast<double> ();
			upper = i.value ()["max"].cast<double> ();
			if (i.value ()["default"].isNumber ()) {
				dflt = i.value ()["default"].cast<double> ();
			} else {
				dflt = lower;
			}
			if (i.value ()["digits"].isNumber ()) {
				digits = i.value ()["digits"].cast<int> ();
			}
			_widgets.push_back (new LuaDialogSlider (key, title, lower, upper, dflt, digits, i.value ()["scalepoints"]));
		} else if (type == "number") {
			double lower, upper, dflt, step;
			int digits = 0;
			if (!i.value ()["min"].isNumber ()) { continue; }
			if (!i.value ()["max"].isNumber ()) { continue; }
			lower = i.value ()["min"].cast<double> ();
			upper = i.value ()["max"].cast<double> ();
			if (i.value ()["default"].isNumber ()) {
				dflt = i.value ()["default"].cast<double> ();
			} else {
				dflt = lower;
			}
			if (i.value ()["step"].isNumber ()) {
				step = i.value ()["step"].cast<double> ();
			} else {
				step = 1.0;
			}
			if (i.value ()["digits"].isNumber ()) {
				digits = i.value ()["digits"].cast<int> ();
			}
			_widgets.push_back (new LuaDialogSpinBox (key, title, lower, upper, dflt, step, digits));
		} else if (type == "dropdown") {
			std::string dflt;
			if (!i.value ()["values"].isTable ()) {
				continue;
			}
			if (i.value ()["default"].isString ()) {
				dflt = i.value ()["default"].cast<std::string> ();
			}
			_widgets.push_back (new LuaDialogDropDown (key, title, i.value ()["values"], dflt));
		} else if (type == "file") {
			std::string path;
			if (i.value ()["path"].isString ()) {
				path = i.value ()["path"].cast<std::string> ();
			}
			_widgets.push_back (new LuaFileChooser (key, title, Gtk::FILE_CHOOSER_ACTION_OPEN, path));
		} else if (type == "folder") {
			std::string path;
			if (i.value ()["path"].isString ()) {
				path = i.value ()["path"].cast<std::string> ();
			}
			_widgets.push_back (new LuaFileChooser (key, title, Gtk::FILE_CHOOSER_ACTION_SELECT_FOLDER, path));
		}
	}

	_ad.add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	_ad.add_button (Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);

	Gtk::Table* table = Gtk::manage (new Gtk::Table ());
	table->set_col_spacings (4);
	table->set_row_spacings (8);
	_ad.get_vbox ()->pack_start (*table);
	int row = 0;

	for (DialogWidgets::const_iterator i = _widgets.begin (); i != _widgets.end (); ++i) {
		std::string const& label = (*i)->label ();
		if (!label.empty ()) {
			Gtk::Label* lbl = Gtk::manage (new Gtk::Label (label + ":", Gtk::ALIGN_END, Gtk::ALIGN_CENTER, false));
			table->attach (*lbl, 0, 1, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);
			table->attach (*((*i)->widget ()), 1, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);
		} else if ((*i)->key ().empty ()) {
			table->attach (*((*i)->widget ()), 0, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);
		} else {
			table->attach (*((*i)->widget ()), 1, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::SHRINK);
		}
		++row;
	}
}

Dialog::~Dialog ()
{
	for (DialogWidgets::const_iterator i = _widgets.begin (); i != _widgets.end () ; ++i) {
		delete *i;
	}
	_widgets.clear ();
}

int
Dialog::run (lua_State *L)
{
	_ad.get_vbox ()->show_all ();
	switch (_ad.run ()) {
		case Gtk::RESPONSE_ACCEPT:
			break;
		default:
			lua_pushnil (L);
			return 1;
	}

	luabridge::LuaRef rv (luabridge::newTable (L));
	for (DialogWidgets::const_iterator i = _widgets.begin (); i != _widgets.end () ; ++i) {
		(*i)->assign (&rv);
	}
	luabridge::push (L, rv);
	return 1;
}
