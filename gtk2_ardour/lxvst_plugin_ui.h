#ifndef __lxvst_plugin_ui_h__
#define __lxvst_plugin_ui_h__

#include <sigc++/signal.h>
#include "vst_plugin_ui.h"

#ifdef LXVST_SUPPORT

namespace ARDOUR {
	class PluginInsert;
	class LXVSTPlugin;
}

class LXVSTPluginUI : public VSTPluginUI
{
  public:
	LXVSTPluginUI (boost::shared_ptr<ARDOUR::PluginInsert>, boost::shared_ptr<ARDOUR::VSTPlugin>);
	~LXVSTPluginUI ();

	int get_preferred_height ();
	
	bool start_updating (GdkEventAny *);
	bool stop_updating (GdkEventAny *);

	int package (Gtk::Window&);
	void forward_key_event (GdkEventKey *);
	bool non_gtk_gui () const { return true; }

private:
	void resize_callback ();
	int get_XID ();

	sigc::connection _screen_update_connection;
};

#endif //LXVST_SUPPORT

#endif
