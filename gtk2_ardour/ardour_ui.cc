/*
    Copyright (C) 1999-2002 Paul Davis 

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
#include <cmath>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>
#include <fstream>

#include <iostream>

#include <gtkmm.h>
#include <pbd/error.h>
#include <pbd/basename.h>
#include <pbd/pathscanner.h>
#include <pbd/failed_constructor.h>
#include <gtkmm2ext/gtk_ui.h>
#include <gtkmm2ext/pix.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/click_box.h>
#include <gtkmm2ext/selector.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/stop_signal.h>
#include <gtkmm2ext/popup.h>

#include <midi++/port.h>
#include <midi++/mmc.h>

#include <ardour/ardour.h>
#include <ardour/port.h>
#include <ardour/audioengine.h>
#include <ardour/playlist.h>
#include <ardour/utils.h>
#include <ardour/diskstream.h>
#include <ardour/filesource.h>
#include <ardour/recent_sessions.h>
#include <ardour/session_diskstream.h>
#include <ardour/port.h>
#include <ardour/audio_track.h>

#include "ardour_ui.h"
#include "ardour_message.h"
#include "public_editor.h"
#include "audio_clock.h"
#include "keyboard.h"
#include "mixer_ui.h"
#include "prompter.h"
#include "opts.h"
#include "keyboard_target.h"
#include "add_route_dialog.h"
#include "new_session_dialog.h"
#include "about.h"
#include "utils.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtkmm2ext;
using namespace Gtk;
using namespace sigc;

ARDOUR_UI *ARDOUR_UI::theArdourUI = 0;
SoundFileSelector* ARDOUR_UI::sfdb_window = 0;

sigc::signal<void,bool> ARDOUR_UI::Blink;
sigc::signal<void>      ARDOUR_UI::RapidScreenUpdate;
sigc::signal<void>      ARDOUR_UI::SuperRapidScreenUpdate;
sigc::signal<void,jack_nframes_t> ARDOUR_UI::Clock;

/* XPM */
static const gchar *h_meter_strip_xpm[] = {
"186 5 187 2",
"  	c None",
". 	c #2BFE00",
"+ 	c #2DFE00",
"@ 	c #2FFE01",
"# 	c #32FE01",
"$ 	c #34FE02",
"% 	c #36FE02",
"& 	c #38FE03",
"* 	c #3BFE03",
"= 	c #3DFD04",
"- 	c #3FFD04",
"; 	c #41FD05",
"> 	c #44FD05",
", 	c #46FD06",
"' 	c #48FD06",
") 	c #4AFD07",
"! 	c #4DFD07",
"~ 	c #4FFD08",
"{ 	c #51FC08",
"] 	c #53FC09",
"^ 	c #56FC09",
"/ 	c #58FC09",
"( 	c #5AFC0A",
"_ 	c #5CFC0A",
": 	c #5FFC0B",
"< 	c #61FC0B",
"[ 	c #63FB0C",
"} 	c #65FB0C",
"| 	c #68FB0D",
"1 	c #6AFB0D",
"2 	c #6CFB0E",
"3 	c #6EFB0E",
"4 	c #71FB0F",
"5 	c #73FB0F",
"6 	c #75FB10",
"7 	c #77FA10",
"8 	c #7AFA11",
"9 	c #7CFA11",
"0 	c #7EFA12",
"a 	c #80FA12",
"b 	c #83FA12",
"c 	c #85FA13",
"d 	c #87FA13",
"e 	c #89FA14",
"f 	c #8CF914",
"g 	c #8EF915",
"h 	c #90F915",
"i 	c #92F916",
"j 	c #95F916",
"k 	c #97F917",
"l 	c #99F917",
"m 	c #9BF918",
"n 	c #9EF818",
"o 	c #A0F819",
"p 	c #A2F819",
"q 	c #A4F81A",
"r 	c #A7F81A",
"s 	c #A9F81A",
"t 	c #ABF81B",
"u 	c #ADF81B",
"v 	c #B0F81C",
"w 	c #B2F71C",
"x 	c #B4F71D",
"y 	c #B6F71D",
"z 	c #B9F71E",
"A 	c #BBF71E",
"B 	c #BDF71F",
"C 	c #BFF71F",
"D 	c #C2F720",
"E 	c #C4F720",
"F 	c #C6F621",
"G 	c #C8F621",
"H 	c #CBF622",
"I 	c #CDF622",
"J 	c #CFF623",
"K 	c #D1F623",
"L 	c #D4F624",
"M 	c #D6F624",
"N 	c #D8F524",
"O 	c #DAF525",
"P 	c #DDF525",
"Q 	c #DFF526",
"R 	c #E1F526",
"S 	c #E3F527",
"T 	c #E6F527",
"U 	c #E8F528",
"V 	c #EAF528",
"W 	c #ECF429",
"X 	c #EFF429",
"Y 	c #F1F42A",
"Z 	c #F3F42A",
"` 	c #F5F42B",
" .	c #F8F42B",
"..	c #FAF42C",
"+.	c #FCF42C",
"@.	c #FFF42D",
"#.	c #FFF22C",
"$.	c #FFF12B",
"%.	c #FFF02A",
"&.	c #FFEF2A",
"*.	c #FFEE29",
"=.	c #FFED28",
"-.	c #FFEC28",
";.	c #FFEB27",
">.	c #FFE926",
",.	c #FFE826",
"'.	c #FFE725",
").	c #FFE624",
"!.	c #FFE524",
"~.	c #FFE423",
"{.	c #FFE322",
"].	c #FFE222",
"^.	c #FFE021",
"/.	c #FFDF20",
"(.	c #FFDE20",
"_.	c #FFDD1F",
":.	c #FFDC1E",
"<.	c #FFDB1E",
"[.	c #FFDA1D",
"}.	c #FFD91C",
"|.	c #FFD71B",
"1.	c #FFD61B",
"2.	c #FFD51A",
"3.	c #FFD419",
"4.	c #FFD319",
"5.	c #FFD218",
"6.	c #FFD117",
"7.	c #FFD017",
"8.	c #FFCF16",
"9.	c #FFCD15",
"0.	c #FFCC15",
"a.	c #FFCB14",
"b.	c #FFCA13",
"c.	c #FFC913",
"d.	c #FFC812",
"e.	c #FFC711",
"f.	c #FFC611",
"g.	c #FFC410",
"h.	c #FFC30F",
"i.	c #FFC20F",
"j.	c #FFC10E",
"k.	c #FFC00D",
"l.	c #FFBF0C",
"m.	c #FFBE0C",
"n.	c #FFBD0B",
"o.	c #FFBB0A",
"p.	c #FFBA0A",
"q.	c #FFB909",
"r.	c #FFB808",
"s.	c #FFB708",
"t.	c #FFB607",
"u.	c #FFB506",
"v.	c #FFB406",
"w.	c #FFB205",
"x.	c #FFB104",
"y.	c #FFB004",
"z.	c #FFAF03",
"A.	c #FFAE02",
"B.	c #FFAD02",
"C.	c #FFAC01",
"D.	c #FFAB00",
"E.	c #FFA900",
"F.	c #F11F00",
"G.	c #F21E00",
"H.	c #F21C00",
"I.	c #F31B00",
"J.	c #F31A00",
"K.	c #F41800",
"L.	c #F41700",
"M.	c #F51600",
"N.	c #F61400",
"O.	c #F61300",
"P.	c #F71100",
"Q.	c #F71000",
"R.	c #F80F00",
"S.	c #F90D00",
"T.	c #F90C00",
"U.	c #FA0B00",
"V.	c #FA0900",
"W.	c #FB0800",
"X.	c #FC0600",
"Y.	c #FC0500",
"Z.	c #FD0400",
"`.	c #FD0200",
" +	c #FE0100",
".+	c #FE0000",
"++	c #FF0000",
". + @ # $ % & * = - ; > , ' ) ! ~ { ] ^ / ( _ : < [ } | 1 2 3 4 5 6 7 8 9 0 a b c d e f g h i j k l m n o p q r s t u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z `  ...+.@.@.#.$.%.&.*.=.-.;.>.,.'.).!.~.{.].^./.(._.:.<.[.}.|.1.2.3.4.5.6.7.8.9.0.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.A.B.C.D.E.F.G.H.I.J.K.L.M.N.O.P.Q.R.S.T.U.V.W.X.Y.Z.`. +.+",
". + @ # $ % & * = - ; > , ' ) ! ~ { ] ^ / ( _ : < [ } | 1 2 3 4 5 6 7 8 9 0 a b c d e f g h i j k l m n o p q r s t u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z `  ...+.@.@.#.$.%.&.*.=.-.;.>.,.'.).!.~.{.].^./.(._.:.<.[.}.|.1.2.3.4.5.6.7.8.9.0.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.A.B.C.D.E.F.G.H.I.J.K.L.M.N.O.P.Q.R.S.T.U.V.W.X.Y.Z.`. +.+",
". + @ # $ % & * = - ; > , ' ) ! ~ { ] ^ / ( _ : < [ } | 1 2 3 4 5 6 7 8 9 0 a b c d e f g h i j k l m n o p q r s t u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z `  ...+.@.@.#.$.%.&.*.=.-.;.>.,.'.).!.~.{.].^./.(._.:.<.[.}.|.1.2.3.4.5.6.7.8.9.0.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.A.B.C.D.E.F.G.H.I.J.K.L.M.N.O.P.Q.R.S.T.U.V.W.X.Y.Z.`. +++",
". + @ # $ % & * = - ; > , ' ) ! ~ { ] ^ / ( _ : < [ } | 1 2 3 4 5 6 7 8 9 0 a b c d e f g h i j k l m n o p q r s t u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z `  ...+.@.@.#.$.%.&.*.=.-.;.>.,.'.).!.~.{.].^./.(._.:.<.[.}.|.1.2.3.4.5.6.7.8.9.0.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.A.B.C.D.E.F.G.H.I.J.K.L.M.N.O.P.Q.R.S.T.U.V.W.X.Y.Z.`. +++",
". + @ # $ % & * = - ; > , ' ) ! ~ { ] ^ / ( _ : < [ } | 1 2 3 4 5 6 7 8 9 0 a b c d e f g h i j k l m n o p q r s t u v w x y z A B C D E F G H I J K L M N O P Q R S T U V W X Y Z `  ...+.@.@.#.$.%.&.*.=.-.;.>.,.'.).!.~.{.].^./.(._.:.<.[.}.|.1.2.3.4.5.6.7.8.9.0.a.b.c.d.e.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z.A.B.C.D.E.F.G.H.I.J.K.L.M.N.O.P.Q.R.S.T.U.V.W.X.Y.Z.`. +++"};

