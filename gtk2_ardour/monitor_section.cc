#include <gdkmm/pixbuf.h>

#include "pbd/compose.h"
#include "pbd/error.h"

#include "gtkmm2ext/bindable_button.h"
#include "gtkmm2ext/tearoff.h"
#include "gtkmm2ext/actions.h"

#include "ardour/dB.h"
#include "ardour/monitor_processor.h"
#include "ardour/route.h"
#include "ardour/utils.h"

#include "ardour_ui.h"
#include "monitor_section.h"
#include "utils.h"
#include "volume_controller.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace std;

Glib::RefPtr<ActionGroup> MonitorSection::monitor_actions;
Glib::RefPtr<Gdk::Pixbuf> MonitorSection::big_knob_pixbuf;
Glib::RefPtr<Gdk::Pixbuf> MonitorSection::little_knob_pixbuf;

MonitorSection::MonitorSection (Session* s)
        : AxisView (s)
        , RouteUI (s)
        , main_table (2, 3)
        , meter (s)
        , _tearoff (0)
        , gain_adjustment (1.0, 0.0, 1.0, 0.01, 0.1)
        , gain_control (0)
        , dim_adjustment (0.2, 0.0, 1.0, 0.01, 0.1) 
        , dim_control (0)
        , solo_boost_adjustment (1.0, 1.0, 2.0, 0.01, 0.1) 
        , solo_boost_control (0)
        , solo_in_place_button (solo_model_group, _("SiP"))
        , afl_button (solo_model_group, _("AFL"))
        , pfl_button (solo_model_group, _("PFL"))
        , cut_all_button (_("MUTE"))
        , dim_all_button (_("dim"))
        , mono_button (_("mono"))
        , rude_solo_button (_("soloing"))

