/*
 * Copyright (C) 2005-2009 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2008-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __gtk_ardour_option_editor_h__
#define __gtk_ardour_option_editor_h__

#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/filechooserbutton.h>
#include <gtkmm/label.h>
#include <gtkmm/notebook.h>
#include <gtkmm/scale.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/treestore.h>
#include <gtkmm/treeview.h>
#include <gtkmm/window.h>

#include "widgets/slider_controller.h"

#include "actions.h"
#include "ardour_window.h"
#include "audio_clock.h"
#include "ardour/types.h"

/** @file option_editor.h
 *  @brief Base class for option editing dialog boxes.
 *
 *  Code to provided the basis for dialogs which allow the user to edit options
 *  from an ARDOUR::Configuration class.
 *
 *  The idea is that we have an OptionEditor class which is the dialog box.
 *  This is essentially a GTK Notebook.  OptionEditorComponent objects can
 *  then be added to the OptionEditor, and these components are arranged on
 *  the pages of the Notebook.  There is also an OptionEditorComponent hierarchy
 *  here, providing things like boolean and combobox option components.
 *
 *  It is intended that OptionEditor be subclassed to implement a particular
 *  options dialog.
 */

namespace PBD {
	class Configuration;
}

class OptionEditorPage;

/** Base class for components of an OptionEditor dialog */
class OptionEditorComponent
{
public:
	virtual ~OptionEditorComponent() {}

	/** Called when a configuration parameter's value has changed.
	 *  @param p parameter name
	 */
	virtual void parameter_changed (std::string const & p) = 0;

	/** Called to instruct the object to set its UI state from the configuration */
	virtual void set_state_from_config () = 0;

	/** Called to instruct the object to add itself to an OptionEditorPage */
	virtual void add_to_page (OptionEditorPage *) = 0;

	void add_widget_to_page (OptionEditorPage*, Gtk::Widget*);
	void add_widgets_to_page (OptionEditorPage*, Gtk::Widget*, Gtk::Widget*, bool expand = true);

	void set_note (std::string const &);

	virtual Gtk::Widget& tip_widget() = 0;

protected:
	void maybe_add_note (OptionEditorPage *, int);

	std::string _note;
};

/** A component which provides a subheading within the dialog */
class OptionEditorHeading : public OptionEditorComponent
{
public:
	OptionEditorHeading (std::string const &);

	void parameter_changed (std::string const &) {}
	void set_state_from_config () {}
	void add_to_page (OptionEditorPage *);

	Gtk::Widget& tip_widget() { return *_label; }

private:
	Gtk::Label* _label; ///< the label used for the heading
};

/** Expanding layout helper to push elements to the left on a single column page  */
class OptionEditorBlank : public OptionEditorComponent
{
public:
	OptionEditorBlank () {}

	void parameter_changed (std::string const &) {}
	void set_state_from_config () {}
	void add_to_page (OptionEditorPage *);

	Gtk::Widget& tip_widget() { return _dummy; }

private:
	Gtk::EventBox _dummy;
};

class RcConfigDisplay : public OptionEditorComponent
{
public:
	RcConfigDisplay (std::string const &, std::string const &, sigc::slot<std::string>, char s = '\0');
	void add_to_page (OptionEditorPage *);
	void parameter_changed (std::string const & p);
	void set_state_from_config ();
	Gtk::Widget& tip_widget() { return *_info; }
protected:
	sigc::slot<std::string> _get;
	Gtk::Label* _label;
	Gtk::Label* _info;
	std::string _id;
	char _sep;
};

class RcActionButton : public OptionEditorComponent
{
public:
	RcActionButton (std::string const & t, const Glib::SignalProxy0< void >::SlotType & slot, std::string const & l = "");
	void add_to_page (OptionEditorPage *);

	void parameter_changed (std::string const & p) {}
	void set_state_from_config () {}
	Gtk::Widget& tip_widget() { return *_button; }

protected:
	Gtk::Button* _button;
	Gtk::Label* _label;
	std::string _name;
};

/** Base class for components which provide UI to change an option */
class Option : public OptionEditorComponent
{
public:
	/** Construct an Option.
	 *  @param i Option id (e.g. "plugins-stop-with-transport")
	 *  @param n User-visible name (e.g. "Stop plugins when the transport is stopped")
	 */
	Option (std::string const & i,
		std::string const & n
		)
		: _id (i),
		  _name (n)
	{}