/* XPM */
static const gchar * v_meter_strip_xpm[] = {
"5 250 230 2",
"  	c None",
". 	c #FE0000",
"+ 	c #FF0000",
"@ 	c #FE0100",
"# 	c #FD0200",
"$ 	c #FD0300",
"% 	c #FD0400",
"& 	c #FC0500",
"* 	c #FC0600",
"= 	c #FC0700",
"- 	c #FB0800",
"; 	c #FA0900",
"> 	c #FA0A00",
", 	c #FA0B00",
"' 	c #F90C00",
") 	c #F90D00",
"! 	c #F80E00",
"~ 	c #F80F00",
"{ 	c #F71000",
"] 	c #F71100",
"^ 	c #F61200",
"/ 	c #F61300",
"( 	c #F61400",
"_ 	c #F51600",
": 	c #F41700",
"< 	c #F41800",
"[ 	c #F31A00",
"} 	c #F31B00",
"| 	c #F21C00",
"1 	c #F21E00",
"2 	c #F11F00",
"3 	c #F54A00",
"4 	c #FFA900",
"5 	c #FFAB00",
"6 	c #FFAC01",
"7 	c #FFAD02",
"8 	c #FFAE02",
"9 	c #FFAF03",
"0 	c #FFB004",
"a 	c #FFB104",
"b 	c #FFB205",
"c 	c #FFB406",
"d 	c #FFB506",
"e 	c #FFB607",
"f 	c #FFB708",
"g 	c #FFB808",
"h 	c #FFB909",
"i 	c #FFBA0A",
"j 	c #FFBB0A",
"k 	c #FFBC0A",
"l 	c #FFBD0B",
"m 	c #FFBE0C",
"n 	c #FFBF0C",
"o 	c #FFC00D",
"p 	c #FFC10E",
"q 	c #FFC20F",
"r 	c #FFC30F",
"s 	c #FFC410",
"t 	c #FFC511",
"u 	c #FFC611",
"v 	c #FFC711",
"w 	c #FFC812",
"x 	c #FFC913",
"y 	c #FFCA13",
"z 	c #FFCB14",
"A 	c #FFCC15",
"B 	c #FFCD15",
"C 	c #FFCF16",
"D 	c #FFD017",
"E 	c #FFD117",
"F 	c #FFD218",
"G 	c #FFD319",
"H 	c #FFD419",
"I 	c #FFD51A",
"J 	c #FFD61B",
"K 	c #FFD71B",
"L 	c #FFD81C",
"M 	c #FFD91C",
"N 	c #FFDA1D",
"O 	c #FFDB1E",
"P 	c #FFDC1E",
"Q 	c #FFDD1F",
"R 	c #FFDE20",
"S 	c #FFDF20",
"T 	c #FFE021",
"U 	c #FFE222",
"V 	c #FFE322",
"W 	c #FFE423",
"X 	c #FFE524",
"Y 	c #FFE624",
"Z 	c #FFE725",
"` 	c #FFE826",
" .	c #FFE926",
"..	c #FFEA26",
"+.	c #FFEB27",
"@.	c #FFEC28",
"#.	c #FFED28",
"$.	c #FFEE29",
"%.	c #FFEF2A",
"&.	c #FFF02A",
"*.	c #FFF12B",
"=.	c #FFF22C",
"-.	c #FFF32D",
";.	c #FFF42D",
">.	c #FDF42C",
",.	c #FBF42C",
"'.	c #FAF42C",
").	c #F8F42B",
"!.	c #F6F42B",
"~.	c #F4F42B",
"{.	c #F3F42A",
"].	c #F1F42A",
"^.	c #F0F429",
"/.	c #EEF429",
"(.	c #ECF429",
"_.	c #EAF528",
":.	c #E9F528",
"<.	c #E7F528",
"[.	c #E5F527",
"}.	c #E3F527",
"|.	c #E2F526",
"1.	c #E0F526",
"2.	c #DFF526",
"3.	c #DDF525",
"4.	c #DBF525",
"5.	c #D9F525",
"6.	c #D8F524",
"7.	c #D6F624",
"8.	c #D5F624",
"9.	c #D3F624",
"0.	c #D1F623",
"a.	c #CFF623",
"b.	c #CEF622",
"c.	c #CCF622",
"d.	c #CBF622",
"e.	c #C9F621",
"f.	c #C7F621",
"g.	c #C5F621",
"h.	c #C4F720",
"i.	c #C2F720",
"j.	c #C0F71F",
"k.	c #BEF71F",
"l.	c #BDF71F",
"m.	c #BBF71E",
"n.	c #BAF71E",
"o.	c #B8F71E",
"p.	c #B6F71D",
"q.	c #B5F71D",
"r.	c #B3F71D",
"s.	c #B2F71C",
"t.	c #B0F81C",
"u.	c #AEF81B",
"v.	c #ACF81B",
"w.	c #ABF81B",
"x.	c #A9F81A",
"y.	c #A8F81A",
"z.	c #A6F81A",
"A.	c #A4F81A",
"B.	c #A2F819",
"C.	c #A1F819",
"D.	c #9FF819",
"E.	c #9EF818",
"F.	c #9BF918",
"G.	c #9AF917",
"H.	c #98F917",
"I.	c #97F917",
"J.	c #95F916",
"K.	c #93F916",
"L.	c #91F916",
"M.	c #90F915",
"N.	c #8EF915",
"O.	c #8DF914",
"P.	c #8BF914",
"Q.	c #89FA14",
"R.	c #87FA13",
"S.	c #86FA13",
"T.	c #84FA13",
"U.	c #83FA12",
"V.	c #81FA12",
"W.	c #7FFA12",
"X.	c #7DFA12",
"Y.	c #7CFA11",
"Z.	c #7AFA11",
"`.	c #78FA10",
" +	c #76FA10",
".+	c #75FB10",
"++	c #73FB0F",
"@+	c #72FB0F",
"#+	c #70FB0F",
"$+	c #6EFB0E",
"%+	c #6DFB0E",
"&+	c #6BFB0E",
"*+	c #6AFB0D",
"=+	c #68FB0D",
"-+	c #66FB0C",
";+	c #64FB0C",
">+	c #63FB0C",
",+	c #61FC0B",
"'+	c #60FC0B",
")+	c #5EFC0B",
"!+	c #5CFC0A",
"~+	c #5AFC0A",
"{+	c #59FC09",
"]+	c #57FC09",
"^+	c #56FC09",
"/+	c #53FC09",
"(+	c #52FC08",
"_+	c #50FC08",
":+	c #4FFD08",
"<+	c #4DFD07",
"[+	c #4BFD07",
"}+	c #49FD07",
"|+	c #48FD06",
"1+	c #46FD06",
"2+	c #45FD05",
"3+	c #43FD05",
"4+	c #41FD05",
"5+	c #3FFD04",
"6+	c #3EFD04",
"7+	c #3CFD04",
"8+	c #3BFE03",
"9+	c #39FE03",
"0+	c #37FE02",
"a+	c #35FE02",
"b+	c #34FE02",
"c+	c #32FE01",
"d+	c #30FE01",
"e+	c #2EFE01",
"f+	c #2DFE00",
"g+	c #2BFE00",
". . + + + ",
". . + + + ",
"@ @ @ @ @ ",
"# # # # # ",
"$ $ $ $ $ ",
"% % % % % ",
"& & & & & ",
"* * * * * ",
"= = = = = ",
"- - - - - ",
"; ; ; ; ; ",
"> > > > > ",
", , , , , ",
"' ' ' ' ' ",
") ) ) ) ) ",
"! ! ! ! ! ",
"~ ~ ~ ~ ~ ",
"{ { { { { ",
"] ] ] ] ] ",
"^ ^ ^ ^ ^ ",
"/ / / / / ",
"( ( ( ( ( ",
"_ _ _ _ _ ",
": : : : : ",
": : : : : ",
"< < < < < ",
"[ [ [ [ [ ",
"} } } } } ",
"} } } } } ",
"| | | | | ",
"1 1 1 1 1 ",
"2 2 2 2 2 ",
"3 3 3 3 3 ",
"4 4 4 4 4 ",
"5 5 5 5 5 ",
"6 6 6 6 6 ",
"6 6 6 6 6 ",
"7 7 7 7 7 ",
"8 8 8 8 8 ",
"9 9 9 9 9 ",
"9 9 9 9 9 ",
"0 0 0 0 0 ",
"a a a a a ",
"a a a a a ",
"b b b b b ",
"c c c c c ",
"d d d d d ",
"d d d d d ",
"e e e e e ",
"f f f f f ",
"g g g g g ",
"g g g g g ",
"h h h h h ",
"i i i i i ",
"j j j j j ",
"k k k k k ",
"l l l l l ",
"m m m m m ",
"n n n n n ",
"n n n n n ",
"o o o o o ",
"p p p p p ",
"q q q q q ",
"q q q q q ",
"r r r r r ",
"s s s s s ",
"t t t t t ",
"u u u u u ",
"v v v v v ",
"w w w w w ",
"x x x x x ",
"x x x x x ",
"y y y y y ",
"z z z z z ",
"A A A A A ",
"A A A A A ",
"B B B B B ",
"C C C C C ",
"D D D D D ",
"D D D D D ",
"E E E E E ",
"F F F F F ",
"G G G G G ",
"G G G G G ",
"H H H H H ",
"I I I I I ",
"I I I I I ",
"J J J J J ",
"K K K K K ",
"L L L L L ",
"M M M M M ",
"N N N N N ",
"O O O O O ",
"P P P P P ",
"P P P P P ",
"Q Q Q Q Q ",
"R R R R R ",
"S S S S S ",
"S S S S S ",
"T T T T T ",
"U U U U U ",
"V V V V V ",
"V V V V V ",
"W W W W W ",
"X X X X X ",
"Y Y Y Y Y ",
"Y Y Y Y Y ",
"Z Z Z Z Z ",
"` ` ` ` ` ",
" . . . . .",
"..........",
"+.+.+.+.+.",
"@.@.@.@.@.",
"#.#.#.#.#.",
"#.#.#.#.#.",
"$.$.$.$.$.",
"%.%.%.%.%.",
"&.&.&.&.&.",
"&.&.&.&.&.",
"*.*.*.*.*.",
"=.=.=.=.=.",
"-.-.-.-.-.",
";.;.;.;.;.",
";.;.;.;.;.",
">.>.>.>.>.",
",.,.,.,.,.",
"'.'.'.'.'.",
").).).).).",
"!.!.!.!.!.",
"~.~.~.~.~.",
"{.{.{.{.{.",
"].].].].].",
"^.^.^.^.^.",
"/././././.",
"(.(.(.(.(.",
"_._._._._.",
":.:.:.:.:.",
"<.<.<.<.<.",
"[.[.[.[.[.",
"}.}.}.}.}.",
"|.|.|.|.|.",
"1.1.1.1.1.",
"2.2.2.2.2.",
"3.3.3.3.3.",
"4.4.4.4.4.",
"5.5.5.5.5.",
"6.6.6.6.6.",
"7.7.7.7.7.",
"8.8.8.8.8.",
"9.9.9.9.9.",
"0.0.0.0.0.",
"a.a.a.a.a.",
"b.b.b.b.b.",
"c.c.c.c.c.",
"d.d.d.d.d.",
"e.e.e.e.e.",
"f.f.f.f.f.",
"g.g.g.g.g.",
"h.h.h.h.h.",
"i.i.i.i.i.",
"j.j.j.j.j.",
"k.k.k.k.k.",
"l.l.l.l.l.",
"m.m.m.m.m.",
"n.n.n.n.n.",
"o.o.o.o.o.",
"p.p.p.p.p.",
"q.q.q.q.q.",
"r.r.r.r.r.",
"s.s.s.s.s.",
"t.t.t.t.t.",
"u.u.u.u.u.",
"v.v.v.v.v.",
"w.w.w.w.w.",
"x.x.x.x.x.",
"y.y.y.y.y.",
"z.z.z.z.z.",
"A.A.A.A.A.",
"B.B.B.B.B.",
"C.C.C.C.C.",
"D.D.D.D.D.",
"E.E.E.E.E.",
"F.F.F.F.F.",
"G.G.G.G.G.",
"H.H.H.H.H.",
"I.I.I.I.I.",
"J.J.J.J.J.",
"K.K.K.K.K.",
"L.L.L.L.L.",
"M.M.M.M.M.",
"N.N.N.N.N.",
"O.O.O.O.O.",
"P.P.P.P.P.",
"Q.Q.Q.Q.Q.",
"R.R.R.R.R.",
"S.S.S.S.S.",
"T.T.T.T.T.",
"U.U.U.U.U.",
"V.V.V.V.V.",
"W.W.W.W.W.",
"X.X.X.X.X.",
"Y.Y.Y.Y.Y.",
"Z.Z.Z.Z.Z.",
"`.`.`.`.`.",
" + + + + +",
".+.+.+.+.+",
"++++++++++",
"@+@+@+@+@+",
"#+#+#+#+#+",
"$+$+$+$+$+",
"%+%+%+%+%+",
"&+&+&+&+&+",
"*+*+*+*+*+",
"=+=+=+=+=+",
"-+-+-+-+-+",
";+;+;+;+;+",
">+>+>+>+>+",
",+,+,+,+,+",
"'+'+'+'+'+",
")+)+)+)+)+",
"!+!+!+!+!+",
"~+~+~+~+~+",
"{+{+{+{+{+",
"]+]+]+]+]+",
"^+^+^+^+^+",
"/+/+/+/+/+",
"(+(+(+(+(+",
"_+_+_+_+_+",
":+:+:+:+:+",
"<+<+<+<+<+",
"[+[+[+[+[+",
"}+}+}+}+}+",
"|+|+|+|+|+",
"1+1+1+1+1+",
"2+2+2+2+2+",
"3+3+3+3+3+",
"4+4+4+4+4+",
"5+5+5+5+5+",
"6+6+6+6+6+",
"7+7+7+7+7+",
"8+8+8+8+8+",
"9+9+9+9+9+",
"0+0+0+0+0+",
"a+a+a+a+a+",
"b+b+b+b+b+",
"c+c+c+c+c+",
"d+d+d+d+d+",
"e+e+e+e+e+",
"f+f+f+f+f+",
"g+g+g+g+g+"};

