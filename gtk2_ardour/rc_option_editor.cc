#include <gtkmm/liststore.h>
#include <gtkmm/stock.h>
#include <gtkmm/scale.h>
#include <gtkmm2ext/utils.h>
#include "pbd/fpu.h"
#include "midi++/manager.h"
#include "midi++/factory.h"
#include "ardour/rc_configuration.h"
#include "ardour/control_protocol_manager.h"
#include "control_protocol/control_protocol.h"
#include "rc_option_editor.h"
#include "utils.h"
#include "midi_port_dialog.h"
#include "sfdb_ui.h"
#include "keyboard.h"
#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace ARDOUR;

class MIDIPorts : public OptionEditorBox
{
public:
	MIDIPorts (RCConfiguration* c)
		: _rc_config (c),
		  _add_port_button (Stock::ADD)
	{
		_store = ListStore::create (_model);
		_view.set_model (_store);
		_view.append_column (_("Name"), _model.name);
		_view.get_column(0)->set_resizable (true);
		_view.get_column(0)->set_expand (true);
		_view.append_column_editable (_("Online"), _model.online);
		_view.append_column_editable (_("Trace input"), _model.trace_input);
		_view.append_column_editable (_("Trace output"), _model.trace_output);

		HBox* h = manage (new HBox);
		h->set_spacing (4);
		h->pack_start (_view, true, true);

		VBox* v = manage (new VBox);
		v->set_spacing (4);
		v->pack_start (_add_port_button, false, false);
		h->pack_start (*v, false, false);

		_box->pack_start (*h);

		Table* t = manage (new Table (2, 4));
		t->set_spacings (12);

		int n = 0;
		Label* l = manage (new Label (_("MTC:")));
		l->set_alignment (1, 0.5);
		t->attach (*l, 0, 1, n, n + 1, EXPAND | FILL, FILL);
		t->attach (_mtc_combo, 1, 2, n, n + 1, EXPAND | FILL, EXPAND | FILL);
		++n;
		
		l = manage (new Label (_("MIDI clock:")));
		l->set_alignment (1, 0.5);
		t->attach (*l, 0, 1, n, n + 1, FILL, FILL);
		t->attach (_midi_clock_combo, 1, 2, n, n + 1, FILL, FILL);
		++n;

		l = manage (new Label (_("MMC:")));
		l->set_alignment (1, 0.5);
		t->attach (*l, 0, 1, n, n + 1, FILL, FILL);
		t->attach (_mmc_combo, 1, 2, n, n + 1, FILL, FILL);
		++n;

		l = manage (new Label (_("MIDI parameter control:")));
		l->set_alignment (1, 0.5);
		t->attach (*l, 0, 1, n, n + 1, FILL, FILL);
		t->attach (_mpc_combo, 1, 2, n, n + 1, FILL, FILL);
		++n;

		_box->pack_start (*t, true, true);

		ports_changed ();

		_store->signal_row_changed().connect (mem_fun (*this, &MIDIPorts::model_changed));

		_add_port_button.signal_clicked().connect (mem_fun (*this, &MIDIPorts::add_port_clicked));
		_mtc_combo.signal_changed().connect (mem_fun (*this, &MIDIPorts::mtc_combo_changed));
		_mmc_combo.signal_changed().connect (mem_fun (*this, &MIDIPorts::mmc_combo_changed));
		_mpc_combo.signal_changed().connect (mem_fun (*this, &MIDIPorts::mpc_combo_changed));
		_midi_clock_combo.signal_changed().connect (mem_fun (*this, &MIDIPorts::midi_clock_combo_changed));
	}

	void parameter_changed (string const & p)
	{
		if (p == "mtc-port-name") {
			_mtc_combo.set_active_text (_rc_config->get_mtc_port_name());
		} else if (p == "mmc-port-name") {
			_mmc_combo.set_active_text (_rc_config->get_mmc_port_name());
		} else if (p == "midi-port-name") {
			_mpc_combo.set_active_text (_rc_config->get_midi_port_name());
		} else if (p == "midi-clock-port-name") {
			_midi_clock_combo.set_active_text (_rc_config->get_midi_clock_port_name());
		} 
	}

	void set_state_from_config ()
	{
		parameter_changed ("mtc-port-name");
		parameter_changed ("mmc-port-name");
		parameter_changed ("midi-port-name");
		parameter_changed ("midi-clock-port-name");
	}

	void mtc_combo_changed ()
	{
		_rc_config->set_mtc_port_name (_mtc_combo.get_active_text());
	}

	void mmc_combo_changed ()
	{
		_rc_config->set_mmc_port_name (_mmc_combo.get_active_text());
	}

	void mpc_combo_changed ()
	{
		_rc_config->set_midi_port_name (_mpc_combo.get_active_text());
	}

	void midi_clock_combo_changed ()
	{
		_rc_config->set_midi_clock_port_name (_midi_clock_combo.get_active_text());
	}
	
private:

	void model_changed (TreeModel::Path const & p, TreeModel::iterator const & i)
	{
		TreeModel::Row r = *i;

		MIDI::Port* port = r[_model.port];
		if (!port) {
			return;
		}

		if (port->input()) {
			
			if (r[_model.online] == port->input()->offline()) {
				port->input()->set_offline (!r[_model.online]);
			}

			if (r[_model.trace_input] != port->input()->tracing()) {
				port->input()->trace (r[_model.trace_input], &cerr, string (port->name()) + _(" input: "));
			}
		}

		if (port->output()) {

			if (r[_model.trace_output] != port->output()->tracing()) {
				port->output()->trace (r[_model.trace_output], &cerr, string (port->name()) + _(" output: "));
			}
			
		}

		
				
	}

