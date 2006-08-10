/*
    Copyright (C) 2001 Paul Davis 

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

    $Id: /local/undo/gtk2_ardour/region_editor.h 5 2006-05-31T02:48:48.738745Z paul  $
*/

#ifndef __gtk_ardour_region_edit_h__
#define __gtk_ardour_region_edit_h__

#include "ardour_dialog.h"

namespace ARDOUR { class Session; }

/** Just a useless stub for now... */
class RegionEditor : public ArdourDialog
{
  public:
	RegionEditor(ARDOUR::Session& s)
	: ArdourDialog ("region editor")
	, _session(s)
	{}

	virtual ~RegionEditor () {}

  protected:
	ARDOUR::Session&     _session;
};

#endif /* __gtk_ardour_region_edit_h__ */