static const char* channel_setup_names[] = {
	"mono",
	"stereo",
	"3 channels",
	"4 channels",
	"5 channels",
	"8 channels",
	"manual setup",
	0
};

vector<string> channel_combo_strings;

ARDOUR_UI::ARDOUR_UI (int *argcp, char **argvp[], string rcfile)

	: Gtkmm2ext::UI ("ardour", argcp, argvp, rcfile),

	  primary_clock (X_("TransportClockDisplay"), true, false, true),
	  secondary_clock (X_("SecondaryClockDisplay"), true, false, true),
	  preroll_clock (X_("PreRollClock"), true, true),
	  postroll_clock (X_("PostRollClock"), true, true),

	  /* adjuster table */

	  adjuster_table (3, 3),

	  /* preroll stuff */

	  preroll_button (_("pre\nroll")),
	  postroll_button (_("post\nroll")),

	  /* big clock */

	  big_clock ("BigClockDisplay", true),

	  /* transport */

	  shuttle_units_button (_("% ")),
	  shuttle_style_button (_("spring")),
	  
	  punch_in_button (_("punch\nin")),
	  punch_out_button (_("punch\nout")),
	  auto_return_button (_("auto\nreturn")),
	  auto_play_button (_("auto\nplay")),
	  auto_input_button (_("auto\ninput")),
	  click_button (_("click")),
	  follow_button (_("follow\nPH")),
	  auditioning_alert_button (_("AUDITIONING")),
	  solo_alert_button (_("SOLO")),

	  session_selector (1, 0),

	  shown_flag (false)

{
	using namespace Gtk::Menu_Helpers;

	Gtkmm2ext::init();

	/* actually, its already loaded, but ... */

	cerr << "Loading UI configuration file " << rcfile << endl;

	about = 0;

	if (theArdourUI == 0) {
		theArdourUI = this;
	}

	editor = 0;
	mixer = 0;
	session = 0;
	_session_is_new = false;
	big_clock_window = 0;
	session_selector_window = 0;
	last_key_press_time = 0;
	connection_editor = 0;
	add_route_dialog = 0;
	route_params = 0;
	meter_bridge = 0;
	option_editor = 0;
	location_ui = 0;
	sfdb_window = 0;
	new_session_window = 0;
	open_session_selector = 0;
	have_configure_timeout = false;
	have_disk_overrun_displayed = false;
	have_disk_underrun_displayed = false;
	_will_create_new_session_automatically = false;
	session_loaded = false;

	last_configure_time.tv_sec = 0;
	last_configure_time.tv_usec = 0;

	shuttle_grabbed = false;
	shuttle_fract = 0.0;

	set_shuttle_units (Percentage);
	set_shuttle_behaviour (Sprung);

	shuttle_unit_menu.items().push_back (MenuElem (_("Percentage"), bind (mem_fun(*this, &ARDOUR_UI::set_shuttle_units),
								      Percentage)));
	shuttle_unit_menu.items().push_back (MenuElem (_("Semitones"), bind (mem_fun(*this, &ARDOUR_UI::set_shuttle_units),
								      Semitones)));

	shuttle_style_menu.items().push_back (MenuElem (_("Sprung"), bind (mem_fun(*this, &ARDOUR_UI::set_shuttle_behaviour),
								   Sprung)));
	shuttle_style_menu.items().push_back (MenuElem (_("Wheel"), bind (mem_fun(*this, &ARDOUR_UI::set_shuttle_behaviour),
								  Wheel)));

	gettimeofday (&last_peak_grab, 0);
	gettimeofday (&last_shuttle_request, 0);

	ARDOUR::DiskStream::CannotRecordNoInput.connect (mem_fun(*this, &ARDOUR_UI::cannot_record_no_input));
	ARDOUR::DiskStream::DeleteSources.connect (mem_fun(*this, &ARDOUR_UI::delete_sources_in_the_right_thread));
	ARDOUR::DiskStream::DiskOverrun.connect (mem_fun(*this, &ARDOUR_UI::disk_overrun_handler));
	ARDOUR::DiskStream::DiskUnderrun.connect (mem_fun(*this, &ARDOUR_UI::disk_underrun_handler));

	/* handle pending state with a dialog */

	ARDOUR::Session::AskAboutPendingState.connect (mem_fun(*this, &ARDOUR_UI::pending_state_dialog));

	channel_combo_strings = internationalize (channel_setup_names);
	
	/* have to wait for AudioEngine and Configuration before proceeding */
}

void
ARDOUR_UI::cannot_record_no_input (DiskStream* ds)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &ARDOUR_UI::cannot_record_no_input), ds));
	
	string msg = compose (_("\
You cannot record-enable\n\
track %1\n\
because it has no input connections.\n\
You would be wasting space recording silence."),
			      ds->name());

	ArdourMessage message (editor, X_("cannotrecord"), msg);
}

void
ARDOUR_UI::set_engine (AudioEngine& e)
{
	engine = &e;

	engine->Stopped.connect (mem_fun(*this, &ARDOUR_UI::engine_stopped));
	engine->Running.connect (mem_fun(*this, &ARDOUR_UI::engine_running));
	engine->Halted.connect (mem_fun(*this, &ARDOUR_UI::engine_halted));
	engine->SampleRateChanged.connect (mem_fun(*this, &ARDOUR_UI::update_sample_rate));

	_tooltips.enable();

	keyboard = new Keyboard;
 	install_keybindings ();

	FastMeter::set_vertical_xpm (v_meter_strip_xpm);
	FastMeter::set_horizontal_xpm (h_meter_strip_xpm);

	if (setup_windows ()) {
		throw failed_constructor ();
	}

	if (GTK_ARDOUR::show_key_actions) {
		KeyboardTarget::show_all_actions ();
		exit (0);
	}

	/* start with timecode, metering enabled
	*/
	
	blink_timeout_tag = -1;

	/* this being a GUI and all, we want peakfiles */

	FileSource::set_build_peakfiles (true);
	FileSource::set_build_missing_peakfiles (true);

	if (Source::start_peak_thread ()) {
		throw failed_constructor();
	}

	/* start the time-of-day-clock */
	
	update_wall_clock ();
	Main::timeout.connect (mem_fun(*this, &ARDOUR_UI::update_wall_clock), 60000);

	update_disk_space ();
	update_cpu_load ();
	update_sample_rate (engine->frame_rate());

	starting.connect (mem_fun(*this, &ARDOUR_UI::startup));
	stopping.connect (mem_fun(*this, &ARDOUR_UI::shutdown));
}

ARDOUR_UI::~ARDOUR_UI ()
{
	save_ardour_state ();

	if (keyboard) {
		delete keyboard;
	}

	if (editor) {
		delete editor;
	}

	if (mixer) {
		delete mixer;
	}

	if (add_route_dialog) {
		delete add_route_dialog;
	}

	Source::stop_peak_thread ();
}

gint
ARDOUR_UI::configure_timeout ()
{
	struct timeval now;
	struct timeval diff;

	if (last_configure_time.tv_sec == 0 && last_configure_time.tv_usec == 0) {
		/* no configure events yet */
		return TRUE;
	}

	gettimeofday (&now, 0);
	timersub (&now, &last_configure_time, &diff);

	/* force a gap of 0.5 seconds since the last configure event
	 */

	if (diff.tv_sec == 0 && diff.tv_usec < 500000) {
		return TRUE;
	} else {
		have_configure_timeout = false;
		save_ardour_state ();
		return FALSE;
	}
}

gboolean
ARDOUR_UI::configure_handler (GdkEventConfigure* conf)
{
	if (have_configure_timeout) {
		gettimeofday (&last_configure_time, 0);
	} else {
		TimeoutSig t;
		t.connect (mem_fun(*this, &ARDOUR_UI::configure_timeout), 100);
		have_configure_timeout = true;
	}
		
	return FALSE;
}

void
ARDOUR_UI::save_ardour_state ()
{
	if (!keyboard || !mixer || !editor) {
		return;
	}
	
	/* XXX this is all a bit dubious. add_extra_xml() uses
	   a different lifetime model from add_instant_xml().
	*/

	XMLNode* node = new XMLNode (keyboard->get_state());
	Config->add_extra_xml (*node);
	Config->save_state();

	XMLNode& enode (static_cast<Stateful*>(editor)->get_state());
	XMLNode& mnode (mixer->get_state());

	if (session) {
		session->add_instant_xml(enode, session->path());
		session->add_instant_xml(mnode, session->path());
	} else {
		Config->add_instant_xml(enode, Config->get_user_ardour_path());
		Config->add_instant_xml(mnode, Config->get_user_ardour_path());
	}
}

