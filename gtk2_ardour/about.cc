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
#include "ardour/version.h"
#include "ardour/filesystem_paths.h"

#include "utils.h"
#include "version.h"

#include "about.h"
#include "configinfo.h"
#include "rgb_macros.h"
#include "ardour_ui.h"

#include "i18n.h"

using namespace Gtk;
using namespace Gdk;
using namespace std;
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
	N_("André Colomb"),
	N_("Paul Davis"),
	N_("Gerard van Dongen"),
	N_("Colin Fletcher"),
	N_("Dave Flick"),
	N_("Hans Fugal"),
        N_("Robin Gareus"),
	N_("Christopher George"),
	N_("Chris Goddard"),
	N_("J. Abelardo Gutierrez"),
	N_("Jeremy Hall"),
	N_("Audun Halland"),
	N_("David Halter"),
	N_("Steve Harris"),
	N_("Melvin Ray Herr"),
	N_("Carl Hetherington"),
	N_("Rob Holland"),
	N_("Robert Jordens"),
	N_("Stefan Kersten"),
	N_("Armand Klenk"),
	N_("Matt Krai"),
	N_("Nick Lanham"),
	N_("Colin Law"),
	N_("Joshua Leach"),
	N_("Ben Loftis"),
	N_("Nick Mainsbridge"),
	N_("Tim Mayberry"),
	N_("Doug Mclain"),
	N_("Jack O'Quin"),
	N_("Nimal Ratnayake"),
	N_("David Robillard"),
	N_("Taybin Rutkin"),
	N_("Andreas Ruge"),
	N_("Sampo Savolainen"),
	N_("Rodrigo Severo"),
	N_("Per Sigmond"),
	N_("Lincoln Spiteri"),
	N_("Mike Start"),
	N_("Mark Stewart"),
	N_("Roland Stigge"),
	N_("Petter Sundlöf"),
	N_("Mike Täht"),
	N_("Roy Vegard"),
	N_("Thorsten Wilms"),
	0
};

static const char* translators[] = {
	N_("French:\n\tAlain Fréhel <alain.frehel@free.fr>\n\tChristophe Combelles <ccomb@free.fr>\n\tMartin Blanchard\n\tRomain Arnaud <roming22@gmail.com>\n"),
	N_("German:\n\tKarsten Petersen <kapet@kapet.de>\
\n\tSebastian Arnold <mail@sebastian-arnold.net>\
\n\tRobert Schwede <schwede@ironshark.com>\
\n\tBenjamin Scherrer <realhangman@web.de>\
\n\tEdgar Aichinger <edogawa@aon.at>\
\n\tRichard Oax <richard@pagliacciempire.de>\n"),
	N_("Italian:\n\tFilippo Pappalardo <filippo@email.it>\n\tRaffaele Morelli <raffaele.morelli@gmail.com>\n"),
	N_("Portuguese:\n\tRui Nuno Capela <rncbc@rncbc.org>\n"),
	N_("Brazilian Portuguese:\n\tAlexander da Franca Fernandes <alexander@nautae.eti.br>\
\n\tChris Ross <chris@tebibyte.org>\n"),
	N_("Spanish:\n\t Alex Krohn <alexkrohn@fastmail.fm>\n\tPablo Fernández <pablo.fbus@gmail.com>\n"),
	N_("Russian:\n\t Igor Blinov <pitstop@nm.ru>\
\n\tAlexandre Prokoudine <alexandre.prokoudine@gmail.com>\n"),
	N_("Greek:\n\t Klearchos Gourgourinis <muadib@in.gr>\n"),
	N_("Swedish:\n\t Petter Sundlöf <petter.sundlof@gmail.com>\n"),
	N_("Polish:\n\t Piotr Zaryk <pzaryk@gmail.com>\n"),
	N_("Czech:\n\t Pavel Fric <pavelfric@seznam.cz>\n"),
	N_("Norwegian:\n\t Eivind Ødegård\n"),
	N_("Chinese:\n\t Rui-huai Zhang <zrhzrh@mail.ustc.edu.cn>\n"),
	0
};

