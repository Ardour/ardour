#ifndef __gtkmm2ext_cairo_packer_h__
#define __gtkmm2ext_cairo_packer_h__

#include <gtkmm/box.h>

class CairoPacker 
{
  public:
	CairoPacker () {}
	virtual ~CairoPacker () {}

  protected:
	virtual void draw_background (Gtk::Widget&, GdkEventExpose*);
};

class CairoHPacker : public CairoPacker, public Gtk::HBox
{
  public:
	CairoHPacker ();
	~CairoHPacker() {}

	bool on_expose_event (GdkEventExpose*);
};

class CairoVPacker : public CairoPacker, public Gtk::VBox
{
  public:
	CairoVPacker ();
	~CairoVPacker () {}

	bool on_expose_event (GdkEventExpose*);
};

#endif /* __gtkmm2ext_cairo_packer_h__ */
