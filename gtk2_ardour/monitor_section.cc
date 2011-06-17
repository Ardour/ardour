#include <gdkmm/pixbuf.h>

#include "pbd/compose.h"
#include "pbd/error.h"

#include "gtkmm2ext/bindable_button.h"
#include "gtkmm2ext/tearoff.h"
#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/motionfeedback.h"

#include "ardour/dB.h"
#include "ardour/monitor_processor.h"
#include "ardour/route.h"
#include "ardour/utils.h"

#include "ardour_ui.h"
#include "gui_thread.h"
#include "monitor_section.h"
#include "public_editor.h"
#include "volume_controller.h"
#include "utils.h"

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
        , _tearoff (0)
        , gain_control (0)
        , dim_control (0)
        , solo_boost_control (0)
        , solo_cut_control (0)
        , solo_in_place_button (solo_model_group, _("SiP"))
        , afl_button (solo_model_group, _("AFL"))
        , pfl_button (solo_model_group, _("PFL"))
        , cut_all_button (_("MUTE"))
        , dim_all_button (_("dim"))
        , mono_button (_("mono"))
        , rude_solo_button (_("soloing"))
        , rude_iso_button (_("isolated"))
        , rude_audition_button (_("auditioning"))
        , exclusive_solo_button (_("Exclusive"))
        , solo_mute_override_button (_("Solo/Mute"))
{
        Glib::RefPtr<Action> act;

        if (!monitor_actions) {

                /* do some static stuff */

                register_actions ();

        }

        set_session (s);

        VBox* spin_packer;
        Label* spin_label;

        /* Dim */

        dim_control = new VolumeController (little_knob_pixbuf, boost::shared_ptr<Controllable>(), 0.0, 0.01, 0.1, true, 30, 30, true, true);

        HBox* dim_packer = manage (new HBox);
        dim_packer->show ();

        spin_label = manage (new Label (_("Dim Cut")));
        spin_packer = manage (new VBox);
        spin_packer->show ();
        spin_packer->set_spacing (6);
        spin_packer->pack_start (*dim_control, false, false);
        spin_packer->pack_start (*spin_label, false, false);

        dim_packer->set_spacing (12);
        dim_packer->pack_start (*spin_packer, true, false);

        /* Rude Solo */

	rude_solo_button.set_name ("TransportSoloAlert");
        rude_solo_button.show ();

	rude_iso_button.set_name ("MonitorIsoAlert");
        rude_iso_button.show ();

	rude_audition_button.set_name ("TransportAuditioningAlert");
        rude_audition_button.show ();

        ARDOUR_UI::Blink.connect (sigc::mem_fun (*this, &MonitorSection::do_blink));

	rude_solo_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MonitorSection::cancel_solo), false);
        UI::instance()->set_tip (rude_solo_button, _("When active, something is soloed.\nClick to de-solo everything"));

	rude_iso_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MonitorSection::cancel_isolate), false);
        UI::instance()->set_tip (rude_iso_button, _("When active, something is solo-isolated.\nClick to de-isolate everything"));

	rude_audition_button.signal_button_press_event().connect (sigc::mem_fun(*this, &MonitorSection::cancel_audition), false);
        UI::instance()->set_tip (rude_audition_button, _("When active, auditioning is active.\nClick to stop the audition"));

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

        /* Solo Boost */

        solo_boost_control = new VolumeController (little_knob_pixbuf, boost::shared_ptr<Controllable>(), 0.0, 0.01, 0.1, true, 30, 30, true, true);

        HBox* solo_packer = manage (new HBox);
        solo_packer->set_spacing (12);
        solo_packer->show ();

        spin_label = manage (new Label (_("Solo Boost")));
        spin_packer = manage (new VBox);
        spin_packer->show ();
        spin_packer->set_spacing (6);
        spin_packer->pack_start (*solo_boost_control, false, false);
        spin_packer->pack_start (*spin_label, false, false);

        solo_packer->pack_start (*spin_packer, false, true);

        /* Solo (SiP) cut */

        solo_cut_control = new VolumeController (little_knob_pixbuf, boost::shared_ptr<Controllable>(), 0.0, 0.01, 0.1, true, 30, 30, false, false);

        spin_label = manage (new Label (_("SiP Cut")));
        spin_packer = manage (new VBox);
        spin_packer->show ();
        spin_packer->set_spacing (6);
        spin_packer->pack_start (*solo_cut_control, false, false);
        spin_packer->pack_start (*spin_label, false, false);

        solo_packer->pack_start (*spin_packer, false, true);

        exclusive_solo_button.set_name (X_("MonitorOptButton"));
        ARDOUR_UI::instance()->set_tip (&exclusive_solo_button, _("Exclusive solo means that only 1 solo is active at a time"));

        act = ActionManager::get_action (X_("Monitor"), X_("toggle-exclusive-solo"));
        if (act) {
                act->connect_proxy (exclusive_solo_button);
        }

        solo_mute_override_button.set_name (X_("MonitorOptButton"));
        ARDOUR_UI::instance()->set_tip (&solo_mute_override_button, _("If enabled, solo will override mute\n(a soloed & muted track or bus will be audible)"));

        act = ActionManager::get_action (X_("Monitor"), X_("toggle-mute-overrides-solo"));
        if (act) {
                act->connect_proxy (solo_mute_override_button);
        }

        HBox* solo_opt_box = manage (new HBox);
        solo_opt_box->set_spacing (12);
        solo_opt_box->set_homogeneous (true);
        solo_opt_box->pack_start (exclusive_solo_button);
        solo_opt_box->pack_start (solo_mute_override_button);
        solo_opt_box->show ();

        upper_packer.set_spacing (12);

        Gtk::HBox* rude_box = manage (new HBox);
        rude_box->pack_start (rude_solo_button, true, true);
        rude_box->pack_start (rude_iso_button, true, true);

        upper_packer.pack_start (*rude_box, false, false);
        upper_packer.pack_start (rude_audition_button, false, false);
        upper_packer.pack_start (solo_model_box, false, false);
        upper_packer.pack_start (*solo_opt_box, false, false);
        upper_packer.pack_start (*solo_packer, false, false);

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

        cut_all_button.set_name (X_("MonitorMuteButton"));
        cut_all_button.unset_flags (Gtk::CAN_FOCUS);
        cut_all_button.set_size_request (50,50);
        cut_all_button.show ();

        HBox* bbox = manage (new HBox);

        bbox->set_spacing (12);
        bbox->pack_start (mono_button, true, true);
        bbox->pack_start (dim_all_button, true, true);

        dim_all_button.set_name (X_("MonitorDimButton"));
        dim_all_button.unset_flags (Gtk::CAN_FOCUS);
        mono_button.set_name (X_("MonitorMonoButton"));
        mono_button.unset_flags (Gtk::CAN_FOCUS);

        lower_packer.set_spacing (12);
        lower_packer.pack_start (*bbox, false, false);
        lower_packer.pack_start (cut_all_button, false, false);

        /* Gain */

        gain_control = new VolumeController (big_knob_pixbuf, boost::shared_ptr<Controllable>(), 0.781787, 0.01, 0.1, true, 80, 80, false, false);

        spin_label = manage (new Label (_("Gain")));
        spin_packer = manage (new VBox);
        spin_packer->show ();
        spin_packer->set_spacing (6);
        spin_packer->pack_start (*gain_control, false, false);
        spin_packer->pack_start (*spin_label, false, false);

        lower_packer.pack_start (*spin_packer, true, true);

        vpacker.set_border_width (12);
        vpacker.set_spacing (12);
        vpacker.pack_start (upper_packer, false, false);
        vpacker.pack_start (*dim_packer, false, false);
        vpacker.pack_start (main_table, false, false);
        vpacker.pack_start (lower_packer, false, false);

        hpacker.set_border_width (12);
        hpacker.set_spacing (12);
        hpacker.pack_start (vpacker, true, true);

        gain_control->show_all ();
        dim_control->show_all ();
        solo_boost_control->show_all ();

        main_table.show ();
        hpacker.show ();
        upper_packer.show ();
        lower_packer.show ();
        vpacker.show ();

        populate_buttons ();
        map_state ();
        assign_controllables ();

        _tearoff = new TearOff (hpacker);

        /* if torn off, make this a normal window */
        _tearoff->tearoff_window().set_type_hint (Gdk::WINDOW_TYPE_HINT_NORMAL);
        _tearoff->tearoff_window().set_title (X_("Monitor"));
        _tearoff->tearoff_window().signal_key_press_event().connect (sigc::ptr_fun (forward_key_press), false);

        /* catch changes that affect us */

        Config->ParameterChanged.connect (config_connection, invalidator (*this), ui_bind (&MonitorSection::parameter_changed, this, _1), gui_context());
}