static const char* gpl = X_("\n\
Ardour comes with NO WARRANTY. It is free software, and you are welcome to redistribute it\n\
under the terms of the GNU General Public License, shown below.\n\
\n\
		    GNU GENERAL PUBLIC LICENSE\n\
		       Version 2, June 1991\n\
\n\
 Copyright (C) 1989, 1991 Free Software Foundation, Inc.\n\
     59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n\
 Everyone is permitted to copy and distribute verbatim copies\n\
 of this license document, but changing it is not allowed.\n\
\n\
			    Preamble\n\
\n\
  The licenses for most software are designed to take away your\n\
freedom to share and change it.  By contrast, the GNU General Public\n\
License is intended to guarantee your freedom to share and change free\n\
software--to make sure the software is free for all its users.  This\n\
General Public License applies to most of the Free Software\n\
Foundation's software and to any other program whose authors commit to\n\
using it.  (Some other Free Software Foundation software is covered by\n\
the GNU Library General Public License instead.)  You can apply it to\n\
your programs, too.\n\
\n\
  When we speak of free software, we are referring to freedom, not\n\
price.  Our General Public Licenses are designed to make sure that you\n\
have the freedom to distribute copies of free software (and charge for\n\
this service if you wish), that you receive source code or can get it\n\
if you want it, that you can change the software or use pieces of it\n\
in new free programs; and that you know you can do these things.\n\
\n\
  To protect your rights, we need to make restrictions that forbid\n\
anyone to deny you these rights or to ask you to surrender the rights.\n\
These restrictions translate to certain responsibilities for you if you\n\
distribute copies of the software, or if you modify it.\n\
\n\
  For example, if you distribute copies of such a program, whether\n\
gratis or for a fee, you must give the recipients all the rights that\n\
you have.  You must make sure that they, too, receive or can get the\n\
source code.  And you must show them these terms so they know their\n\
rights.\n\
\n\
  We protect your rights with two steps: (1) copyright the software, and\n\
(2) offer you this license which gives you legal permission to copy,\n\
distribute and/or modify the software.\n\
\n\
  Also, for each author's protection and ours, we want to make certain\n\
that everyone understands that there is no warranty for this free\n\
software.  If the software is modified by someone else and passed on, we\n\
want its recipients to know that what they have is not the original, so\n\
that any problems introduced by others will not reflect on the original\n\
authors' reputations.\n\
\n\
  Finally, any free program is threatened constantly by software\n\
patents.  We wish to avoid the danger that redistributors of a free\n\
program will individually obtain patent licenses, in effect making the\n\
program proprietary.  To prevent this, we have made it clear that any\n\
patent must be licensed for everyone's free use or not licensed at all.\n\
\n\
  The precise terms and conditions for copying, distribution and\n\
modification follow.\n\
\n\
		    GNU GENERAL PUBLIC LICENSE\n\
   TERMS AND CONDITIONS FOR COPYING, DISTRIBUTION AND MODIFICATION\n\
\n\
  0. This License applies to any program or other work which contains\n\
a notice placed by the copyright holder saying it may be distributed\n\
under the terms of this General Public License.  The \"Program\", below,\n\
refers to any such program or work, and a \"work based on the Program\"\n\
means either the Program or any derivative work under copyright law:\n\
that is to say, a work containing the Program or a portion of it,\n\
either verbatim or with modifications and/or translated into another\n\
language.  (Hereinafter, translation is included without limitation in\n\
the term \"modification\".)  Each licensee is addressed as \"you\".\n\
\n\
Activities other than copying, distribution and modification are not\n\
covered by this License; they are outside its scope.  The act of\n\
running the Program is not restricted, and the output from the Program\n\
is covered only if its contents constitute a work based on the\n\
Program (independent of having been made by running the Program).\n\
Whether that is true depends on what the Program does.\n\
\n\
  1. You may copy and distribute verbatim copies of the Program's\n\
source code as you receive it, in any medium, provided that you\n\
conspicuously and appropriately publish on each copy an appropriate\n\
copyright notice and disclaimer of warranty; keep intact all the\n\
notices that refer to this License and to the absence of any warranty;\n\
and give any other recipients of the Program a copy of this License\n\
along with the Program.\n\
\n\
You may charge a fee for the physical act of transferring a copy, and\n\
you may at your option offer warranty protection in exchange for a fee.\n\
\n\
  2. You may modify your copy or copies of the Program or any portion\n\
of it, thus forming a work based on the Program, and copy and\n\
distribute such modifications or work under the terms of Section 1\n\
above, provided that you also meet all of these conditions:\n\
\n\
    a) You must cause the modified files to carry prominent notices\n\
    stating that you changed the files and the date of any change.\n\
\n\
    b) You must cause any work that you distribute or publish, that in\n\
    whole or in part contains or is derived from the Program or any\n\
    part thereof, to be licensed as a whole at no charge to all third\n\
    parties under the terms of this License.\n\
\n\
    c) If the modified program normally reads commands interactively\n\
    when run, you must cause it, when started running for such\n\
    interactive use in the most ordinary way, to print or display an\n\
    announcement including an appropriate copyright notice and a\n\
    notice that there is no warranty (or else, saying that you provide\n\
    a warranty) and that users may redistribute the program under\n\
    these conditions, and telling the user how to view a copy of this\n\
    License.  (Exception: if the Program itself is interactive but\n\
    does not normally print such an announcement, your work based on\n\
    the Program is not required to print an announcement.)\n\
\n\
These requirements apply to the modified work as a whole.  If\n\
identifiable sections of that work are not derived from the Program,\n\
and can be reasonably considered independent and separate works in\n\
themselves, then this License, and its terms, do not apply to those\n\
sections when you distribute them as separate works.  But when you\n\
distribute the same sections as part of a whole which is a work based\n\
on the Program, the distribution of the whole must be on the terms of\n\
this License, whose permissions for other licensees extend to the\n\
entire whole, and thus to each and every part regardless of who wrote it.\n\
\n\
Thus, it is not the intent of this section to claim rights or contest\n\
your rights to work written entirely by you; rather, the intent is to\n\
exercise the right to control the distribution of derivative or\n\
collective works based on the Program.\n\
\n\
In addition, mere aggregation of another work not based on the Program\n\
with the Program (or with a work based on the Program) on a volume of\n\
a storage or distribution medium does not bring the other work under\n\
the scope of this License.\n\
\n\
  3. You may copy and distribute the Program (or a work based on it,\n\
under Section 2) in object code or executable form under the terms of\n\
Sections 1 and 2 above provided that you also do one of the following:\n\
\n\
    a) Accompany it with the complete corresponding machine-readable\n\
    source code, which must be distributed under the terms of Sections\n\
    1 and 2 above on a medium customarily used for software interchange; or,\n\
\n\
    b) Accompany it with a written offer, valid for at least three\n\
    years, to give any third party, for a charge no more than your\n\
    cost of physically performing source distribution, a complete\n\
    machine-readable copy of the corresponding source code, to be\n\
    distributed under the terms of Sections 1 and 2 above on a medium\n\
    customarily used for software interchange; or,\n\
\n\
    c) Accompany it with the information you received as to the offer\n\
    to distribute corresponding source code.  (This alternative is\n\
    allowed only for noncommercial distribution and only if you\n\
    received the program in object code or executable form with such\n\
    an offer, in accord with Subsection b above.)\n\
\n\
The source code for a work means the preferred form of the work for\n\
making modifications to it.  For an executable work, complete source\n\
code means all the source code for all modules it contains, plus any\n\
associated interface definition files, plus the scripts used to\n\
control compilation and installation of the executable.  However, as a\n\
special exception, the source code distributed need not include\n\
anything that is normally distributed (in either source or binary\n\
form) with the major components (compiler, kernel, and so on) of the\n\
operating system on which the executable runs, unless that component\n\
itself accompanies the executable.\n\
\n\
If distribution of executable or object code is made by offering\n\
access to copy from a designated place, then offering equivalent\n\
access to copy the source code from the same place counts as\n\
distribution of the source code, even though third parties are not\n\
compelled to copy the source along with the object code.\n\
\n\
  4. You may not copy, modify, sublicense, or distribute the Program\n\
except as expressly provided under this License.  Any attempt\n\
otherwise to copy, modify, sublicense or distribute the Program is\n\
void, and will automatically terminate your rights under this License.\n\
However, parties who have received copies, or rights, from you under\n\
this License will not have their licenses terminated so long as such\n\
parties remain in full compliance.\n\
\n\
  5. You are not required to accept this License, since you have not\n\
signed it.  However, nothing else grants you permission to modify or\n\
distribute the Program or its derivative works.  These actions are\n\
prohibited by law if you do not accept this License.  Therefore, by\n\
modifying or distributing the Program (or any work based on the\n\
Program), you indicate your acceptance of this License to do so, and\n\
all its terms and conditions for copying, distributing or modifying\n\
the Program or works based on it.\n\
\n\
  6. Each time you redistribute the Program (or any work based on the\n\
Program), the recipient automatically receives a license from the\n\
original licensor to copy, distribute or modify the Program subject to\n\
these terms and conditions.  You may not impose any further\n\
restrictions on the recipients' exercise of the rights granted herein.\n\
You are not responsible for enforcing compliance by third parties to\n\
this License.\n\
\n\
  7. If, as a consequence of a court judgment or allegation of patent\n\
infringement or for any other reason (not limited to patent issues),\n\
conditions are imposed on you (whether by court order, agreement or\n\
otherwise) that contradict the conditions of this License, they do not\n\
excuse you from the conditions of this License.  If you cannot\n\
distribute so as to satisfy simultaneously your obligations under this\n\
License and any other pertinent obligations, then as a consequence you\n\
may not distribute the Program at all.  For example, if a patent\n\
license would not permit royalty-free redistribution of the Program by\n\
all those who receive copies directly or indirectly through you, then\n\
the only way you could satisfy both it and this License would be to\n\
refrain entirely from distribution of the Program.\n\
\n\
If any portion of this section is held invalid or unenforceable under\n\
any particular circumstance, the balance of the section is intended to\n\
apply and the section as a whole is intended to apply in other\n\
circumstances.\n\
\n\
It is not the purpose of this section to induce you to infringe any\n\
patents or other property right claims or to contest validity of any\n\
such claims; this section has the sole purpose of protecting the\n\
integrity of the free software distribution system, which is\n\
implemented by public license practices.  Many people have made\n\
generous contributions to the wide range of software distributed\n\
through that system in reliance on consistent application of that\n\
system; it is up to the author/donor to decide if he or she is willing\n\
to distribute software through any other system and a licensee cannot\n\
impose that choice.\n\
\n\
This section is intended to make thoroughly clear what is believed to\n\
be a consequence of the rest of this License.\n\
\n\
  8. If the distribution and/or use of the Program is restricted in\n\
certain countries either by patents or by copyrighted interfaces, the\n\
original copyright holder who places the Program under this License\n\
may add an explicit geographical distribution limitation excluding\n\
those countries, so that distribution is permitted only in or among\n\
countries not thus excluded.  In such case, this License incorporates\n\
the limitation as if written in the body of this License.\n\
\n\
  9. The Free Software Foundation may publish revised and/or new versions\n\
of the General Public License from time to time.  Such new versions will\n\
be similar in spirit to the present version, but may differ in detail to\n\
address new problems or concerns.\n\
\n\
Each version is given a distinguishing version number.  If the Program\n\
specifies a version number of this License which applies to it and \"any\n\
later version\", you have the option of following the terms and conditions\n\
either of that version or of any later version published by the Free\n\
Software Foundation.  If the Program does not specify a version number of\n\
this License, you may choose any version ever published by the Free Software\n\
Foundation.\n\
\n\
  10. If you wish to incorporate parts of the Program into other free\n\
programs whose distribution conditions are different, write to the author\n\
to ask for permission.  For software which is copyrighted by the Free\n\
Software Foundation, write to the Free Software Foundation; we sometimes\n\
make exceptions for this.  Our decision will be guided by the two goals\n\
of preserving the free status of all derivatives of our free software and\n\
of promoting the sharing and reuse of software generally.\n\
\n\
			    NO WARRANTY\n\
\n\
  11. BECAUSE THE PROGRAM IS LICENSED FREE OF CHARGE, THERE IS NO WARRANTY\n\
FOR THE PROGRAM, TO THE EXTENT PERMITTED BY APPLICABLE LAW.  EXCEPT WHEN\n\
OTHERWISE STATED IN WRITING THE COPYRIGHT HOLDERS AND/OR OTHER PARTIES\n\
PROVIDE THE PROGRAM \"AS IS\" WITHOUT WARRANTY OF ANY KIND, EITHER EXPRESSED\n\
OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF\n\
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE ENTIRE RISK AS\n\
TO THE QUALITY AND PERFORMANCE OF THE PROGRAM IS WITH YOU.  SHOULD THE\n\
PROGRAM PROVE DEFECTIVE, YOU ASSUME THE COST OF ALL NECESSARY SERVICING,\n\
REPAIR OR CORRECTION.\n\
\n\
  12. IN NO EVENT UNLESS REQUIRED BY APPLICABLE LAW OR AGREED TO IN WRITING\n\
WILL ANY COPYRIGHT HOLDER, OR ANY OTHER PARTY WHO MAY MODIFY AND/OR\n\
REDISTRIBUTE THE PROGRAM AS PERMITTED ABOVE, BE LIABLE TO YOU FOR DAMAGES,\n\
INCLUDING ANY GENERAL, SPECIAL, INCIDENTAL OR CONSEQUENTIAL DAMAGES ARISING\n\
OUT OF THE USE OR INABILITY TO USE THE PROGRAM (INCLUDING BUT NOT LIMITED\n\
TO LOSS OF DATA OR DATA BEING RENDERED INACCURATE OR LOSSES SUSTAINED BY\n\
YOU OR THIRD PARTIES OR A FAILURE OF THE PROGRAM TO OPERATE WITH ANY OTHER\n\
PROGRAMS), EVEN IF SUCH HOLDER OR OTHER PARTY HAS BEEN ADVISED OF THE\n\
POSSIBILITY OF SUCH DAMAGES.\n\
\n\
		     END OF TERMS AND CONDITIONS\n\
\n\
	    How to Apply These Terms to Your New Programs\n\
\n\
  If you develop a new program, and you want it to be of the greatest\n\
possible use to the public, the best way to achieve this is to make it\n\
free software which everyone can redistribute and change under these terms.\n\
\n\
  To do so, attach the following notices to the program.  It is safest\n\
to attach them to the start of each source file to most effectively\n\
convey the exclusion of warranty; and each file should have at least\n\
the \"copyright\" line and a pointer to where the full notice is found.\n\
\n\
    <one line to give the program's name and a brief idea of what it does.>\n\
    Copyright (C) <year>  <name of author>\n\
\n\
    This program is free software; you can redistribute it and/or modify\n\
    it under the terms of the GNU General Public License as published by\n\
    the Free Software Foundation; either version 2 of the License, or\n\
    (at your option) any later version.\n\
\n\
    This program is distributed in the hope that it will be useful,\n\
    but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
    GNU General Public License for more details.\n\
\n\
    You should have received a copy of the GNU General Public License\n\
    along with this program; if not, write to the Free Software\n\
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA\n\
\n\
\n\
Also add information on how to contact you by electronic and paper mail.\n\
\n\
If the program is interactive, make it output a short notice like this\n\
when it starts in an interactive mode:\n\
\n\
    Gnomovision version 69, Copyright (C) year  name of author\n\
    Gnomovision comes with ABSOLUTELY NO WARRANTY; for details type `show w'.\n\
    This is free software, and you are welcome to redistribute it\n\
    under certain conditions; type `show c' for details.\n\
\n\
The hypothetical commands `show w' and `show c' should show the appropriate\n\
parts of the General Public License.  Of course, the commands you use may\n\
be called something other than `show w' and `show c'; they could even be\n\
mouse-clicks or menu items--whatever suits your program.\n\
\n\
You should also get your employer (if you work as a programmer) or your\n\
school, if any, to sign a \"copyright disclaimer\" for the program, if\n\
necessary.  Here is a sample; alter the names:\n\
\n\
  Yoyodyne, Inc., hereby disclaims all copyright interest in the program\n\
  `Gnomovision' (which makes passes at compilers) written by James Hacker.\n\
\n\
  <signature of Ty Coon>, 1 April 1989\n\
  Ty Coon, President of Vice\n\
\n\
This General Public License does not permit incorporating your program into\n\
proprietary programs.  If your program is a subroutine library, you may\n\
consider it more useful to permit linking proprietary applications with the\n\
library.  If this is what you want to do, use the GNU Library General\n\
Public License instead of this License.\n\
");

