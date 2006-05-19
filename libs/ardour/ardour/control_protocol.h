#ifndef ardour_control_protocols_h
#define ardour_control_protocols_h

#include <string>
#include <vector>
#include <list>
#include <sigc++/sigc++.h>

#include <ardour/basic_ui.h>

namespace ARDOUR {

class Route;
class Session;

class ControlProtocol : public sigc::trackable, public BasicUI {
  public:
	ControlProtocol (Session&, std::string name);
	virtual ~ControlProtocol();

	std::string name() const { return _name; }

	virtual int set_active (bool yn) = 0;
	bool get_active() const { return _active; }

	sigc::signal<void> ActiveChanged;


	/* signals that a control protocol can emit and other (presumably graphical)
	   user interfaces can respond to
	*/

	static sigc::signal<void> ZoomToSession;
	static sigc::signal<void> ZoomIn;
	static sigc::signal<void> ZoomOut;
	static sigc::signal<void> Enter;
	static sigc::signal<void,float> ScrollTimeline;

	/* the model here is as follows:

	   we imagine most control surfaces being able to control
	   from 1 to N tracks at a time, with a session that may
	   contain 1 to M tracks, where M may be smaller, larger or
	   equal to N. 

	   the control surface has a fixed set of physical controllers
	   which can potentially be mapped onto different tracks/busses
	   via some mechanism.

	   therefore, the control protocol object maintains
	   a table that reflects the current mapping between
	   the controls and route object.
	*/

	void set_route_table_size (uint32_t size);
	void set_route_table (uint32_t table_index, ARDOUR::Route*);

	void route_set_rec_enable (uint32_t table_index, bool yn);
	bool route_get_rec_enable (uint32_t table_index);

	float route_get_gain (uint32_t table_index);
	void route_set_gain (uint32_t table_index, float);
	float route_get_effective_gain (uint32_t table_index);

	float route_get_peak_input_power (uint32_t table_index, uint32_t which_input);

	bool route_get_muted (uint32_t table_index);
	void route_set_muted (uint32_t table_index, bool);

	bool route_get_soloed (uint32_t table_index);
	void route_set_soloed (uint32_t table_index, bool);

	std::string route_get_name (uint32_t table_index);

  protected:
	std::vector<ARDOUR::Route*> route_table;
	std::string _name;
	bool _active;

	void next_track (uint32_t initial_id);
	void prev_track (uint32_t initial_id);
};

extern "C" {
	struct ControlProtocolDescriptor {
	    const char* name;      /* descriptive */
	    const char* id;        /* unique and version-specific */
	    void*       ptr;       /* protocol can store a value here */
	    void*       module;    /* not for public access */
	    int         mandatory; /* if non-zero, always load and do not make optional */
	    ControlProtocol* (*initialize)(ControlProtocolDescriptor*,Session*);
	    void             (*destroy)(ControlProtocolDescriptor*,ControlProtocol*);
	    
	};
}

}

#endif // ardour_control_protocols_h