	void setup_ports_combo (ComboBoxText& c)
	{
		c.clear_items ();
		MIDI::Manager::PortMap const & ports = MIDI::Manager::instance()->get_midi_ports ();
		for (MIDI::Manager::PortMap::const_iterator i = ports.begin(); i != ports.end(); ++i) {
			c.append_text (i->first);
		}
	}

	void ports_changed ()
	{
		/* XXX: why is this coming from here? */
		MIDI::Manager::PortMap const & ports = MIDI::Manager::instance()->get_midi_ports ();

		_store->clear ();

		for (MIDI::Manager::PortMap::const_iterator i = ports.begin(); i != ports.end(); ++i) {

			TreeModel::Row r = *_store->append ();

			r[_model.name] = i->first;

			if (i->second->input()) {
				r[_model.online] = !i->second->input()->offline();
				i->second->input()->OfflineStatusChanged.connect (bind (mem_fun (*this, &MIDIPorts::port_offline_changed), i->second));
				r[_model.trace_input] = i->second->input()->tracing();
			}

			if (i->second->output()) {
				r[_model.trace_output] = i->second->output()->tracing();
			}

			r[_model.port] = i->second;
		}

		setup_ports_combo (_mtc_combo);
		setup_ports_combo (_midi_clock_combo);
		setup_ports_combo (_mmc_combo);
		setup_ports_combo (_mpc_combo);
	}

	void port_offline_changed (MIDI::Port* p)
	{
		if (!p->input()) {
			return;
		}

		for (TreeModel::Children::iterator i = _store->children().begin(); i != _store->children().end(); ++i) {
			if ((*i)[_model.port] == p) {
				(*i)[_model.online] = !p->input()->offline();
			}
		}
	}

	void add_port_clicked ()
	{
		MidiPortDialog dialog;

		dialog.set_position (WIN_POS_MOUSE);

		dialog.show ();
		
		int const r = dialog.run ();

		switch (r) {
		case RESPONSE_ACCEPT:
			break;
		default:
			return;
			break;
		}

		Glib::ustring const mode = dialog.port_mode_combo.get_active_text ();
		string smod;

		if (mode == _("input")) {
			smod = X_("input");
		} else if (mode == (_("output"))) {
			smod = X_("output");
		} else {
			smod = "duplex";
		}

		XMLNode node (X_("MIDI-port"));
		
		node.add_property ("tag", dialog.port_name.get_text());
		node.add_property ("device", X_("ardour")); // XXX this can't be right for all types
		node.add_property ("type", MIDI::PortFactory::default_port_type());
		node.add_property ("mode", smod);
		
		if (MIDI::Manager::instance()->add_port (node) != 0) {
			ports_changed ();
		}
	}
	
	class MIDIModelColumns : public TreeModelColumnRecord
	{
	public:
		MIDIModelColumns ()
		{
			add (name);
			add (online);
			add (trace_input);
			add (trace_output);
			add (port);
		}

		TreeModelColumn<string> name;
		TreeModelColumn<bool> online;
		TreeModelColumn<bool> trace_input;
		TreeModelColumn<bool> trace_output;
		TreeModelColumn<MIDI::Port*> port;
	};

	RCConfiguration* _rc_config;
	Glib::RefPtr<ListStore> _store;
	MIDIModelColumns _model;
	TreeView _view;
	Button _add_port_button;
	ComboBoxText _mtc_combo;
	ComboBoxText _midi_clock_combo;
	ComboBoxText _mmc_combo;
	ComboBoxText _mpc_combo;
};


class ClickOptions : public OptionEditorBox
{
public:
	ClickOptions (RCConfiguration* c, ArdourDialog* p)
		: _rc_config (c),
		  _parent (p)
	{
		Table* t = manage (new Table (2, 3));
		t->set_spacings (4);

		Label* l = manage (new Label (_("Click audio file:")));
		l->set_alignment (1, 0.5);
		t->attach (*l, 0, 1, 0, 1, FILL);
		t->attach (_click_path_entry, 1, 2, 0, 1, FILL);
		Button* b = manage (new Button (_("Browse...")));
		b->signal_clicked().connect (mem_fun (*this, &ClickOptions::click_browse_clicked));
		t->attach (*b, 2, 3, 0, 1, FILL);

		l = manage (new Label (_("Click emphasis audio file:")));
		l->set_alignment (1, 0.5);
		t->attach (*l, 0, 1, 1, 2, FILL);
		t->attach (_click_emphasis_path_entry, 1, 2, 1, 2, FILL);
		b = manage (new Button (_("Browse...")));
		b->signal_clicked().connect (mem_fun (*this, &ClickOptions::click_emphasis_browse_clicked));
		t->attach (*b, 2, 3, 1, 2, FILL);

		_box->pack_start (*t, false, false);
	}
	
	void parameter_changed (string const & p)
	{
		if (p == "click-sound") {
			_click_path_entry.set_text (_rc_config->get_click_sound());
		} else if (p == "click-emphasis-sound") {
			_click_emphasis_path_entry.set_text (_rc_config->get_click_emphasis_sound());
		}
	}

	void set_state_from_config ()
	{
		parameter_changed ("click-sound");
		parameter_changed ("click-emphasis-sound");
	}

private:	

	void click_browse_clicked ()
	{
		SoundFileChooser sfdb (*_parent, _("Choose Click"));

		sfdb.show_all ();
		sfdb.present ();
		
		if (sfdb.run () == RESPONSE_OK) {
			click_chosen (sfdb.get_filename());
		}
	}

	void click_chosen (string const & path)
	{
		_click_path_entry.set_text (path);
		_rc_config->set_click_sound (path);
	}

	void click_emphasis_browse_clicked ()
	{
		SoundFileChooser sfdb (*_parent, _("Choose Click Emphasis"));

		sfdb.show_all ();
		sfdb.present ();
		
		if (sfdb.run () == RESPONSE_OK) {
			click_emphasis_chosen (sfdb.get_filename());
		}
	}