About::About ()
	: config_info (0)
#ifdef WITH_PAYMENT_OPTIONS
	, paypal_pixmap (paypal_xpm)
#endif
{
	// set_type_hint(Gdk::WINDOW_TYPE_HINT_SPLASHSCREEN);

	string t;

	std::string splash_file;

	SearchPath spath(ardour_data_search_path());

	if (find_file_in_search_path (spath, "splash.png", splash_file)) {
		set_logo (Gdk::Pixbuf::create_from_file (splash_file));
	} else {
		error << "Could not find splash file" << endmsg;
	}

	set_authors (authors);

	for (int n = 0; translators[n]; ++n) {
		t += translators[n];
		t += ' ';
	}

	set_translator_credits (t);
	set_copyright (_("Copyright (C) 1999-2012 Paul Davis\n"));
	set_license (gpl);
	set_name (X_("Ardour"));
	set_website (X_("http://ardour.org/"));
	set_website_label (_("http://ardour.org/"));
	set_version ((string_compose(_("%1\n(built from revision %2)"),
				     VERSIONSTRING,
				     svn_revision)));

	Gtk::Button* config_button = manage (new Button (_("Config")));

	get_action_area()->add (*config_button);
	get_action_area()->reorder_child (*config_button, 0);
	config_button->signal_clicked().connect (mem_fun (*this, &About::show_config_info));
}

About::~About ()
{
	delete config_info;
}

void
About::show_config_info ()
{
	if (!config_info) {
		config_info = new ConfigInfoDialog;
	}

	config_info->run ();
	config_info->hide ();
}

