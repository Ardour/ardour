#ifndef __ardour_basic_ui_h__
#define __ardour_basic_ui_h__

namespace ARDOUR {
	class Session;
}

class BasicUI {
  public:
	BasicUI (ARDOUR::Session&);
	virtual ~BasicUI ();
	
	void add_marker ();

	/* transport control */

	void loop_toggle ();
	void goto_start ();
	void goto_end ();
	void rewind ();
	void ffwd ();
	void transport_stop ();
	void transport_play ();
	void set_transport_speed (float speed);
	float get_transport_speed (float speed);

	void save_state ();
	void prev_marker ();
	void next_marker ();
	void undo ();
	void redo ();
	void toggle_punch_in ();
	void toggle_punch_out ();

	void rec_enable_toggle ();
	void toggle_all_rec_enables ();

  protected:
	BasicUI ();
	ARDOUR::Session* session;
};

#endif /* __ardour_basic_ui_h__ */