{
        Glib::RefPtr<Action> act;

        if (!monitor_actions) {

                /* do some static stuff */

                register_actions ();

        }
        
        _route = _session->control_out ();

        if (!_route) {
                throw failed_constructor ();
        }

        _monitor = _route->monitor_control ();

        if (!_monitor) {
                throw failed_constructor ();
        }

        VBox* sub_knob_packer = manage (new VBox);
        sub_knob_packer->set_spacing (12);

        VBox* spin_packer;
        Label* spin_label;

        gain_control = new VolumeController (big_knob_pixbuf, &gain_adjustment, true);
        gain_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &MonitorSection::gain_value_changed));
        gain_control->spinner().signal_output().connect (sigc::bind (sigc::mem_fun (*this, &MonitorSection::nonlinear_gain_printer), 
                                                                     &gain_control->spinner()));

        spin_label = manage (new Label (_("Gain (dB)")));
        spin_packer = manage (new VBox);
        spin_packer->show ();
        spin_packer->set_spacing (6);
        spin_packer->pack_start (*gain_control, false, false);
        spin_packer->pack_start (*spin_label, false, false);

        sub_knob_packer->pack_start (*spin_packer, false, false);
                
        dim_control = new VolumeController (little_knob_pixbuf, &dim_adjustment, true, 30, 30);
        dim_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &MonitorSection::dim_level_changed));
        dim_control->spinner().signal_output().connect (sigc::bind (sigc::mem_fun (*this, &MonitorSection::linear_gain_printer), 
                                                                    &dim_control->spinner()));

        HBox* dim_packer = manage (new HBox);
        dim_packer->show ();

        spin_label = manage (new Label (_("Dim Cut (dB)")));
        spin_packer = manage (new VBox);
        spin_packer->show ();
        spin_packer->set_spacing (6);
        spin_packer->pack_start (*dim_control, false, false);
        spin_packer->pack_start (*spin_label, false, false); 

        dim_packer->set_spacing (12);
        dim_packer->pack_start (*spin_packer, false, false);

        VBox* keep_dim_under_vertical_size_control = manage (new VBox);
        keep_dim_under_vertical_size_control->pack_start (dim_all_button, true, false);
        keep_dim_under_vertical_size_control->show ();
        dim_all_button.set_size_request (40,40);
        dim_all_button.show ();

        dim_packer->pack_start (*keep_dim_under_vertical_size_control, false, false);
        sub_knob_packer->pack_start (*dim_packer, false, true);

        solo_boost_control = new VolumeController (little_knob_pixbuf, &solo_boost_adjustment, true, 30, 30);
        solo_boost_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &MonitorSection::solo_boost_changed));
        solo_boost_control->spinner().signal_output().connect (sigc::bind (sigc::mem_fun (*this, &MonitorSection::linear_gain_printer),
                                                                           &solo_boost_control->spinner()));

        HBox* solo_packer = manage (new HBox);
        solo_packer->show ();

        spin_label = manage (new Label (_("Solo Boost (dB)")));
        spin_packer = manage (new VBox);
        spin_packer->show ();
        spin_packer->set_spacing (6);
        spin_packer->pack_start (*solo_boost_control, false, false);
        spin_packer->pack_start (*spin_label, false, false); 

        VBox* keep_rude_solo_under_vertical_size_control = manage (new VBox);
        keep_rude_solo_under_vertical_size_control->show ();
        keep_rude_solo_under_vertical_size_control->pack_start (rude_solo_button, true, false);

        solo_packer->set_spacing (12);
        solo_packer->pack_start (*spin_packer, false, false);
        solo_packer->pack_start (*keep_rude_solo_under_vertical_size_control, true, false);

	rude_solo_button.set_name ("TransportSoloAlert");
        rude_solo_button.show ();

        ARDOUR_UI::Blink.connect (sigc::mem_fun (*this, &MonitorSection::solo_blink));
	rude_solo_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MonitorSection::cancel_solo));
        UI::instance()->set_tip (rude_solo_button, _("When active, something is soloed.\nClick to de-solo everything"));

        sub_knob_packer->pack_start (*solo_packer, false, true);

        knob_packer.pack_start (*sub_knob_packer, false, true);

        sub_knob_packer->show ();
        knob_packer.show ();
        gain_control->show_all ();
        dim_control->show_all ();
        solo_boost_control->show_all ();

        meter.set_meter (&_route->peak_meter());
        meter.setup_meters (300, 5);
        
        table_knob_packer.pack_start (main_table, true, true);
        table_knob_packer.pack_start (knob_packer, false, false);

        table_knob_packer.show ();

        solo_model_box.set_spacing (6);
        solo_model_box.pack_start (solo_in_place_button, false, false);
        solo_model_box.pack_start (afl_button, false, false);
        solo_model_box.pack_start (pfl_button, false, false);

        solo_in_place_button.show ();
        afl_button.show ();
        pfl_button.show ();
        solo_model_box.show ();

        act = ActionManager::get_action (X_("Solo"), X_("solo-use-in-place"));
        if (act) {
                act->connect_proxy (solo_in_place_button);
        } 

        act = ActionManager::get_action (X_("Solo"), X_("solo-use-afl"));
        if (act) {
                act->connect_proxy (afl_button);
        } 

        act = ActionManager::get_action (X_("Solo"), X_("solo-use-pfl"));
        if (act) {
                act->connect_proxy (pfl_button);
        } 

        upper_packer.pack_start (solo_model_box, false, false);

        act = ActionManager::get_action (X_("Monitor"), X_("monitor-cut-all"));
        if (act) {
                act->connect_proxy (cut_all_button);
        } 

        act = ActionManager::get_action (X_("Monitor"), X_("monitor-dim-all"));
        if (act) {
                act->connect_proxy (dim_all_button);
        } 

        act = ActionManager::get_action (X_("Monitor"), X_("monitor-mono"));
        if (act) {
                act->connect_proxy (mono_button);
        } 

        cut_all_button.set_size_request (50,50);
        cut_all_button.show ();

        lower_packer.set_spacing (12);
        lower_packer.pack_start (mono_button, false, false);
        lower_packer.pack_start (cut_all_button, false, false);

        vpacker.set_border_width (12);
        vpacker.set_spacing (12);
        vpacker.pack_start (upper_packer, false, false);
        vpacker.pack_start (table_knob_packer, false, false);
        vpacker.pack_start (lower_packer, false, false);

        VBox* keep_meter_under_control = manage (new VBox);
        keep_meter_under_control->pack_start (meter, false, false);
        keep_meter_under_control->show ();

        hpacker.set_border_width (12);
        hpacker.set_spacing (12);
        hpacker.pack_start (*keep_meter_under_control, false, false);
        hpacker.pack_start (vpacker, true, true);

        main_table.show ();
        hpacker.show ();
        upper_packer.show ();
        lower_packer.show ();
        vpacker.show ();
        meter.show_all ();

        populate_buttons ();
        map_state ();

        _tearoff = new TearOff (hpacker);
        /* if torn off, make this a normal window */
        _tearoff->tearoff_window().set_type_hint (Gdk::WINDOW_TYPE_HINT_NORMAL);
        _tearoff->tearoff_window().set_title (X_("Monitor"));
}

