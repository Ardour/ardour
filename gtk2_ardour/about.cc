/*
    Copyright (C) 2003 Paul Davis 

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

    $Id$
*/

#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdio>
#include <ctime>
#include <cstdlib>

#include <gtkmm/label.h>
#include <gtkmm/text.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/notebook.h>

#include <ardour/ardour.h>
#include <ardour/version.h>

#include "utils.h"
#include "version.h"

#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/doi.h>

#include "about.h"
#include "rgb_macros.h"
#include "ardour_ui.h"

#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace sigc;
using namespace ARDOUR;

#ifdef WITH_PAYMENT_OPTIONS

/* XPM */
static const gchar * paypal_xpm[] = {
"62 31 33 1",
" 	c None",
".	c #325781",
"+	c #154170",
"@	c #C1CDDA",
"#	c #4E6E92",
"$	c #D1D5DA",
"%	c #88A0B8",
"&	c #B4C4D3",
"*	c #C8D3DE",
"=	c #D7E1E9",
"-	c #002158",
";	c #F6F8FA",
">	c #44658B",
",	c #E7ECF0",
"'	c #A4B7CA",
")	c #9DB0C4",
"!	c #E3F1F7",
"~	c #708CA9",
"{	c #E1E7ED",
"]	c #567698",
"^	c #7C96B1",
"/	c #E7F5FA",
"(	c #EEF1F4",
"_	c #6883A2",
":	c #244873",
"<	c #BBBBBB",
"[	c #E9E9E9",
"}	c #063466",
"|	c #22364D",
"1	c #94A7BD",
"2	c #000000",
"3	c #EAF7FC",
"4	c #FFFFFF",
"1'111111111111111111111111111111111111111111111111111111111%_#",
"%333333333333333333333333333333333333333333333333333333333333.",
"%444444444444444444444444444444444444444444444444444444444444:",
"_4333333!!!!!!33333333333333333333!!!!!!33333333333!%%%%1334[:",
"_444444@+}}}}+>)44444444444444444,:}}}}}.^(44444444@}..+.44($:",
"_433333^:&&&&)_}_33///33333333333&+)&&&'~+./3///333^.(;#]33($:",
"_444444>_444444'}_>...#%####~,]##..444444=+#]...>1;#_4;.144($:",
"_43333!+'4,>#=4(:+_%%%]}}#~#}_+~~:]44_>&44#}_%%%_+>:14=}@33($:",
"_44444*+$4&--)4(+%44444%-)4=--'4{+14,}-~44##44444&}}*4)+444($:",
"_433331:;4):_;4*}_]:.$4*-~4{}>44#-=4@.#{4;+>_:.&4,++;4_#333($:",
"_44444_#444444=.-.%&*,41-#4(:@4'-:(44444(_-:^&*,4*}#44.%444($:",
"_43333:%4;@@'~+-%44*&44]-.;;'4,:-#44*@&%:-];4{'(4)-%4{+&333($:",
"_4444{}@4*}}+>#:;4^-#4;.>+,444_+:^4(:}+.]}=4'-+(4_-&4&+{444($:",
"_4333'+(41:*=3'.44*)(4=+)+*44@}%+@4=}&=/@}{4{1{44:+,4^.3333($:",
"_4444~>,,]#444*})(;**,':*}'4;._@}=,%:444(+~(;{&,*}.,,>~4444($:",
"_4333>}}}}^3333~}::}}}}>].;4^+=~}}}}]3333'}+:}}}}}}}}}'3333($:",
"_4444$@@@@(44444$))@*@*^}$4=}14=@@@@{44444=))&*@@@@@@@;4444($:",
"_433333333333333333333=+:%%.>/33333333333333333333333333333($:",
"_4444444444444444444441....>=444444444444444444444444444444($:",
"_4333333333333333333333333333333333333333333333333333333333($:",
"_4444444444444444444444444444444444444444444444444444444444($:",
"_4333333333333333333333333333333333333333333333333333333333($:",
"_4444442222444222442444242444244222242444242222244222244444($:",
"_4333332333232333233232332232233233332233233323332333333333($:",
"_4444442222442222244424442424244222442424244424444222444444($:",
"_4333332333332333233323332333233233332332233323333333233333($:",
"_4444442444442444244424442444244222242444244424442222444444($:",
"_433333333333333333333333333333333333333333333333333333333344:",
"#4([[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[=&:",
".=&<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<1|",
"::||||||||||||||||||||||||||||||||||||||||||||||||||||||||||||"};
#endif

static gint 
stoppit (GdkEventButton* ev, Gtk::Notebook* notebook)
{
	gtk_signal_emit_stop_by_name (GTK_OBJECT(notebook->gobj()),
				      "button_release_event");
	return TRUE;
}