	void parameter_changed (std::string const & p)
	{
		if (p == _id) {
			set_state_from_config ();
		}
	}

	virtual void set_state_from_config () = 0;
	virtual void add_to_page (OptionEditorPage*) = 0;

	std::string id () const {
		return _id;
	}

protected:
	std::string _id;
	std::string _name;
};

/** Just a Gtk Checkbutton, masquerading as an option component */
class CheckOption : public OptionEditorComponent , public Gtkmm2ext::Activatable, public sigc::trackable
{
public:
	CheckOption (std::string const &, std::string const &, Glib::RefPtr<Gtk::Action> act );
	void set_state_from_config () {}
	void parameter_changed (std::string const &) {}
	void add_to_page (OptionEditorPage*);

	void set_sensitive (bool yn) {
		_button->set_sensitive (yn);
	}

	Gtk::Widget& tip_widget() { return *_button; }

protected:
	void action_toggled ();
	void action_sensitivity_changed () {}
	void action_visibility_changed () {}

	virtual void toggled ();

	Gtk::CheckButton*      _button; ///< UI button
	Gtk::Label*            _label; ///< label for button, so we can use markup
};

/** Component which provides the UI to handle a boolean option using a GTK CheckButton */
class BoolOption : public Option
{
public:
	BoolOption (std::string const &, std::string const &, sigc::slot<bool>, sigc::slot<bool, bool>);
	void set_state_from_config ();
	void add_to_page (OptionEditorPage*);

	void set_sensitive (bool yn) {
		_button->set_sensitive (yn);
	}

	Gtk::Widget& tip_widget() { return *_button; }

protected:
	virtual void toggled ();

	sigc::slot<bool>       _get; ///< slot to get the configuration variable's value
	sigc::slot<bool, bool> _set;  ///< slot to set the configuration variable's value
	Gtk::CheckButton*      _button; ///< UI button
	Gtk::Label*            _label; ///< label for button, so we can use markup
};

class RouteDisplayBoolOption : public BoolOption
{
public:
	RouteDisplayBoolOption (std::string const &, std::string const &, sigc::slot<bool>, sigc::slot<bool, bool>);

protected:
	virtual void toggled ();
};

/** Component which allows to add any GTK Widget - intended for single buttons and custom stateless objects */
class FooOption : public OptionEditorComponent
{
public:
	FooOption (Gtk::Widget *w) : _w (w) {}

	void add_to_page (OptionEditorPage* p) {
		add_widget_to_page (p, _w);
	}

	Gtk::Widget& tip_widget() { return *_w; }
	void set_state_from_config () {}
	void parameter_changed (std::string const &) {}
private:
	Gtk::Widget *_w;
};

/** Component which provides the UI to handle a string option using a GTK Entry */
class EntryOption : public Option
{
public:
	EntryOption (std::string const &, std::string const &, sigc::slot<std::string>, sigc::slot<bool, std::string>);
	void set_state_from_config ();
	void add_to_page (OptionEditorPage*);
	void set_sensitive (bool);
	void set_invalid_chars (std::string i) { _invalid = i; }

	Gtk::Widget& tip_widget() { return *_entry; }

private:
	void activated ();
	bool focus_out (GdkEventFocus*);
	void filter_text (const Glib::ustring&, int*);

	sigc::slot<std::string> _get; ///< slot to get the configuration variable's value
	sigc::slot<bool, std::string> _set;  ///< slot to set the configuration variable's value
	Gtk::Label* _label; ///< UI label
	Gtk::Entry* _entry; ///< UI entry
	std::string _invalid;
};


/** Component which provides the UI to handle an enumerated option using a GTK ComboBox.
 *  The template parameter is the enumeration.
 */