void
ARDOUR_UI::startup ()
{
	/* Once the UI is up and running, start the audio engine. Doing
	   this before the UI is up and running can cause problems
	   when not running with SCHED_FIFO, because the amount of
	   CPU and disk work needed to get the UI started can interfere
	   with the scheduling of the audio thread.
	*/

	Gtk::Main::idle.connect (mem_fun(*this, &ARDOUR_UI::start_engine));
}

void
ARDOUR_UI::finish()
{
	if (session && session->dirty()) {
		switch (ask_about_saving_session(_("quit"))) {
		case -1:
			return;
			break;
		case 1:
			/* use the default name */
			if (save_state_canfail ("")) {
				/* failed - don't quit */
				ArdourMessage (editor, X_("badsave dialog"),
					       _("\
Ardour was unable to save your session.\n\n\
If you still wish to quit, please use the\n\n\
\"Just quit\" option."));
				return;
			}
			break;
		case 0:
			break;
		}
	}

	quit();
}

int
ARDOUR_UI::ask_about_saving_session (string what)
{
	ArdourDialog window ("saving dialog");
	Gtk::VBox   packer;
	Gtk::Label  prompt_label;
	Gtk::HBox   button_packer;

	string msg;

	msg = compose(_("Save and %1"), what);
	
	Gtk::Button save_button (msg);
	save_button.set_name ("EditorGTKButton");

	msg = compose(_("Just %1"), what);

	Gtk::Button nosave_button (msg);
	nosave_button.set_name ("EditorGTKButton");

	msg = compose(_("Don't %1"), what);

	Gtk::Button noquit_button (msg);
	noquit_button.set_name ("EditorGTKButton");

	string prompt;
	string type;

	if (session->snap_name() == session->name()) {
		type = _("session");
	} else {
		type = _("snapshot");
	}
	prompt = compose(_("The %1\n\"%2\"\nhas not been saved.\n\nAny changes made this time\nwill be lost unless you save it.\n\nWhat do you want to do?"), 
			 type, session->snap_name());
	
	prompt_label.set_text (prompt);
	prompt_label.set_alignment (0.5, 0.5);
	prompt_label.set_name (X_("PrompterLabel"));

	save_button.signal_clicked().connect (bind(mem_fun(window,&ArdourDialog::stop), 1));
	nosave_button.signal_clicked().connect (bind(mem_fun(window,&ArdourDialog::stop), 0));
	noquit_button.signal_clicked().connect (bind(mem_fun(window,&ArdourDialog::stop), -1));

	button_packer.set_spacing (10);
	button_packer.pack_start (save_button);
	button_packer.pack_start (nosave_button);
	button_packer.pack_start (noquit_button);
	
	packer.set_spacing (10);
	packer.set_border_width (10);
	packer.pack_start (prompt_label);
	packer.pack_start (button_packer);

	window.set_name (_("Prompter"));
	window.set_title (_("ardour: save session?"));
	window.set_position (Gtk::WIN_POS_MOUSE);
	window.set_modal (true);
	window.add (packer);
	window.show_all ();

	window.realize();
	window.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));
	window.set_keyboard_input (true);

	save_the_session = 0;

	editor->ensure_float (window);

	window.run ();

	return window.run_status();
}
	
gint
ARDOUR_UI::every_second ()
{
	update_cpu_load ();
	update_buffer_load ();
	update_disk_space ();
	// update_disk_rate ();
	return TRUE;
}

gint
ARDOUR_UI::every_point_one_seconds ()
{
	struct timeval now;
	struct timeval diff;
	
	/* do not attempt to grab peak power more than once per cycle.
	 */

	gettimeofday (&now, 0);
	timersub (&now, &last_peak_grab, &diff);

	if ((diff.tv_usec + (diff.tv_sec * 1000000)) >= engine->usecs_per_cycle()) {
		IO::GrabPeakPower(); /* EMIT_SIGNAL */
		last_peak_grab = now;
	} 

	update_speed_display ();
	RapidScreenUpdate(); /* EMIT_SIGNAL */
	return TRUE;
}

gint
ARDOUR_UI::every_point_zero_one_seconds ()
{
	SuperRapidScreenUpdate(); /* EMIT_SIGNAL */
	return TRUE;
}

void
ARDOUR_UI::update_sample_rate (jack_nframes_t ignored)
{
	char buf[32];

	ENSURE_GUI_THREAD (bind (mem_fun(*this, &ARDOUR_UI::update_sample_rate), ignored));

	if (!engine->connected()) {

		snprintf (buf, sizeof (buf), _("disconnected"));

	} else {

		jack_nframes_t rate = engine->frame_rate();
		
		if (fmod (rate, 1000.0) != 0.0) {
			snprintf (buf, sizeof (buf), _("SR: %.1f kHz / %4.1f msecs"), 
				  (float) rate/1000.0f,
				  (engine->frames_per_cycle() / (float) rate) * 1000.0f);
		} else {
			snprintf (buf, sizeof (buf), _("SR: %u kHz / %4.1f msecs"), 
				  rate/1000,
				  (engine->frames_per_cycle() / (float) rate) * 1000.0f);
		}
	}

	sample_rate_label.set_text (buf);
}

void
ARDOUR_UI::update_cpu_load ()
{
	char buf[32];
	snprintf (buf, sizeof (buf), _("DSP Load: %.1f%%"), engine->get_cpu_load());
	cpu_load_label.set_text (buf);
}

void
ARDOUR_UI::update_disk_rate ()
{
	char buf[64];

	if (session) {
		snprintf (buf, sizeof (buf), _("Disk r:%5.1f w:%5.1f MB/s"), 
			  session->read_data_rate()/1048576.0f, session->write_data_rate()/1048576.0f);
		disk_rate_label.set_text (buf);
	} else {
		disk_rate_label.set_text ("");
	}
}

void
ARDOUR_UI::update_buffer_load ()
{
	char buf[64];

	if (session) {
		snprintf (buf, sizeof (buf), _("Buffers p:%" PRIu32 "%% c:%" PRIu32 "%%"), 
			  session->playback_load(), session->capture_load());
		buffer_load_label.set_text (buf);
	} else {
		buffer_load_label.set_text ("");
	}
}

void
ARDOUR_UI::count_recenabled_diskstreams (DiskStream& ds)
{
	if (ds.record_enabled()) {
		rec_enabled_diskstreams++;
	}
}

void
ARDOUR_UI::update_disk_space()
{
	if (session == 0) {
		return;
	}

	jack_nframes_t frames = session->available_capture_duration();
	char buf[64];

	if (frames == max_frames) {
		strcpy (buf, _("space: 24hrs+"));
	} else {
		int hrs;
		int mins;
		int secs;
		jack_nframes_t fr = session->frame_rate();
		
		if (session->actively_recording()){
			
			rec_enabled_diskstreams = 0;
			session->foreach_diskstream (this, &ARDOUR_UI::count_recenabled_diskstreams);
			
			if (rec_enabled_diskstreams) {
				frames /= rec_enabled_diskstreams;
			}
			
		} else {
			
			/* hmmm. shall we divide by the route count? or the diskstream count?
			   or what? for now, do nothing ...
			*/
			
		}
		
		hrs  = frames / (fr * 3600);
		frames -= hrs * fr * 3600;
		mins = frames / (fr * 60);
		frames -= mins * fr * 60;
		secs = frames / fr;
		
		snprintf (buf, sizeof(buf), _("space: %02dh:%02dm:%02ds"), hrs, mins, secs);
	}

	disk_space_label.set_text (buf);
}		  

gint
ARDOUR_UI::update_wall_clock ()
{
	time_t now;
	struct tm *tm_now;
	char buf[16];

	time (&now);
	tm_now = localtime (&now);

	sprintf (buf, "%02d:%02d", tm_now->tm_hour, tm_now->tm_min);
	wall_clock_label.set_text (buf);

	return TRUE;
}

void
ARDOUR_UI::toggle_recording_plugins ()
{
	/* XXX use toggle_some_session_state */

	if (session == 0) {
		return;
	}

	session->set_recording_plugins (!session->get_recording_plugins());
}
	
void
ARDOUR_UI::toggle_auto_play ()

{
	toggle_some_session_state (auto_play_button,
				   &Session::get_auto_play,
				   &Session::set_auto_play);
}

void
ARDOUR_UI::toggle_auto_return ()

{
	toggle_some_session_state (auto_return_button,
				   &Session::get_auto_return,
				   &Session::set_auto_return);
}

void
ARDOUR_UI::toggle_click ()
{
	toggle_some_session_state (click_button,
				   &Session::get_clicking,
				   &Session::set_clicking);
}

void
ARDOUR_UI::follow_changed ()
{
	bool x;

	if (!editor) {
		return;
	}

	if (follow_button.get_active() != (x = editor->follow_playhead())) {
		follow_button.set_active (x);
	}
}

void
ARDOUR_UI::toggle_follow ()
{
	bool x;

	if (!editor) {
		return;
	}

	if (editor->follow_playhead() != (x = follow_button.get_active())) {
		editor->set_follow_playhead (x);
	}
}

void
ARDOUR_UI::toggle_session_auto_loop ()
{
	if (session) {
		if (session->get_auto_loop()) {
			if (session->transport_rolling()) {
				transport_roll();
			}
			else {
				session->request_auto_loop (false);
			}
		}
		else {
			session->request_auto_loop (true);
		}
	}
}

void
ARDOUR_UI::toggle_session_punch_in ()
{
	if (session) {
		session->set_punch_in (!session->get_punch_in());
	}
}

void
ARDOUR_UI::toggle_punch_out ()
{
	toggle_some_session_state (punch_out_button,
				   &Session::get_punch_out,
				   &Session::set_punch_out);
}

void
ARDOUR_UI::toggle_punch_in ()
{
	toggle_some_session_state (punch_in_button,
				   &Session::get_punch_in,
				   &Session::set_punch_in);
}

void
ARDOUR_UI::map_button_state ()

{
	map_some_session_state (auto_return_button,
				&Session::get_auto_return);
	map_some_session_state (auto_play_button,
				&Session::get_auto_play);
	map_some_session_state (auto_input_button,
				&Session::get_auto_input);
	map_some_session_state (punch_in_button,
				&Session::get_punch_in);
	map_some_session_state (punch_out_button,
				&Session::get_punch_out);
	map_some_session_state (click_button,
				&Session::get_clicking);
}

void
ARDOUR_UI::queue_map_control_change (Session::ControlType t)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &ARDOUR_UI::map_control_change), t));
}