MonitorSection::~MonitorSection ()
{
        for (ChannelButtons::iterator i = _channel_buttons.begin(); i != _channel_buttons.end(); ++i) {
                delete *i;
        }

        _channel_buttons.clear ();

        delete gain_control;
        delete dim_control;
        delete solo_boost_control;
        delete _tearoff;
}

void
MonitorSection::set_session (Session* s)
{
        AxisView::set_session (s);

        if (_session) {

                _route = _session->monitor_out ();

                if (_route) {
                        /* session with monitor section */
                        _monitor = _route->monitor_control ();
                        assign_controllables ();
                } else {
                        /* session with no monitor section */
                        _monitor.reset ();
                        _route.reset ();
                }

        } else {
                /* no session */

                _monitor.reset ();
                _route.reset ();
                control_connections.drop_connections ();
                rude_iso_button.set_active (false);
                rude_solo_button.set_active (false);

                assign_controllables ();
        }
}

MonitorSection::ChannelButtonSet::ChannelButtonSet ()
        : cut (X_(""))
        , dim (X_(""))
        , solo (X_(""))
        , invert (X_(""))
{
        cut.set_name (X_("MonitorMuteButton"));
        dim.set_name (X_("MonitorDimButton"));
        solo.set_name (X_("MixerSoloButton"));
        invert.set_name (X_("MonitorInvertButton"));

        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (cut.gobj()), false);
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (dim.gobj()), false);
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (invert.gobj()), false);
        gtk_activatable_set_use_action_appearance (GTK_ACTIVATABLE (solo.gobj()), false);

        cut.unset_flags (Gtk::CAN_FOCUS);
        dim.unset_flags (Gtk::CAN_FOCUS);
        solo.unset_flags (Gtk::CAN_FOCUS);
        invert.unset_flags (Gtk::CAN_FOCUS);
}

