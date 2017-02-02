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

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/application.h"
#include "gtkmm2ext/window_title.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/utils.h"

#include "canvas/types.h"
#include "canvas/canvas.h"
#include "canvas/container.h"
#include "canvas/colors.h"
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
	case Transmitter::Error:
		prefix = "[ERROR]: ";
		break;
	case Transmitter::Info:
		prefix = "[INFO]: ";
		break;
	case Transmitter::Warning:
		prefix = "[WARNING]: ";
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

ArdourButton*
make_transport_button (std::string const & action, Gtkmm2ext::ArdourIcon::Icon icon)
{
	ArdourButton* button = new ArdourButton;
	button->set_name ("transport button");
	Glib::RefPtr<Gtk::Action> act;
	act = ActionManager::get_action (action.c_str());
	button->set_related_action (act);
	button->set_icon (icon);
	return button;
}

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

	void initialize_canvas (ArdourCanvas::Canvas& canvas);

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

	Gtkmm2ext::WindowTitle title ("Canvas Toolbar Test");
	_main_window.set_title (title.get_string());
	_main_window.set_flags (CAN_FOCUS);
	_main_window.signal_delete_event().connect (sigc::mem_fun (*this, &CANVAS_UI::main_window_delete_event));

	canvas = new ArdourCanvas::GtkCanvas ();

	initialize_canvas (*canvas);

	canvas->signal_size_request().connect (sigc::mem_fun (*this, &CANVAS_UI::canvas_size_request));
	canvas->signal_size_allocate().connect (sigc::mem_fun (*this, &CANVAS_UI::canvas_size_allocated));

	_main_window.add (*canvas);
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

void
CANVAS_UI::initialize_canvas (ArdourCanvas::Canvas& canvas)
{
	using namespace ArdourCanvas;
	canvas.set_background_color (rgba_to_color (0.0, 0.0, 0.4, 1.0));

	ScrollGroup* scroll_group = new ScrollGroup (canvas.root(),
			ScrollGroup::ScrollSensitivity (ScrollGroup::ScrollsVertically|ScrollGroup::ScrollsHorizontally));

	grid = new ArdourCanvas::Grid (scroll_group);

	grid->set_padding (3.0);

	grid->set_row_spacing (3.0);
	grid->set_col_spacing (3.0);
	grid->set_homogenous (true);

	ArdourCanvas::Widget* w1 = new ArdourCanvas::Widget (&canvas, *make_transport_button ("Transport/Roll", ArdourIcon::TransportPlay));
	CANVAS_DEBUG_NAME (w1, "w1");
	grid->place (w1, 0, 0);
	ArdourCanvas::Widget* w2 = new ArdourCanvas::Widget (&canvas, *make_transport_button ("Transport/Stop", ArdourIcon::TransportStop));
	CANVAS_DEBUG_NAME (w2, "w2");
	grid->place (w2, 1, 0);
	ArdourCanvas::Widget* w3 = new ArdourCanvas::Widget (&canvas, *make_transport_button ("Transport/Record", ArdourIcon::RecButton));
	CANVAS_DEBUG_NAME (w3, "w3");
	grid->place (w3, 2, 0);
	ArdourCanvas::Widget* w4 = new ArdourCanvas::Widget (&canvas, *make_transport_button ("Transport/Loop", ArdourIcon::TransportLoop));
	CANVAS_DEBUG_NAME (w4, "w4");
	grid->place (w4, 3, 0);
}

void
CANVAS_UI::canvas_size_request (Gtk::Requisition* req)
{
	ArdourCanvas::Rect bbox = canvas->root()->bounding_box();
	req->width = bbox.width();
	req->height = bbox.height();
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

	if (!ARDOUR::init (false, true, localedir)) {
		cerr << "Ardour failed to initialize\n" << endl;
		::exit (EXIT_FAILURE);
	}

	if (!Glib::thread_supported()) {
		Glib::thread_init();
	}

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, 0);

	log_receiver.listen_to (error);
	log_receiver.listen_to (info);
	log_receiver.listen_to (fatal);
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
