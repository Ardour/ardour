MAINTAINERCLEANFILES = autom4te.cache  Makefile.in aclocal.m4 configure \
						config.h.in stamp-h.in config.guess mkinstalldirs \
                        missing install-sh config.sub ltconfig \
                        ltmain.sh acinclude.m4 config.log config.status \
						depcomp ClReader.cc ClReader.hh ClReader-lex.cc

EXTRA_DIST = ClReader.l ClReader.y

SUBDIRS = cassowary

noinst_LIBRARIES    = libcassowary.a

libcassowary_a_SOURCES = \
	ClAbstractVariable.cc \
	ClConstraint.cc \
	ClLinearExpression.cc \
	ClSolver.cc \
	ClSimplexSolver.cc \
	ClStrength.cc \
	ClSymbolicWeight.cc \
	ClTableau.cc \
	ClVariable.cc \
	ClFloatVariable.cc \
	ClSlackVariable.cc \
	ClDummyVariable.cc \
	ClReader-lex.cc

ClReader-lex.cc: ClReader.l cassowary/ClReader.h ClReader.cc 
	$(LEX) -Pcl -o$@ $<

ClReader.cc: ClReader.y cassowary/ClReader.h
	$(YACC) -d -pcl --output-file $@ $<
