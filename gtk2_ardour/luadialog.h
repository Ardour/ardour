/*
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#ifndef _gtk2ardour_luadialog_h_
#define _gtk2ardour_luadialog_h_

#include <cassert>
#include <gtkmm/table.h>
#include <gtkmm/progressbar.h>

#include "LuaBridge/LuaBridge.h"

#include "ardour_message.h"

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

	ArdourMessageDialog _message_dialog;
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

/** Synchronous GUI-thread Progress dialog
 *
 * This shows a modal progress dialog with an optional
 * "Cancel" button. Since it runs in the UI thread
 * the script needs to regularly call progress(),
 * as well as close the dialog, as needed.
 */
class ProgressWindow : public ArdourDialog
{
public:
	/** Create a new progress window.
	 * @param title Window title
	 * @param allow_cancel include a "Cancel" option
	 */
	ProgressWindow (std::string const& title, bool allow_cancel);

	/** Report progress and update GUI.
	 * @param prog progress in range 0..1 show a bar, values outside this range show a pulsing dialog.
	 * @param text optional text to show on the progress-bar
	 * @return true if cancel was clicked, false otherwise
	 */
	bool progress (float prog, std::string const& text = "");

	bool canceled () const {
		return _canceled;
	}

	/** Close and hide the dialog.
	 *
	 * This is required to be at the end, since the dialog
	 * is modal and prevents other UI operations while visible.
	 */
	void done ();

private:
	void cancel_clicked () {
		_canceled = true;
	}

	Gtk::ProgressBar _bar;
	bool             _canceled;
};

}; // namespace

#endif