void
ARDOUR_UI::map_control_change (Session::ControlType t)
{
	switch (t) {
	case Session::AutoPlay:
		map_some_session_state (auto_play_button, &Session::get_auto_play);
		break;

	case Session::AutoLoop:
		break;

	case Session::AutoReturn:
		map_some_session_state (auto_return_button, &Session::get_auto_return);
		break;

	case Session::AutoInput:
		map_some_session_state (auto_input_button, &Session::get_auto_input);
		break;

	case Session::PunchOut:
		map_some_session_state (punch_in_button, &Session::get_punch_out);
		break;

	case Session::PunchIn:
		map_some_session_state (punch_in_button, &Session::get_punch_in);
		break;

	case Session::Clicking:
		map_some_session_state (click_button, &Session::get_clicking);
		break;

	case Session::SlaveType:
//		map_some_session_state (mtc_slave_button, &Session::get_mtc_slave);
		break;

	case Session::SendMTC:
//		map_some_session_state (send_mtc_button, &Session::get_send_mtc);
		break;

	case Session::SendMMC:
//		map_some_session_state (send_mmc_button, &Session::get_send_mmc);
		break;

	case Session::MMCControl:       
//		map_some_session_state (mmc_control_button, &Session::get_mmc_control);
		break;

	case Session::MidiFeedback:       
//		map_some_session_state (mmc_control_button, &Session::get_mmc_control);
		break;
	case Session::MidiControl:       
//		map_some_session_state (mmc_control_button, &Session::get_mmc_control);
		break;
		
	case Session::Live:
		break;

	case Session::RecordingPlugins:
		break;

	case Session::CrossFadesActive:
		break;
		
	case Session::EditingMode:
		break;

	case Session::PlayRange:
		break;

	case Session::AlignChoice:
		/* don't care, this is handled by the options editor */
		break;
	case Session::SeamlessLoop:
		/* don't care, this is handled by the options editor */
		break;
	       
	}
}

void
ARDOUR_UI::control_methods_adjusted ()

{
	int which_method;

	which_method = (int) online_control_button->adjustment.get_value();
	switch (which_method) {
	case 0:
		allow_mmc_and_local ();
		break;
	case 1:
		allow_mmc_only ();
		break;
	case 2:
		allow_local_only ();
		break;
	default:
		fatal << _("programming error: impossible control method") << endmsg;
	}
}
	

void
ARDOUR_UI::mmc_device_id_adjusted ()

{
#if 0
	if (mmc) {
		int dev_id = (int) mmc_id_button->adjustment.get_value();
		mmc->set_device_id (dev_id);
	}
#endif
}

void
ARDOUR_UI::map_some_session_state (ToggleButton& button,
				   bool (Session::*get)() const)
	
{
	bool x;

	if (session == 0) {
		return;
	}
	
	if (button.get_active() != (x = (session->*get)())) {
		button.set_active (x);
	}
}

void
ARDOUR_UI::toggle_some_session_state (ToggleButton& button,
				      bool (Session::*get)() const,
				      void (Session::*set)(bool))

{
	bool button_state;
	bool session_state;

	if (session == 0) {
		return;
	}

	button_state = button.get_active ();
	session_state = (session->*get)();

	if (button_state != session_state) {
		(session->*set) (button_state);
#if 0
	
		/* check that it worked, and reverse
		   the button state if it didn't
		*/

		if ((session->*get)() != button_state) {
			button->set_active (!button_state);
		}
#endif

	}
}	

gint
ARDOUR_UI::session_menu (GdkEventButton *ev)
{
	session_popup_menu->popup (0, 0, 0, 0);
	return TRUE;
}

void
ARDOUR_UI::redisplay_recent_sessions ()
{
	using namespace Gtkmm2ext;
	using namespace Gtk::CTree_Helpers;

	vector<string *> *sessions;
	vector<string *>::iterator i;
	RecentSessionsSorter cmp;

	/* ---------------------------------------- */
	/* XXX MAKE ME A FUNCTION (no CTree::clear() in gtkmm 1.2) */

	gtk_ctree_remove_node (session_selector.gobj(), NULL);
	/* ---------------------------------------- */


	RecentSessions rs;
	ARDOUR::read_recent_sessions (rs);

	if (rs.empty()) {
		session_selector.thaw();
		return;
	}
	/* sort them alphabetically */
	sort(rs.begin(), rs.end(), cmp);
	sessions = new vector<string*>;
	for (RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		sessions->push_back (new string ((*i).second));
	}

	session_selector.freeze();

	for (i = sessions->begin(); i != sessions->end(); ++i) {

		vector<string*>* states;
		vector<const gchar*> item;
		string fullpath = *(*i);

		/* remove any trailing / */

		if (fullpath[fullpath.length()-1] == '/') {
			fullpath = fullpath.substr (0, fullpath.length()-1);
		}

		/* now get available states for this session */

		if ((states = Session::possible_states(fullpath)) == 0) {
			/* no state file? */
			continue;
		}

		/* OK, try to add entries for this session */


		/* add the parent */

		item.clear ();
		string basen = PBD::basename (fullpath);
		item.push_back (basen.c_str());
		session_selector.rows().push_back (Element (item));

		session_selector.rows().back().set_data (new string (fullpath), deferred_delete<string>);

		if (states->size() == 1) {

			/* only 1 state, show it at the top level */

			session_selector.rows().back().set_leaf (true);

		} else {

			session_selector.rows().back().set_leaf (false);

			vector<string *>::iterator i2;
			
			/* add the children */
			
			for (i2 = states->begin(); i2 != states->end(); ++i2) {
				
				string statename = *(*i2);
				
				item.clear ();
				item.push_back (statename.c_str());
				
				session_selector.rows().back().subtree().push_back (Element (item));
				session_selector.rows().back().subtree().back().set_data (new string (statename),
											  deferred_delete<string>);
				session_selector.rows().back().subtree().back().set_leaf (true);
				
				delete *i2;
			}
		}

		delete states;
	}

	session_selector.thaw();
	delete sessions;
}

void
ARDOUR_UI::session_selection (Gtk::CTree_Helpers::Row row, gint column)
{
	using namespace Gtk::CTree_Helpers;

	string session_name;
	string session_path;
	string session_state;

	if (!row.is_leaf()) {
		row.expand();
		return;
	}

	string *stp = static_cast<string *> (row.get_data());

	if ((*stp)[0] != '/' && (*stp)[0] != '.') {
		
		/* its a state file node, so get the parent for the session information,
		   and combine with the state file name.
		*/
		
		string *spp = static_cast<string *> (row.get_parent().get_data());
		
		session_name = *spp;
		session_path = *spp;
		session_state = *stp;
		
	} else {
		
		/* its a session directory node, so just get the session path,
		   and use "default" to load the state.
		*/
		
		string *spp = static_cast<string *> (row.get_data());
		
		session_name = *spp;
		session_path = *spp;
		session_state = PBD::basename (*spp);
	}

	session_selector_window->hide ();
	_session_is_new = false;
	load_session (session_path, session_state);
}

void
ARDOUR_UI::build_session_selector ()
{
	session_selector_window = new ArdourDialog ("session selector");
	
	Gtk::VBox *vpacker = new Gtk::VBox;
	Gtk::ScrolledWindow *scroller = new Gtk::ScrolledWindow;
	Gtk::HBox *button_packer = new Gtk::HBox;
	Gtk::Button *cancel_button = new Gtk::Button (_("cancel"));
	Gtk::Button *rescan_button = new Gtk::Button (_("rescan"));

	button_packer->pack_start (*rescan_button);
	button_packer->pack_start (*cancel_button);

	vpacker->pack_start (*scroller);
	vpacker->pack_start (*button_packer, false, false);

	scroller->add (session_selector);
	scroller->set_policy(Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);

	session_selector_window->add (*vpacker);
	session_selector_window->set_name ("SessionSelectorWindow");
	session_selector_window->set_size_request (200, 400);

	session_selector_window->signal_delete_event().connect (bind (ptr_fun (just_hide_it), static_cast<Gtk::Window*>(session_selector_window)));
	cancel_button-.signal_clicked().connect (bind (mem_fun(*this, &ARDOUR_UI::hide_dialog), session_selector_window));
	session_selector.tree_select_row.connect (mem_fun(*this, &ARDOUR_UI::session_selection));
}

void
ARDOUR_UI::fs_cancel_clicked (Gtk::FileSelection* fs)
{
	fs->hide_all();
	fs->get_selection_entry()->set_text("");
	allow_focus (false);
}

gint 
ARDOUR_UI::fs_delete_event (GdkEventAny* ev, Gtk::FileSelection* fs)
{
	fs_cancel_clicked (fs);
	return 1;
}

void
ARDOUR_UI::open_session ()
{
	/* popup selector window */

	if (open_session_selector == 0) {
		open_session_selector = new Gtk::FileSelection(_("open session"));
		open_session_selector->get_ok_button()-.signal_clicked().connect (mem_fun(*this, &ARDOUR_UI::open_ok_clicked));
		open_session_selector->get_cancel_button()-.signal_clicked().connect (bind (mem_fun(*this, &ARDOUR_UI::fs_cancel_clicked), open_session_selector));
		open_session_selector->signal_delete_event().connect (bind (mem_fun(*this, &ARDOUR_UI::fs_delete_event), open_session_selector));
	}

	open_session_selector->show_all ();
	allow_focus (true);

	/* wait for selection */
}

void
ARDOUR_UI::open_ok_clicked ()
{
	open_session_selector->hide_all();
	string session_path = open_session_selector->get_filename();
	string path, name;
	bool isnew;

	if (session_path.length() > 0) {
		if (Session::find_session (session_path, path, name, isnew) == 0) {
			_session_is_new = isnew;
			load_session (path, name);
		}
	}

	open_session_selector->get_selection_entry()->set_text("");
	
	/* XXX hack hack hack */

	GtkCList* clist = (GtkCList*) open_session_selector->gobj()->file_list;
	gtk_clist_unselect_all (clist);

	allow_focus(false);
}

void
ARDOUR_UI::open_recent_session ()
{
	/* popup selector window */

	if (session_selector_window == 0) {
		build_session_selector ();
	}

	redisplay_recent_sessions ();
	session_selector_window->show_all ();

	/* wait for selection */
}

void
ARDOUR_UI::session_add_midi_track ()
{
	cerr << _("Patience is a virtue.\n");
}

void
ARDOUR_UI::session_add_audio_route (bool disk, int32_t input_channels, int32_t output_channels)
{
	Route* route;

	if (session == 0) {
		warning << _("You cannot add a track without a session already loaded.") << endmsg;
		return;
	}

	try { 
		if (disk) {
			if ((route = session->new_audio_track (input_channels, output_channels)) == 0) {
				error << _("could not create new audio track") << endmsg;
			}
		} else {
			if ((route = session->new_audio_route (input_channels, output_channels)) == 0) {
				error << _("could not create new audio bus") << endmsg;
			}
		}
		
#if CONTROLOUTS
		if (need_control_room_outs) {
			pan_t pans[2];
			
			pans[0] = 0.5;
			pans[1] = 0.5;
			
			route->set_stereo_control_outs (control_lr_channels);
			route->control_outs()->set_stereo_pan (pans, this);
		}
#endif /* CONTROLOUTS */
	}

	catch (...) {
		ArdourMessage msg (editor, X_("noport dialog"),
				   _("There are insufficient JACK ports available\n\
to create a new track or bus.\n\
You should save Ardour, exit and\n\
restart JACK with more ports."));
	}
}