void
MonitorSection::populate_buttons ()
{
        if (!_monitor) {
                return;
        }

        Glib::RefPtr<Action> act;
        uint32_t nchans = _monitor->output_streams().n_audio();

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

        const uint32_t row_offset = 1;

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

                Label* label = manage (new Label (l));
                main_table.attach (*label, 0, 1, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);

                ChannelButtonSet* cbs = new ChannelButtonSet;

                _channel_buttons.push_back (cbs);

                main_table.attach (cbs->cut, 1, 2, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);
                main_table.attach (cbs->dim, 2, 3, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);
                main_table.attach (cbs->solo, 3, 4, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);
                main_table.attach (cbs->invert, 4, 5, i+row_offset, i+row_offset+1, SHRINK|FILL, SHRINK|FILL);

                snprintf (buf, sizeof (buf), "monitor-cut-%u", i+1);
                act = ActionManager::get_action (X_("Monitor"), buf);
                if (act) {
                        act->connect_proxy (cbs->cut);
                }

                snprintf (buf, sizeof (buf), "monitor-dim-%u", i+1);
                act = ActionManager::get_action (X_("Monitor"), buf);
                if (act) {
                        act->connect_proxy (cbs->dim);
                }

                snprintf (buf, sizeof (buf), "monitor-solo-%u", i+1);
                act = ActionManager::get_action (X_("Monitor"), buf);
                if (act) {
                        act->connect_proxy (cbs->solo);
                }

                snprintf (buf, sizeof (buf), "monitor-invert-%u", i+1);
                act = ActionManager::get_action (X_("Monitor"), buf);
                if (act) {
                        act->connect_proxy (cbs->invert);
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

void
MonitorSection::toggle_exclusive_solo ()
{
        if (!_monitor) {
                return;
        }

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), "toggle-exclusive-solo");
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                Config->set_exclusive_solo (tact->get_active());
        }

}


