/*
 * Copyright (C) 2025 Paul Davis <paul@linuxaudiosystems.com>
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

#pragma once

#include <vector>
#include <ytkmm/entry.h>
#include <ytkmm/liststore.h>
#include "gtkmm2ext/visibility.h"

namespace Gtkmm2ext {

class LIBGTKMM2EXT_API StringCompletion : public Gtk::EntryCompletion
{
  public:
	StringCompletion ();
	StringCompletion (std::vector<Glib::ustring> strVector, bool norepeat = true);

	virtual ~StringCompletion();

	void add_string (Glib::ustring str, bool norepeat=true);
	void clear_strings();
	void delete_string (Glib::ustring str);
	void insert_vector (std::vector<Glib::ustring> strVector, bool norepeat = true);

	void set_match_anywhere ();
	void set_case_fold (bool);

	static Glib::RefPtr<StringCompletion> create();
	static Glib::RefPtr<StringCompletion> create (std::vector<Glib::ustring> strVector, bool norepeat = true);

  protected:
	Glib::RefPtr<Gtk::ListStore> m_refCompletionModel;
	bool case_fold;

	class CompletionRecord : public Gtk::TreeModel::ColumnRecord
	{
            public:
		CompletionRecord()
		{
			add(col_text);
		}
		Gtk::TreeModelColumn<Glib::ustring> col_text;
	};

	CompletionRecord m_completionRecord;
	bool _delete_string (const Gtk::TreeModel::iterator &iter, const Glib::ustring &str);
	bool _string_exists (const Glib::ustring &str);
	bool match_anywhere (Glib::ustring const & str,  Gtk::TreeModel::const_iterator const & iter);

	void init();
};

} /* namespace */
