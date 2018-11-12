/*
    Copyright (C) 2000-2010 Paul Davis

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
	~SessionOptionEditor ();

private:
	void parameter_changed (std::string const &);

	ARDOUR::SessionConfiguration* _session_config;

	bool set_use_monitor_section (bool);
	bool get_use_monitor_section ();

	ComboOption<float>* _vpu;
	ComboOption<ARDOUR::SampleFormat>* _sf;
	EntryOption* _take_name;

	void save_defaults ();
};

#endif /* __gtk_ardour_session_option_editor_h__ */
