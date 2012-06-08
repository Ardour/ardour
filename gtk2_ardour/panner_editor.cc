/*
    Copyright (C) 2012 Paul Davis

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

#include "panner_editor.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;

PannerEditor::PannerEditor (string t)
	: ArdourDialog (t)
{
	Button* b = add_button (_("Close"), RESPONSE_CANCEL);
	b->signal_clicked().connect (sigc::mem_fun(*this, &PannerEditor::close_button_clicked));
}

void
PannerEditor::close_button_clicked ()
{
	hide ();
}
