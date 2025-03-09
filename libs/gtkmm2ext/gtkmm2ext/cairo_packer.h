#ifndef __gtkmm2ext_cairo_packer_h__
#define __gtkmm2ext_cairo_packer_h__

#include <ytkmm/box.h>

#include "gtkmm2ext/visibility.h"

class LIBGTKMM2EXT_API CairoPacker
{
public:
	CairoPacker () {}
	virtual ~CairoPacker () {}

	virtual Gdk::Color get_bg () const = 0;

protected:
	virtual void draw_background (Gtk::Widget&, GdkEventExpose*);
};

class LIBGTKMM2EXT_API CairoHPacker : public CairoPacker, public Gtk::HBox
{
public:
	CairoHPacker ();
	~CairoHPacker() {}

	Gdk::Color get_bg () const;

	bool on_expose_event (GdkEventExpose*);
	void on_realize ();
	void on_size_allocate (Gtk::Allocation& alloc);
};

class LIBGTKMM2EXT_API CairoVPacker : public CairoPacker, public Gtk::VBox
{
public:
	CairoVPacker ();
	~CairoVPacker () {}

	Gdk::Color get_bg () const;

	bool on_expose_event (GdkEventExpose*);
	void on_realize ();
	void on_size_allocate (Gtk::Allocation& alloc);
};

#endif /* __gtkmm2ext_cairo_packer_h__ */
