/*
    Copyright (C) 2000 Paul Davis

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

#ifndef __ardour_mixer_snapshot_dialog_h__
#define __ardour_mixer_snapshot_dialog_h__

#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/entry.h>
#include <gtkmm/liststore.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treemodel.h>
#include <gtkmm/treeview.h>

#include "gtkmm2ext/dndtreeview.h"

#include "ardour/mixer_snapshot.h"

#include "ardour_dialog.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

class MixerSnapshotDialog : public ArdourDialog
{
    public:
        MixerSnapshotDialog();
        ~MixerSnapshotDialog();

        int run();

    private:

        Gtkmm2ext::DnDTreeView<std::string> snap_display;
        Gtk::ScrolledWindow scroller;

};
#endif