static const char* author_names[] = {
	N_("Marcus Andersson"),
	N_("Jeremy Hall"),
	N_("Steve Harris"),
	N_("Tim Mayberry"),
	N_("Mark Stewart"),
	N_("Sam Chessman"),
	N_("Jack O'Quin"),
	N_("Matt Krai"),
	N_("Ben Bell"),
	N_("Gerard van Dongen"),
	N_("Thomas Charbonnel"),
	N_("Nick Mainsbridge"),
	N_("Colin Law"),
	N_("Sampo Savolainen"),
	N_("Joshua Leach"),
	N_("Rob Holland"),
	N_("Per Sigmond"),
	N_("Doug Mclain"),
	0
};

static const char* translators[] = {
	N_("French:\n\tAlain Fréhel <alain.frehel@free.fr>"),
	N_("German:\n\tKarsten Petersen <kapet@kapet.de>"),
	N_("Italian:\n\tFilippo Pappalardo <filippo@email.it>"),
	N_("Portuguese:\n\tRui Nuno Capela <rncbc@rncbc.org>"),
	N_("Brazilian Portuguese:\n\tAlexander da Franca Fernandes <alexander@nautae.eti.br>\
\n\tChris Ross <chris@tebibyte.org>"),
	N_("Spanish:\n\t Alex Krohn <alexkrohn@fastmail.fm>"),
	N_("Russian:\n\t Igor Blinov <pitstop@nm.ru>"),
	0
};


About::About (ARDOUR_UI * ui)
	: Window (GTK_WINDOW_TOPLEVEL), _ui (ui)
#ifdef WITH_PAYMENT_OPTIONS
	, paypal_pixmap (paypal_xpm)
#endif
{
	using namespace Notebook_Helpers;

	about_index = 0;
	about_cnt = 0;
	drawn = false;

	Gtk::Label* small_label = manage (new Label (_(
"Copyright (C) 1999-2005 Paul Davis\n"
"Ardour comes with ABSOLUTELY NO WARRANTY\n"
"This is free software, and you are welcome to redistribute it\n"
"under certain conditions; see the file COPYING for details.\n")));

	Gtk::Label* version_label = 
		manage (new Label 
			(compose(_("Ardour: %1\n(built with ardour/gtk %2.%3.%4 libardour: %5.%6.%7)"), 
				 VERSIONSTRING, 
				 gtk_ardour_major_version, 
				 gtk_ardour_minor_version, 
				 gtk_ardour_micro_version, 
				 libardour_major_version, 
				 libardour_minor_version, 
				 libardour_micro_version))); 

	Notebook* notebook = manage (new Notebook);

	ScrolledWindow* author_scroller = manage (new ScrolledWindow);
	Text* author_text = manage (new Text);

	author_text->set_editable (false);
	author_text->set_name (X_("AboutText"));

	string str = _(
"Primary author:\n\t\
Paul Davis\n\n\
Major developers:\n\t\
Jesse Chappell\n\t\
Taybin Rutkin\n\
Contributors:\n\t");

	for (int32_t n = 0; author_names[n] != 0; ++n) {
		str += _(author_names[n]);
		str += "\n\t";
	}

	author_text->insert (str);

	author_scroller->add (*author_text);
	author_scroller->set_size_request (-1, 75);
	author_scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	ScrolledWindow* translator_scroller = manage (new ScrolledWindow);
	Text* translator_text = manage (new Text);

	translator_text->set_editable (false);
	translator_text->set_name (X_("AboutText"));

	str = "";
	
	for (int32_t n = 0; translators[n] != 0; ++n) {
		str += _(translators[n]);
		str += '\n';
	}

	translator_text->insert (str);
	
	translator_scroller->add (*translator_text);
	translator_scroller->set_size_request (-1, 75);
	translator_scroller->set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	Label* author_tab_label = manage (new Label (_("Authors")));
	Label* translator_tab_label = manage (new Label (_("Translators")));

	notebook->pages().push_back (TabElem (*author_scroller, *author_tab_label));
	notebook->pages().push_back (TabElem (*translator_scroller, *translator_tab_label));

	notebook->set_name ("AboutNoteBook");
	notebook->button_release_event.connect_after (bind (ptr_fun (stoppit), notebook));

	logo_pixmap = 0;
	logo_height = 0;
	logo_width = 0;
	
	set_name ("AboutWindow");
	set_title ("ardour: about");
	set_wmclass ("ardour_about", "Ardour");
 	
	vbox.set_border_width (5);
	vbox.set_spacing (5);

	if (load_logo_size ()) {
		logo_area.set_size_request (logo_width, logo_height);
		load_logo (*this);

		vbox.pack_start (logo_area, false, false);
		logo_area.expose_event.connect (mem_fun(*this, &About::logo_area_expose));
	} else {
		expose_event.connect (mem_fun(*this, &About::logo_area_expose));
	}

 	small_label->set_name  ("AboutWindowSmallLabel");
	version_label->set_name("AboutWindowSmallLabel");

	first_label.set_name ("AboutWindowLabel");
	third_label.set_name ("AboutWindowPDLabel");
	second_label.set_name ("AboutWindowLabel");

	subvbox.pack_start (*small_label, false, false);
	subvbox.pack_start (*version_label, false, false);
	subvbox.pack_start (*notebook, true, true);

#ifdef WITH_PAYMENT_OPTIONS
	paypal_button.add (paypal_pixmap);
	
	HBox *payment_box = manage (new HBox);
	payment_box->pack_start (paypal_button, true, false);

	subvbox.pack_start (*payment_box, false, false);
#endif

	delete_event.connect (bind (ptr_fun (just_hide_it), static_cast<Gtk::Window*> (this)));

	add (vbox);
	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	set_position (GTK_WIN_POS_CENTER);

	show_all ();
	subvbox.hide ();

	/* wait for the first logo expose event to complete so that
	   we know we are fully drawn.
	*/

	while (!drawn) {
		gtk_main_iteration ();
	}
}