template <class T>
class ComboOption : public Option
{
public:
	/** Construct an ComboOption.
	 *  @param i id
	 *  @param n User-visible name.
	 *  @param g Slot to get the variable's value.
	 *  @param s Slot to set the variable's value.
	 */
	ComboOption (
		std::string const & i,
		std::string const & n,
		sigc::slot<T> g,
		sigc::slot<bool, T> s
		)
		: Option (i, n)
		, _get (g)
		, _set (s)
	{
		_label = Gtk::manage (new Gtk::Label (n + ":"));
		_label->set_alignment (0, 0.5);
		_combo = Gtk::manage (new Gtk::ComboBoxText);
		_combo->signal_changed().connect (sigc::mem_fun (*this, &ComboOption::changed));
	}

	void set_state_from_config ()
	{
		uint32_t r = 0;
		while (r < _options.size() && _get () != _options[r]) {
			++r;
		}

		if (r < _options.size()) {
			_combo->set_active (r);
		}
	}

	void add_to_page (OptionEditorPage* p)
	{
		add_widgets_to_page (p, _label, _combo);
	}

	/** Add an allowed value for this option.
	 *  @param e Enumeration.
	 *  @param o User-visible name for this value.
	 */
	void add (T e, std::string const & o)
	{
		_options.push_back (e);
		_combo->append_text (o);
		/* Remove excess space.
		 * gtk_combo_box_size_requet() does the following:
		 * {
		 *   gtk_widget_size_request (GTK_BIN (widget)->child, &bin_req);
		 *   gtk_combo_box_remeasure (combo_box);
		 *   bin_req.width = MAX (bin_req.width, priv->width);
		 * }
		 *
		 * - gtk_combo_box_remeasure() measures the extents of all children
		 *   correctly using gtk_cell_view_get_size_of_row() and sets priv->width.
		 * - The direct child (current active item as entry) is however too large.
		 *   Likely because Ardour's clearlooks.rc.in does not correctly set this up).
		 */
		_combo->get_child()->set_size_request (20, -1);
	}

	void clear ()
	{
		_combo->clear_items();
		_options.clear ();
	}

	void changed ()
	{
		uint32_t const r = _combo->get_active_row_number ();
		if (r < _options.size()) {
			_set (_options[r]);
		}
	}
	void set_sensitive (bool yn)
	{
		_combo->set_sensitive (yn);
	}

	Gtk::Widget& tip_widget() { return *_combo; }

private:
	sigc::slot<T> _get;
	sigc::slot<bool, T> _set;
	Gtk::Label* _label;
	Gtk::ComboBoxText* _combo;
	std::vector<T> _options;
};


/** Component which provides the UI for a GTK HScale.
 */
class HSliderOption : public Option
{
public:
	HSliderOption (
			std::string const& i,
			std::string const& n,
			sigc::slot<float> g,
			sigc::slot<bool, float> s,
			double lower, double upper,
			double step_increment = 1,
			double page_increment = 10,
			double mult = 1.0,
			bool logarithmic = false
		);

	void set_state_from_config ();
	virtual void changed ();
	void add_to_page (OptionEditorPage* p);
	void set_sensitive (bool yn);

	Gtk::Widget& tip_widget() { return _hscale; }
	Gtk::HScale& scale() { return _hscale; }

protected:
	sigc::slot<float> _get;
	sigc::slot<bool, float> _set;
	Gtk::Adjustment _adj;
	Gtk::HScale _hscale;
	Gtk::Label _label;
	double _mult;
	bool _log;
};


/** Component which provides the UI to handle an enumerated option using a GTK ComboBox.
 *  The template parameter is the enumeration.
 */
class ComboStringOption : public Option
{
public:
	/** Construct an ComboOption.
	 *  @param i id
	 *  @param n User-visible name.
	 *  @param g Slot to get the variable's value.
	 *  @param s Slot to set the variable's value.
	 */
	ComboStringOption (
		std::string const & i,
		std::string const & n,
		sigc::slot<std::string> g,
		sigc::slot<bool, std::string> s
		);

	void set_state_from_config ();
	void add_to_page (OptionEditorPage* p);

	/** Set the allowed strings for this option
	 *  @param strings a vector of allowed strings
	 */
	void set_popdown_strings (const std::vector<std::string>& strings);

	void clear ();
	void changed ();
	void set_sensitive (bool yn);

	Gtk::Widget& tip_widget() { return *_combo; }

private:
	sigc::slot<std::string> _get;
	sigc::slot<bool, std::string> _set;
	Gtk::Label* _label;
	Gtk::ComboBoxText* _combo;
};