MonitorSection::~MonitorSection ()
{
        delete gain_control;
        delete dim_control;
        delete solo_boost_control;
        delete _tearoff;
}

void
MonitorSection::populate_buttons ()
{
        Glib::RefPtr<Action> act;
        uint32_t nchans = _route->monitor_control()->output_streams().n_audio();
        
        main_table.resize (nchans+1, 5);
        main_table.set_col_spacings (6);
        main_table.set_row_spacings (6);
        main_table.set_homogeneous (true);

        Label* l1 = manage (new Label (X_("out")));
        main_table.attach (*l1, 0, 1, 0, 1, SHRINK|FILL, SHRINK|FILL);
        l1 = manage (new Label (X_("cut")));
        main_table.attach (*l1, 1, 2, 0, 1, SHRINK|FILL, SHRINK|FILL);
        l1 = manage (new Label (X_("dim")));
        main_table.attach (*l1, 2, 3, 0, 1, SHRINK|FILL, SHRINK|FILL);
        l1 = manage (new Label (X_("solo")));
        main_table.attach (*l1, 3, 4, 0, 1, SHRINK|FILL, SHRINK|FILL);
        l1 = manage (new Label (X_("inv")));
        main_table.attach (*l1, 4, 5, 0, 1, SHRINK|FILL, SHRINK|FILL);

#if 0
        /* the "all" buttons for cut & dim */
        
        Label *la = manage (new Label (X_("all")));
        main_table.attach (*la, 0, 1, 1, 2, SHRINK|FILL, SHRINK|FILL);


        /* cut all */

        BindableToggleButton* ca = manage (new BindableToggleButton (X_("")));
        ca->set_name (X_("MixerMuteButton"));
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (ca->gobj()), false);
        main_table.attach (*ca, 1, 2, 1, 2, SHRINK|FILL, SHRINK|FILL);

        act = ActionManager::get_action (X_("Monitor"), X_("monitor-cut-all"));
        if (act) {
                act->connect_proxy (*ca);
        } 

        /* dim all */

        BindableToggleButton* da = manage (new BindableToggleButton (X_("")));
        da->set_name (X_("MixerMuteButton"));
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (da->gobj()), false);
        main_table.attach (*da, 2, 3, 1, 2, SHRINK|FILL, SHRINK|FILL);

        act = ActionManager::get_action (X_("Monitor"), X_("monitor-dim-all"));
        if (act) {
                act->connect_proxy (*da);
        } 

        uint32_t row_offset = 2;
#else
        uint32_t row_offset = 1;
#endif

        for (uint32_t i = 0; i < nchans; ++i) {
                
                string l;
                char buf[64];

                if (nchans == 2) {
                        if (i == 0) {
                                l = "L";
                        } else {
                                l = "R";
                        }
                } else {
                        char buf[32];
                        snprintf (buf, sizeof (buf), "%d", i+1);
                        l = buf;
                }

                Label* c1 = manage (new Label (l));
                main_table.attach (*c1, 0, 1, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);
                
                /* Cut */

                BindableToggleButton* c2 = manage (new BindableToggleButton (X_("")));
                c2->set_name (X_("MixerMuteButton"));
                gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (c2->gobj()), false);
                main_table.attach (*c2, 1, 2, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);
                
                snprintf (buf, sizeof (buf), "monitor-cut-%u", i+1);
                act = ActionManager::get_action (X_("Monitor"), buf);
                if (act) {
                        act->connect_proxy (*c2);
                } 

                /* Dim */

                BindableToggleButton* c3 = manage (new BindableToggleButton (X_("")));
                c3->set_name (X_("MixerMuteButton"));
                gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (c2->gobj()), false);
                main_table.attach (*c3, 2, 3, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);

                snprintf (buf, sizeof (buf), "monitor-dim-%u", i+1);
                act = ActionManager::get_action (X_("Monitor"), buf);
                if (act) {
                        act->connect_proxy (*c3);
                }

                /* Solo */

                BindableToggleButton* c4 = manage (new BindableToggleButton (X_("")));
                c4->set_name (X_("MixerSoloButton"));
                gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (c2->gobj()), false);
                main_table.attach (*c4, 3, 4, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);

                snprintf (buf, sizeof (buf), "monitor-solo-%u", i+1);
                act = ActionManager::get_action (X_("Monitor"), buf);
                if (act) {
                        act->connect_proxy (*c4);
                }

                /* Invert (Polarity/Phase) */

                BindableToggleButton* c5 = manage (new BindableToggleButton (X_("")));
                c5->set_name (X_("MixerPhaseInvertButton"));
                gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (c2->gobj()), false);
                main_table.attach (*c5, 4, 5, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);

                snprintf (buf, sizeof (buf), "monitor-invert-%u", i+1);
                act = ActionManager::get_action (X_("Monitor"), buf);
                if (act) {
                        act->connect_proxy (*c5);
                }

        }

        main_table.show_all ();
}