void
ARDOUR_UI::diskstream_added (DiskStream* ds)
{
	// meter_bridge_dialog_check->set_sensitive (true);
}

void
ARDOUR_UI::do_transport_locate (jack_nframes_t new_position)
{
	jack_nframes_t _preroll;

	if (session) {
		_preroll = session->convert_to_frames_at (new_position, session->preroll);

		if (new_position > _preroll) {
			new_position -= _preroll;
		} else {
			new_position = 0;
		}

		session->request_locate (new_position);
	}
}

void
ARDOUR_UI::transport_goto_start ()
{
	if (session) {
		session->request_locate (0);

		
		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/
		
		if (editor) {
			editor->reposition_x_origin (0);
		}
	}
}

void
ARDOUR_UI::transport_goto_end ()
{
	if (session) {
		jack_nframes_t frame = session->current_end_frame();
		session->request_locate (frame);

		/* force displayed area in editor to start no matter
		   what "follow playhead" setting is.
		*/
		
		if (editor) {
			editor->reposition_x_origin (frame);
		}
	}
}

gint 
ARDOUR_UI::mouse_transport_stop (GdkEventButton *ev)
{


	if (session) {
			if (session->transport_stopped()) {
			session->request_locate (session->last_transport_start());
		} else {
			if (session->get_auto_loop()) {
				session->request_auto_loop (false);
			}

			Keyboard::ModifierMask mask = Keyboard::ModifierMask (Keyboard::Control|Keyboard::Shift);
			session->request_stop (Keyboard::modifier_state_equals (ev->state, mask));
			}
	}

	return TRUE;
}

gint
ARDOUR_UI::mouse_transport_roll (GdkEventButton* ev)
{
	transport_roll ();
	return TRUE;
}

void
ARDOUR_UI::transport_stop ()
{
	if (!session) {
		return;
	}

	if (session->is_auditioning()) {
		session->cancel_audition ();
		return;
	}
	
	if (session->get_auto_loop()) {
		session->request_auto_loop (false);
	}
	
	session->request_stop ();
}

void
ARDOUR_UI::transport_stop_and_forget_capture ()
{
	if (session) {
		session->request_stop (true);
	}
}

void
ARDOUR_UI::remove_last_capture()
{
	if (editor) {
		editor->remove_last_capture();
	}
}

void
ARDOUR_UI::transport_record ()
{
	if (session) {
		switch (session->record_status()) {
		case Session::Disabled:
			if (session->ntracks() == 0) {
				string txt = _("Please create 1 or more track\nbefore trying to record.\nCheck the Session menu.");
				ArdourMessage msg (editor, X_("cannotrecenable"), txt);
				return;
			}
			session->maybe_enable_record ();
			break;
		case Session::Recording:
		case Session::Enabled:
			session->disable_record ();
		}
	}
}

void
ARDOUR_UI::transport_roll ()
{
	bool rolling;

	if (!session) {
		return;
	}

	rolling = session->transport_rolling ();

	if (session->get_auto_loop()) {
		session->request_auto_loop (false);
		auto_loop_button.set_active (false);
		roll_button.set_active (true);
	} else if (session->get_play_range ()) {
		session->request_play_range (false);
		play_selection_button.set_active (false);
	} else if (rolling) {
		session->request_locate (session->last_transport_start(), true);
	}

	session->request_transport_speed (1.0f);
}

void
ARDOUR_UI::transport_loop()
{
	if (session) {
		if (session->get_auto_loop()) {
			if (session->transport_rolling()) {
				Location * looploc = session->locations()->auto_loop_location();
				if (looploc) {
					session->request_locate (looploc->start(), true);
				}
			}
		}
		else {
			session->request_auto_loop (true);
		}
	}
}

void
ARDOUR_UI::transport_play_selection ()
{
	if (!session) {
		return;
	}

	if (!session->get_play_range()) {
		session->request_stop ();
	}

	editor->play_selection ();
}

void
ARDOUR_UI::transport_rewind (int option)
{
	float current_transport_speed;
 
       	if (session) {
		current_transport_speed = session->transport_speed();
		
		if (current_transport_speed >= 0.0f) {
			switch (option) {
			case 0:
				session->request_transport_speed (-1.0f);
				break;
			case 1:
				session->request_transport_speed (-4.0f);
				break;
			case -1:
				session->request_transport_speed (-0.5f);
				break;
			}
		} else {
			/* speed up */
			session->request_transport_speed (current_transport_speed * 1.5f);
		}
	}
}

void
ARDOUR_UI::transport_forward (int option)
{
	float current_transport_speed;
	
	if (session) {
		current_transport_speed = session->transport_speed();
		
		if (current_transport_speed <= 0.0f) {
			switch (option) {
			case 0:
				session->request_transport_speed (1.0f);
				break;
			case 1:
				session->request_transport_speed (4.0f);
				break;
			case -1:
				session->request_transport_speed (0.5f);
				break;
			}
		} else {
			/* speed up */
			session->request_transport_speed (current_transport_speed * 1.5f);
		}
	}
}

void
ARDOUR_UI::toggle_monitor_enable (guint32 dstream)
{
	if (session == 0) {
		return;
	}

	DiskStream *ds;

	if ((ds = session->diskstream_by_id (dstream)) != 0) {
		Port *port = ds->io()->input (0);
		port->request_monitor_input (!port->monitoring_input());
	}
}

void
ARDOUR_UI::toggle_record_enable (guint32 dstream)
{
	if (session == 0) {
		return;
	}

	DiskStream *ds;

	if ((ds = session->diskstream_by_id (dstream)) != 0) {
		ds->set_record_enabled (!ds->record_enabled(), this);
	}
}

void
ARDOUR_UI::queue_transport_change ()
{
	Gtkmm2ext::UI::instance()->call_slot (mem_fun(*this, &ARDOUR_UI::map_transport_state));
}

void
ARDOUR_UI::map_transport_state ()
{
	float sp = session->transport_speed();

	if (sp == 1.0f) {
		transport_rolling ();
	} else if (sp < 0.0f) {
		transport_rewinding ();
	} else if (sp > 0.0f) {
		transport_forwarding ();
	} else {
		transport_stopped ();
	}
}

void
ARDOUR_UI::send_all_midi_feedback ()
{
	if (session) {
		session->send_all_midi_feedback();
	}
}

void
ARDOUR_UI::allow_local_only ()
{

}

void
ARDOUR_UI::allow_mmc_only ()
{

}

void
ARDOUR_UI::allow_mmc_and_local ()
{

}

void
ARDOUR_UI::GlobalClickBox::printer (char buf[32], Adjustment &adj, void *arg)
{
	snprintf (buf, sizeof(buf), "%s", ((GlobalClickBox *) arg)->strings[
		(int) adj.get_value()].c_str());
}

void
ARDOUR_UI::engine_stopped ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::engine_stopped));

	jack_disconnect_item->set_sensitive (false);
	jack_reconnect_item->set_sensitive (true);
	jack_bufsize_menu->set_sensitive (false);
}


void
ARDOUR_UI::engine_running ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::engine_running));

	jack_disconnect_item->set_sensitive (true);
	jack_reconnect_item->set_sensitive (false);
	jack_bufsize_menu->set_sensitive (true);
}

void
ARDOUR_UI::engine_halted ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::engine_halted));

	jack_disconnect_item->set_sensitive (false);
	jack_reconnect_item->set_sensitive (true);
	jack_bufsize_menu->set_sensitive (false);

	update_sample_rate (0);

	ArdourMessage msg (editor, X_("halted"),
			   _("\
JACK has either been shutdown or it\n\
disconnected Ardour because Ardour\n\
was not fast enough. You can save the\n\
session and/or try to reconnect to JACK ."));
}

int32_t
ARDOUR_UI::do_engine_start ()
{
	try { 
		engine->start();
	}

	catch (AudioEngine::PortRegistrationFailure& err) {
		engine->stop ();
		error << _("Unable to create all required ports")
		      << endmsg;
		unload_session ();
		return -1;
	}

	catch (...) {
		engine->stop ();
		error << _("Unable to start the session running")
		      << endmsg;
		unload_session ();
		return -2;
	}
	
	return 0;
}

gint
ARDOUR_UI::start_engine ()
{
	if (do_engine_start () == 0) {
		if (session && _session_is_new) {
			/* we need to retain initial visual 
			   settings for a new session 
			*/
			session->save_state ("");
		}

		/* there is too much going on, in too many threads, for us to 
		   end up with a clean session. So wait 1 second after loading,
		   and fix it up. its ugly, but until i come across a better
		   solution, its what we have.
		*/

		Main::timeout.connect (mem_fun(*this, &ARDOUR_UI::make_session_clean), 1000);
	}

	return FALSE;
}

void
ARDOUR_UI::update_clocks ()
{
	 Clock (session->audible_frame()); /* EMIT_SIGNAL */
}

void
ARDOUR_UI::start_clocking ()
{
	clock_signal_connection = RapidScreenUpdate.connect (mem_fun(*this, &ARDOUR_UI::update_clocks));
}

void
ARDOUR_UI::stop_clocking ()
{
	clock_signal_connection.disconnect ();
}
	
void
ARDOUR_UI::toggle_clocking ()
{
#if 0
	if (clock_button.get_active()) {
		start_clocking ();
	} else {
		stop_clocking ();
	}
#endif
}

gint
ARDOUR_UI::_blink (void *arg)

{
	((ARDOUR_UI *) arg)->blink ();
	return TRUE;
}

void
ARDOUR_UI::blink ()
{
	 Blink (blink_on = !blink_on); /* EMIT_SIGNAL */
}

void
ARDOUR_UI::start_blinking ()
{
	/* Start the blink signal. Everybody with a blinking widget
	   uses Blink to drive the widget's state.
	*/

	if (blink_timeout_tag < 0) {
		blink_on = false;	
		blink_timeout_tag = gtk_timeout_add (240, _blink, this);
	}
}

void
ARDOUR_UI::stop_blinking ()
{
	if (blink_timeout_tag >= 0) {
		gtk_timeout_remove (blink_timeout_tag);
		blink_timeout_tag = -1;
	}
}


void
ARDOUR_UI::add_diskstream_to_menu (DiskStream& dstream)
{
	using namespace Gtk;
	using namespace Menu_Helpers;

	if (dstream.hidden()) {
		return;
	}

	MenuList& items = diskstream_menu->items();
	items.push_back (MenuElem (dstream.name(), bind (mem_fun(*this, &ARDOUR_UI::diskstream_selected), (gint32) dstream.id())));
}
	
void
ARDOUR_UI::diskstream_selected (gint32 id)
{
	selected_dstream = id;
	Main::quit ();
}

gint32
ARDOUR_UI::select_diskstream (GdkEventButton *ev)
{
	using namespace Gtk;
	using namespace Menu_Helpers;

	if (session == 0) {
		return -1;
	}

	diskstream_menu = new Menu();
	diskstream_menu->set_name ("ArdourContextMenu");
	using namespace Gtk;
	using namespace Menu_Helpers;

	MenuList& items = diskstream_menu->items();
	items.push_back (MenuElem (_("No Stream"), (bind (mem_fun(*this, &ARDOUR_UI::diskstream_selected), -1))));

	session->foreach_diskstream (this, &ARDOUR_UI::add_diskstream_to_menu);

	if (ev) {
		diskstream_menu->popup (ev->button, ev->time);
	} else {
		diskstream_menu->popup (0, 0);
	}

	selected_dstream = -1;

	Main::run ();

	delete diskstream_menu;

	return selected_dstream;
}

