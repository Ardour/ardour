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

#include <iostream>

#include "pbd/convert.h"
#include "gtkmm2ext/string_completion.h"

using namespace Gtkmm2ext;

StringCompletion::StringCompletion()
	: case_fold (true)
{
	init();
}

StringCompletion::StringCompletion (std::vector < Glib::ustring > strVector, bool norepeat)
	: case_fold (true)
{
	init();
	insert_vector(strVector, norepeat);
}

StringCompletion::~StringCompletion()
{
}

void
StringCompletion::init ()
{
	m_refCompletionModel = Gtk::ListStore::create(m_completionRecord);

	set_model(m_refCompletionModel);
	set_text_column (m_completionRecord.col_text);
}

void
StringCompletion::add_string (Glib::ustring str, bool norepeat)
{
	if ((!norepeat) || (!_string_exists (str))) {
		Gtk::TreeModel::Row row =*(m_refCompletionModel->append());
		row[m_completionRecord.col_text] = str;
	}
}

Glib::RefPtr<StringCompletion>
StringCompletion::create()
{
	return Glib::RefPtr<StringCompletion> (new StringCompletion());
}

void
StringCompletion::clear_strings()
{
	m_refCompletionModel->clear();
}

void
StringCompletion::delete_string(Glib::ustring str)
{
	m_refCompletionModel->foreach_iter(sigc::bind<const Glib::ustring& >(sigc::mem_fun( *this, &StringCompletion::_delete_string), str));
}

bool
StringCompletion::_delete_string (const Gtk::TreeModel::iterator & iter, const Glib::ustring & str)
{
	if (Gtk::TreeModel::Row(*iter)[m_completionRecord.col_text] == str) {
		m_refCompletionModel->erase(iter);
		return true;
	}
	return false;
}

bool
StringCompletion::_string_exists (const Glib::ustring & str)
{
	for (auto & i : m_refCompletionModel->children()) {
		if (Gtk::TreeModel::Row(i)[m_completionRecord.col_text] == str) {
			return true;
		}
	}
	return false;
}

Glib::RefPtr<StringCompletion>
StringCompletion::create (std::vector < Glib::ustring > strVector, bool norepeat)
{
	return Glib::RefPtr<StringCompletion> ( new StringCompletion(strVector) );
}

void
StringCompletion::insert_vector (std::vector<Glib::ustring> strVector, bool norepeat)
{
	for (auto & s : strVector) {
		add_string (s, norepeat);
	}
}

bool
StringCompletion::match_anywhere (Glib::ustring const & str,  Gtk::TreeModel::const_iterator const & iter)
{
	Glib::ustring r = Gtk::TreeModel::Row (*iter)[m_completionRecord.col_text];
	if (case_fold) {
		return PBD::downcase (r).find (PBD::downcase (str)) != Glib::ustring::npos;
	}
	return r.find (str) != Glib::ustring::npos;
}

void
StringCompletion::set_match_anywhere ()
{
	set_match_func (sigc::mem_fun (this, &StringCompletion::match_anywhere));
}

void
StringCompletion::set_case_fold (bool yn)
{
	case_fold = yn;
}