void 
MonitorSection::set_button_names ()
{
        rec_enable_button_label.set_text ("rec");
        mute_button_label.set_text ("rec");
        solo_button_label.set_text ("rec");
}

Widget&
MonitorSection::pack_widget () const
{
        return *_tearoff;
}

void
MonitorSection::dim_all ()
{
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), "monitor-dim-all");
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_dim_all (tact->get_active());
        }

}

void
MonitorSection::cut_all ()
{
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), "monitor-cut-all");
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_cut_all (tact->get_active());
        }
}

void
MonitorSection::mono ()
{
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), "monitor-mono");
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_mono (tact->get_active());
        }
}

void
MonitorSection::cut_channel (uint32_t chn)
{
        char buf[64];
        snprintf (buf, sizeof (buf), "monitor-cut-%u", chn);

        --chn; // 0-based in backend

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), buf);
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_cut (chn, tact->get_active());
        }
}

void
MonitorSection::dim_channel (uint32_t chn)
{
        char buf[64];
        snprintf (buf, sizeof (buf), "monitor-dim-%u", chn);

        --chn; // 0-based in backend

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), buf);
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_dim (chn, tact->get_active());
        }

}

void
MonitorSection::solo_channel (uint32_t chn)
{
        char buf[64];
        snprintf (buf, sizeof (buf), "monitor-solo-%u", chn);

        --chn; // 0-based in backend

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), buf);
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_solo (chn, tact->get_active());
        }

}

void
MonitorSection::invert_channel (uint32_t chn)
{
        char buf[64];

        --chn; // 0-based in backend

        snprintf (buf, sizeof (buf), "monitor-invert-%u", chn);
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), buf);
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_polarity (chn, tact->get_active());
        }

}

void
MonitorSection::register_actions ()
{
        string action_name;
        string action_descr;

        monitor_actions = ActionGroup::create (X_("Monitor"));
	ActionManager::add_action_group (monitor_actions);

        ActionManager::register_toggle_action (monitor_actions, "monitor-mono", "", 
                                               sigc::mem_fun (*this, &MonitorSection::mono));

        ActionManager::register_toggle_action (monitor_actions, "monitor-cut-all", "", 
                                               sigc::mem_fun (*this, &MonitorSection::cut_all));

        ActionManager::register_toggle_action (monitor_actions, "monitor-dim-all", "", 
                                               sigc::mem_fun (*this, &MonitorSection::dim_all));

        /* note the 1-based counting for naming vs. 0-based for action */

        for (uint32_t chn = 1; chn <= 16; ++chn) {

                /* for the time being, do not use the action description because it always
                   shows up in the buttons, which is undesirable.
                */

                action_name = string_compose (X_("monitor-cut-%1"), chn);
                action_descr = string_compose (_("Cut Monitor Chn %1"), chn);
                ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), "", 
                                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::cut_channel), chn));

                action_name = string_compose (X_("monitor-dim-%1"), chn);
                action_descr = string_compose (_("Dim Monitor Chn %1"), chn+1);
                ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), "", 
                                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::dim_channel), chn));

                action_name = string_compose (X_("monitor-solo-%1"), chn);
                action_descr = string_compose (_("Solo Monitor Chn %1"), chn);
                ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), "", 
                                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::solo_channel), chn));

                action_name = string_compose (X_("monitor-invert-%1"), chn);
                action_descr = string_compose (_("Invert Monitor Chn %1"), chn);
                ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), "", 
                                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::invert_channel), chn));

        }


        Glib::RefPtr<ActionGroup> solo_actions = ActionGroup::create (X_("Solo"));
        RadioAction::Group solo_group;

        ActionManager::register_radio_action (solo_actions, solo_group, "solo-use-in-place", "",
                                              sigc::mem_fun (*this, &MonitorSection::solo_use_in_place));
        ActionManager::register_radio_action (solo_actions, solo_group, "solo-use-afl", "",
                                              sigc::mem_fun (*this, &MonitorSection::solo_use_afl));
        ActionManager::register_radio_action (solo_actions, solo_group, "solo-use-pfl", "",
                                              sigc::mem_fun (*this, &MonitorSection::solo_use_pfl));

	ActionManager::add_action_group (solo_actions);
}

