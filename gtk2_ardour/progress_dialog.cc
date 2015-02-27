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
#include <gtkmm.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm2ext/gtk_ui.h>
#include "gui_thread.h"

#include "progress_dialog.h"
#include "i18n.h"

#include "utils.h"

using namespace Gtk;

ProgressDialog::ProgressDialog (const std::string& title,
                                      const std::string& top_message,
                                      const std::string& progress_message,
                                      const std::string& bottom_message)
: WavesDialog ( _("progress_dialog.xml"), true, false )
, _top_label ( get_label ("top_label") )
, _bottom_label ( get_label ("bottom_label") )
, _progress_bar (get_progressbar ("progress_bar"))
, _cancel_button ((get_waves_button ("cancel_button")))
, num_of_steps (0)
, cur_step (0)
, hide_automatically (false)
, cancel_visible (false)
{
    init (title, top_message, progress_message, bottom_message);
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

    set_title (title);
    set_top_label (top_message);
    set_progress_label (progress_message);
    set_bottom_label (bottom_message);
    
    _cancel_button.signal_clicked.connect (sigc::mem_fun (*this, &ProgressDialog::cancel_clicked));
}

void
ProgressDialog::cancel_clicked (WavesButton*)
{
    CancelClicked (); // EMIT SIGNAL
}

void
ProgressDialog::set_top_label (std::string message)
{
    const size_t n_characters_in_line = 300 / 6; // 300 - size of the label, see progress_dialog.xml, 6 - average width of the one character
    _top_label.set_text ( ARDOUR_UI_UTILS::split_on_lines (message, n_characters_in_line) );
}

void
ProgressDialog::set_progress_label (std::string message)
{
    _progress_bar.set_text (message);
}

void
ProgressDialog::set_bottom_label (std::string message)
{
    const size_t n_characters_in_line = 300 / 6; // 300 - size of the label, see progress_dialog.xml, 6 - average width of the one character
    _bottom_label.set_text ( ARDOUR_UI_UTILS::split_on_lines (message,  n_characters_in_line) );
}

void
ProgressDialog::update_info (double new_progress, const char* top_message, const char* progress_message, const char* bottom_message)
{
    set_progress (new_progress);
    if (top_message)
        set_top_label (top_message);
    if (progress_message)
        set_progress_label (progress_message);
    if (bottom_message)
        set_bottom_label (bottom_message);
}

void
ProgressDialog::set_num_of_steps (unsigned int n, bool hide_automatically)
{
    num_of_steps = n;
    cur_step = 0;
    this->hide_automatically = hide_automatically;
    _progress_bar.set_fraction (0.0);
    set_bottom_label("0 %");
}
void
ProgressDialog::add_progress_step ()
{
   if (cur_step == num_of_steps)
	return;
        
    ++cur_step;
    
    set_bottom_label (string_compose ("%1 %", int ( ( float (cur_step) / (num_of_steps)) * 100)));
    set_progress (float (cur_step) / (num_of_steps));

    if (hide_automatically && cur_step == num_of_steps){
        hide_pd ();
    }
}

void
ProgressDialog::show_pd ()
{
    Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&ProgressDialog::show_pd_in_gui_thread , this));
}

void
ProgressDialog::show_pd_in_gui_thread ()
{
   WavesDialog::show ();
    /* Make sure the progress dialog is drawn */
    while (Glib::MainContext::get_default()->iteration (false)) {
        /* do nothing */
    }
}

void
ProgressDialog::hide_pd ()
{
    set_progress (0);
    WavesDialog::hide ();
}

void
ProgressDialog::show_cancel_button ()
{
    _cancel_button.show ();
    cancel_visible = true;
}

void
ProgressDialog::hide_cancel_button ()
{
    _cancel_button.hide ();
    cancel_visible = false;
}

void
ProgressDialog::set_cancel_button_sensitive (bool sensitive)
{
    _cancel_button.set_sensitive (sensitive);
}

void
ProgressDialog::set_progress (float p)
{
    // IF WE ARE NOT IN GUI THREAD
    // we push method set_progress () to gui event loop
    // from which it will be called afterwards
    Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&ProgressDialog::set_progress_in_gui_thread , this, p));
}

void
ProgressDialog::set_progress_in_gui_thread (float p)
{
    // it's provided that this method can be called
    // just from gui thread
    _progress_bar.set_fraction (p);

    // Make sure the progress widget gets updated
    while (Glib::MainContext::get_default()->iteration (false)) {
        /* do nothing */
    }
}

void
ProgressDialog::on_response (int response_id)
{
    if (response_id == Gtk::RESPONSE_DELETE_EVENT && cancel_visible) {
        // this happens on Close Button pressed (at the top left corner only on Mac)
        // and cancel button is visible
        CancelClicked (); // EMIT SIGNAL
    }
}

void
ProgressDialog::on_default_response ()
{
    // we should do nothing here
    return ;
}

void
ProgressDialog::on_esc_response ()
{
    // if cancel button is using
    // we should emit signal
    if (cancel_visible) {
        CancelClicked (); // EMIT SIGNAL
    }
}

bool
ProgressDialog::on_key_press_event (GdkEventKey* ev)
{
    switch (ev->keyval)
    {
        case GDK_Return:
        case GDK_KP_Enter:
            on_default_response ();
            return true;
        case GDK_Escape:
            on_esc_response ();
            return true;
    }
    return Gtk::Dialog::on_key_press_event (ev);
}
