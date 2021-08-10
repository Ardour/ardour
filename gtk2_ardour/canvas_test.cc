#include <glibmm.h>
#include <gtkmm/main.h>
#include <gtkmm/box.h>
#include <gtkmm/window.h>

#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/failed_constructor.h"
#include "pbd/pthread_utils.h"
#include "pbd/receiver.h"
#include "pbd/transmitter.h"

#include "ardour/audioengine.h"
#include "ardour/filename_extensions.h"
#include "ardour/types.h"

#include "gtkmm2ext/application.h"
#include "gtkmm2ext/window_title.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"

#include "canvas/types.h"
#include "canvas/canvas.h"
#include "canvas/container.h"
#include "gtkmm2ext/colors.h"
#include "canvas/debug.h"
#include "canvas/grid.h"
#include "canvas/scroll_group.h"
#include "canvas/text.h"
#include "canvas/widget.h"

#include "ardour_button.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtkmm2ext;
using namespace Gtk;

#include "ardour/vst_types.h"

static const char* localedir = LOCALEDIR;
int vstfx_init (void*) { return 0; }
void vstfx_exit () {}
void vstfx_destroy_editor (VSTState*) {}

class LogReceiver : public Receiver
{
  protected:
    void receive (Transmitter::Channel chn, const char * str);
};

static LogReceiver log_receiver;

void
LogReceiver::receive (Transmitter::Channel chn, const char * str)
{
	const char *prefix = "";

	switch (chn) {
	case Transmitter::Debug:
		/* ignore */
		return;
	case Transmitter::Info:
		prefix = "[INFO]: ";
		break;
	case Transmitter::Warning:
		prefix = "[WARNING]: ";
		break;
	case Transmitter::Error:
		prefix = "[ERROR]: ";
		break;
	case Transmitter::Fatal:
		prefix = "[FATAL]: ";
		break;
	case Transmitter::Throw:
		/* this isn't supposed to happen */
		cerr << "Game Over\n";
		abort ();
	}

	/* note: iostreams are already thread-safe: no external
	   lock required.
	*/

	std::cout << prefix << str << std::endl;

	if (chn == Transmitter::Fatal) {
		::exit (9);
	}
}

/* ***************************************************************************/
/* ***************************************************************************/
/* ***************************************************************************/

class CANVAS_UI : public Gtkmm2ext::UI, public ARDOUR::SessionHandlePtr
{
public:
	CANVAS_UI (int *argcp, char **argvp[], const char* localedir);
	~CANVAS_UI();
private:
	int starting ();
	bool main_window_delete_event (GdkEventAny* ev) { finish (); return true; }
	void finish () { quit (); }
	Gtk::Window _main_window;

	ArdourCanvas::Container* initialize_canvas (ArdourCanvas::Canvas& canvas);

	void canvas_size_request (Gtk::Requisition* req);
	void canvas_size_allocated (Gtk::Allocation& alloc);

	ArdourCanvas::GtkCanvas* canvas;
	ArdourCanvas::Container* group;
	ArdourCanvas::Grid* grid;

	ArdourButton test_button;
};

/* ***************************************************************************/

CANVAS_UI::CANVAS_UI (int *argcp, char **argvp[], const char* localedir)
	: Gtkmm2ext::UI (PROGRAM_NAME, X_("gui"), argcp, argvp)
{
	Gtkmm2ext::init (localedir);
	UIConfiguration::instance().post_gui_init ();

	Gtkmm2ext::WindowTitle title ("Canvas Test");
	_main_window.set_title (title.get_string());
	_main_window.set_flags (CAN_FOCUS);
	_main_window.signal_delete_event().connect (sigc::mem_fun (*this, &CANVAS_UI::main_window_delete_event));


	VBox* b = manage (new VBox);
	Label* l = manage (new Label ("Hello there"));

	canvas = new ArdourCanvas::GtkCanvas ();
	group = initialize_canvas (*canvas);

	canvas->signal_size_request().connect (sigc::mem_fun (*this, &CANVAS_UI::canvas_size_request));
	canvas->signal_size_allocate().connect (sigc::mem_fun (*this, &CANVAS_UI::canvas_size_allocated));
	test_button.signal_clicked.connect (sigc::mem_fun (*this, &CANVAS_UI::finish));

	test_button.set_text ("Don't click me");

	b->pack_start (*l, false, 0);
	b->pack_start (*canvas, true, 0);

	_main_window.add (*b);
	_main_window.show_all ();
}

CANVAS_UI::~CANVAS_UI ()
{
}

int
CANVAS_UI::starting ()
{
	Application* app = Application::instance ();
	app->ready ();
	return 0;
}