	void click_emphasis_chosen (string const & path)
	{
		_click_emphasis_path_entry.set_text (path);
		_rc_config->set_click_emphasis_sound (path);
	}

	RCConfiguration* _rc_config;
	ArdourDialog* _parent;
	Entry _click_path_entry;
	Entry _click_emphasis_path_entry;
};

class UndoOptions : public OptionEditorBox
{
public:
	UndoOptions (RCConfiguration* c) :
		_rc_config (c),
		_limit_undo_button (_("Limit undo history to")),
		_save_undo_button (_("Save undo history of"))
	{
		Table* t = new Table (2, 3);
		t->set_spacings (4);

		t->attach (_limit_undo_button, 0, 1, 0, 1, FILL);
		_limit_undo_spin.set_range (0, 512);
		_limit_undo_spin.set_increments (1, 10);
		t->attach (_limit_undo_spin, 1, 2, 0, 1, FILL | EXPAND);
		Label* l = manage (new Label (_("commands")));
		l->set_alignment (0, 0.5);
		t->attach (*l, 2, 3, 0, 1);

		t->attach (_save_undo_button, 0, 1, 1, 2, FILL);
		_save_undo_spin.set_range (0, 512);
		_save_undo_spin.set_increments (1, 10);
		t->attach (_save_undo_spin, 1, 2, 1, 2, FILL | EXPAND);
		l = manage (new Label (_("commands")));
		l->set_alignment (0, 0.5);
		t->attach (*l, 2, 3, 1, 2);

		_box->pack_start (*t);

		_limit_undo_button.signal_toggled().connect (mem_fun (*this, &UndoOptions::limit_undo_toggled));
		_limit_undo_spin.signal_value_changed().connect (mem_fun (*this, &UndoOptions::limit_undo_changed));
		_save_undo_button.signal_toggled().connect (mem_fun (*this, &UndoOptions::save_undo_toggled));
		_save_undo_spin.signal_value_changed().connect (mem_fun (*this, &UndoOptions::save_undo_changed));
	}

	void parameter_changed (string const & p)
	{
		if (p == "history-depth") {
			int32_t const d = _rc_config->get_history_depth();
			_limit_undo_button.set_active (d != 0);
			_limit_undo_spin.set_sensitive (d != 0);
			_limit_undo_spin.set_value (d);
		} else if (p == "save-history") {
			bool const x = _rc_config->get_save_history ();
			_save_undo_button.set_active (x);
			_save_undo_spin.set_sensitive (x);
		} else if (p == "save-history-depth") {
			_save_undo_spin.set_value (_rc_config->get_saved_history_depth());
		}
	}

	void set_state_from_config ()
	{
		parameter_changed ("save-history");
		parameter_changed ("history-depth");
		parameter_changed ("save-history-depth");
	}

	void limit_undo_toggled ()
	{
		bool const x = _limit_undo_button.get_active ();
		_limit_undo_spin.set_sensitive (x);
		int32_t const n = x ? 16 : 0;
		_limit_undo_spin.set_value (n);
		_rc_config->set_history_depth (n);
	}

	void limit_undo_changed ()
	{
		_rc_config->set_history_depth (_limit_undo_spin.get_value_as_int ());
	}

	void save_undo_toggled ()
	{
		bool const x = _save_undo_button.get_active ();
		_rc_config->set_save_history (x);
	}

	void save_undo_changed ()
	{
		_rc_config->set_saved_history_depth (_save_undo_spin.get_value_as_int ());
	}
	
private:
	RCConfiguration* _rc_config;
	CheckButton _limit_undo_button;
	SpinButton _limit_undo_spin;
	CheckButton _save_undo_button;
	SpinButton _save_undo_spin;
};



static const struct {
    const char *name;
    guint modifier;
} modifiers[] = {

#ifdef GTKOSX

	/* Command = Meta
	   Option/Alt = Mod1
	*/

	{ "Shift", GDK_SHIFT_MASK },
	{ "Command", GDK_META_MASK },
	{ "Control", GDK_CONTROL_MASK },
	{ "Option", GDK_MOD1_MASK },
	{ "Command-Shift", GDK_MOD1_MASK|GDK_SHIFT_MASK },
	{ "Command-Option", GDK_MOD1_MASK|GDK_MOD5_MASK },
	{ "Shift-Option", GDK_SHIFT_MASK|GDK_MOD5_MASK },
	{ "Shift-Command-Option", GDK_MOD5_MASK|GDK_SHIFT_MASK|GDK_MOD1_MASK },

#else
	{ "Shift", GDK_SHIFT_MASK },
	{ "Control", GDK_CONTROL_MASK },
	{ "Alt (Mod1)", GDK_MOD1_MASK },
	{ "Control-Shift", GDK_CONTROL_MASK|GDK_SHIFT_MASK },
	{ "Control-Alt", GDK_CONTROL_MASK|GDK_MOD1_MASK },
	{ "Shift-Alt", GDK_SHIFT_MASK|GDK_MOD1_MASK },
	{ "Control-Shift-Alt", GDK_CONTROL_MASK|GDK_SHIFT_MASK|GDK_MOD1_MASK },
	{ "Mod2", GDK_MOD2_MASK },
	{ "Mod3", GDK_MOD3_MASK },
	{ "Mod4", GDK_MOD4_MASK },
	{ "Mod5", GDK_MOD5_MASK },
#endif
	{ 0, 0 }
};


class KeyboardOptions : public OptionEditorBox
{
public:
	KeyboardOptions () :
		  _delete_button_adjustment (3, 1, 5),
		  _delete_button_spin (_delete_button_adjustment),
		  _edit_button_adjustment (3, 1, 5),
		  _edit_button_spin (_edit_button_adjustment)
		