About::~About ()
{
}

void
About::show_sub (bool yn)
{
	if (yn) {
		vbox.pack_start (subvbox, true, true);
		subvbox.show_all ();
	} else {
		vbox.remove (subvbox);
		subvbox.hide ();
	}
}

gint
About::button_release_event_impl (GdkEventButton* ev)
{
	hide();

	if (!_ui->shown ()) {
		/* show it immediately */
		_ui->show();
	}
	
	return TRUE;
}

void
About::realize_impl ()
{
	Window::realize_impl ();
	get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));
	// get_window().set_decorations (GdkWMDecoration (0));
}

bool
About::load_logo_size ()
{
	gchar buf[1024];
	FILE *fp;
	string path = find_data_file ("splash.ppm");

	if (path.length() == 0) {
		return false;
	}

	if ((fp = fopen (path.c_str(), "rb")) == 0) {
		error << compose (_("cannot open splash image file \"%1\""), path) << endmsg;
		return false;
	}
	
	fgets (buf, sizeof (buf), fp);
	if (strcmp (buf, "P6\n") != 0) {
		fclose (fp);
		return false;
	}
	
	fgets (buf, sizeof (buf), fp);
	fgets (buf, sizeof (buf), fp);
	sscanf (buf, "%d %d", &logo_width, &logo_height);
	fclose (fp);
	return true;
}

bool
About::load_logo (Gtk::Window& window)
{
	GdkGC* gc;
	gchar   buf[1024];
	guchar *pixelrow;
	FILE *fp;
	gint count;
	gint i;
	string path;

	path = find_data_file ("splash.ppm");

	if (path.length() == 0) {
		return false;
	}
	
	if ((fp = fopen (path.c_str(), "rb")) == 0) {
		return false;
	}
	
	fgets (buf, sizeof (buf), fp);
	if (strcmp (buf, "P6\n") != 0) {
		fclose (fp);
		return false;
	}
	
	fgets (buf, sizeof (buf), fp);
	fgets (buf, sizeof (buf), fp);
	sscanf (buf, "%d %d", &logo_width, &logo_height);
	
	fgets (buf, sizeof (buf), fp);
	if (strcmp (buf, "255\n") != 0) {
		fclose (fp);
		return false;
	}

	Gtk::Preview preview (GTK_PREVIEW_COLOR);

	preview.size (logo_width, logo_height);
	pixelrow = new guchar[logo_width * 3];
	
	for (i = 0; i < logo_height; i++) {
		count = fread (pixelrow, sizeof (unsigned char), logo_width * 3, fp);
		if (count != (logo_width * 3))
		{
			delete [] pixelrow;
			fclose (fp);
			return false;
		}
		preview.draw_row (pixelrow, 0, i, logo_width);
	}
	
	window.realize ();

	logo_pixmap = gdk_pixmap_new (GTK_WIDGET(window.gobj())->window, logo_width, logo_height,
				      gtk_preview_get_visual()->depth);
	gc = gdk_gc_new (logo_pixmap);
	gtk_preview_put (preview.gobj(), logo_pixmap, gc, 0, 0, 0, 0, logo_width, logo_height);
	gdk_gc_destroy (gc);
	
	delete [] pixelrow;
	fclose (fp);
	
	return true;
}

gint
About::logo_area_expose (GdkEventExpose* ev)
{
	if (!drawn) {
		drawn = true;
	}

	if (logo_pixmap) {
		logo_area.get_window().draw_pixmap (logo_area.get_style()->get_black_gc(),
						    Gdk::Pixmap (logo_pixmap),
						    0, 0,
						    ((logo_area.width() - logo_width) / 2),
						    ((logo_area.height() - logo_height) / 2),
						    logo_width, logo_height);
		gdk_flush ();
	}

	return FALSE;
}

#ifdef WITH_PAYMENT_OPTIONS
void
About::goto_paypal ()
{
	char buf[PATH_MAX+16];
	char *argv[4];
	char *docfile = "foo";
	int grandchild;
	
	if (fork() == 0) {

		/* child */

		if ((grandchild = fork()) == 0) {
			
			/* grandchild */
			
			argv[0] = "mozilla";
			argv[1] = "-remote";
			snprintf (buf, sizeof(buf), "openurl(%s)", docfile);
			argv[2] = buf;
			argv[3] = 0;

			execvp ("mozilla", argv);
			error << "could not start mozilla" << endmsg;

		} else {
			int status;
			waitpid (grandchild, &status, 0);
		}

	} 
}
#endif
