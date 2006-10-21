/*
    Copyright (C) 2000-2002 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_route_h__
#define __ardour_route_h__

#include <cmath>
#include <list>
#include <set>
#include <map>
#include <string>

#include <boost/shared_ptr.hpp>

#include <pbd/fastlog.h>
#include <glibmm/thread.h>
#include <pbd/xml++.h>
#include <pbd/undo.h>
#include <pbd/stateful.h> 
#include <pbd/controllable.h>
#include <pbd/destructible.h>

#include <ardour/ardour.h>
#include <ardour/io.h>
#include <ardour/session.h>
#include <ardour/redirect.h>
#include <ardour/types.h>

namespace ARDOUR {

class Insert;
class Send;
class RouteGroup;

enum mute_type {
    PRE_FADER =    0x1,
    POST_FADER =   0x2,
    CONTROL_OUTS = 0x4,
    MAIN_OUTS =    0x8
};

class Route : public IO
{
  protected:

        typedef list<boost::shared_ptr<Redirect> > RedirectList;
  public:

	enum Flag {
		Hidden = 0x1,
		MasterOut = 0x2,
		ControlOut = 0x4
	};


	Route (Session&, std::string name, int input_min, int input_max, int output_min, int output_max,
	       Flag flags = Flag(0), DataType default_type = DataType::AUDIO);
	
	Route (Session&, const XMLNode&);
	virtual ~Route();

	std::string comment() { return _comment; }
	void set_comment (std::string str, void *src);

	long order_key(std::string name) const;
	void set_order_key (std::string name, long n);

	bool hidden() const { return _flags & Hidden; }
	bool master() const { return _flags & MasterOut; }
	bool control() const { return _flags & ControlOut; }

	/* these are the core of the API of a Route. see the protected sections as well */


	virtual int  roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
			   nframes_t offset, int declick, bool can_record, bool rec_monitors_input);

	virtual int  no_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
			      nframes_t offset, bool state_changing, bool can_record, bool rec_monitors_input);

	virtual int  silent_roll (nframes_t nframes, nframes_t start_frame, nframes_t end_frame, 
				  nframes_t offset, bool can_record, bool rec_monitors_input);
	virtual void toggle_monitor_input ();
	virtual bool can_record() { return false; }
	virtual void set_record_enable (bool yn, void *src) {}
	virtual bool record_enabled() const { return false; }
	virtual void handle_transport_stopped (bool abort, bool did_locate, bool flush_redirects);
	virtual void set_pending_declick (int);

	/* end of vfunc-based API */

	/* override IO::set_gain() to provide group control */

	void set_gain (gain_t val, void *src);
	void inc_gain (gain_t delta, void *src);

	bool active() const { return _active; }
	void set_active (bool yn);

	void set_solo (bool yn, void *src);
	bool soloed() const { return _soloed; }

	void set_solo_safe (bool yn, void *src);
	bool solo_safe() const { return _solo_safe; }

	void set_mute (bool yn, void *src);
	bool muted() const { return _muted; }

	void set_mute_config (mute_type, bool, void *src);
	bool get_mute_config (mute_type);
	
	void       set_edit_group (RouteGroup *, void *);
	void       drop_edit_group (void *);
	RouteGroup *edit_group () { return _edit_group; }

	void       set_mix_group (RouteGroup *, void *);
	void       drop_mix_group (void *);
	RouteGroup *mix_group () { return _mix_group; }

	virtual void  set_meter_point (MeterPoint, void *src);
	MeterPoint  meter_point() const { return _meter_point; }

	/* Redirects */

	void flush_redirects ();

	template<class T> void foreach_redirect (T *obj, void (T::*func)(boost::shared_ptr<Redirect>)) {
		Glib::RWLock::ReaderLock lm (redirect_lock);
		for (RedirectList::iterator i = _redirects.begin(); i != _redirects.end(); ++i) {
			(obj->*func) (*i);
		}
	}

	boost::shared_ptr<Redirect> nth_redirect (uint32_t n) {
		Glib::RWLock::ReaderLock lm (redirect_lock);
		RedirectList::iterator i;
		for (i = _redirects.begin(); i != _redirects.end() && n; ++i, --n);
		if (i == _redirects.end()) {
			return boost::shared_ptr<Redirect> ();
		} else {
			return *i;
		}
	}
	
	ChanCount max_redirect_outs () const { return redirect_max_outs; }
	
	// FIXME: remove/replace err_streams parameters with something appropriate
	// they are used by 'wierd_plugin_dialog'(sic) to display the number of input streams
	// at the insertion point if the insert fails
	int add_redirect (boost::shared_ptr<Redirect>, void *src, uint32_t* err_streams = 0);
	int add_redirects (const RedirectList&, void *src, uint32_t* err_streams = 0);
	int remove_redirect (boost::shared_ptr<Redirect>, void *src, uint32_t* err_streams = 0);
	int copy_redirects (const Route&, Placement, uint32_t* err_streams = 0);
	int sort_redirects (uint32_t* err_streams = 0);

	void clear_redirects (void *src);
	void all_redirects_flip();
	void all_redirects_active (bool state);

	virtual nframes_t update_total_latency();
	nframes_t signal_latency() const { return _own_latency; }
	virtual void set_latency_delay (nframes_t);

	sigc::signal<void,void*> solo_changed;
	sigc::signal<void,void*> solo_safe_changed;
	sigc::signal<void,void*> comment_changed;
	sigc::signal<void,void*> mute_changed;
	sigc::signal<void,void*> pre_fader_changed;
	sigc::signal<void,void*> post_fader_changed;
	sigc::signal<void,void*> control_outs_changed;
	sigc::signal<void,void*> main_outs_changed;
	sigc::signal<void,void*> redirects_changed;
	sigc::signal<void,void*> record_enable_changed;
	sigc::signal<void,void*> edit_group_changed;
	sigc::signal<void,void*> mix_group_changed;
	sigc::signal<void>       active_changed;
	sigc::signal<void,void*> meter_change;

	/* gui's call this for their own purposes. */

	sigc::signal<void,std::string,void*> gui_changed;

	/* stateful */

	XMLNode& get_state();
	int set_state(const XMLNode& node);
	virtual XMLNode& get_template();

	sigc::signal<void,void*> SelectedChanged;

	/* undo */

	UndoAction get_memento() const;
	void set_state (state_id_t);

	int set_control_outs (const vector<std::string>& ports);
	IO* control_outs() { return _control_outs; }

	bool feeds (boost::shared_ptr<Route>);
	set<boost::shared_ptr<Route> > fed_by;

	struct ToggleControllable : public PBD::Controllable {
	    enum ToggleType {
		    MuteControl = 0,
		    SoloControl
	    };
	    
	    ToggleControllable (std::string name, Route&, ToggleType);
	    void set_value (float);
	    float get_value (void) const;

	    Route& route;
	    ToggleType type;
	};

	PBD::Controllable& solo_control() {
		return _solo_control;
	}

	PBD::Controllable& mute_control() {
		return _mute_control;
	}
	
	void protect_automation ();
	
	void set_remote_control_id (uint32_t id);
	uint32_t remote_control_id () const;
	sigc::signal<void> RemoteControlIDChanged;

  protected:
	friend class Session;

	void set_solo_mute (bool yn);
	void set_block_size (nframes_t nframes);
	bool has_external_redirects() const;
	void curve_reallocate ();

  protected:
	unsigned char _flags;

	/* tight cache-line access here is more important than sheer speed of
	   access.
	*/

	bool                     _muted : 1;
	bool                     _soloed : 1;
	bool                     _solo_muted : 1;
	bool                     _solo_safe : 1;
	bool                     _recordable : 1;
	bool                     _active : 1;
	bool                     _mute_affects_pre_fader : 1;
	bool                     _mute_affects_post_fader : 1;
	bool                     _mute_affects_control_outs : 1;
	bool                     _mute_affects_main_outs : 1;
	bool                     _silent : 1;
	bool                     _declickable : 1;
	int                      _pending_declick;
	
	MeterPoint               _meter_point;

	gain_t                    solo_gain;
	gain_t                    mute_gain;
	gain_t                    desired_solo_gain;
	gain_t                    desired_mute_gain;

	nframes_t            check_initial_delay (nframes_t, nframes_t&, nframes_t&);

	nframes_t           _initial_delay;
	nframes_t           _roll_delay;
	nframes_t           _own_latency;
	RedirectList             _redirects;
	Glib::RWLock      redirect_lock;
	IO                      *_control_outs;
	Glib::Mutex      control_outs_lock;
	RouteGroup              *_edit_group;
	RouteGroup              *_mix_group;
	std::string              _comment;
	bool                     _have_internal_generator;

	ToggleControllable _solo_control;
	ToggleControllable _mute_control;
	
	void passthru (nframes_t start_frame, nframes_t end_frame, 
		       nframes_t nframes, nframes_t offset, int declick, bool meter_inputs);

	virtual void process_output_buffers (BufferSet& bufs,
				     nframes_t start_frame, nframes_t end_frame,
				     nframes_t nframes, nframes_t offset, bool with_redirects, int declick,
				     bool meter);

  protected:

	virtual XMLNode& state(bool);

	void passthru_silence (nframes_t start_frame, nframes_t end_frame,
	                       nframes_t nframes, nframes_t offset, int declick,
	                       bool meter);
	
	void silence (nframes_t nframes, nframes_t offset);
	
	sigc::connection input_signal_connection;

	state_id_t _current_state_id;
	ChanCount redirect_max_outs;
	uint32_t _remote_control_id;

	uint32_t pans_required() const;
	ChanCount n_process_buffers ();

  private:
	void init ();

	static uint32_t order_key_cnt;
	typedef std::map<std::string,long> OrderKeys;
	OrderKeys order_keys;

	void input_change_handler (IOChange, void *src);
	void output_change_handler (IOChange, void *src);

	bool legal_redirect (Redirect&);
	int reset_plugin_counts (uint32_t*); /* locked */
	int _reset_plugin_counts (uint32_t*); /* unlocked */

	/* plugin count handling */

	struct InsertCount {
	    boost::shared_ptr<ARDOUR::Insert> insert;
	    int32_t cnt;
	    int32_t in;
	    int32_t out;

	    InsertCount (boost::shared_ptr<ARDOUR::Insert> ins) : insert (ins), cnt (-1) {}
	};
	
	int32_t apply_some_plugin_counts (std::list<InsertCount>& iclist);
	int32_t check_some_plugin_counts (std::list<InsertCount>& iclist, int32_t required_inputs, uint32_t* err_streams);

	void set_deferred_state ();
	void add_redirect_from_xml (const XMLNode&);
	void redirect_active_proxy (Redirect*, void*);
};

} // namespace ARDOUR

#endif /* __ardour_route_h__ */