void
MonitorSection::solo_use_in_place ()
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Solo"), X_("solo-use-in-place"));

        if (act) {
                Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (act);
                if (ract) {
                        Config->set_solo_control_is_listen_control (!ract->get_active());
                }
        }
}

void
MonitorSection::solo_use_afl ()
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/
        
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Solo"), X_("solo-use-afl"));
        if (act) {
                Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (act);
                if (ract) {
                        if (ract->get_active()) {
                                Config->set_listen_position (AfterFaderListen);
                                Config->set_solo_control_is_listen_control (true);
                        }
                }
        }
}

void
MonitorSection::solo_use_pfl ()
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Solo"), X_("solo-use-afl"));
        if (act) {
                Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (act);
                if (ract) {
                        if (ract->get_active()) {
                                Config->set_listen_position (PreFaderListen);
                                Config->set_solo_control_is_listen_control (true);
                        }
                }
        }
}

void
MonitorSection::fast_update ()
{
        meter.update_meters ();
}

void
MonitorSection::setup_knob_images ()
{
        try {
                
                big_knob_pixbuf = ::get_icon ("knob");
                
        }  catch (...) {
                
                error << "No knob image found (or not loadable) at "
                      << " .... "
                      << endmsg;
                throw failed_constructor ();
        }
        
        try {
                
                little_knob_pixbuf = ::get_icon ("littleknob");
                
        }  catch (...) {
                
                error << "No knob image found (or not loadable) at "
                      << " .... "
                      << endmsg;
                throw failed_constructor ();
        }
}

void
MonitorSection::gain_value_changed ()
{
        _route->set_gain (slider_position_to_gain (gain_adjustment.get_value()), this);
}

void
MonitorSection::dim_level_changed ()
{
        _monitor->set_dim_level (dim_adjustment.get_value());
}

void
MonitorSection::solo_boost_changed ()
{
        _monitor->set_solo_boost_level (solo_boost_adjustment.get_value());
}

bool
MonitorSection::nonlinear_gain_printer (SpinButton* button)
{
        double val = button->get_adjustment()->get_value();
        char buf[16];
        snprintf (buf, sizeof (buf), "%.1f", accurate_coefficient_to_dB (slider_position_to_gain (val)));
        button->set_text (buf);
        return true;
}

bool
MonitorSection::linear_gain_printer (SpinButton* button)
{
        double val = button->get_adjustment()->get_value();
        char buf[16];
        snprintf (buf, sizeof (buf), "%.1f", accurate_coefficient_to_dB (val));
        button->set_text (buf);
        return true;
}

void
MonitorSection::map_state ()
{
        gain_control->get_adjustment()->set_value (gain_to_slider_position (_route->gain_control()->get_value()));
        dim_control->get_adjustment()->set_value (_monitor->dim_level());
        solo_boost_control->get_adjustment()->set_value (_monitor->solo_boost_level());

        const char *action_name;

        if (Config->get_solo_control_is_listen_control()) {
		switch (Config->get_listen_position()) {
		case AfterFaderListen:
                        action_name = X_("solo-use-afl");
			break;
		case PreFaderListen:
                        action_name = X_("solo-use-afl");
			break;
		}
        } else {
                action_name = X_("solo-use-in-place");
        }
        
        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Solo"), action_name);
        if (act) {
                Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (act);
                if (ract) {
                        ract->set_active (true);
                }
        }
}

void
MonitorSection::solo_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->soloing() || _session->listening()) {
		if (onoff) {
			rude_solo_button.set_state (STATE_ACTIVE);
		} else {
			rude_solo_button.set_state (STATE_NORMAL);
		}
	} else {
		rude_solo_button.set_active (false);
		rude_solo_button.set_state (STATE_NORMAL);
	}
}

bool
MonitorSection::cancel_solo (GdkEventButton* ev)
{
        if (_session && _session->soloing()) {
                _session->set_solo (_session->get_routes(), false);
        }

        return true;
}
