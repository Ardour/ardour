/*
 * Copyright (C) 2005-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_gtk_automation_time_axis_h__
#define __ardour_gtk_automation_time_axis_h__

#include <list>
#include <string>
#include <utility>

#include <boost/shared_ptr.hpp>

#include "ardour/types.h"
#include "ardour/automatable.h"
#include "ardour/automation_list.h"

#include "canvas/rectangle.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"

#include "time_axis_view.h"
#include "automation_controller.h"

namespace ARDOUR {
	class Session;
	class Stripable;
	class AutomationControl;
}

class PublicEditor;
class TimeSelection;
class RegionSelection;
class PointSelection;
class AutomationLine;
class Selection;
class Selectable;
class AutomationStreamView;
class AutomationController;
class ItemCounts;

class AutomationTimeAxisView : public TimeAxisView
{
public:
	AutomationTimeAxisView (ARDOUR::Session*,
	                        boost::shared_ptr<ARDOUR::Stripable>,
	                        boost::shared_ptr<ARDOUR::Automatable>,
	                        boost::shared_ptr<ARDOUR::AutomationControl>,
	                        Evoral::Parameter,
	                        PublicEditor&,
	                        TimeAxisView& parent,
	                        bool show_regions,
	                        ArdourCanvas::Canvas& canvas,
	                        const std::string & name = "", /* translatable */
	                        const std::string & plug_name = "");

	~AutomationTimeAxisView();

	virtual void set_height (uint32_t, TrackHeightMode m = OnlySelf);
	void set_samples_per_pixel (double);
	std::string name() const { return _name; }
	Gdk::Color color () const;
	void update_name_from_param ();

	boost::shared_ptr<ARDOUR::Stripable> stripable() const;
	ARDOUR::PresentationInfo const & presentation_info () const;

	void add_automation_event (GdkEvent *, Temporal::timepos_t const &, double, bool with_guard_points);

	void clear_lines ();

	/** @return Our AutomationLine, if this view has one, or 0 if it uses AutomationRegionViews */
	boost::shared_ptr<AutomationLine> line() { return _line; }

	/** @return All AutomationLines associated with this view */
	std::list<boost::shared_ptr<AutomationLine> > lines () const;

	void set_selected_points (PointSelection&);
	void get_selectables (Temporal::timepos_t const &, Temporal::timepos_t const &, double top, double bot, std::list<Selectable *>&, bool within = false);
	void get_inverted_selectables (Selection&, std::list<Selectable*>& results);

	void show_timestretch (Temporal::timepos_t const &/*start*/, Temporal::timepos_t const & /*end*/, int /*layers*/, int /*layer*/) {}
	void hide_timestretch () {}

	/* editing operations */

	void cut_copy_clear (Selection&, Editing::CutCopyOp);
	bool paste (Temporal::timepos_t const &, const Selection&, PasteContext&);

	int  set_state (const XMLNode&, int version);

	std::string state_id() const;
	static bool parse_state_id (std::string const &, PBD::ID &, bool &, Evoral::Parameter &);

	boost::shared_ptr<ARDOUR::AutomationControl> control() const   { return _control; }
	boost::shared_ptr<AutomationController>      controller() const { return _controller; }
	Evoral::Parameter parameter () const {
		return _parameter;
	}

	ArdourCanvas::Item* base_item () const {
		return _base_rect;
	}

	bool has_automation () const;

	boost::shared_ptr<ARDOUR::Stripable> parent_stripable () {
		return _stripable;
	}

	bool show_regions () const {
		return _show_regions;
	}

protected:
	/* Note that for MIDI controller "automation" (in regions), all of these
	 * may be set.  In this case, _automatable is likely _route so the
	 * controller will send immediate events out the route's MIDI port. */

	/** parent strip */
	boost::shared_ptr<ARDOUR::Stripable> _stripable;
	/** control */
	boost::shared_ptr<ARDOUR::AutomationControl> _control;
	/** control owner; may be _stripable, something else (e.g. a pan control), or NULL */
	boost::shared_ptr<ARDOUR::Automatable> _automatable;
	/** controller owner */
	boost::shared_ptr<AutomationController> _controller;
	Evoral::Parameter _parameter;

	ArdourCanvas::Rectangle* _base_rect;
	boost::shared_ptr<AutomationLine> _line;

	std::string _name;

	/** AutomationStreamView if we are editing region-based automation (for MIDI), otherwise 0 */
	AutomationStreamView* _view;

	bool    first_call_to_set_height;

	ArdourWidgets::ArdourButton   hide_button;
	ArdourWidgets::ArdourDropdown auto_dropdown;
	Gtk::Label*        plugname;
	bool               plugname_packed;

	Gtk::CheckMenuItem*     auto_off_item;
	Gtk::CheckMenuItem*     auto_play_item;
	Gtk::CheckMenuItem*     auto_touch_item;
	Gtk::CheckMenuItem*     auto_write_item;
	Gtk::CheckMenuItem*     auto_latch_item;

	Gtk::CheckMenuItem* mode_discrete_item;
	Gtk::CheckMenuItem* mode_line_item;
	Gtk::CheckMenuItem* mode_log_item;
	Gtk::CheckMenuItem* mode_exp_item;

	bool _show_regions;

	void add_line (boost::shared_ptr<AutomationLine>);

	void clear_clicked ();
	void hide_clicked ();

	virtual bool can_edit_name() const {return false;}

	void build_display_menu ();

	void cut_copy_clear_one (AutomationLine&, Selection&, Editing::CutCopyOp);
	bool paste_one (Temporal::timepos_t const &, unsigned, float times, const Selection&, ItemCounts& counts, bool greedy=false);
	void route_going_away ();

	void set_automation_state (ARDOUR::AutoState);
	bool ignore_state_request;
	bool ignore_mode_request;

	bool propagate_time_selection () const;

	void automation_state_changed ();

	void set_interpolation (ARDOUR::AutomationList::InterpolationStyle);
	void interpolation_changed (ARDOUR::AutomationList::InterpolationStyle);

	PBD::ScopedConnectionList _list_connections;
	PBD::ScopedConnectionList _stripable_connections;

	void entered ();
	void exited ();

	//void set_colors ();
	void color_handler ();

	static Pango::FontDescription name_font;
	static bool have_name_font;

	std::string automation_state_off_string () const;

private:
	int set_state_2X (const XMLNode &, int);
};

#endif /* __ardour_gtk_automation_time_axis_h__ */
