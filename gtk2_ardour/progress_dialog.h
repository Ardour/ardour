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
#include "waves_button.h"
#include "progress_reporter.h"

namespace Gtk {
    class Label;
    class ProgressBar;
}

class ProgressDialog : public WavesDialog
{
public:
    ProgressDialog (const std::string& title="",
                    const std::string& top_message="",
                    const std::string& progress_message="",
                    const std::string& bottom_message="");
    ~ProgressDialog () {}
    void set_top_label (std::string message);
    void set_progress_label (std::string message);
    void set_bottom_label (std::string message);
    // initialize num of processing steps (thread-unsafe method)
    void set_num_of_steps (unsigned int, bool hide_automatically = false);
    // increment cur_step of progress process (thread-safe method)
    // it's expected that set_num_of_steps () was called previously
    void add_progress_step ();
    void update_info (double new_progress, const char* top_message, const char* progress_message, const char* bottom_message);
    void show_pd ();
	void hide_pd ();
    void show_cancel_button ();
    void hide_cancel_button ();
    void set_cancel_button_sensitive (bool sensitive);
    void set_progress (float);
    bool on_key_press_event (GdkEventKey*);
    PBD::Signal0<void> CancelClicked;

protected:
    void on_response(int response_id);
    void on_default_response ();
    void on_esc_response ();
    
private:
    void init (const std::string& title,
               const std::string& top_message,
               const std::string& progress_message,
               const std::string& bottom_message);
	void show_pd_in_gui_thread ();
    void set_progress_in_gui_thread (float);
    Gtk::Label& _top_label;
    Gtk::Label& _bottom_label;
    Gtk::ProgressBar& _progress_bar;
    WavesButton& _cancel_button;
    
    void cancel_clicked (WavesButton*);
    unsigned int num_of_steps;
    unsigned int cur_step;
    bool hide_automatically;
    bool cancel_visible;
};

#endif /* __progress_dialog_h__ */