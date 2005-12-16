#ifndef __ardour_gtk_tempo_dialog_h__
#define __ardour_gtk_tempo_dialog_h__

#include <gtkmm/entry.h>
#include <gtkmm/frame.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/comboboxtext.h>

#include <ardour/types.h>
#include <ardour/tempo.h>

#include "ardour_dialog.h"

struct TempoDialog : public ArdourDialog 
{
    Gtk::Entry   bpm_entry;
    Gtk::Frame   bpm_frame;
    Gtk::VBox    vpacker;
    Gtk::Button  ok_button;
    Gtk::Button  cancel_button;
    Gtk::HBox    button_box;
    Gtk::HBox    hspacer1;
    Gtk::VBox    vspacer1;
    Gtk::Entry   when_bar_entry;
    Gtk::Entry   when_beat_entry;
    Gtk::Label   when_bar_label;
    Gtk::Label   when_beat_label;
    Gtk::Table   when_table;
    Gtk::Frame   when_frame;
    char buf[64];
    
    TempoDialog (ARDOUR::TempoMap&, jack_nframes_t, ARDOUR::stringcr_t action);
    TempoDialog (ARDOUR::TempoSection&, ARDOUR::stringcr_t action);

    double get_bpm ();
    bool   get_bbt_time (ARDOUR::BBT_Time&);
    
  private:
    void init (const ARDOUR::BBT_Time& start, double, bool);
};

struct MeterDialog : public ArdourDialog 
{
    Gtk::Entry   bpb_entry;
    Gtk::ComboBoxText note_types;
    vector<string> strings;
    Gtk::Frame   note_frame;
    Gtk::Frame   bpb_frame;
    Gtk::VBox    vpacker;
    Gtk::Button  ok_button;
    Gtk::Button  cancel_button;
    Gtk::HBox    button_box;
    Gtk::HBox    hspacer1, hspacer2;
    Gtk::VBox    vspacer1, vspacer2;
    Gtk::Entry   when_bar_entry;
    Gtk::Entry   when_beat_entry;
    Gtk::Label   when_bar_label;
    Gtk::Label   when_beat_label;
    Gtk::Table   when_table;
    Gtk::Frame   when_frame;
    char buf[64];
    
    MeterDialog (ARDOUR::TempoMap&, jack_nframes_t, ARDOUR::stringcr_t action);
    MeterDialog (ARDOUR::MeterSection&, ARDOUR::stringcr_t action);

    double get_bpb ();
    double get_note_type ();
    bool   get_bbt_time (ARDOUR::BBT_Time&);

  private:
    void init (const ARDOUR::BBT_Time&, double, double, bool);
};

#endif /* __ardour_gtk_tempo_dialog_h__ */
