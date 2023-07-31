/*
 * Copyright (C) 2017-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2018 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_selection_h__
#define __ardour_selection_h__

#include <memory>
#include <set>
#include <vector>

#include "pbd/stateful.h"

#include "ardour/presentation_info.h"
#include "ardour/types.h"

namespace ARDOUR {

class AutomationControl;
class RouteGroup;
class Session;
class Stripable;
class VCAManager;
class PresentationInfo;

class LIBARDOUR_API CoreSelection : public PBD::Stateful {
  public:
	CoreSelection (Session& s);
	~CoreSelection ();

	void toggle (std::shared_ptr<Stripable>, std::shared_ptr<AutomationControl>);
	void add (std::shared_ptr<Stripable>, std::shared_ptr<AutomationControl>);
	void remove (std::shared_ptr<Stripable>, std::shared_ptr<AutomationControl>);
	void set (std::shared_ptr<Stripable>, std::shared_ptr<AutomationControl>);
	void set (StripableList&);

	void select_next_stripable (bool mixer_order, bool routes_only);
	void select_prev_stripable (bool mixer_order, bool routes_only);
	bool select_stripable_and_maybe_group (std::shared_ptr<Stripable> s, bool with_group, bool routes_only, RouteGroup*);

	void clear_stripables();

	std::shared_ptr<Stripable> first_selected_stripable () const;

	bool selected (std::shared_ptr<const Stripable>) const;
	bool selected (std::shared_ptr<const AutomationControl>) const;
	uint32_t selected() const;

	struct StripableAutomationControl {
		std::shared_ptr<Stripable> stripable;
		std::shared_ptr<AutomationControl> controllable;
		int order;

		StripableAutomationControl (std::shared_ptr<Stripable> s, std::shared_ptr<AutomationControl> c, int o)
			: stripable (s), controllable (c), order (o) {}
	};

	typedef std::vector<StripableAutomationControl> StripableAutomationControls;

	void get_stripables (StripableAutomationControls&) const;
	void get_stripables_for_op (StripableList&, std::shared_ptr<Stripable> base, bool (RouteGroup::*group_predicate)() const) const;
	void get_stripables_for_op (std::shared_ptr<StripableList>, std::shared_ptr<Stripable> base, bool (RouteGroup::*group_predicate)() const) const;

	XMLNode& get_state () const;
	int set_state (const XMLNode&, int version);

  protected:
	friend class AutomationControl;
	void remove_control_by_id (PBD::ID const &);

  protected:
	friend class Stripable;
	friend class Session;
	friend class VCAManager;
	void remove_stripable_by_id (PBD::ID const &);

  private:
	mutable Glib::Threads::RWLock _lock;
	std::atomic<int>             _selection_order;

	Session& session;

	struct SelectedStripable {
		SelectedStripable (std::shared_ptr<Stripable>, std::shared_ptr<AutomationControl>, int);
		SelectedStripable (PBD::ID const & s, PBD::ID const & c, int o)
			: stripable (s), controllable (c), order (o) {}

		PBD::ID stripable;
		PBD::ID controllable;
		int order;

		bool operator< (SelectedStripable const & other) const {
			if (stripable == other.stripable) {
				return controllable < other.controllable;
			}
			return stripable < other.stripable;
		}
	};

	typedef std::set<SelectedStripable> SelectedStripables;

	std::weak_ptr<ARDOUR::Stripable> _first_selected_stripable;

	SelectedStripables _stripables;

	void send_selection_change ();

	template<typename IterTypeCore>
		void select_adjacent_stripable (bool mixer_order, bool routes_only,
		                                IterTypeCore (StripableList::*begin_method)(),
		                                IterTypeCore (StripableList::*end_method)());
};

} // namespace ARDOUR

#endif /* __ardour_selection_h__ */
