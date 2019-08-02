/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_session_option_editor_h__
#define __gtk_ardour_session_option_editor_h__

#include "option_editor.h"

namespace ARDOUR {
	class Session;
	class SessionConfiguration;
}

class SessionOptionEditor : public OptionEditorWindow
{
public:
	SessionOptionEditor (ARDOUR::Session* s);

private:
	void parameter_changed (std::string const &);

	ARDOUR::SessionConfiguration* _session_config;

	ComboOption<float>* _vpu;
	ComboOption<ARDOUR::SampleFormat>* _sf;
	EntryOption* _take_name;

	void save_defaults ();
};

#endif /* __gtk_ardour_session_option_editor_h__ */
