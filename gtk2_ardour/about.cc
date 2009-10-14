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

*/

#include <algorithm>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <cstdio>
#include <ctime>
#include <cstdlib>

#include "pbd/file_utils.h"

#include "ardour/svn_revision.h"
#include "ardour/ardour.h"
#include "ardour/version.h"
#include "ardour/filesystem_paths.h"

#include "utils.h"
#include "version.h"

#include "about.h"
#include "rgb_macros.h"
#include "ardour_ui.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gdk;
using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;

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

static const char* authors[] = {
	N_("Brian Ahr"),
	N_("John Anderson"),
	N_("Marcus Andersson"),
	N_("Nedko Arnaudov"),
	N_("Hans Baier"),
	N_("Ben Bell"),
	N_("Sakari Bergen"),
	N_("Chris Cannam"),
	N_("Jesse Chappell"),
	N_("Thomas Charbonnel"),
	N_("Sam Chessman"),
	N_("Paul Davis"),
	N_("Gerard van Dongen"),
	N_("Colin Fletcher"),
	N_("Hans Fugal"),
	N_("Christopher George"),
	N_("Jeremy Hall"),
	N_("Audun Halland"),
	N_("Steve Harris"),
	N_("Carl Hetherington"),
	N_("Rob Holland"),
	N_("Robert Jordens"),
	N_("Stefan Kersten"),
	N_("Armand Klenk"),
	N_("Matt Krai"),
	N_("Colin Law"),
	N_("Joshua Leach"),
	N_("Ben Loftis"),
	N_("Nick Mainsbridge"),
	N_("Tim Mayberry"),
	N_("Doug Mclain"),
	N_("Jack O'Quin"),
	N_("Nimal Ratnayake"),
	N_("Dave Robillard"),
	N_("Taybin Rutkin"),
	N_("Sampo Savolainen"),
	N_("Per Sigmond"),
	N_("Lincoln Spiteri"),
	N_("Mark Stewart"),
	N_("Roland Stigge"),
	N_("Petter Sundlöf"),
	N_("Mike Täht"),
	N_("Thorsten Wilms"),
	0
};

static const char* translators[] = {
	N_("French:\n\tAlain Fréhel <alain.frehel@free.fr>\n\tChristophe Combelles <ccomb@free.fr>\n"),
	N_("German:\n\tKarsten Petersen <kapet@kapet.de>\
\n\tSebastian Arnold <mail@sebastian-arnold.net>\
\n\tRobert Schwede<schwede@ironshark.com>\n"),
	N_("Italian:\n\tFilippo Pappalardo <filippo@email.it>\n"),
	N_("Portuguese:\n\tRui Nuno Capela <rncbc@rncbc.org>\n"),
	N_("Brazilian Portuguese:\n\tAlexander da Franca Fernandes <alexander@nautae.eti.br>\
\n\tChris Ross <chris@tebibyte.org>\n"),
	N_("Spanish:\n\t Alex Krohn <alexkrohn@fastmail.fm>\n"),
	N_("Russian:\n\t Igor Blinov <pitstop@nm.ru>\n"),
	N_("Greek:\n\t Klearchos Gourgourinis <muadib@in.gr>\n"),
	N_("Swedish:\n\t Petter Sundlöf <petter.sundlof@gmail.com>\n"),
	N_("Polish:\n\t Piotr Zaryk <pzaryk@gmail.com>\n"),
	0
};


About::About ()
#ifdef WITH_PAYMENT_OPTIONS
	: paypal_pixmap (paypal_xpm)
#endif
{
	// set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);

	string path;
	string t;

	sys::path splash_file;

	SearchPath spath(ardour_search_path() + system_data_search_path());

	if (find_file_in_search_path (spath, "splash.png", splash_file)) {
		set_logo (Gdk::Pixbuf::create_from_file (splash_file.to_string()));
	} else {
		error << "Could not find splash file" << endmsg;
	}

	set_authors (authors);

	for (int n = 0; translators[n]; ++n) {
		t += translators[n];
		t += ' ';
	}

	set_translator_credits (t);
	set_copyright (_("Copyright (C) 1999-2009 Paul Davis\n"));
	set_license (_("Ardour comes with ABSOLUTELY NO WARRANTY\n"
		       "This is free software, and you are welcome to redistribute it\n"
		       "under certain conditions; see the file COPYING for details.\n"));
	set_name (X_("ardour"));
	set_website (X_("http://ardour.org/"));
	set_website_label (_("visit http://www.ardour.org/"));
	set_version ((string_compose(_("%1\n(built from revision %2)"),
				     VERSIONSTRING,
				     svn_revision)));


#ifdef WITH_PAYMENT_OPTIONS
	paypal_button.add (paypal_pixmap);

	HBox *payment_box = manage (new HBox);
	payment_box->pack_start (paypal_button, true, false);

	subvbox.pack_start (*payment_box, false, false);
#endif

}

About::~About ()
{
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
