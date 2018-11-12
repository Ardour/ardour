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

#ifndef _gtk2ardour_luadialog_h_
#define _gtk2ardour_luadialog_h_

#include <cassert>
#include <gtkmm/table.h>
#include <gtkmm/messagedialog.h>

#include "LuaBridge/LuaBridge.h"

namespace LuaDialog {

class Message {
public:

	enum MessageType {
		Info, Warning, Question, Error
	};

	enum ButtonType {
		OK, Close, Cancel, Yes_No, OK_Cancel
	};

	Message (std::string const&, std::string const&, Message::MessageType, Message::ButtonType);

	int run ();

private:
	Message (Message const&); // prevent copy construction

	static Gtk::ButtonsType to_gtk_bt (ButtonType bt);
	static Gtk::MessageType to_gtk_mt (MessageType mt);

	Gtk::MessageDialog _message_dialog;
};

class LuaDialogWidget {
public:
	LuaDialogWidget (std::string const& key, std::string const& label, int col = 0, int colspan = -1)
		: _key (key), _label (label), _col (col), _colspan (colspan)
	{
		if (_colspan < 0) {
			_colspan = label.empty () ? 1 : 2;
		}
	}

	virtual ~LuaDialogWidget () {}

	virtual Gtk::Widget* widget () = 0;
	virtual void assign (luabridge::LuaRef* rv) const = 0;
	std::string const&  label () const { return _label; }
	std::string const&  key   () const { return _key; }
	int                 col   () const { return _col; }
	int                 span  () const { return _colspan; }

	void set_col  (int col)  { _col = col; }
	void set_span (int span) { _colspan = span; }

protected:
	std::string _key;
	std::string _label;
	int _col;
	int _colspan;
};


class Dialog {
public:
	Dialog (std::string const&, luabridge::LuaRef);
	~Dialog ();
	int run (lua_State *L);

private:
	Dialog (Dialog const&); // prevent copy construction
	void table_size_alloc (Gtk::Allocation&);

	ArdourDialog _ad;
	Gtk::ScrolledWindow _scroller;
	typedef std::vector<LuaDialogWidget*> DialogWidgets;
	DialogWidgets _widgets;
	std::string _title;
};

}; // namespace

#endif
