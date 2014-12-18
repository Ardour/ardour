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

#ifndef __progress_dialog_h__
#define __progress_dialog_h__

#include "waves_dialog.h"
#include "progress_reporter.h"

namespace Gtk {
    class Label;
    class ProgressBar;
}

class ProgressDialog : public WavesDialog, public ProgressReporter
{
public:
    static ProgressDialog *instance ();
    void set_top_label (std::string message);
    void set_progress_label (std::string message);
    void set_bottom_label (std::string message);
    void update_info (double new_progress, const char* top_message, const char* progress_message, const char* bottom_message);

private:
    ProgressDialog (const std::string& title="",
                    const std::string& top_message="",
                    const std::string& progress_message="",
                    const std::string& bottom_message="");
    ~ProgressDialog () {}
    void update_progress_gui (float);
    void init (const std::string& title,
               const std::string& top_message,
               const std::string& progress_message,
               const std::string& bottom_message);

    
    Gtk::Label& _top_label;
    Gtk::Label& _bottom_label;
    Gtk::ProgressBar& _progress_bar;
};

#endif /* __progress_dialog_h__ */