	{
		/* internationalize and prepare for use with combos */

		vector<string> dumb;
		for (int i = 0; modifiers[i].name; ++i) {
			dumb.push_back (_(modifiers[i].name));
		}

		set_popdown_strings (_edit_modifier_combo, dumb);
		_edit_modifier_combo.signal_changed().connect (mem_fun(*this, &KeyboardOptions::edit_modifier_chosen));

		for (int x = 0; modifiers[x].name; ++x) {
			if (modifiers[x].modifier == Keyboard::edit_modifier ()) {
				_edit_modifier_combo.set_active_text (_(modifiers[x].name));
				break;
			}
		}

		Table* t = manage (new Table (4, 4));
		t->set_spacings (4);

		Label* l = manage (new Label (_("Edit using:")));
		l->set_name ("OptionsLabel");
		l->set_alignment (1.0, 0.5);

		t->attach (*l, 0, 1, 0, 1, FILL | EXPAND, FILL);
		t->attach (_edit_modifier_combo, 1, 2, 0, 1, FILL | EXPAND, FILL);

		l = manage (new Label (_("+ button")));
		l->set_name ("OptionsLabel");

		t->attach (*l, 3, 4, 0, 1, FILL | EXPAND, FILL);
		t->attach (_edit_button_spin, 4, 5, 0, 1, FILL | EXPAND, FILL);

		_edit_button_spin.set_name ("OptionsEntry");
		_edit_button_adjustment.set_value (Keyboard::edit_button());
		_edit_button_adjustment.signal_value_changed().connect (mem_fun(*this, &KeyboardOptions::edit_button_changed));

		set_popdown_strings (_delete_modifier_combo, dumb);
		_delete_modifier_combo.signal_changed().connect (mem_fun(*this, &KeyboardOptions::delete_modifier_chosen));

		for (int x = 0; modifiers[x].name; ++x) {
			if (modifiers[x].modifier == Keyboard::delete_modifier ()) {
				_delete_modifier_combo.set_active_text (_(modifiers[x].name));
				break;
			}
		}

		l = manage (new Label (_("Delete using:")));
		l->set_name ("OptionsLabel");
		l->set_alignment (1.0, 0.5);

		t->attach (*l, 0, 1, 1, 2, FILL | EXPAND, FILL);
		t->attach (_delete_modifier_combo, 1, 2, 1, 2, FILL | EXPAND, FILL);

		l = manage (new Label (_("+ button")));
		l->set_name ("OptionsLabel");

		t->attach (*l, 3, 4, 1, 2, FILL | EXPAND, FILL);
		t->attach (_delete_button_spin, 4, 5, 1, 2, FILL | EXPAND, FILL);

		_delete_button_spin.set_name ("OptionsEntry");
		_delete_button_adjustment.set_value (Keyboard::delete_button());
		_delete_button_adjustment.signal_value_changed().connect (mem_fun(*this, &KeyboardOptions::delete_button_changed));

		set_popdown_strings (_snap_modifier_combo, dumb);
		_snap_modifier_combo.signal_changed().connect (mem_fun(*this, &KeyboardOptions::snap_modifier_chosen));

		for (int x = 0; modifiers[x].name; ++x) {
			if (modifiers[x].modifier == (guint) Keyboard::snap_modifier ()) {
				_snap_modifier_combo.set_active_text (_(modifiers[x].name));
				break;
			}
		}

		l = manage (new Label (_("Ignore snap using:")));
		l->set_name ("OptionsLabel");
		l->set_alignment (1.0, 0.5);

		t->attach (*l, 0, 1, 2, 3, FILL | EXPAND, FILL);
		t->attach (_snap_modifier_combo, 1, 2, 2, 3, FILL | EXPAND, FILL);

		vector<string> strs;

		for (map<string,string>::iterator bf = Keyboard::binding_files.begin(); bf != Keyboard::binding_files.end(); ++bf) {
			strs.push_back (bf->first);
		}

		set_popdown_strings (_keyboard_layout_selector, strs);
		_keyboard_layout_selector.set_active_text (Keyboard::current_binding_name());
		_keyboard_layout_selector.signal_changed().connect (mem_fun (*this, &KeyboardOptions::bindings_changed));

		l = manage (new Label (_("Keyboard layout:")));
		l->set_name ("OptionsLabel");
		l->set_alignment (1.0, 0.5);

		t->attach (*l, 0, 1, 3, 4, FILL | EXPAND, FILL);
		t->attach (_keyboard_layout_selector, 1, 2, 3, 4, FILL | EXPAND, FILL);

		_box->pack_start (*t, false, false);
	}

	void parameter_changed (string const & p)
	{
		/* XXX: these aren't really config options... */
	}

	void set_state_from_config ()
	{
		/* XXX: these aren't really config options... */
	}

private:

	void bindings_changed ()
	{
		string const txt = _keyboard_layout_selector.get_active_text();

		/* XXX: config...?  for all this keyboard stuff */
		
		for (map<string,string>::iterator i = Keyboard::binding_files.begin(); i != Keyboard::binding_files.end(); ++i) {
			if (txt == i->first) {
				if (Keyboard::load_keybindings (i->second)) {
					Keyboard::save_keybindings ();
				}
			}
		}
	}