void
ARDOUR_UI::name_io_setup (AudioEngine& engine, 
			  string& buf,
			  IO& io,
			  bool in)
{
	if (in) {
		if (io.n_inputs() == 0) {
			buf = _("none");
			return;
		}
		
		/* XXX we're not handling multiple ports yet. */

		const char **connections = io.input(0)->get_connections();
		
		if (connections == 0 || connections[0] == '\0') {
			buf = _("off");
		} else {
			buf = connections[0];
		}

		free (connections);

	} else {

		if (io.n_outputs() == 0) {
			buf = _("none");
			return;
		}
		
		/* XXX we're not handling multiple ports yet. */

		const char **connections = io.output(0)->get_connections();
		
		if (connections == 0 || connections[0] == '\0') {
			buf = _("off");
		} else {
			buf = connections[0];
		}

		free (connections);
	}
}

void
ARDOUR_UI::snapshot_session ()
{
	ArdourPrompter prompter (true);
	string now;
	time_t n;

	time (&n);
	now = ctime (&n);
	now = now.substr (0, now.length() - 1);

	prompter.set_name ("Prompter");
	prompter.set_prompt (_("Name for snapshot"));
	prompter.set_initial_text (now);
	prompter.done.connect (Gtk::Main::quit.slot());
	prompter.show_all ();

	Gtk::Main::run ();

	if (prompter.status == Gtkmm2ext::Prompter::entered) {
		string snapname;
		
		prompter.get_result (snapname);
		if (snapname.length()){
			save_state (snapname);
		}
	}
}

void
ARDOUR_UI::save_state (string name)
{
	(void) save_state_canfail (name);
}
		
int
ARDOUR_UI::save_state_canfail (string name)
{
	if (session) {
		int ret;

		if (name.length() == 0) {
			name = session->snap_name();
		}

		if ((ret = session->save_state (name)) != 0) {
			return ret;
		}
	}
	save_ardour_state (); /* XXX cannot fail? yeah, right ... */
	return 0;
}

void
ARDOUR_UI::restore_state (string name)
{
	if (session) {
		if (name.length() == 0) {
			name = session->name();
		}
		session->restore_state (name);
	}
}

void
ARDOUR_UI::allow_focus (bool yn)
{
	if (keyboard) {
		keyboard->allow_focus (yn);
	}
}

void
ARDOUR_UI::primary_clock_value_changed ()
{
	if (session) {
		session->request_locate (primary_clock.current_time ());
	}
}

void
ARDOUR_UI::secondary_clock_value_changed ()
{
	if (session) {
		session->request_locate (secondary_clock.current_time ());
	}
}

void
ARDOUR_UI::rec_enable_button_blink (bool onoff, DiskStream *dstream, Widget *w)
{
	if (session && dstream && dstream->record_enabled()) {

		Session::RecordState rs;
		
		rs = session->record_status ();

		switch (rs) {
		case Session::Disabled:
		case Session::Enabled:
			if (w->get_state() != GTK_STATE_SELECTED) {
				w->set_state (GTK_STATE_SELECTED);
			}
			break;

		case Session::Recording:
			if (w->get_state() != GTK_STATE_ACTIVE) {
				w->set_state (GTK_STATE_ACTIVE);
			}
			break;
		}

	} else {
		if (w->get_state() != Gtk::STATE_NORMAL) {
			w->set_state (Gtk::STATE_NORMAL);
		}
	}
}

void
ARDOUR_UI::transport_rec_enable_blink (bool onoff) 
{
	if (session == 0) {
		return;
	}
	
	switch (session->record_status()) {
	case Session::Enabled:
		if (onoff) {
			rec_button.set_state (GTK_STATE_ACTIVE);
		} else {
			rec_button.set_state (Gtk::STATE_NORMAL);
		}
		break;

	case Session::Recording:
		rec_button.set_state (GTK_STATE_ACTIVE);
		break;

	default:
		rec_button.set_active (false);
		rec_button.set_state (Gtk::STATE_NORMAL);
		break;
	}
}

gint
ARDOUR_UI::generic_focus_in_event (GdkEventFocus *ev)
{
	ARDOUR_UI::instance()->allow_focus (true);
	return FALSE;
}

gint
ARDOUR_UI::generic_focus_out_event (GdkEventFocus *ev)
{
	ARDOUR_UI::instance()->allow_focus (false);
	return FALSE;
}

gint
ARDOUR_UI::hide_and_quit (GdkEventAny *ev, ArdourDialog *window)
{
	window->hide();
	Gtk::Main::quit ();
	return TRUE;
}

void
ARDOUR_UI::start_keyboard_prefix ()
{
	keyboard->start_prefix();
}

void
ARDOUR_UI::save_template ()

{
	ArdourPrompter prompter (true);
	prompter.set_name ("Prompter");
	prompter.set_prompt (_("Name for mix template:"));
	prompter.set_initial_text(session->name() + _("-template"));

	prompter.done.connect(Gtk::Main::quit.slot());
	prompter.show_all();
	
	Gtk::Main::run();
	
	if (prompter.status == Gtkmm2ext::Prompter::entered) {
		string name;

		prompter.get_result (name);

		if (name.length()) {
			session->save_template (name);
		}
	}
}

void
ARDOUR_UI::new_session (bool startup, string predetermined_path)
{
	if (new_session_window == 0){
		new_session_window = new NewSessionDialog (*engine, startup, predetermined_path);
		editor->ensure_float (*new_session_window);
	}

	new_session_window->run ();

	/* write favorites either way */
	Session::FavoriteDirs favs;
	new_session_window->file_selector.get_favorites (favs);
	Session::write_favorite_dirs (favs);

	if (new_session_window->run_status()) {
		return;
	}

	string session_path = new_session_window->file_selector.get_path ();
	string session_name = PBD::basename (session_path);

	// Check that it doesn't already exist.
	access(session_path.c_str(), R_OK); 
	if (errno != ENOENT){
		error << compose(_("Session %1 already exists at %2"), session_name, session_path) << endmsg;
		return;
	}

	_session_is_new = true;

	if (session_path[session_path.length()-1] != '/') {

		string template_name = new_session_window->get_template_name ();

		if (template_name.length()) {

			load_session (session_path, session_name, &template_name);

		} else {

			uint32_t cchns;
			uint32_t mchns;
			Session::AutoConnectOption iconnect;
			Session::AutoConnectOption oconnect;

			if (new_session_window->use_control_button.get_active()) {
				cchns = (uint32_t) channel_combo_get_channel_count (new_session_window->control_out_channel_combo);
			} else {
				cchns = 0;
			}
			if (new_session_window->use_master_button.get_active()) {
				mchns = (uint32_t) channel_combo_get_channel_count (new_session_window->master_out_channel_combo);
			} else {
				mchns = 0;
			}

			if (new_session_window->connect_to_physical_inputs_button.get_active()) {
				iconnect = Session::AutoConnectPhysical;
			} else {
				iconnect = Session::AutoConnectOption (0);
			}

			if (new_session_window->connect_to_master_button.get_active ()) {
				oconnect = Session::AutoConnectMaster;
			} else if (new_session_window->connect_to_physical_outputs_button.get_active ()) {
				oconnect = Session::AutoConnectPhysical;
			} else {
				oconnect = Session::AutoConnectOption (0);
			} 

			uint32_t nphysin = (uint32_t) new_session_window->in_count_adjustment.get_value();
			uint32_t nphysout = (uint32_t) new_session_window->out_count_adjustment.get_value();

			build_session (session_path, session_name, cchns, mchns, iconnect, oconnect, nphysin, nphysout, 
				       engine->frame_rate() * 60 * 5);
		}
	}
}

int
ARDOUR_UI::load_session (string path, string snap_name, string* mix_template)
{
	Session *new_session;
	int x;
	session_loaded = false;
	x = unload_session ();

	if (x < 0) {
		return -1;
	} else if (x > 0) {
		return 0;
	}

	/* if it already exists, we must have write access */

	if (::access (path.c_str(), F_OK) == 0 && ::access (path.c_str(), W_OK)) {
		ArdourMessage msg (editor, X_("noaccess dialog"), _("\
You do not have write access to this session.\n\
This prevents the session from being loaded."));
		return -1;
	}

	try {
		new_session = new Session (*engine, path, snap_name, mix_template);
	}

	catch (...) {

		error << compose(_("Session \"%1 (snapshot %2)\" did not load successfully"), path, snap_name) << endmsg;
		return -1;
	}

	connect_to_session (new_session);

	//if (engine->running()) {
	//mixer->show_window();
	//}
	session_loaded = true;
	return 0;
}

int
ARDOUR_UI::make_session_clean ()
{
	if (session) {
		session->set_clean ();
	}

	return FALSE;
}

int
ARDOUR_UI::build_session (string path, string snap_name, 
			  uint32_t control_channels,
			  uint32_t master_channels, 
			  Session::AutoConnectOption input_connect,
			  Session::AutoConnectOption output_connect,
			  uint32_t nphysin,
			  uint32_t nphysout,
			  jack_nframes_t initial_length)
{
	Session *new_session;
	int x;

	session_loaded = false;
	x = unload_session ();
	if (x < 0) {
		return -1;
	} else if (x > 0) {
		return 0;
	}
	
	_session_is_new = true;

	try {
		new_session = new Session (*engine, path, snap_name, input_connect, output_connect,
					   control_channels, master_channels, nphysin, nphysout, initial_length);
	}

	catch (...) {

		error << compose(_("Session \"%1 (snapshot %2)\" did not load successfully"), path, snap_name) << endmsg;
		return -1;
	}

	connect_to_session (new_session);

	//if (engine->running()) {
	//mixer->show_window();
	//}
	session_loaded = true;
	return 0;
}

void
ARDOUR_UI::hide_dialog (ArdourDialog *dialog)
{
	dialog->hide_all();
}

void
ARDOUR_UI::show ()
{
	if (editor) {
		editor->show_window ();
		shown_flag = true;
	}

	if (session && mixer) {
		mixer->show_window ();
	}
	
	if (about) {
		about->get_window().raise ();
	}
}

void
ARDOUR_UI::show_splash ()
{
	if (about == 0) {
		about = new About(this);
		about->show_all();
		about->show_sub (true);
		about->get_window().raise ();
	}
	else {
		about->get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));
		about->show_all ();
		about->get_window().raise ();
	}
}

void
ARDOUR_UI::hide_splash ()
{
	if (about) {
		// about->hide();
	}
}

