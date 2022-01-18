/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2021 Robin Gareus <robin@gareus.org>
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
#ifndef __gtk_ardour_editor_regions_h__
#define __gtk_ardour_editor_regions_h__

#include "editor_component.h"
#include "region_list_base.h"
#include "region_selection.h"

class EditorRegions : public EditorComponent, public RegionListBase
{
public:
	EditorRegions (Editor*);

	void set_selected (RegionSelection&);
	void selection_mapover (sigc::slot<void, boost::shared_ptr<ARDOUR::Region>>);
	void remove_unused_regions ();

	boost::shared_ptr<ARDOUR::Region> get_single_selection ();

	void unselect_all ()
	{
		_display.get_selection ()->unselect_all ();
	}

protected:
	void regions_changed (boost::shared_ptr<ARDOUR::RegionList>, PBD::PropertyChange const&);

private:
	void init ();
	void selection_changed ();
	bool button_press (GdkEventButton*);
	void show_context_menu (int button, int time);
};

#endif /* __gtk_ardour_editor_regions_h__ */