	void edit_modifier_chosen ()
	{
		string const txt = _edit_modifier_combo.get_active_text();
		
		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == _(modifiers[i].name)) {
				Keyboard::set_edit_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void delete_modifier_chosen ()
	{
		string const txt = _delete_modifier_combo.get_active_text();
		
		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == _(modifiers[i].name)) {
				Keyboard::set_delete_modifier (modifiers[i].modifier);
				break;
			}
		}
	}
	
	void snap_modifier_chosen ()
	{
		string const txt = _snap_modifier_combo.get_active_text();
		
		for (int i = 0; modifiers[i].name; ++i) {
			if (txt == _(modifiers[i].name)) {
				Keyboard::set_snap_modifier (modifiers[i].modifier);
				break;
			}
		}
	}

	void delete_button_changed ()
	{
		Keyboard::set_delete_button (_delete_button_spin.get_value_as_int());
	}

	void edit_button_changed ()
	{
		Keyboard::set_edit_button (_edit_button_spin.get_value_as_int());
	}
	
	ComboBoxText _keyboard_layout_selector;
	ComboBoxText _edit_modifier_combo;
	ComboBoxText _delete_modifier_combo;
	ComboBoxText _snap_modifier_combo;
	Adjustment _delete_button_adjustment;
	SpinButton _delete_button_spin;
	Adjustment _edit_button_adjustment;
	SpinButton _edit_button_spin;
};

class FontScalingOptions : public OptionEditorBox
{
public:
	FontScalingOptions (RCConfiguration* c) :
		_rc_config (c),
		_dpi_adjustment (50, 50, 250, 1, 10),
		_dpi_slider (_dpi_adjustment)
	{
		_dpi_adjustment.set_value (_rc_config->get_font_scale ());

		Label* l = manage (new Label (_("Font scaling:")));
		l->set_name ("OptionsLabel");

		_dpi_slider.set_update_policy (UPDATE_DISCONTINUOUS);
		HBox* h = manage (new HBox);
		h->set_spacing (4);
		h->pack_start (*l, false, false);
		h->pack_start (_dpi_slider, true, true);

		_box->pack_start (*h, false, false);

		_dpi_adjustment.signal_value_changed().connect (mem_fun (*this, &FontScalingOptions::dpi_changed));
	}

	void parameter_changed (string const & p)
	{
		if (p == "font-scale") {
			_dpi_adjustment.set_value (_rc_config->get_font_scale() / 1024.);
		}
	}

	void set_state_from_config ()
	{
		parameter_changed ("font-scale");
	}
	
private:
	
	void dpi_changed ()
	{
		_rc_config->set_font_scale ((long) floor (_dpi_adjustment.get_value() * 1024));
		/* XXX: should be triggered from the parameter changed signal */
		reset_dpi ();
	}

	RCConfiguration* _rc_config;
	Adjustment _dpi_adjustment;
	HScale _dpi_slider;
};


class ControlSurfacesOptions : public OptionEditorBox
{
public:
	ControlSurfacesOptions ()
	{
		_store = ListStore::create (_model);
		_view.set_model (_store);
		_view.append_column (_("Name"), _model.name);
		_view.get_column(0)->set_resizable (true);
		_view.get_column(0)->set_expand (true);
		_view.append_column_editable (_("Enabled"), _model.enabled);
		_view.append_column_editable (_("Feedback"), _model.feedback);

		_box->pack_start (_view, false, false);

		_store->signal_row_changed().connect (mem_fun (*this, &ControlSurfacesOptions::model_changed));
	}

	void parameter_changed (std::string const &)
	{

	}

	void set_state_from_config ()
	{
		_store->clear ();
		
		ControlProtocolManager& m = ControlProtocolManager::instance ();
		for (list<ControlProtocolInfo*>::iterator i = m.control_protocol_info.begin(); i != m.control_protocol_info.end(); ++i) {

			if (!(*i)->mandatory) {
				TreeModel::Row r = *_store->append ();
				r[_model.name] = (*i)->name;
				r[_model.enabled] = ((*i)->protocol || (*i)->requested);
				r[_model.feedback] = ((*i)->protocol && (*i)->protocol->get_feedback ());
				r[_model.protocol_info] = *i;
			}
		}
	}

private:

	void model_changed (TreeModel::Path const & p, TreeModel::iterator const & i)
	{
		TreeModel::Row r = *i;

		ControlProtocolInfo* cpi = r[_model.protocol_info];
		if (!cpi) {
			return;
		}

		bool const was_enabled = (cpi->protocol != 0);
		bool const is_enabled = r[_model.enabled];

		if (was_enabled != is_enabled) {
			if (!was_enabled) {
				ControlProtocolManager::instance().instantiate (*cpi);
			} else {
				ControlProtocolManager::instance().teardown (*cpi);
			}
		}

		bool const was_feedback = (cpi->protocol && cpi->protocol->get_feedback ());
		bool const is_feedback = r[_model.feedback];

		if (was_feedback != is_feedback && cpi->protocol) {
			cpi->protocol->set_feedback (is_feedback);
		}
	}

        class ControlSurfacesModelColumns : public TreeModelColumnRecord
	{
	public:
		
		ControlSurfacesModelColumns ()
		{
			add (name);
			add (enabled);
			add (feedback);
			add (protocol_info);
		}

		TreeModelColumn<string> name;
		TreeModelColumn<bool> enabled;
		TreeModelColumn<bool> feedback;
		TreeModelColumn<ControlProtocolInfo*> protocol_info;
	};

	Glib::RefPtr<ListStore> _store;
	ControlSurfacesModelColumns _model;
	TreeView _view;
};


