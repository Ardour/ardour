/*
 Copyright (C) 2014 Waves Audio Ltd.
 
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

#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include "progress_dialog.h"
#include "i18n.h"

using namespace Gtk;
//ProgressDialog *ProgressDialog::theProgressDialog = 0;

ProgressDialog::ProgressDialog (const std::string& title,
                                      const std::string& top_message,
                                      const std::string& progress_message,
                                      const std::string& bottom_message)
: WavesDialog ( _("progress_dialog.xml"), true, false )
, _top_label ( get_label ("top_label") )
, _bottom_label ( get_label ("bottom_label") )
, _progress_bar (get_progressbar ("progress_bar"))
{
    init (title, top_message, progress_message, bottom_message);
}

ProgressDialog*
ProgressDialog::instance ()
{
    static ProgressDialog theProgressDialog;
    return &theProgressDialog;
}

void
ProgressDialog::init (const std::string& title,
                         const std::string& top_message,
                         const std::string& progress_message,
                         const std::string& bottom_message)
{
    set_modal (true);
    set_resizable (false);
    set_position (Gtk::WIN_POS_CENTER_ALWAYS);
    set_type_hint (Gdk::WINDOW_TYPE_HINT_NORMAL);
    

    set_title (title);
    set_top_label (top_message);
    set_progress_label (progress_message);
    set_bottom_label (bottom_message);
}

void
ProgressDialog::set_top_label (std::string message)
{
    _top_label.set_text (message);
}

void
ProgressDialog::set_progress_label (std::string message)
{
    _progress_bar.set_text (message);
}

void
ProgressDialog::set_bottom_label (std::string message)
{
    _bottom_label.set_text (message);
}

void
ProgressDialog::update_info (double new_progress, const char* top_message, const char* progress_message, const char* bottom_message)
{
    update_progress_gui (new_progress);
    if (top_message)
        set_top_label (top_message);
    if (progress_message)
        set_progress_label (progress_message);
    if (bottom_message)
        set_bottom_label (bottom_message);
}

void
ProgressDialog::update_progress_gui (float p)
{
    _progress_bar.set_fraction (p);
}