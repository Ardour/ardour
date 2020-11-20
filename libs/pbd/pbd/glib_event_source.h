#ifndef __libpbd_glib_event_source_h__
#define __libpbd_glib_event_source_h__

#include <boost/function.hpp>

#include <glibmm/main.h>

class GlibEventLoopSource : public Glib::Source
{
  public:
	GlibEventLoopSource () {};

	bool prepare (int& timeout);
	bool check();
	bool dispatch (sigc::slot_base*);
};


class GlibEventLoopCallback : public GlibEventLoopSource
{
  public:
	GlibEventLoopCallback (boost::function<void()> callback) : _callback (callback) {}

	bool check() {
		_callback();
		return false;
	}

  private:
	boost::function<void()> _callback;
};

#endif /* __libpbd_glib_event_source_h__ */