RCOptionEditor::RCOptionEditor ()
	: OptionEditor (Config, _("Ardour Preferences")),
	  _rc_config (Config)
{
	/* MISC */

	add_option (_("Misc"), new OptionEditorHeading (_("Metering")));
	
	ComboOption<float>* mht = new ComboOption<float> (
		"meter-hold",
		_("Meter hold time"),
		mem_fun (*_rc_config, &RCConfiguration::get_meter_hold),
		mem_fun (*_rc_config, &RCConfiguration::set_meter_hold)
		);

	mht->add (MeterHoldOff, _("off"));
	mht->add (MeterHoldShort, _("short"));
	mht->add (MeterHoldMedium, _("medium"));
	mht->add (MeterHoldLong, _("long"));
	
	add_option (_("Misc"), mht);

	ComboOption<float>* mfo = new ComboOption<float> (
		"meter-falloff",
		_("Meter fall-off"),
		mem_fun (*_rc_config, &RCConfiguration::get_meter_falloff),
		mem_fun (*_rc_config, &RCConfiguration::set_meter_falloff)
		);

	mfo->add (METER_FALLOFF_OFF, _("off"));
	mfo->add (METER_FALLOFF_SLOWEST, _("slowest"));
	mfo->add (METER_FALLOFF_SLOW, _("slow"));
	mfo->add (METER_FALLOFF_MEDIUM, _("medium"));
	mfo->add (METER_FALLOFF_FAST, _("fast"));
	mfo->add (METER_FALLOFF_FASTER, _("faster"));
	mfo->add (METER_FALLOFF_FASTEST, _("fastest"));

	add_option (_("Misc"), mfo);

	add_option (_("Misc"), new OptionEditorHeading (_("Undo")));
	
	add_option (_("Misc"), new UndoOptions (_rc_config));

	add_option (_("Misc"), new OptionEditorHeading (_("Misc")));
	
#ifndef GTKOSX
	/* font scaling does nothing with GDK/Quartz */
	add_option (_("Misc"), new FontScalingOptions (_rc_config));
#endif	

	add_option (_("Misc"),
	     new BoolOption (
		     "verify-remove-last-capture",
		     _("Verify removal of last capture"),
		     mem_fun (*_rc_config, &RCConfiguration::get_verify_remove_last_capture),
		     mem_fun (*_rc_config, &RCConfiguration::set_verify_remove_last_capture)
		     ));
	
	add_option (_("Misc"),
	     new BoolOption (
		     "periodic-safety-backups",
		     _("Make periodic backups of the session file"),
		     mem_fun (*_rc_config, &RCConfiguration::get_periodic_safety_backups),
		     mem_fun (*_rc_config, &RCConfiguration::set_periodic_safety_backups)
		     ));

	add_option (_("Misc"),
	     new BoolOption (
		     "sync-all-route-ordering",
		     _("Syncronise editor and mixer track order"),
		     mem_fun (*_rc_config, &RCConfiguration::get_sync_all_route_ordering),
		     mem_fun (*_rc_config, &RCConfiguration::set_sync_all_route_ordering)
		     ));

	add_option (_("Misc"),
	     new BoolOption (
		     "only-copy-imported-files",
		     _("Always copy imported files"),
		     mem_fun (*_rc_config, &RCConfiguration::get_only_copy_imported_files),
		     mem_fun (*_rc_config, &RCConfiguration::set_only_copy_imported_files)
		     ));

	add_option (_("Misc"),
	     new BoolOption (
		     "default-narrow_ms",
		     _("Use narrow mixer strips"),
		     mem_fun (*_rc_config, &RCConfiguration::get_default_narrow_ms),
		     mem_fun (*_rc_config, &RCConfiguration::set_default_narrow_ms)
		     ));

	add_option (_("Misc"),
	     new BoolOption (
		     "name-new-markers",
		     _("Name new markers"),
		     mem_fun (*_rc_config, &RCConfiguration::get_name_new_markers),
		     mem_fun (*_rc_config, &RCConfiguration::set_name_new_markers)
		     ));

	/* TRANSPORT */

	add_option (_("Transport"),
	     new BoolOption (
		     "latched-record-enable",
		     _("Keep record-enable engaged on stop"),
		     mem_fun (*_rc_config, &RCConfiguration::get_latched_record_enable),
		     mem_fun (*_rc_config, &RCConfiguration::set_latched_record_enable)
		     ));

	add_option (_("Transport"),
	     new BoolOption (
		     "stop-recording-on-xrun",
		     _("Stop recording when an xrun occurs"),
		     mem_fun (*_rc_config, &RCConfiguration::get_stop_recording_on_xrun),
		     mem_fun (*_rc_config, &RCConfiguration::set_stop_recording_on_xrun)
		     ));

	add_option (_("Transport"),
	     new BoolOption (
		     "create-xrun-marker",
		     _("Create markers where xruns occur"),
		     mem_fun (*_rc_config, &RCConfiguration::get_create_xrun_marker),
		     mem_fun (*_rc_config, &RCConfiguration::set_create_xrun_marker)
		     ));

	add_option (_("Transport"),
	     new BoolOption (
		     "stop-at-session-end",
		     _("Stop at the end of the session"),
		     mem_fun (*_rc_config, &RCConfiguration::get_stop_at_session_end),
		     mem_fun (*_rc_config, &RCConfiguration::set_stop_at_session_end)
		     ));

	add_option (_("Transport"),
	     new BoolOption (
		     "primary-clock-delta-edit-cursor",
		     _("Primary clock delta to edit cursor"),
		     mem_fun (*_rc_config, &RCConfiguration::get_primary_clock_delta_edit_cursor),
		     mem_fun (*_rc_config, &RCConfiguration::set_primary_clock_delta_edit_cursor)
		     ));

	add_option (_("Transport"),
	     new BoolOption (
		     "secondary-clock-delta-edit-cursor",
		     _("Secondary clock delta to edit cursor"),
		     mem_fun (*_rc_config, &RCConfiguration::get_secondary_clock_delta_edit_cursor),
		     mem_fun (*_rc_config, &RCConfiguration::set_secondary_clock_delta_edit_cursor)
		     ));

	add_option (_("Transport"),
	     new BoolOption (
		     "disable-disarm-during-roll",
		     _("Disable record disarm when transport is rolling"),
		     mem_fun (*_rc_config, &RCConfiguration::get_disable_disarm_during_roll),
		     mem_fun (*_rc_config, &RCConfiguration::set_disable_disarm_during_roll)
		     ));

	/* EDITOR */

	add_option (_("Editor"),
	     new BoolOption (
		     "link-region-and-track-selection",
		     _("Link selection of regions and tracks"),
		     mem_fun (*_rc_config, &RCConfiguration::get_link_region_and_track_selection),
		     mem_fun (*_rc_config, &RCConfiguration::set_link_region_and_track_selection)
		     ));

	add_option (_("Editor"),
	     new BoolOption (
		     "automation-follows-regions",
		     _("Move relevant automation when regions are moved"),
		     mem_fun (*_rc_config, &RCConfiguration::get_automation_follows_regions),
		     mem_fun (*_rc_config, &RCConfiguration::set_automation_follows_regions)
		     ));

	add_option (_("Editor"),
	     new BoolOption (
		     "show-track-meters",
		     _("Show meters on tracks in the editor"),
		     mem_fun (*_rc_config, &RCConfiguration::get_show_track_meters),
		     mem_fun (*_rc_config, &RCConfiguration::set_show_track_meters)
		     ));

	add_option (_("Editor"),
	     new BoolOption (
		     "use-overlap-equivalency",
		     _("Use overlap equivalency for regions"),
		     mem_fun (*_rc_config, &RCConfiguration::get_use_overlap_equivalency),
		     mem_fun (*_rc_config, &RCConfiguration::set_use_overlap_equivalency)
		     ));

	add_option (_("Editor"),
	     new BoolOption (
		     "rubberbanding-snaps-to-grid",
		     _("Make rubberband selection rectangle snap to the grid"),
		     mem_fun (*_rc_config, &RCConfiguration::get_rubberbanding_snaps_to_grid),
		     mem_fun (*_rc_config, &RCConfiguration::set_rubberbanding_snaps_to_grid)
		     ));

	add_option (_("Editor"),
	     new BoolOption (
		     "show-waveforms",
		     _("Show waveforms in regions"),
		     mem_fun (*_rc_config, &RCConfiguration::get_show_waveforms),
		     mem_fun (*_rc_config, &RCConfiguration::set_show_waveforms)
		     ));

	ComboOption<WaveformScale>* wfs = new ComboOption<WaveformScale> (
		"waveform-scale",
		_("Waveform scale"),
		mem_fun (*_rc_config, &RCConfiguration::get_waveform_scale),
		mem_fun (*_rc_config, &RCConfiguration::set_waveform_scale)
		);

	wfs->add (Linear, _("linear"));
	wfs->add (Logarithmic, _("logarithmic"));

	add_option (_("Editor"), wfs);
	
	ComboOption<WaveformShape>* wfsh = new ComboOption<WaveformShape> (
		"waveform-shape",
		_("Waveform shape"),
		mem_fun (*_rc_config, &RCConfiguration::get_waveform_shape),
		mem_fun (*_rc_config, &RCConfiguration::set_waveform_shape)
		);

	wfsh->add (Traditional, _("traditional"));
	wfsh->add (Rectified, _("rectified"));

	add_option (_("Editor"), wfsh);

	/* AUDIO */

	add_option (_("Audio"), new OptionEditorHeading (_("Solo")));

	ComboOption<SoloModel>* sm = new ComboOption<SoloModel> (
		"solo-model",
		_("Solo"),
		mem_fun (*_rc_config, &RCConfiguration::get_solo_model),
		mem_fun (*_rc_config, &RCConfiguration::set_solo_model)
		);

	sm->add (InverseMute, _("in place"));
	sm->add (SoloBus, _("via bus"));

	add_option (_("Audio"), sm);

	add_option (_("Audio"),
	     new BoolOption (
		     "solo-latched",
		     _("Latched solo"),
		     mem_fun (*_rc_config, &RCConfiguration::get_solo_latched),
		     mem_fun (*_rc_config, &RCConfiguration::set_solo_latched)
		     ));

	add_option (_("Audio"),
	     new BoolOption (
		     "show-solo-mutes",
		     _("Show solo muting"),
		     mem_fun (*_rc_config, &RCConfiguration::get_show_solo_mutes),
		     mem_fun (*_rc_config, &RCConfiguration::set_show_solo_mutes)
		     ));

	add_option (_("Audio"),
	     new BoolOption (
		     "solo-mute-override",
		     _("Override muting"),
		     mem_fun (*_rc_config, &RCConfiguration::get_solo_mute_override),
		     mem_fun (*_rc_config, &RCConfiguration::set_solo_mute_override)
		     ));

	add_option (_("Audio"), new OptionEditorHeading (_("Monitoring")));

	ComboOption<MonitorModel>* mm = new ComboOption<MonitorModel> (
		"monitoring-model",
		_("Monitoring handled by"),
		mem_fun (*_rc_config, &RCConfiguration::get_monitoring_model),
		mem_fun (*_rc_config, &RCConfiguration::set_monitoring_model)
		);

	mm->add (HardwareMonitoring, _("JACK"));
	mm->add (SoftwareMonitoring, _("ardour"));
	mm->add (ExternalMonitoring, _("audio hardware"));

	add_option (_("Audio"), mm);

	add_option (_("Audio"),
	     new BoolOption (
		     "tape-machine-mode",
		     _("Tape machine mode"),
		     mem_fun (*_rc_config, &RCConfiguration::get_tape_machine_mode),
		     mem_fun (*_rc_config, &RCConfiguration::set_tape_machine_mode)
		     ));

	add_option (_("Audio"), new OptionEditorHeading (_("Connection of tracks and busses")));

	ComboOption<AutoConnectOption>* iac = new ComboOption<AutoConnectOption> (
		"input-auto-connect",
		_("Connect track and bus inputs"),
		mem_fun (*_rc_config, &RCConfiguration::get_input_auto_connect),
		mem_fun (*_rc_config, &RCConfiguration::set_input_auto_connect)
		);

	iac->add (AutoConnectPhysical, _("automatically to physical inputs"));
	iac->add (ManualConnect, _("manually"));

	add_option (_("Audio"), iac);

	ComboOption<AutoConnectOption>* oac = new ComboOption<AutoConnectOption> (
		"output-auto-connect",
		_("Connect track and bus outputs"),
		mem_fun (*_rc_config, &RCConfiguration::get_output_auto_connect),
		mem_fun (*_rc_config, &RCConfiguration::set_output_auto_connect)
		);

	oac->add (AutoConnectPhysical, _("automatically to physical outputs"));
	oac->add (AutoConnectMaster, _("automatically to master outputs"));
	oac->add (ManualConnect, _("manually"));

	add_option (_("Audio"), oac);

	add_option (_("Audio"), new OptionEditorHeading (_("Denormals")));

	add_option (_("Audio"),
	     new BoolOption (
		     "denormal-protection",
		     _("Use DC bias to protect against denormals"),
		     mem_fun (*_rc_config, &RCConfiguration::get_denormal_protection),
		     mem_fun (*_rc_config, &RCConfiguration::set_denormal_protection)
		     ));

	ComboOption<DenormalModel>* dm = new ComboOption<DenormalModel> (
		"denormal-model",
		_("Processor handling"),
		mem_fun (*_rc_config, &RCConfiguration::get_denormal_model),
		mem_fun (*_rc_config, &RCConfiguration::set_denormal_model)
		);
	
	dm->add (DenormalNone, _("no processor handling"));
	
	FPU fpu;
	
	if (fpu.has_flush_to_zero()) {
		dm->add (DenormalFTZ, _("use FlushToZero"));
	}
	
	if (fpu.has_denormals_are_zero()) {
		dm->add (DenormalDAZ, _("use DenormalsAreZero"));
	}
	
	if (fpu.has_flush_to_zero() && fpu.has_denormals_are_zero()) {
		dm->add (DenormalFTZDAZ, _("use FlushToZero and DenormalsAreZerO"));
	}
	
	add_option (_("Audio"), dm);

	add_option (_("Audio"), new OptionEditorHeading (_("Plugins")));

	add_option (_("Audio"),
	     new BoolOption (
		     "plugins-stop-with-transport",
		     _("Stop plugins when the transport is stopped"),
		     mem_fun (*_rc_config, &RCConfiguration::get_plugins_stop_with_transport),
		     mem_fun (*_rc_config, &RCConfiguration::set_plugins_stop_with_transport)
		     ));

	add_option (_("Audio"),
	     new BoolOption (
		     "do-not-record-plugins",
		     _("Disable plugins during recording"),
		     mem_fun (*_rc_config, &RCConfiguration::get_do_not_record_plugins),
		     mem_fun (*_rc_config, &RCConfiguration::set_do_not_record_plugins)
		     ));

	add_option (_("Audio"),
	     new BoolOption (
		     "new-plugins-active",
		     _("Make new plugins active"),
		     mem_fun (*_rc_config, &RCConfiguration::get_new_plugins_active),
		     mem_fun (*_rc_config, &RCConfiguration::set_new_plugins_active)
		     ));

	add_option (_("Audio"),
	     new BoolOption (
		     "auto-analyse-audio",
		     _("Enable automatic analysis of audio"),
		     mem_fun (*_rc_config, &RCConfiguration::get_auto_analyse_audio),
		     mem_fun (*_rc_config, &RCConfiguration::set_auto_analyse_audio)
		     ));

	/* MIDI CONTROL */

	add_option (_("MIDI control"), new MIDIPorts (_rc_config));

	add_option (_("MIDI control"),
	     new SpinOption<uint8_t> (
		     "mmc-receive-device-id",
		     _("Inbound MMC device ID"),
		     mem_fun (*_rc_config, &RCConfiguration::get_mmc_receive_device_id),
		     mem_fun (*_rc_config, &RCConfiguration::set_mmc_receive_device_id),
		     0, 128, 1, 10
		     ));

	add_option (_("MIDI control"),
	     new SpinOption<uint8_t> (
		     "mmc-send-device-id",
		     _("Outbound MMC device ID"),
		     mem_fun (*_rc_config, &RCConfiguration::get_mmc_send_device_id),
		     mem_fun (*_rc_config, &RCConfiguration::set_mmc_send_device_id),
		     0, 128, 1, 10
		     ));

	add_option (_("MIDI control"),
	     new SpinOption<int32_t> (
		     "initial-program-change",
		     _("Initial program change"),
		     mem_fun (*_rc_config, &RCConfiguration::get_initial_program_change),
		     mem_fun (*_rc_config, &RCConfiguration::set_initial_program_change),
		     -1, 65536, 1, 10
		     ));

	/* CONTROL SURFACES */

	add_option (_("Control surfaces"), new ControlSurfacesOptions);

	ComboOption<RemoteModel>* rm = new ComboOption<RemoteModel> (
		"remote-model",
		_("Control surface remote ID"),
		mem_fun (*_rc_config, &RCConfiguration::get_remote_model),
		mem_fun (*_rc_config, &RCConfiguration::set_remote_model)
		);

	rm->add (UserOrdered, _("assigned by user"));
	rm->add (MixerOrdered, _("follows order of mixer"));
	rm->add (EditorOrdered, _("follows order of editor"));

	add_option (_("Control surfaces"), rm);

	/* CLICK */

	add_option (_("Click"), new ClickOptions (_rc_config, this));

	/* KEYBOARD */

	add_option (_("Keyboard"), new KeyboardOptions);
}


