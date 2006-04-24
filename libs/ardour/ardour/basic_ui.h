#ifndef __ardour_basic_ui_h__
#define __ardour_basic_ui_h__

namespace ARDOUR {
	class Session;
}

class BasicUI {
  public:
	BasicUI (ARDOUR::Session&);
	virtual ~BasicUI ();
	
	void loop_toggle ();
	void goto_start ();
	void goto_end ();
	void add_marker ();
	void rewind ();
	void ffwd ();
	void transport_stop ();
	void transport_play ();
	void rec_enable_toggle ();
	void save_state ();
	void prev_marker ();
	void next_marker ();
	void move_at (float speed);
	void undo ();
	void redo ();
	void toggle_all_rec_enables ();

  protected:
	ARDOUR::Session& session;
};

#endif /* __ardour_basic_ui_h__ */