/** Component which provides the UI to handle a boolean option which needs
 *  to be represented as a ComboBox to be clear to the user.
 */
class BoolComboOption : public Option
{
public:
	BoolComboOption (
		std::string const &,
		std::string const &,
		std::string const &,
		std::string const &,
		sigc::slot<bool>,
		sigc::slot<bool, bool>
		);

	void set_state_from_config ();
	void add_to_page (OptionEditorPage *);
	void changed ();
	void set_sensitive (bool);

	Gtk::Widget& tip_widget() { return *_combo; }

private:
	sigc::slot<bool> _get;
	sigc::slot<bool, bool> _set;
	Gtk::Label* _label;
	Gtk::ComboBoxText* _combo;
};


/** Component which provides the UI to handle an numeric option using a GTK SpinButton */
template <class T>
class SpinOption : public Option
{
public:
	/** Construct an SpinOption.
	 *  @param i id
	 *  @param n User-visible name.
	 *  @param g Slot to get the variable's value.
	 *  @param s Slot to set the variable's value.
	 *  @param min Variable minimum value.
	 *  @param max Variable maximum value.
	 *  @param step Step for the spin button.
	 *  @param page Page step for the spin button.
	 *  @param unit Unit name.
	 *  @param scale Scaling factor (such that for a value x in the spinbutton, x * scale is written to the config)
	 *  @param digits Number of decimal digits to show.
	 */
	SpinOption (
		std::string const & i,
		std::string const & n,
		sigc::slot<T> g,
		sigc::slot<bool, T> s,
		T min,
		T max,
		T step,
		T page,
		std::string const & unit = "",
		float scale = 1,
		unsigned digits = 0
		)
		: Option (i, n)
		, _get (g)
		, _set (s)
		, _scale (scale)
	{
		_label = Gtk::manage (new Gtk::Label (n + ":"));
		_label->set_alignment (0, 0.5);

		_spin = Gtk::manage (new Gtk::SpinButton);
		_spin->set_range (min, max);
		_spin->set_increments (step, page);
		_spin->set_digits(digits);

		_box = Gtk::manage (new Gtk::HBox);
		_box->pack_start (*_spin, true, true);
		_box->set_spacing (4);
		if (unit.length()) {
			_box->pack_start (*Gtk::manage (new Gtk::Label (unit)), false, false);
		}

		_spin->signal_value_changed().connect (sigc::mem_fun (*this, &SpinOption::changed));
	}

	void set_state_from_config ()
	{
		_spin->set_value (_get () / _scale);
	}

	void add_to_page (OptionEditorPage* p)
	{
		add_widgets_to_page (p, _label, _box, false);
	}

	void changed ()
	{
		_set (static_cast<T> (_spin->get_value ()) * _scale);
	}

	Gtk::Widget& tip_widget() { return *_spin; }

private:
	sigc::slot<T> _get;
	sigc::slot<bool, T> _set;
	float _scale;
	Gtk::Label* _label;
	Gtk::HBox* _box;
	Gtk::SpinButton* _spin;
};

class FaderOption : public Option
{
public:

	FaderOption (std::string const &, std::string const &, sigc::slot<ARDOUR::gain_t> g, sigc::slot<bool, ARDOUR::gain_t> s);
	void set_state_from_config ();
	void add_to_page (OptionEditorPage *);

	Gtk::Widget& tip_widget() { return *_db_slider; }

private:
	void db_changed ();
	void on_activate ();
	bool on_key_press (GdkEventKey* ev);

	Gtk::Adjustment _db_adjustment;
	ArdourWidgets::HSliderController* _db_slider;
	Gtk::Entry _db_display;
	Gtk::Label _label;
	Gtk::HBox _box;
	Gtk::VBox _fader_centering_box;
	sigc::slot<ARDOUR::gain_t> _get;
	sigc::slot<bool, ARDOUR::gain_t> _set;
};

class WidgetOption : public Option
{
  public:
	WidgetOption (std::string const & i, std::string const & n, Gtk::Widget& w);

	void add_to_page (OptionEditorPage*);
	void parameter_changed (std::string const &) {}
	void set_state_from_config () {}

	Gtk::Widget& tip_widget() { return *_widget; }

  private:
	Gtk::Widget* _widget;
};

