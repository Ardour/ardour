#ifndef __gtkmm2ext_cairo_packer_h__
#define __gtkmm2ext_cairo_packer_h__

#include <gtkmm/box.h>

class CairoPacker 
{
  public:
	CairoPacker () {}
	virtual ~CairoPacker () {}

        virtual Gdk::Color get_bg () const = 0;

  protected:
	virtual void draw_background (Gtk::Widget&, GdkEventExpose*);
};

class CairoHPacker : public CairoPacker, public Gtk::HBox
{
  public:
	CairoHPacker ();
	~CairoHPacker() {}

        Gdk::Color get_bg () const;

	bool on_expose_event (GdkEventExpose*);
        void on_realize ();
};

class CairoVPacker : public CairoPacker, public Gtk::VBox
{
  public:
	CairoVPacker ();
	~CairoVPacker () {}

        Gdk::Color get_bg () const;

	bool on_expose_event (GdkEventExpose*);
        void on_realize ();
};

#endif /* __gtkmm2ext_cairo_packer_h__ */
