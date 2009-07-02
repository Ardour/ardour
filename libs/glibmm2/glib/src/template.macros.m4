dnl-----------------------------------------------------------------------
dnl 
dnl Karls M4 macros for the signal system used by gtk--
dnl 
dnl  Copyright (C) 1998-2002 The gtkmm Development Team
dnl 
dnl  Currently maintained by Tero Pulkkinen. <terop@modeemi.cs.tut.fi>
dnl                                                         
dnl  This library is free software; you can redistribute it and/or
dnl  modify it under the terms of the GNU Lesser General Public
dnl  License as published by the Free Software Foundation; either
dnl  version 2.1 of the License, or (at your option) any later version.
dnl 
dnl  This library is distributed in the hope that it will be useful,
dnl  but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
dnl  Lesser General Public License for more details.
dnl 
dnl  You should have received a copy of the GNU Lesser General Public
dnl  License along with this library; if not, write to the Free
dnl  Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
dnl 
dnl-----------------------------------------------------------------------
dnl  Recursion prevention.  (Don't attempt to understand why this works!) 
changequote(, )dnl
changequote([, ])dnl
pushdef([DIVERSION],divnum)dnl
divert(-1)dnl

ifdef([__template_macros__],[],[
define(__template_macros__)
dnl-----------------------------------------------------------------------


dnl
dnl  M4 macros for general sanity 
dnl

dnl  M4 Quotas are hard to work with, so use braces like autoconf
dnl    (which are matched by vi, emacs)
changequote(, )
changequote([, ])

dnl
dnl  M4 comments conflict with compiler directives
changecom(, )

dnl  BRACE(text) => [text]
dnl    When we want something to appear with braces 
define([BRACE],[[[$*]]])

dnl
dnl  PROT(macro)
dnl    If a macro generates an output with commas we need to protect it
dnl    from being broken down and interpreted
define([PROT],[[$*]])

dnl
dnl  LOWER(string)
dnl    lowercase a string
define([LOWER],[translit([$*],[ABCDEFGHIJKLMNOPQRSTUVWXYZ],[abcdefghijklmnopqrstuvwxyz])])

dnl
dnl  UPPER(string)
dnl    uppercase a string
define([UPPER],[translit([$*],[abcdefghijklmnopqrstuvwxyz],[ABCDEFGHIJKLMNOPQRSTUVWXYZ])])
define([UPPER_SAFE],[translit([$*],[abcdefghijklmnopqrstuvwxyz.-],[ABCDEFGHIJKLMNOPQRSTUVWXYZ__])])

dnl
dnl  BASENAME(string)
dnl    extract the basename of a string
define([BASENAME],[patsubst([$*],[^.*/],[])])

dnl
dnl  M4NAME(string)
dnl    extract the basename of a string
define([M4NAME],[patsubst(BASENAME([$*]),[\.m4$],[])])

dnl  NUM(arg,arg,...)
dnl    M4 defines $# very badly (empty list=1).  So we need a better one
define([NUM],[ifelse(len([$*]),0,0,[$#])])

dnl
dnl  IF(cond,string1,string2)
dnl    places string1 if length (without spaces) of cond is zero,
dnl    else string2
define([IF],[ifelse(len(PROT(translit([$1],[ ]))),0,[$3],[$2])])
dnl define([IF],[ifelse(len(PROT(patsubst([$1],[ ]))),0,[$3],[$2])])

dnl
dnl minclude(filename)
dnl   This includes only the macros from a file but throws away the output.
dnl   Used to take the macros from a file without getting it extra output.
define([minclude],[IF([$1],[dnl
pushdef([CURRENT_DIVERSION],divnum)dnl
divert(-1)
include($1)
divert(CURRENT_DIVERSION)dnl
popdef([CURRENT_DIVERSION])dnl],[[minclude]])])

dnl
dnl    makes the current filename into a string approprate for use as 
dnl    C identified define.  (Defaults to this library name)
dnl
dnl    example:  (filename test.hh.m4) 
dnl      __header__          => SIGCXX_TEST_H 
dnl      __header__(MYHEAD)  => MYHEAD_TEST_H 
dnl define([__header__],[ifelse($1,,[SIGCXX],UPPER($1))[_]UPPER(patsubst(translit(BASENAME(__file__),[.-],[__]),[_m4],[]))])
define([__header__],[ifelse($1,,[_GLIBMM],UPPER($1))[_]UPPER_SAFE(M4NAME(__file__))])

dnl
dnl Set of M4 macros for variable argument template building
dnl

dnl ARGS(name,number)
dnl  Builds a comma seperated protected list of numbered names  
dnl  Use this as short hand to specify arguement names
dnl
dnl  ARGS(arg,3)  => ARG1,ARG2,ARG3
define([_ARGS],[ifelse(eval($2<$3),0,[$1$2],[$1$2,_ARGS($1,eval($2+1),$3)])])
define([ARGS],[ifelse(eval($2>0),1,[PROT(_ARGS(UPPER([$1]),1,$2))],[PROT])])

dnl
dnl LIST(string1,string2,...)
dnl   These are intended for making extended argument lists
dnl   parameters are in pairs,  the first is output if the 
dnl   2nd is nonzero length,  the process is then repeated 
dnl   with the next set of arguments.
dnl
dnl   Macro expansions that expand to result in commas must call
dnl   PROT to prevent permature expansion.  ARG* macros do
dnl   this automatically.  (If unsure, add braces until it stops
dnl   interpreting inter macros, remove one set of braces, if
dnl   still not right use PROT)
dnl 
dnl   (LIST is probably the most useful macro in the set.)
define([LIST],[ifelse($#,0,,$#,1,[$1],[$1],,[LIST(shift($@))],[__LIST($@)])])
define([__LIST],[ifelse($#,0,,$#,1,[$1],[$1[]ifelse([$2],,,[[,]])__LIST(shift($@))])])

dnl
dnl ARG_LOOP(macro_name,seperator,argument_list)
dnl   Very powerful macro for construction of list of variables
dnl   formated in specify ways.  To use define a macro taking
dnl   one variable which is called the format.  The second argument
dnl   is a seperator which will appear between each argument.
dnl   The rest is then interpreted as arguments to form the list.
dnl
dnl   Example:
dnl     define([FOO],[foo([$1])])
dnl     ARG_LOOP([FOO],[[, ]],A,B,C)
dnl
dnl     Gives:  foo(A), foo(B), foo(C)
dnl 
define([_ARG_LOOP],[dnl
ifelse(NUM($*),0,,NUM($*),1,[dnl
indir(LOOP_FORMAT,[$1])],[dnl
indir(LOOP_FORMAT,[$1])[]LOOP_SEPERATOR[]_ARG_LOOP(shift($*))])])

define([ARG_LOOP],[dnl
pushdef([LOOP_FORMAT],[[$1]])dnl
pushdef([LOOP_SEPERATOR],[$2])dnl
_ARG_LOOP(shift(shift($*)))[]dnl
popdef([LOOP_FORMAT])dnl
popdef([LOOP_SEPERATOR])dnl
])


dnl 
dnl  Define some useful formats for use with ARG_LOOP.
define([FORMAT_ARG_CLASS],[class [$1]])
define([FORMAT_ARG_BOTH],[[$1] LOWER([$1])])
define([FORMAT_ARG_REF],[Type<[$1]>::ref LOWER([$1])])
define([FORMAT_ARG_TYPE],[[$1]])
define([FORMAT_ARG_NAME],[LOWER($1)])
define([FORMAT_ARG_CBNAME],[LOWER($1)_])
define([FORMAT_ARG_CBDECL],[[$1] LOWER([$1])_;])
define([FORMAT_ARG_CBINIT],[LOWER([$1])_(LOWER([$1]))])


dnl
dnl  The following functions generate various types of parameter lists
dnl    For parameter lists
dnl      ARG_CLASS([P1,P2]) ->  class P1,class P2
dnl      ARG_BOTH([P1,P2])  ->  P1 p1,P2 p2
dnl      ARG_TYPE([P1,P2])  ->  P1,P2
dnl      ARG_NAME([P1,P2])  ->  p1,p2
dnl    For callback lists
dnl      ARG_CBNAME([C1,C2]) ->  c1_,c2_
dnl      ARG_CBINIT([C1,C2]) ->  c1_(c1),c2_(c2)
dnl      ARG_CBDECL([C1,C2]) ->  C1 c1_; C2 c2_;
dnl
define([ARG_CLASS],[PROT(ARG_LOOP([FORMAT_ARG_CLASS],[[,]],$*))])
define([ARG_BOTH],[PROT(ARG_LOOP([FORMAT_ARG_BOTH],[[,]],$*))])
define([ARG_REF],[PROT(ARG_LOOP([FORMAT_ARG_REF],[[,]],$*))])
define([ARG_TYPE],[PROT([$*])])
define([ARG_NAME],[PROT(LOWER($*))])
define([ARG_CBNAME],[PROT(ARG_LOOP([FORMAT_ARG_CBNAME],[[,]],$*))])
define([ARG_CBDECL],[PROT(ARG_LOOP([FORMAT_ARG_CBDECL],[ ],$*))])
define([ARG_CBINIT],[PROT(ARG_LOOP([FORMAT_ARG_CBINIT],[[,]],$*))])


dnl
dnl T_DROP(string)
dnl   Removes unnecessary <> with empty templates
dnl   (occasionally useful)
define([T_DROP],[ifelse([$1],<>,,[$*])])

dnl
dnl DROP(string,drop)
dnl   Removes unnecessary strings if they match drop
dnl   (occasionally useful)
define([DROP],[ifelse([$1],[$2],,[$*])])

dnl
dnl LINE(linenum) 
dnl   places a #line statement if __debug__ set
dnl   Use this at top of macro template and following 
dnl   macros that contain newlines.
dnl
dnl   example:  
dnl      LINE(]__line__[)dnl
define([LINE],[ifdef([__debug__],[#line $1 "]__file__["
])])

dnl-----------------------------------------------------------------------
dnl End of recursion protection.  Do not put anything below this line.
])
divert(DIVERSION)dnl
popdef([DIVERSION])dnl