void
ARDOUR_UI::display_cleanup_results (Session::cleanup_report& rep, const gchar* list_title, string msg)
{
	size_t removed;

	removed = rep.paths.size();

	if (removed == 0) {
		ArdourMessage msg (editor, X_("cleanupresults"),
				   _("\
No audio files were ready for cleanup\n\n\
If this seems suprising, check for any existing\n\
snapshots. These may still include regions that\n\
require some unused files to continue to exist."));
		return;
	} 

	ArdourDialog results ("cleanup results");
	
	const gchar* list_titles[] = { 
		list_title,
		0
	};
	
	Gtk::CList list (internationalize (list_titles));
	Gtk::ScrolledWindow list_scroller;
	Gtk::Label txt;
	Gtk::Button ok_button (_("OK"));
	Gtk::VBox vpacker;
	const char* rowtext[1];
	
	list_scroller.set_policy (Gtk::POLICY_NEVER, Gtk::POLICY_AUTOMATIC);
	
	vpacker.set_border_width (10);
	vpacker.set_spacing (10);

	if (rep.space < 1048576.0f) {
		if (removed > 1) {
			txt.set_text (compose (msg, removed, _("files"), (float) rep.space / 1024.0f, "kilo"));
		} else {
			txt.set_text (compose (msg, removed, _("file"), (float) rep.space / 1024.0f, "kilo"));
		}
	} else {
		if (removed > 1) {
			txt.set_text (compose (msg, removed, _("files"), (float) rep.space / 1048576.0f, "mega"));
		} else {
			txt.set_text (compose (msg, removed, _("file"), (float) rep.space / 1048576.0f, "mega"));
		}
	}

	vpacker.pack_start (txt, false, false);
	
	for (vector<string>::iterator i = rep.paths.begin(); i != rep.paths.end(); ++i) {
		rowtext[0] = (*i).c_str();
		list.rows().push_back (rowtext);
	}
	
	list_scroller.add_with_viewport (list);
	list_scroller.set_size_request (-1, 250);
	
	vpacker.pack_start (list_scroller, true, true);
	vpacker.pack_start (ok_button, false, false);
	
	ok_button.signal_clicked().connect (Main::quit.slot ());
	results.Hiding.connect (Main::quit.slot ());
	
	results.add (vpacker);
	
	results.set_position (Gtk::WIN_POS_MOUSE);
	results.set_title (_("ardour: cleanup"));
	results.set_modal (true);
	results.run ();
}

void
ARDOUR_UI::cleanup ()
{
	if (session == 0) {
		/* shouldn't happen: menu item is insensitive */
		return;
	}

	ArdourDialog checker (X_("cleanup confirm dialog"));
	Gtk::Label label (_("\
Cleanup is a destructive operation.\n\
ALL undo/redo information will be lost if you cleanup.\n\
Unused audio files will be moved to a \"dead sounds\" location."));

	Gtk::Button ok_button (_("Proceed with cleanup"));
	Gtk::Button cancel_button (_("Cancel"));
	Gtk::HBox   bbox;
	Gtk::VBox   vbox;

	bbox.set_border_width (6);
	bbox.set_spacing (12);
	bbox.pack_start (ok_button, true, false);
	bbox.pack_start (cancel_button, true, false);
	
	vbox.set_border_width (6);
	vbox.set_spacing (12);
	vbox.pack_start (label, false, false);
	vbox.pack_start (bbox, false, false);
	
	checker.add (vbox);
	checker.set_name (_("CleanupDialog"));
	checker.set_title (_("ardour cleanup"));
	checker.set_wmclass (_("ardour_cleanup"), "Ardour");
	checker.set_position (Gtk::WIN_POS_MOUSE);
	checker.realize ();
	checker.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));

	ok_button.signal_clicked().connect (bind (mem_fun (checker, &ArdourDialog::stop), 1));
	cancel_button.signal_clicked().connect (bind (mem_fun (checker, &ArdourDialog::stop), 0));

	checker.run ();

	if (checker.run_status() != 1) {
		return;
	}

	Session::cleanup_report rep;

	editor->prepare_for_cleanup ();

	if (session->cleanup_sources (rep)) {
		return;
	}

	display_cleanup_results (rep, 
				 _("cleaned files"),
				 _("\
The following %1 %2 were not in use.\n\
The next time you flush the wastebasket\n\
it will release an additional %3 %4bytes\n\
of disk space"
					 ));
}

void
ARDOUR_UI::flush_trash ()
{
	if (session == 0) {
		/* shouldn't happen: menu item is insensitive */
		return;
	}

	Session::cleanup_report rep;

	if (session->cleanup_trash_sources (rep)) {
		return;
	}

	display_cleanup_results (rep, 
				 _("deleted file"),
				 _("The following %1 file%2 were deleted, releasing %3 %4bytes of disk space"));
}

void
ARDOUR_UI::add_route ()
{
	int count;

	if (!session) {
		return;
	}

	if (add_route_dialog == 0) {
		add_route_dialog = new AddRouteDialog;
		editor->ensure_float (*add_route_dialog);
	}

	if (add_route_dialog->is_visible()) {
		/* we're already doing this */
		return;
	}

	add_route_dialog->run ();

	if (add_route_dialog->run_status()) {
		return;
	}

	if ((count = add_route_dialog->count()) <= 0) {
		return;
	}

	uint32_t input_chan = add_route_dialog->channels ();
	uint32_t output_chan;
	string name_template = add_route_dialog->name_template ();
	bool track = add_route_dialog->track ();

	Session::AutoConnectOption oac = session->get_output_auto_connect();

	if (oac & Session::AutoConnectMaster) {
		output_chan = (session->master_out() ? session->master_out()->n_inputs() : input_chan);
	} else {
		output_chan = input_chan;
	}

	/* XXX do something with name template */

	while (count) {
		if (track) {
			session_add_audio_track (input_chan, output_chan);
		} else {
			session_add_audio_bus (input_chan, output_chan);
		}
		--count;
		
		while (Main::events_pending()) {
			Main::iteration ();
		}
	}
}

XMLNode*
ARDOUR_UI::mixer_settings () const
{
	XMLNode* node = 0;

	if (session) {
		node = session->instant_xml(X_("Mixer"), session->path());
	} else {
		node = Config->instant_xml(X_("Mixer"), Config->get_user_ardour_path());
	}

	if (!node) {
		node = new XMLNode (X_("Mixer"));
	}

	return node;
}

XMLNode*
ARDOUR_UI::editor_settings () const
{
	XMLNode* node = 0;

	if (session) {
		node = session->instant_xml(X_("Editor"), session->path());
	} else {
		node = Config->instant_xml(X_("Editor"), Config->get_user_ardour_path());
	}

	if (!node) {
		node = new XMLNode (X_("Editor"));
	}
	return node;
}

XMLNode*
ARDOUR_UI::keyboard_settings () const
{
	XMLNode* node = 0;

	node = Config->extra_xml(X_("Keyboard"));
	
	if (!node) {
		node = new XMLNode (X_("Keyboard"));
	}
	return node;
}

void
ARDOUR_UI::halt_on_xrun_message ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::halt_on_xrun_message));

	ArdourMessage msg (editor, X_("haltonxrun"),
			   _("Recording was stopped because your system could not keep up."));
}

void 
ARDOUR_UI::delete_sources_in_the_right_thread (list<ARDOUR::Source*>* deletion_list)
{
	ENSURE_GUI_THREAD (bind (mem_fun(*this, &ARDOUR_UI::delete_sources_in_the_right_thread), deletion_list));

	for (list<Source*>::iterator i = deletion_list->begin(); i != deletion_list->end(); ++i) {
		delete *i;
	}

	delete deletion_list;
}

void
ARDOUR_UI::disk_overrun_handler ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::disk_underrun_handler));

	if (!have_disk_overrun_displayed) {
		have_disk_overrun_displayed = true;
		ArdourMessage msg (editor, X_("diskrate dialog"), _("\
The disk system on your computer\n\
was not able to keep up with Ardour.\n\
\n\
Specifically, it failed to write data to disk\n\
quickly enough to keep up with recording.\n"));
		have_disk_overrun_displayed = false;
	}
}

void
ARDOUR_UI::disk_underrun_handler ()
{
	ENSURE_GUI_THREAD (mem_fun(*this, &ARDOUR_UI::disk_underrun_handler));

	if (!have_disk_underrun_displayed) {
		have_disk_underrun_displayed = true;
		ArdourMessage msg (editor, X_("diskrate2 dialog"),
			(_("The disk system on your computer\n\
was not able to keep up with Ardour.\n\
\n\
Specifically, it failed to read data from disk\n\
quickly enough to keep up with playback.\n")));
		have_disk_underrun_displayed = false;
	} 
}

void
ARDOUR_UI::disk_underrun_message_gone ()
{
	have_disk_underrun_displayed = false;
}

void
ARDOUR_UI::disk_overrun_message_gone ()
{
	have_disk_underrun_displayed = false;
}

int
ARDOUR_UI::pending_state_dialog ()
{
	ArdourDialog dialog ("pending state dialog");
	Button use_button (_("Recover from crash"));
	Button cancel_button (_("Ignore crash data"));
	Label  message (_("\
This session appears to have been in\n\
middle of recording when ardour or\n\
the computer was shutdown.\n\
\n\
Ardour can recover any captured audio for\n\
you, or it can ignore it. Please decide\n\
what you would like to do.\n"));
	HBox hpacker;
	VBox vpacker;

	vpacker.set_border_width (12);
	vpacker.set_spacing (7);
	vpacker.pack_start (message);
	vpacker.pack_start (hpacker);

	hpacker.set_spacing (7);
	hpacker.pack_start (use_button);
	hpacker.pack_start (cancel_button);
	
	use_button.signal_clicked().connect (bind (mem_fun (dialog, &ArdourDialog::stop), 0));
	cancel_button.signal_clicked().connect (bind (mem_fun (dialog, &ArdourDialog::stop), 1));

	dialog.add (vpacker);
	dialog.set_position (GTK_WIN_POS_CENTER);
	dialog.show_all ();
	dialog.realize();
	dialog.get_window().set_decorations (GdkWMDecoration (GDK_DECOR_BORDER|GDK_DECOR_RESIZEH));
	
	dialog.run ();

	if (dialog.run_status () == 0) {
		return 1;
	}

	return 0;
}
	
	
void
ARDOUR_UI::disconnect_from_jack ()
{
	if (engine) {
		if( engine->disconnect_from_jack ()) {
			ArdourMessage msg (editor, X_("nojack dialog"),
					   _("Could not disconnect from JACK"));
		}

		update_sample_rate (0);
	}
}

void
ARDOUR_UI::reconnect_to_jack ()
{
	if (engine) {
		if (engine->reconnect_to_jack ()) {
			ArdourMessage msg (editor, X_("nojack dialog"),
					   _("Could not reconnect to JACK"));
		}

		update_sample_rate (0);
	}
}

void
ARDOUR_UI::set_jack_buffer_size (jack_nframes_t nframes)
{
	engine->request_buffer_size (nframes);
	update_sample_rate (0);
}

int
ARDOUR_UI::cmdline_new_session (string path)
{
	if (path[0] != '/') {
		char buf[PATH_MAX+1];
		string str;

		getcwd (buf, sizeof (buf));
		str = buf;
		str += '/';
		str += path;
		path = str;
	}

	new_session (false, path);

	_will_create_new_session_automatically = false; /* done it */
	return FALSE; /* don't call it again */
}