ArdourCanvas::Container*
CANVAS_UI::initialize_canvas (ArdourCanvas::Canvas& canvas)
{
	using namespace ArdourCanvas;
	canvas.set_background_color (rgba_to_color (0.0, 0.0, 0.4, 1.0));

	ScrollGroup* scroll_group = new ScrollGroup (canvas.root(),
			ScrollGroup::ScrollSensitivity (ScrollGroup::ScrollsVertically|ScrollGroup::ScrollsHorizontally));

	grid = new ArdourCanvas::Grid (scroll_group);

	grid->set_padding (40.0);
	grid->set_margin (0.0);

	grid->set_outline_width (3.0);
	grid->set_outline_color (Color (0x3daec1ff));
	grid->set_outline (false);
	grid->set_row_spacing (60.0);
	grid->set_col_spacing (3.0);
	grid->set_homogenous (false);

	ArdourCanvas::Text* text1 = new ArdourCanvas::Text (&canvas);
	text1->set ("hello, world");
	text1->set_color (Color (0xff0000ff));

	ArdourCanvas::Text* text2 = new ArdourCanvas::Text (&canvas);
	text2->set ("goodbye, cruel world");
	text2->set_color (Color (0x00ff00ff));

	ArdourCanvas::Text* text3 = new ArdourCanvas::Text (&canvas);
	text3->set ("I am the third");
	text3->set_color (Color (0xff00ffff));

	ArdourCanvas::Text* text4 = new ArdourCanvas::Text (&canvas);
	text4->set ("I am fourth");
	text4->set_color (Color (0xffff00ff));

	grid->place (text1, 0, 0, 2, 1);
	grid->place (text2, 2, 0);
	grid->place (text3, 0, 2, 1, 2);
	grid->place (text4, 1, 3);

	ArdourButton* button1 = new ArdourButton ("auto-return");
	ArdourButton* button2 = new ArdourButton ("auto-play");
	ArdourButton* button3 = new ArdourButton ("follow range");
	ArdourButton* button4 = new ArdourButton ("auto-input");

	ArdourCanvas::Widget* w1 = new ArdourCanvas::Widget (&canvas, *button1);
	CANVAS_DEBUG_NAME (w1, "w1");
	grid->place (w1, 3, 0, 2, 0);
	ArdourCanvas::Widget* w2 = new ArdourCanvas::Widget (&canvas, *button2);
	CANVAS_DEBUG_NAME (w2, "w2");
	grid->place (w2, 5, 0, 2, 0);
	ArdourCanvas::Widget* w3 = new ArdourCanvas::Widget (&canvas, *button3);
	CANVAS_DEBUG_NAME (w3, "w3");
	grid->place (w3, 3, 1);
	ArdourCanvas::Widget* w4 = new ArdourCanvas::Widget (&canvas, *button4);
	CANVAS_DEBUG_NAME (w4, "w4");
	grid->place (w4, 4, 1);

	//ArdourCanvas::Widget* w = new ArdourCanvas::Widget (scroll_group, test_button);
	//CANVAS_DEBUG_NAME (w, "TheW");

	return new ArdourCanvas::Container (scroll_group);
}

void
CANVAS_UI::canvas_size_request (Gtk::Requisition* req)
{
	req->width = 100;
	req->height = 100;
}

void
CANVAS_UI::canvas_size_allocated (Gtk::Allocation& alloc)
{
}

/* ***************************************************************************/
/* ***************************************************************************/
/* ***************************************************************************/

static CANVAS_UI  *ui = 0;

int main (int argc, char **argv)
{
#if 0
	fixup_bundle_environment (argc, argv, localedir);
	load_custom_fonts();
	// TODO setlocale..
#endif

	if (!ARDOUR::init (true, localedir)) {
		cerr << "Ardour failed to initialize\n" << endl;
		::exit (EXIT_FAILURE);
	}

	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	log_receiver.listen_to (info);
	log_receiver.listen_to (fatal);
	log_receiver.listen_to (error);
	log_receiver.listen_to (warning);

	if (UIConfiguration::instance().pre_gui_init ()) {
		error << _("Could not complete pre-GUI initialization") << endmsg;
		exit (1);
	}

	// we could load a session here, if needed
	// see ../session_utils/common.cc

	ui = new CANVAS_UI (&argc, &argv, localedir);
	ui->run (log_receiver);

	info << "Farewell" << endmsg;

	Gtkmm2ext::Application::instance()->cleanup();
	delete ui;
	ui = 0;

	ARDOUR::cleanup ();
	pthread_cancel_all ();
	return 0;
}
