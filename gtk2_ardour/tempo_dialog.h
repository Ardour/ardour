#ifndef __ardour_gtk_tempo_dialog_h__
#define __ardour_gtk_tempo_dialog_h__

#include <gtkmm/entry.h>
#include <gtkmm/frame.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>
#include <gtkmm/entry.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/comboboxtext.h>

#include <ardour/types.h>
#include <ardour/tempo.h>

#include "ardour_dialog.h"

struct TempoDialog : public ArdourDialog 
{
    Gtk::Adjustment   bpm_adjustment;
    Gtk::SpinButton   bpm_spinner;
    Gtk::Frame        bpm_frame;
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
    
    TempoDialog (ARDOUR::TempoMap&, nframes_t, const string & action);
    TempoDialog (ARDOUR::TempoSection&, const string & action);

    double get_bpm ();
    bool   get_bbt_time (ARDOUR::BBT_Time&);
    
  private:
    void init (const ARDOUR::BBT_Time& start, double, bool);
    void bpm_changed ();
    bool bpm_button_press (GdkEventButton* );
    bool bpm_button_release (GdkEventButton* );
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
    
    MeterDialog (ARDOUR::TempoMap&, nframes_t, const string & action);
    MeterDialog (ARDOUR::MeterSection&, const string & action);

    double get_bpb ();
    double get_note_type ();
    bool   get_bbt_time (ARDOUR::BBT_Time&);

  private:
    void init (const ARDOUR::BBT_Time&, double, double, bool);
    bool bpb_key_press (GdkEventKey* );
    bool bpb_key_release (GdkEventKey* );
    void note_types_change ();
};

#endif /* __ardour_gtk_tempo_dialog_h__ */