class ClockOption : public Option
{
public:
	ClockOption (std::string const &, std::string const &, sigc::slot<std::string>, sigc::slot<bool, std::string>);
	void set_state_from_config ();
	void add_to_page (OptionEditorPage *);
	void set_session (ARDOUR::Session *);

	Gtk::Widget& tip_widget() { return _clock; }
	AudioClock& clock() { return _clock; }

private:
	void save_clock_time ();
	Gtk::Label _label;
	AudioClock _clock;
	sigc::slot<std::string> _get;
	sigc::slot<bool, std::string> _set;
	ARDOUR::Session *_session;
};

class DirectoryOption : public Option
{
public:
	DirectoryOption (std::string const &, std::string const &, sigc::slot<std::string>, sigc::slot<bool, std::string>);

	void set_state_from_config ();
	void add_to_page (OptionEditorPage *);

	Gtk::Widget& tip_widget() { return _file_chooser; }

private:
	void selection_changed ();

	sigc::slot<std::string> _get; ///< slot to get the configuration variable's value
	sigc::slot<bool, std::string> _set;  ///< slot to set the configuration variable's value
	Gtk::FileChooserButton _file_chooser;
	sigc::connection _changed_connection;
};

/** Class to represent a single page in an OptionEditor's notebook.
 *  Pages are laid out using a 3-column table; the 1st column is used
 *  to indent non-headings, and the 2nd and 3rd for actual content.
 */
class OptionEditorPage
{
public:
	OptionEditorPage (Gtk::Notebook&, std::string const &);
	OptionEditorPage ();

	Gtk::VBox box;
	Gtk::Table table;
	std::list<OptionEditorComponent*> components;

private:
	void init ();
};

class OptionEditorMiniPage : public OptionEditorComponent, public OptionEditorPage
{
public:
	OptionEditorMiniPage ()
	{
		box.pack_start (table, false, false);
		box.set_border_width (0);
	}

	void parameter_changed (std::string const &) = 0;
	void set_state_from_config () = 0;
	virtual void add_to_page (OptionEditorPage*);

	Gtk::Widget& tip_widget() { return *table.children().front().get_widget(); }
};

/** The OptionEditor dialog base class */
class OptionEditor : virtual public sigc::trackable
{
public:
	OptionEditor (PBD::Configuration *);
	virtual ~OptionEditor ();

	void add_option (std::string const &, OptionEditorComponent *);
	void add_page (std::string const &, Gtk::Widget& page_widget);

	void set_current_page (std::string const &);

protected:
	virtual void parameter_changed (std::string const &);

	PBD::Configuration* _config;
	Gtk::Notebook& notebook() { return _notebook; }
	Gtk::TreeView& treeview() { return option_treeview; }

	class OptionColumns : public Gtk::TreeModel::ColumnRecord
	{
		public:
			Gtk::TreeModelColumn<std::string> name;
			Gtk::TreeModelColumn<Gtk::Widget*> widget;

			OptionColumns() {
				add (name);
				add (widget);
			}
	};

	OptionColumns option_columns;
	Glib::RefPtr<Gtk::TreeStore> option_tree;

private:
	PBD::ScopedConnection config_connection;
	Gtk::Notebook _notebook;
	Gtk::TreeView option_treeview;
	std::map<std::string, OptionEditorPage*> _pages;

	void add_path_to_treeview (std::string const &, Gtk::Widget&);
	Gtk::TreeModel::iterator find_path_in_treemodel (std::string const & pn,
	                                                 bool create_missing = false);
	void treeview_row_selected ();
};

/** The OptionEditor dialog-as-container base class */
class OptionEditorContainer : public OptionEditor, public Gtk::VBox
{
public:
	OptionEditorContainer (PBD::Configuration *);
	~OptionEditorContainer() {}
private:
	Gtk::HBox hpacker;
};

/** The OptionEditor dialog-as-container base class */
class OptionEditorWindow : public OptionEditor, public ArdourWindow
{
public:
	OptionEditorWindow (PBD::Configuration *, std::string const &);
	~OptionEditorWindow() {}
private:
	Gtk::VBox container;
	Gtk::HBox hpacker;
};

#endif /* __gtk_ardour_option_editor_h__ */