void
MonitorSection::toggle_mute_overrides_solo ()
{
        if (!_monitor) {
                return;
        }

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), "toggle-mute-overrides-solo");
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                Config->set_solo_mute_override (tact->get_active());
        }
}

void
MonitorSection::dim_all ()
{
        if (!_monitor) {
                return;
        }

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), "monitor-dim-all");
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_dim_all (tact->get_active());
        }

}

void
MonitorSection::cut_all ()
{
        if (!_monitor) {
                return;
        }

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), "monitor-cut-all");
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_cut_all (tact->get_active());
        }
}

void
MonitorSection::mono ()
{
        if (!_monitor) {
                return;
        }

        Glib::RefPtr<Action> act = ActionManager::get_action (X_("Monitor"), "monitor-mono");
        if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
                _monitor->set_mono (tact->get_active());
        }
}

void
MonitorSection::cut_channel (uint32_t chn)
{
        if (!_monitor) {
                return;
        }

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
        if (!_monitor) {
                return;
        }

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
        if (!_monitor) {
                return;
        }

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
        if (!_monitor) {
                return;
        }

        char buf[64];
        snprintf (buf, sizeof (buf), "monitor-invert-%u", chn);

        --chn; // 0-based in backend

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
        Glib::RefPtr<Action> act;

        monitor_actions = ActionGroup::create (X_("Monitor"));
	ActionManager::add_action_group (monitor_actions);

        ActionManager::register_toggle_action (monitor_actions, "monitor-mono", "", "Switch monitor to mono",
                                               sigc::mem_fun (*this, &MonitorSection::mono));

        ActionManager::register_toggle_action (monitor_actions, "monitor-cut-all", "", "Cut monitor",
                                               sigc::mem_fun (*this, &MonitorSection::cut_all));

        ActionManager::register_toggle_action (monitor_actions, "monitor-dim-all", "", "Dim monitor",
                                               sigc::mem_fun (*this, &MonitorSection::dim_all));

        act = ActionManager::register_toggle_action (monitor_actions, "toggle-exclusive-solo", "", "Toggle exclusive solo mode",
                                               sigc::mem_fun (*this, &MonitorSection::toggle_exclusive_solo));

        Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
        tact->set_active (Config->get_exclusive_solo());

        act = ActionManager::register_toggle_action (monitor_actions, "toggle-mute-overrides-solo", "", "Toggle mute overrides solo mode",
                                                     sigc::mem_fun (*this, &MonitorSection::toggle_mute_overrides_solo));

        tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
        tact->set_active (Config->get_solo_mute_override());


        /* note the 1-based counting (for naming - backend uses 0-based) */

        for (uint32_t chn = 1; chn <= 16; ++chn) {

                action_name = string_compose (X_("monitor-cut-%1"), chn);
                action_descr = string_compose (_("Cut monitor channel %1"), chn);
                ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), "", action_descr.c_str(),
                                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::cut_channel), chn));

                action_name = string_compose (X_("monitor-dim-%1"), chn);
                action_descr = string_compose (_("Dim monitor channel %1"), chn+1);
                ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), "", action_descr.c_str(),
                                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::dim_channel), chn));

                action_name = string_compose (X_("monitor-solo-%1"), chn);
                action_descr = string_compose (_("Solo monitor channel %1"), chn+1);
                ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), "", action_descr.c_str(),
                                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::solo_channel), chn));

                action_name = string_compose (X_("monitor-invert-%1"), chn);
                action_descr = string_compose (_("Invert monitor channel %1"), chn+1);
                ActionManager::register_toggle_action (monitor_actions, action_name.c_str(), "", action_descr.c_str(),
                                                       sigc::bind (sigc::mem_fun (*this, &MonitorSection::invert_channel), chn));

        }


        Glib::RefPtr<ActionGroup> solo_actions = ActionGroup::create (X_("Solo"));
        RadioAction::Group solo_group;

        ActionManager::register_radio_action (solo_actions, solo_group, "solo-use-in-place", "", "In-place solo",
                                              sigc::mem_fun (*this, &MonitorSection::solo_use_in_place));
        ActionManager::register_radio_action (solo_actions, solo_group, "solo-use-afl", "", "After Fade Listen (AFL) solo",
                                              sigc::mem_fun (*this, &MonitorSection::solo_use_afl));
        ActionManager::register_radio_action (solo_actions, solo_group, "solo-use-pfl", "", "Pre Fade Listen (PFL) solo",
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
MonitorSection::setup_knob_images ()
{
        try {

                big_knob_pixbuf = MotionFeedback::render_pixbuf (80);

        }  catch (...) {

                error << "No usable large knob image" << endmsg;
                throw failed_constructor ();
        }

        if (!big_knob_pixbuf) {
                error << "No usable large knob image" << endmsg;
                throw failed_constructor ();
        }

        try {

                little_knob_pixbuf = MotionFeedback::render_pixbuf (30);

        }  catch (...) {

                error << "No usable small knob image" << endmsg;
                throw failed_constructor ();
        }

        if (!little_knob_pixbuf) {
                error << "No usable small knob image" << endmsg;
                throw failed_constructor ();
        }

}

void
MonitorSection::update_solo_model ()
{
        const char* action_name;
        Glib::RefPtr<Action> act;

        if (Config->get_solo_control_is_listen_control()) {
		switch (Config->get_listen_position()) {
		case AfterFaderListen:
                        action_name = X_("solo-use-afl");
			break;
		case PreFaderListen:
                        action_name = X_("solo-use-pfl");
			break;
		}
        } else {
                action_name = X_("solo-use-in-place");
        }

        act = ActionManager::get_action (X_("Solo"), action_name);
        if (act) {
                Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (act);
                if (ract) {
                        ract->set_active (true);
                }
        }
}

void
MonitorSection::map_state ()
{
        if (!_route || !_monitor) {
                return;
        }

        Glib::RefPtr<Action> act;

        update_solo_model ();

        act = ActionManager::get_action (X_("Monitor"), "monitor-cut-all");
        if (act) {
                Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                if (tact) {
                        tact->set_active (_monitor->cut_all());
                }
        }

        act = ActionManager::get_action (X_("Monitor"), "monitor-dim-all");
        if (act) {
                Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                if (tact) {
                        tact->set_active (_monitor->dim_all());
                }
        }

        act = ActionManager::get_action (X_("Monitor"), "monitor-mono");
        if (act) {
                Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                if (tact) {
                        tact->set_active (_monitor->mono());
                }
        }

        uint32_t nchans = _monitor->output_streams().n_audio();

        assert (nchans == _channel_buttons.size ());

        for (uint32_t n = 0; n < nchans; ++n) {

                char action_name[32];

                snprintf (action_name, sizeof (action_name), "monitor-cut-%u", n+1);
                act = ActionManager::get_action (X_("Monitor"), action_name);
                if (act) {
                        Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                        if (tact) {
                                tact->set_active (_monitor->cut (n));
                        }
                }

                snprintf (action_name, sizeof (action_name), "monitor-dim-%u", n+1);
                act = ActionManager::get_action (X_("Monitor"), action_name);
                if (act) {
                        Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                        if (tact) {
                                tact->set_active (_monitor->dimmed (n));
                        }
                }

                snprintf (action_name, sizeof (action_name), "monitor-solo-%u", n+1);
                act = ActionManager::get_action (X_("Monitor"), action_name);
                if (act) {
                        Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                        if (tact) {
                                tact->set_active (_monitor->soloed (n));
                        }
                }

                snprintf (action_name, sizeof (action_name), "monitor-invert-%u", n+1);
                act = ActionManager::get_action (X_("Monitor"), action_name);
                if (act) {
                        Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
                        if (tact) {
                                tact->set_active (_monitor->inverted (n));
                        }
                }
        }
}

void
MonitorSection::do_blink (bool onoff)
{
        solo_blink (onoff);
        audition_blink (onoff);
}

void
MonitorSection::audition_blink (bool onoff)
{
	if (_session == 0) {
		return;
	}

	if (_session->is_auditioning()) {
		if (onoff) {
			rude_audition_button.set_state (STATE_ACTIVE);
		} else {
			rude_audition_button.set_state (STATE_NORMAL);
		}
	} else {
		rude_audition_button.set_active (false);
		rude_audition_button.set_state (STATE_NORMAL);
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

                if (_session->soloing()) {
                        rude_iso_button.set_active (_session->solo_isolated());
                }

	} else {
		// rude_solo_button.set_active (false);
		rude_solo_button.set_state (STATE_NORMAL);
                rude_iso_button.set_active (false);
	}
}

bool
MonitorSection::cancel_solo (GdkEventButton*)
{
        if (_session) {
                if (_session->soloing()) {
                        _session->set_solo (_session->get_routes(), false);
                } else if (_session->listening()) {
                        _session->set_listen (_session->get_routes(), false);
                }
        }

        return true;
}

bool
MonitorSection::cancel_isolate (GdkEventButton*)
{
        if (_session) {
                boost::shared_ptr<RouteList> rl (_session->get_routes ());
                _session->set_solo_isolated (rl, false, Session::rt_cleanup, true);
        }

        return true;
}

bool
MonitorSection::cancel_audition (GdkEventButton*)
{
	if (_session) {
		_session->cancel_audition();
	}
        return true;
}

void
MonitorSection::parameter_changed (std::string name)
{
        if (name == "solo-control-is-listen-control" ||
            name == "listen-position") {
                update_solo_model ();
        }
}

void
MonitorSection::assign_controllables ()
{
        boost::shared_ptr<Controllable> none;

        if (!gain_control) {
                /* too early - GUI controls not set up yet */
                return;
        }

        if (_session) {
                boost::shared_ptr<Controllable> c = _session->solo_cut_control();
                solo_cut_control->set_controllable (c);
        } else {
                solo_cut_control->set_controllable (none);
        }

        if (_route) {
                gain_control->set_controllable (_route->gain_control());
        } else {
                gain_control->set_controllable (none);
        }

        if (_monitor) {

                cut_all_button.set_controllable (_monitor->cut_control());
                cut_all_button.watch ();
                dim_all_button.set_controllable (_monitor->dim_control());
                dim_all_button.watch ();
                mono_button.set_controllable (_monitor->mono_control());
                mono_button.watch ();

		dim_control->set_controllable (_monitor->dim_level_control ());
		solo_boost_control->set_controllable (_monitor->solo_boost_control ());

        } else {

                cut_all_button.set_controllable (none);
                dim_all_button.set_controllable (none);
                mono_button.set_controllable (none);

                dim_control->set_controllable (none);
                solo_boost_control->set_controllable (none);
        }
}
